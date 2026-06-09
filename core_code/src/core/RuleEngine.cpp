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
                                "*.lastbuildstate", "*.exp", "*.lib"};
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

    // 文档
    QStringList docFiles = {"README*", "LICENSE*", "CHANGELOG*"};
    for (const auto& f : docFiles)
    {
        AddKeepRule(f);
    }
}

void RuleEngine::AddCleanRule(const QString& pattern)
{
    m_cleanPatterns.append(pattern);

    bool isDirRule = pattern.endsWith('/');
    RuleEntry entry;
    entry.pattern = pattern;
    entry.type = RuleType::Clean;
    entry.isDirRule = isDirRule;
    entry.regex = CompilePattern(pattern, isDirRule);
    m_rules.append(entry);
}

void RuleEngine::AddKeepRule(const QString& pattern)
{
    m_keepPatterns.append(pattern);

    bool isDirRule = pattern.endsWith('/');
    RuleEntry entry;
    entry.pattern = pattern;
    entry.type = RuleType::Keep;
    entry.isDirRule = isDirRule;
    entry.regex = CompilePattern(pattern, isDirRule);
    m_rules.append(entry);
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

void RuleEngine::RemoveRule(int index)
{
    if (index < 0 || index >= m_rules.size())
    {
        return;
    }
    m_rules.removeAt(index);
}
