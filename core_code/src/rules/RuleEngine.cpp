// 规则引擎实现：内置规则加载 + glob → 正则编译 + 文件/目录匹配，保留规则优先级最高
#include "RuleEngine.h"

#include <QFileInfo>
#include <QDir>

RuleEngine::RuleEngine()
{
    LoadBuiltinRules();
}

QRegularExpression RuleEngine::CompilePattern(const QString& pattern, bool isDirRule)
{
    QString p = pattern;

    // 目录规则匹配目录名，去掉尾部 / 以得到纯名称
    if (isDirRule && p.endsWith('/'))
    {
        p.chop(1);
    }

    // Qt 5.15+ 原生 wildcardToRegularExpression 将 glob 转为正则
    QRegularExpression rx(QRegularExpression::wildcardToRegularExpression(p));
    rx.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
    return rx;
}

void RuleEngine::LoadBuiltinRules()
{
    // 先清空再加载，保证幂等：构造函数已调用过一次，调用方若再调一次不应重复累积
    // （重复累积会让内置规则翻倍，规则页显示条数与匹配开销都随之出错）
    m_rules.clear();
    SyncPatternLists();

    // IDE 缓存
    QStringList ideCacheDirs = {".vs", ".db", ".idea", ".history", ".vscode"};
    for (const auto& d : ideCacheDirs)
    {
        AddCleanRule(d + "/");
    }
    QStringList ideCacheFiles = {".suo", ".user", ".userosscache", ".sdf", ".cache"};
    for (const auto& f : ideCacheFiles)
    {
        AddCleanRule(f);
    }

    // 编译中间产物
    QStringList compileFiles = {"*.obj", "*.o", "*.ilk", "*.ipch", "*.pch",
                                "*.idb", "*.ipdb", "*.iobj", "*.tlog",
                                "*.lastbuildstate", "*.exp", "*.lib", "*.db"};
    for (const auto& f : compileFiles)
    {
        AddCleanRule(f);
    }

    // 调试/输出
    AddCleanRule("*.pdb");

    // 临时文件
    QStringList tempFiles = {"*.tmp", "*.temp", "*.log", "*~"};
    for (const auto& f : tempFiles)
    {
        AddCleanRule(f);
    }

    // 构建输出目录
    QStringList buildDirs = {"build/", "debug/", "release/", "out/", "bin/", "obj/",
                             "__pycache__/"};
    for (const auto& d : buildDirs)
    {
        AddCleanRule(d);
    }
    AddCleanRule("cmake-build-*/");

    // 保留规则：源码文件
    QStringList sourceFiles = {"*.cpp", "*.c", "*.cc", "*.cxx",
                                "*.h", "*.hpp", "*.hh", "*.inl"};
    for (const auto& f : sourceFiles)
    {
        AddKeepRule(f);
    }

    // Qt 文件
    QStringList qtFiles = {"*.ui", "*.qrc", "*.ts", "*.pro", "*.pri", "*.prf"};
    for (const auto& f : qtFiles)
    {
        AddKeepRule(f);
    }

    // 工程配置文件
    AddKeepRule("CMakeLists.txt");
    AddKeepRule("*.cmake");
    AddKeepRule(".gitignore");
    AddKeepRule(".gitattributes");
    AddKeepRule("*.sln");        // VS 解决方案文件，不可删除

    // 文档
    QStringList docFiles = {"README*", "LICENSE*", "CHANGELOG*"};
    for (const auto& f : docFiles)
    {
        AddKeepRule(f);
    }
}

void RuleEngine::AddCleanRule(const QString& pattern)
{
    const bool isDirRule = pattern.endsWith('/');
    RuleEntry entry;
    entry.pattern = pattern;
    entry.type = RuleType::Clean;
    entry.isDirRule = isDirRule;
    entry.regex = CompilePattern(pattern, isDirRule);
    m_rules.append(entry);
    SyncPatternLists();
}

void RuleEngine::AddKeepRule(const QString& pattern)
{
    const bool isDirRule = pattern.endsWith('/');
    RuleEntry entry;
    entry.pattern = pattern;
    entry.type = RuleType::Keep;
    entry.isDirRule = isDirRule;
    entry.regex = CompilePattern(pattern, isDirRule);
    m_rules.append(entry);
    SyncPatternLists();
}

// 由 m_rules 重建两个模式串清单，保证清单与规则列表内容始终一致
void RuleEngine::SyncPatternLists()
{
    m_cleanPatterns.clear();
    m_keepPatterns.clear();
    m_cleanPatterns.reserve(m_rules.size());
    m_keepPatterns.reserve(m_rules.size());

    for (const auto& rule : m_rules)
    {
        if (rule.type == RuleType::Clean)
        {
            m_cleanPatterns.append(rule.pattern);
        }
        else
        {
            m_keepPatterns.append(rule.pattern);
        }
    }
}

bool RuleEngine::MatchRule(const RuleEntry& entry, const QString& name) const
{
    // 目录规则：提取路径最后一段目录名做匹配
    if (entry.isDirRule)
    {
        QString dirName = QDir(name).dirName();
        return entry.regex.match(dirName).hasMatch();
    }

    // 文件规则：提取文件名做匹配（忽略目录路径部分）
    QFileInfo fi(name);
    return entry.regex.match(fi.fileName()).hasMatch();
}

RuleMatch RuleEngine::MatchFile(const QString& filePath) const
{
    RuleMatch bestMatch{"未匹配", false};

    // 从后往前遍历：后添加的规则优先级更高（自定义 > 内置）
    // 保留规则一旦命中立即返回（最高优先级），清理规则取最后一个命中
    for (int i = m_rules.size() - 1; i >= 0; --i)
    {
        const auto& entry = m_rules.at(i);

        // 跳过目录规则（文件匹配只检查文件规则）
        if (entry.isDirRule)
        {
            continue;
        }

        if (MatchRule(entry, filePath))
        {
            if (entry.type == RuleType::Keep)
            {
                return {entry.pattern, false};
            }
            // 清理规则：记录最佳匹配，继续查找是否还有保留规则命中
            if (bestMatch.ruleName == "未匹配")
            {
                bestMatch = {entry.pattern, true};
            }
        }
    }

    return bestMatch;
}

RuleMatch RuleEngine::MatchDir(const QString& dirPath) const
{
    RuleMatch bestMatch{"未匹配", false};

    // 与 MatchFile 相同的优先级语义，但仅检查目录规则
    for (int i = m_rules.size() - 1; i >= 0; --i)
    {
        const auto& entry = m_rules.at(i);

        // 只匹配目录规则（如 "build/", ".vs/"）
        if (!entry.isDirRule)
        {
            continue;
        }

        if (MatchRule(entry, dirPath))
        {
            if (entry.type == RuleType::Keep)
            {
                return {entry.pattern, false};
            }
            if (bestMatch.ruleName == "未匹配")
            {
                bestMatch = {entry.pattern, true};
            }
        }
    }

    return bestMatch;
}

QStringList RuleEngine::CleanRules() const
{
    return m_cleanPatterns;
}

QStringList RuleEngine::KeepRules() const
{
    return m_keepPatterns;
}

QList<RuleEntry> RuleEngine::GetRules() const
{
    return m_rules;
}

bool RuleEngine::ReplaceRule(int index, const QString& newPattern)
{
    if (index < 0 || index >= m_rules.size() || newPattern.isEmpty())
    {
        return false;
    }

    const RuleType type = m_rules[index].type;
    const bool isDirRule = newPattern.endsWith('/');

    RuleEntry entry;
    entry.pattern = newPattern;
    entry.type = type;
    entry.isDirRule = isDirRule;
    entry.regex = CompilePattern(newPattern, isDirRule);

    // 就地替换：位置不变，故匹配优先级不变
    m_rules[index] = entry;
    SyncPatternLists();
    return true;
}

void RuleEngine::RemoveRule(int index)
{
    if (index < 0 || index >= m_rules.size())
    {
        return;
    }
    m_rules.removeAt(index);
    SyncPatternLists();
}

void RuleEngine::MoveRule(int from, int to)
{
    if (from < 0 || from >= m_rules.size() || to < 0 || to >= m_rules.size())
    {
        return;
    }
    m_rules.move(from, to);
    SyncPatternLists();
}

void RuleEngine::ApplyRulesOrder(const QList<int>& indices)
{
    if (indices.size() != m_rules.size())
    {
        return;
    }
    QList<RuleEntry> oldRules = m_rules;
    m_rules.clear();
    for (int idx : indices)
    {
        if (idx >= 0 && idx < oldRules.size())
        {
            m_rules.append(oldRules[idx]);
        }
    }
    SyncPatternLists();
}

QList<int> RuleEngine::BuildReorderedIndices(const QList<RuleEntry>& rules,
                                             const QStringList& cleanOrder,
                                             const QStringList& keepOrder)
{
    // 按类型建立「模式串 → 旧索引队列」；同一模式串可能出现多次，按出现顺序依次取用
    QHash<QString, QList<int>> cleanIdx;
    QHash<QString, QList<int>> keepIdx;
    for (int i = 0; i < rules.size(); ++i)
    {
        if (rules[i].type == RuleType::Clean)
        {
            cleanIdx[rules[i].pattern].append(i);
        }
        else
        {
            keepIdx[rules[i].pattern].append(i);
        }
    }

    // 把表格行序翻译为引擎索引；模式串在引擎中不存在说明表格已与引擎脱节，整体拒绝
    QList<int> cleanTargets;
    QList<int> keepTargets;
    for (const auto& pattern : cleanOrder)
    {
        QList<int>& queue = cleanIdx[pattern];
        if (queue.isEmpty())
        {
            return {};
        }
        cleanTargets.append(queue.takeFirst());
    }
    for (const auto& pattern : keepOrder)
    {
        QList<int>& queue = keepIdx[pattern];
        if (queue.isEmpty())
        {
            return {};
        }
        keepTargets.append(queue.takeFirst());
    }

    // 逐位回填：遇到清理位取下一个清理索引，遇到保留位取下一个保留索引
    // 引擎可能并非「清理段在前、保留段在后」（自定义规则按添加顺序追加），
    // 逐位回填只重排同类型内部顺序，不会破坏跨类型的优先级结构
    QList<int> newOrder;
    newOrder.reserve(rules.size());
    int ci = 0;
    int ki = 0;
    for (const auto& rule : rules)
    {
        if (rule.type == RuleType::Clean)
        {
            if (ci >= cleanTargets.size())
            {
                return {};
            }
            newOrder.append(cleanTargets[ci++]);
        }
        else
        {
            if (ki >= keepTargets.size())
            {
                return {};
            }
            newOrder.append(keepTargets[ki++]);
        }
    }
    return newOrder;
}

QString RuleEngine::NormalizePattern(const QString& raw, const QStringList& keepPatterns)
{
    // 仅处理「点 + 扩展名」这一种形态
    if (!raw.startsWith('.'))
    {
        return raw;
    }
    // 已含通配符或路径分隔符：本身是完整写法
    if (raw.contains('*') || raw.contains('?') || raw.contains('[')
        || raw.contains('/') || raw.contains('\\'))
    {
        return raw;
    }
    // 点后仍含点：按完整文件名处理（如 .a.b）
    if (raw.mid(1).contains('.'))
    {
        return raw;
    }
    // 已是保留规则中的完整文件名：补成通配反而匹配不到
    if (keepPatterns.contains(raw, Qt::CaseInsensitive))
    {
        return raw;
    }
    return "*" + raw;
}

QString RuleEngine::FormatRuleLine(const RuleEntry& rule)
{
    // 导出格式：模式串 \t 类型（中文），与 ParseRuleLine 严格对应
    const QString typeStr = (rule.type == RuleType::Clean) ? "清理" : "保留";
    return rule.pattern + "\t" + typeStr;
}

bool RuleEngine::ParseRuleLine(const QString& line, QString& pattern, bool& isKeep)
{
    const QString trimmed = line.trimmed();
    // 空行与注释行跳过
    if (trimmed.isEmpty() || trimmed.startsWith('#'))
    {
        return false;
    }

    const QStringList parts = trimmed.split('\t');
    pattern = parts.value(0).trimmed();
    if (pattern.isEmpty())
    {
        return false;
    }

    // 第二列缺省按清理规则处理；仅显式写「保留」才算保留规则
    const QString typeStr = (parts.size() >= 2) ? parts.at(1).trimmed() : QString();
    isKeep = (typeStr == QStringLiteral("保留"));
    return true;
}

QString RuleEngine::GetCategory(const QString& pattern)
{
    // IDE 缓存目录
    static const QStringList ideDirs = {".vs/", ".db/", ".idea/", ".history/", ".vscode/"};
    // IDE 缓存文件
    static const QStringList ideFiles = {"*.suo", "*.user", "*.userosscache", "*.sdf", "*.cache"};

    for (const auto& d : ideDirs)
    {
        if (pattern == d) { return "IDE缓存"; }
    }
    for (const auto& f : ideFiles)
    {
        if (pattern == f) { return "IDE缓存"; }
    }

    // 编译产物
    static const QStringList compileFiles = {"*.obj", "*.o", "*.ilk", "*.ipch", "*.pch",
                                             "*.idb", "*.ipdb", "*.iobj", "*.tlog",
                                             "*.lastbuildstate", "*.exp", "*.lib"};
    for (const auto& f : compileFiles)
    {
        if (pattern == f) { return "编译产物"; }
    }

    // 调试文件
    if (pattern == "*.pdb") { return "调试文件"; }

    // 临时文件
    static const QStringList tempFiles = {"*.tmp", "*.temp", "*.log", "*~"};
    for (const auto& f : tempFiles)
    {
        if (pattern == f) { return "临时文件"; }
    }

    // 构建目录
    static const QStringList buildDirs = {"build/", "debug/", "release/", "out/", "bin/", "obj/",
                                          "__pycache__/", "cmake-build-*/"};
    for (const auto& d : buildDirs)
    {
        if (pattern == d) { return "构建目录"; }
    }

    return "其他";
}

int RuleEngine::GetCategoryPriority(const QString& category)
{
    // 编译产物优先（体积大），构建目录次之
    if (category == "编译产物") { return 0; }
    if (category == "构建目录") { return 1; }
    if (category == "调试文件") { return 2; }
    if (category == "IDE缓存") { return 3; }
    if (category == "临时文件") { return 4; }
    return 5;
}
