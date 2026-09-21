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
    // ReplaceRule: 就地替换规则的模式串，**保持其在列表中的位置不变**
    // index:      规则索引
    // newPattern: 新的模式串
    // 返回:       是否替换成功（索引越界或模式串为空时返回 false，不做任何改动）
    // 用途:       规则页编辑某条规则时调用。若改用「删除 + 追加」实现，
    //             该规则会被挪到列表末尾，静默改变其匹配优先级，
    //             且表格显示顺序会与引擎实际顺序脱节。
    bool ReplaceRule(int index, const QString& newPattern);
    // RemoveRule: 按索引删除规则
    void RemoveRule(int index);
    // MoveRule: 将规则从 from 移动到 to（单步移动；整表重排见 ApplyRulesOrder）
    // 说明: 应用层拖拽排序目前走 ApplyRulesOrder 整表同步，本方法是单步变体，
    //       供「只挪一条」的场景使用（现有调用点为规则引擎探针）。
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

    // NormalizePattern: 把「点 + 扩展名」的裸写法补成通配形式（`.exe` → `*.exe`）
    // raw:          用户输入的原始模式串
    // keepPatterns: 已知的完整文件名集合（通常传 KeepRules()），命中的不做补全
    // 返回:         补全后的模式串；不该补全时原样返回
    // 不做补全的情形：
    //   1) 不以点开头
    //   2) 含通配符或路径分隔符（*.obj、build/ 等本身已是完整写法）
    //   3) 点后仍含点（.a.b 视为完整文件名）
    //   4) 命中 keepPatterns（.gitignore 等整体就是文件名，补成 *.gitignore 反而匹配不到）
    static QString NormalizePattern(const QString& raw, const QStringList& keepPatterns);

    // FormatRuleLine: 把一条规则格式化为规则文件的一行（制表符分隔：模式串 \t 类型）
    // rule: 规则条目
    // 返回: 形如 "*.obj\t清理" 的单行文本（不含换行符）
    static QString FormatRuleLine(const RuleEntry& rule);

    // ParseRuleLine: 解析规则文件的一行
    // line:    原始行，允许首尾空白
    // pattern: 输出——解析出的模式串（已 trim）
    // isKeep:  输出——true = 保留规则，false = 清理规则
    // 返回:    true = 该行是一条有效规则；false = 应跳过（空行或 # 开头的注释行）
    // 格式：制表符分隔；第一列为模式串，第二列可选，仅当为「保留」时按保留规则处理，
    //       其余（含缺省）一律按清理规则处理
    static bool ParseRuleLine(const QString& line, QString& pattern, bool& isKeep);

private:
    // 将通配规则编译为正则
    static QRegularExpression CompilePattern(const QString& pattern, bool isDirRule);

    // 对单个规则条目的匹配
    bool MatchRule(const RuleEntry& entry, const QString& name) const;

    // SyncPatternLists: 由 m_rules 重建两个模式串清单
    // 必要性：m_cleanPatterns / m_keepPatterns 是 m_rules 按类型分列出来的视图，
    //         必须在【每个改动 m_rules 的方法】末尾重建，否则清单会停在旧内容上。
    //         曾漏掉 RemoveRule，删掉的规则仍留在清单里——
    //         KeepRules() 正是 NormalizePattern 判定「完整文件名」的依据，留残留会误判。
    void SyncPatternLists();

    QList<RuleEntry> m_rules;        // 所有规则（保持优先级顺序）

    QStringList m_cleanPatterns;     // 清理规则原始文本
    QStringList m_keepPatterns;      // 保留规则原始文本
};

#endif // RULEENGINE_H
