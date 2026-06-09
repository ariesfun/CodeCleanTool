#ifndef RULEENGINE_H
#define RULEENGINE_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QPair>

// 规则类型
enum class RuleType
{
    Clean,  // 清理规则
    Keep    // 保留规则
};

// 单条规则：模式串 + 预编译正则
struct RuleEntry
{
    QString pattern;            // 原始模式（如 "*.obj"、"build/"）
    RuleType type;              // 规则类型
    QRegularExpression regex;   // 预编译正则
    bool isDirRule;             // 是否为目录规则（以 / 结尾）
};

// 匹配结果
struct RuleMatch
{
    QString ruleName;       // 命中规则名
    bool isCleanTarget;     // true=应清理, false=应保留
};

// 规则引擎：判断文件/目录是否命中清理或保留规则
// 优先级：保留规则 > 清理规则
class RuleEngine
{
public:
    RuleEngine();

    // 加载内置规则
    void LoadBuiltinRules();

    // 添加自定义规则
    void AddCleanRule(const QString& pattern);
    void AddKeepRule(const QString& pattern);

    // 匹配文件路径，返回匹配结果
    RuleMatch MatchFile(const QString& filePath) const;

    // 匹配目录路径（检查目录名是否命中目录规则）
    RuleMatch MatchDir(const QString& dirPath) const;

    // 获取所有规则
    QStringList CleanRules() const;
    QStringList KeepRules() const;

private:
    // 将通配规则编译为正则
    static QRegularExpression CompilePattern(const QString& pattern, bool isDirRule);

    // 对单个规则条目的匹配
    bool MatchRule(const RuleEntry& entry, const QString& name) const;

    QList<RuleEntry> m_rules;        // 所有规则（保持优先级顺序）

    QStringList m_cleanPatterns;     // 清理规则原始文本
    QStringList m_keepPatterns;      // 保留规则原始文本
};

#endif // RULEENGINE_H
