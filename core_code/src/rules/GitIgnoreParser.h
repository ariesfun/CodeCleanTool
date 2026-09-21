#ifndef GITIGNOREPARSER_H
#define GITIGNOREPARSER_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QList>

// 单条 gitignore 规则
struct GitIgnoreRule
{
    QString pattern;            // 原始模式文本
    QRegularExpression regex;   // 预编译正则
    bool isNegation;            // 是否为 ! 反向规则
    bool isDirOnly;             // 是否仅匹配目录（以 / 结尾）
    bool anchored;              // 是否含路径分隔符（锚定到 .gitignore 目录）
};

// .gitignore 解析器：读取并解析 .gitignore 规则，判断路径是否应排除
//
// 当前状态：应用层零调用点（保留的独立能力）。
// 起因：`.gitignore` 联动曾是扫描的排除依据，但它的语义与本工具相悖 ——
//       `.gitignore` 里列的通常正是构建垃圾（.vs/、build/、*.obj、*.exe），
//       恰恰是本工具要清理的对象，按其排除等于把待清理项从结果里藏起来。
//       该联动已整体移除（扫描范围改由规则单独决定，设置页也去掉了对应开关）。
// 保留理由：glob 语义（**、!、锚定）比 RuleEngine 的通配匹配完整，
//           且本身无缺陷、有独立探针覆盖，是可被复用的原子能力。
//           → 删或留都可以，但不要让它处于「既没人用、也没人说明」的状态。
class GitIgnoreParser
{
public:
    GitIgnoreParser();

    // 从文件加载 .gitignore 规则（**替换**已有规则，重复调用不会累积）
    bool LoadFromFile(const QString& filePath);

    // 解析规则文本行（**追加**到已有规则；需要替换请先调 Clear()）
    void ParseRules(const QString& text);

    // 判断相对路径是否应被忽略
    bool IsIgnored(const QString& relativePath, bool isDir = false) const;

    // 获取已加载的规则文本
    QStringList Rules() const;

    // 清空所有规则
    void Clear();

private:
    // 将单条 gitignore 规则编译为正则
    static GitIgnoreRule CompileRule(const QString& pattern);

    // 将 glob 模式转为正则（支持 **、*、?、[abc]）
    static QString GlobToRegex(const QString& glob);

    QList<GitIgnoreRule> m_rules;   // 规则列表（按加载顺序）
    QStringList m_rawRules;         // 原始规则文本
};

#endif // GITIGNOREPARSER_H
