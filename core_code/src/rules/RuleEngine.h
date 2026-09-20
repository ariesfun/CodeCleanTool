#ifndef RULEENGINE_H
#define RULEENGINE_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QPair>
#include <QHash>

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
    // GetRules: 返回所有规则条目（含类型/模式/启用状态），供规则页展示
    QList<RuleEntry> GetRules() const;
    // RemoveRule: 按索引删除规则
    void RemoveRule(int index);
    // MoveRule: 将规则从 from 移动到 to，用于单步拖拽排序同步
    void MoveRule(int from, int to);
    // ApplyRulesOrder: 按 indices 顺序重建 m_rules 列表，用于拖拽排序后整表同步
    void ApplyRulesOrder(const QList<int>& indices);

    // GetCategory: 根据命中规则名返回文件类型分类（用于 UI 颜色标签）
    static QString GetCategory(const QString& pattern);
    // GetCategoryPriority: 返回分类排序优先级（越小越靠前，清理目标优先）
    static int GetCategoryPriority(const QString& category);

    // BuildReorderedIndices: 依据规则管理页两张子表给出的新模式串顺序，计算引擎全局索引重排序列
    // rules:      引擎当前的全部规则（含类型）
    // cleanOrder: 清理段表格的行序（模式串）
    // keepOrder:  保留段表格的行序（模式串）
    // 返回:       长度等于 rules.size() 的全局索引序列，可直接交给 ApplyRulesOrder；
    //             任一模式串无法对应到规则、或某类型数量对不上时返回空列表表示拒绝重排
    // 语义:       仅改变同类型规则内部的相对顺序，保持引擎原有的跨类型排布不变
    static QList<int> BuildReorderedIndices(const QList<RuleEntry>& rules,
                                            const QStringList& cleanOrder,
                                            const QStringList& keepOrder);

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
