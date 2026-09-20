// 探针：RuleEngine 规则顺序调整（拖拽排序同步的引擎侧语义）
// 覆盖：ApplyRulesOrder 按索引重排、尺寸不匹配拒绝、越界索引跳过、
//       MoveRule 前移/后移、MoveRule 越界拒绝、恒等重排不改变内容
//
// 注意：RuleEngine 构造函数会调用 LoadBuiltinRules()，新实例自带内置规则。
//       本探针先在自定义规则追加完成后取 base = 内置规则条数，
//       所有顺序断言只作用于 [base, base+5) 这一段，不依赖内置规则的具体内容。
// 依赖：Qt5::Core

#include <QCoreApplication>
#include <algorithm>
#include <initializer_list>
#include <iostream>

#include "rules/RuleEngine.h"

static int g_passCount = 0;
static int g_failCount = 0;

static void Check(bool condition, const QString& description)
{
    if (condition)
    {
        std::cout << "[PASS] " << description.toStdString() << std::endl;
        ++g_passCount;
    }
    else
    {
        std::cout << "[FAIL] " << description.toStdString() << std::endl;
        ++g_failCount;
    }
}

// 取出 [base, end) 这段规则的模式串，用于自定义规则的顺序断言
static QStringList TailPatterns(const RuleEngine& engine, int base)
{
    const auto rules = engine.GetRules();
    QStringList out;
    for (int i = base; i < rules.size(); ++i)
    {
        out << rules[i].pattern;
    }
    return out;
}

// 取出 [base, end) 这段规则的类型，用于验证重排不串类型
static QList<RuleType> TailTypes(const RuleEngine& engine, int base)
{
    const auto rules = engine.GetRules();
    QList<RuleType> out;
    for (int i = base; i < rules.size(); ++i)
    {
        out << rules[i].type;
    }
    return out;
}

// 构造重排索引：前 base 条内置规则保持原位，后 5 条自定义规则按 order 指定的相对顺序排列
static QList<int> MakeOrder(int base, std::initializer_list<int> order)
{
    QList<int> idx;
    for (int i = 0; i < base; ++i)
    {
        idx << i;
    }
    for (int v : order)
    {
        idx << (base + v);
    }
    return idx;
}

// 构造 count 个相同值的索引列表（Qt5 QList 无 (count, value) 构造函数，手工填充）
static QList<int> RepeatIndex(int count, int value)
{
    QList<int> out;
    for (int i = 0; i < count; ++i)
    {
        out << value;
    }
    return out;
}

// 构造一个已追加 5 条自定义规则（3 清理 + 2 保留）的引擎，并回填 base
static RuleEngine MakeEngine(int& base)
{
    RuleEngine engine;
    base = engine.GetRules().size();          // 内置规则条数（36 清理 + 22 保留 = 58）
    engine.AddCleanRule("*.c1");
    engine.AddCleanRule("*.c2");
    engine.AddCleanRule("*.c3");
    engine.AddKeepRule("*.k1");
    engine.AddKeepRule("*.k2");
    return engine;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== RuleEngine 规则顺序探针 ===" << std::endl;
    std::cout << std::endl;

    // 基线确认：构造即携带内置规则，且自定义规则追加在末尾
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        Check(base > 0, QString("构造函数已加载内置规则（base=%1 条）").arg(base));
        const QStringList tail = TailPatterns(engine, base);
        const QStringList expected = {"*.c1", "*.c2", "*.c3", "*.k1", "*.k2"};
        Check(tail == expected, "自定义规则追加在末尾且顺序为 c1 c2 c3 k1 k2");
    }

    const QStringList expectedBase = {"*.c1", "*.c2", "*.c3", "*.k1", "*.k2"};

    // 1. 恒等重排：顺序不变
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        engine.ApplyRulesOrder(MakeOrder(base, {0, 1, 2, 3, 4}));
        Check(TailPatterns(engine, base) == expectedBase, "恒等重排后顺序不变");
    }

    // 2. 逆序重排：自定义段顺序完全反转，内置段不受影响
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        engine.ApplyRulesOrder(MakeOrder(base, {4, 3, 2, 1, 0}));
        const QStringList expected = {"*.k2", "*.k1", "*.c3", "*.c2", "*.c1"};
        Check(TailPatterns(engine, base) == expected, "逆序重排后自定义段顺序反转");
    }

    // 3. 尺寸不匹配：整体拒绝，顺序不变
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        const QStringList before = TailPatterns(engine, base);
        const int total = engine.GetRules().size();
        engine.ApplyRulesOrder(RepeatIndex(total - 1, 0));   // 少一个
        Check(TailPatterns(engine, base) == before, "索引数量不足时拒绝重排（顺序不变）");
        engine.ApplyRulesOrder(RepeatIndex(total + 1, 0));   // 多一个
        Check(TailPatterns(engine, base) == before, "索引数量过多时拒绝重排（顺序不变）");
    }

    // 4. 越界索引：被跳过，只保留合法项
    //    注意 MakeOrder 会给相对值加上 base，负值会被映射回内置段，
    //    因此真正的越界值必须在构造完索引列表后直接插入
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        const int total = engine.GetRules().size();
        QList<int> idx = MakeOrder(base, {0, 2, 4});   // 内置段 + c1 c3 k2
        idx.insert(1, -1);                              // 越界负索引
        idx.insert(3, 9999);                            // 越界正索引（远大于规则总数）
        engine.ApplyRulesOrder(idx);
        const QStringList expected = {"*.c1", "*.c3", "*.k2"};
        Check(TailPatterns(engine, base) == expected, "越界索引被跳过，仅保留合法项");
        Check(engine.GetRules().size() < total, "越界跳过后规则总数减少");
    }

    // 5. MoveRule 向后移动（模拟拖拽把首条拖到末位）
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        const int total = engine.GetRules().size();
        engine.MoveRule(base, total - 1);
        const QStringList expected = {"*.c2", "*.c3", "*.k1", "*.k2", "*.c1"};
        Check(TailPatterns(engine, base) == expected, "MoveRule 向后移动：首条移至末位");
    }

    // 6. MoveRule 向前移动（模拟把末条拖到首位）
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        const int total = engine.GetRules().size();
        engine.MoveRule(total - 1, base);
        const QStringList expected = {"*.k2", "*.c1", "*.c2", "*.c3", "*.k1"};
        Check(TailPatterns(engine, base) == expected, "MoveRule 向前移动：末条移至首位");
    }

    // 7. MoveRule 越界：不改变任何内容
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        const QStringList before = TailPatterns(engine, base);
        const int total = engine.GetRules().size();
        engine.MoveRule(-1, 2);
        engine.MoveRule(0, total);
        engine.MoveRule(total + 5, 1);
        Check(TailPatterns(engine, base) == before, "MoveRule 越界调用不改变顺序");
    }

    // 8. 重排不丢失规则数量
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        const int total = engine.GetRules().size();
        engine.ApplyRulesOrder(MakeOrder(base, {1, 0, 3, 2, 4}));
        Check(engine.GetRules().size() == total, "重排后规则总数不变");
    }

    // 9. 重排后规则类型仍跟随原规则（不串类型）
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        engine.ApplyRulesOrder(MakeOrder(base, {3, 0, 4, 1, 2}));
        const QStringList patterns = TailPatterns(engine, base);
        const QList<RuleType> types = TailTypes(engine, base);
        const QStringList expectedPatterns = {"*.k1", "*.c1", "*.k2", "*.c2", "*.c3"};
        Check(patterns == expectedPatterns, "重排后模式顺序正确（k1 c1 k2 c2 c3）");
        if (types.size() == 5)
        {
            Check(types[0] == RuleType::Keep && types[1] == RuleType::Clean
                      && types[2] == RuleType::Keep && types[3] == RuleType::Clean
                      && types[4] == RuleType::Clean,
                  "重排后类型跟随原规则（Keep Clean Keep Clean Clean）");
        }
        else
        {
            Check(false, "重排后类型列表长度应为 5");
        }
    }

    // 10. 空索引重排：尺寸不匹配故被拒绝，引擎内容不变（不应崩溃）
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        const int total = engine.GetRules().size();
        const QStringList before = TailPatterns(engine, base);
        engine.ApplyRulesOrder({});
        Check(engine.GetRules().size() == total, "空索引重排被拒绝且不崩溃（总数不变）");
        Check(TailPatterns(engine, base) == before, "空索引重排后内容不变");
    }

    // ===== BuildReorderedIndices：规则管理页两张子表 → 引擎索引重排序列 =====
    // 注意：界面两张子表展示的是该类型的【全部】规则（含内置），
    //       因此这里传入的必须是完整列表，只给自定义子集会因数量对不上而被拒绝

    // 取某类型的全部模式串（按引擎当前顺序），即表格中应有的行序
    auto cleanAllOf = [](const RuleEngine& e) -> QStringList
    {
        QStringList out;
        for (const auto& r : e.GetRules())
        {
            if (r.type == RuleType::Clean) { out << r.pattern; }
        }
        return out;
    };
    auto keepAllOf = [](const RuleEngine& e) -> QStringList
    {
        QStringList out;
        for (const auto& r : e.GetRules())
        {
            if (r.type == RuleType::Keep) { out << r.pattern; }
        }
        return out;
    };

    // 11. 传入引擎自身的顺序时应得到恒等索引
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        const QList<int> idx = RuleEngine::BuildReorderedIndices(
            engine.GetRules(), cleanAllOf(engine), keepAllOf(engine));
        Check(idx.size() == engine.GetRules().size(), "顺序一致时返回完整索引序列");
        bool identity = (idx.size() == engine.GetRules().size());
        for (int i = 0; identity && i < idx.size(); ++i)
        {
            if (idx[i] != i) { identity = false; }
        }
        Check(identity, "顺序一致时索引序列为恒等映射");
    }

    // 12. 清理段整体逆序：清理规则全段反转，保留段不受影响
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        const QStringList keepBefore = keepAllOf(engine);
        QStringList cleanRev = cleanAllOf(engine);
        std::reverse(cleanRev.begin(), cleanRev.end());

        const QList<int> idx = RuleEngine::BuildReorderedIndices(
            engine.GetRules(), cleanRev, keepBefore);
        engine.ApplyRulesOrder(idx);

        Check(cleanAllOf(engine) == cleanRev, "清理段整体逆序后生效");
        Check(keepAllOf(engine) == keepBefore, "清理段逆序不影响保留段顺序");
    }

    // 13. 关键：跨类型排布不被破坏
    //    构造 Clean Keep Clean Keep 的交错尾部，仅交换同类型内部顺序
    {
        RuleEngine engine;
        const int base = engine.GetRules().size();
        engine.AddCleanRule("*.x1");
        engine.AddKeepRule("*.y1");
        engine.AddCleanRule("*.x2");
        engine.AddKeepRule("*.y2");

        // 仅在各自类型内把最后两条对调，保持其它规则原位
        QStringList co = cleanAllOf(engine);
        std::swap(co[co.size() - 2], co[co.size() - 1]);
        QStringList ko = keepAllOf(engine);
        std::swap(ko[ko.size() - 2], ko[ko.size() - 1]);

        const QList<int> idx = RuleEngine::BuildReorderedIndices(engine.GetRules(), co, ko);
        engine.ApplyRulesOrder(idx);

        const QStringList expected = {"*.x2", "*.y2", "*.x1", "*.y1"};
        Check(TailPatterns(engine, base) == expected, "交错排布下重排：位置不变、同类型内部换序");
        const QList<RuleType> types = TailTypes(engine, base);
        Check(types.size() == 4
                  && types[0] == RuleType::Clean && types[1] == RuleType::Keep
                  && types[2] == RuleType::Clean && types[3] == RuleType::Keep,
              "交错排布下重排：跨类型排布保持为 Clean Keep Clean Keep");
    }

    // 14. 模式串在引擎中不存在时整体拒绝，返回空列表
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        QStringList co = cleanAllOf(engine);
        co[0] = "*.NOT_EXIST";
        const QList<int> idx = RuleEngine::BuildReorderedIndices(
            engine.GetRules(), co, keepAllOf(engine));
        Check(idx.isEmpty(), "清理段含未知模式串时返回空列表（拒绝重排）");
    }
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        QStringList ko = keepAllOf(engine);
        ko[0] = "*.NOT_EXIST";
        const QList<int> idx = RuleEngine::BuildReorderedIndices(
            engine.GetRules(), cleanAllOf(engine), ko);
        Check(idx.isEmpty(), "保留段含未知模式串时返回空列表（拒绝重排）");
    }

    // 15. 某类型数量对不上时拒绝（表格少一行 / 多一行）
    {
        int base = 0;
        RuleEngine engine = MakeEngine(base);
        QStringList co = cleanAllOf(engine);
        co.removeLast();
        Check(RuleEngine::BuildReorderedIndices(engine.GetRules(), co, keepAllOf(engine)).isEmpty(),
              "清理段条目数不足时返回空列表（拒绝重排）");

        QStringList co2 = cleanAllOf(engine);
        co2 << "*.extra";
        Check(RuleEngine::BuildReorderedIndices(engine.GetRules(), co2, keepAllOf(engine)).isEmpty(),
              "清理段条目数过多时返回空列表（拒绝重排）");
    }

    // 16. 重复模式串：同名规则各占一个独立索引，不重复占用也不丢失
    {
        RuleEngine engine;
        const int base = engine.GetRules().size();
        engine.AddCleanRule("*.dup");
        engine.AddCleanRule("*.dup");

        const QList<int> idx = RuleEngine::BuildReorderedIndices(
            engine.GetRules(), cleanAllOf(engine), keepAllOf(engine));
        Check(idx.size() == engine.GetRules().size(), "重复模式串：返回完整索引序列");

        // 索引序列不应有重复项，否则重排会丢规则
        bool hasDuplicate = false;
        for (int i = 0; i < idx.size() && !hasDuplicate; ++i)
        {
            for (int j = i + 1; j < idx.size(); ++j)
            {
                if (idx[i] == idx[j]) { hasDuplicate = true; break; }
            }
        }
        Check(!hasDuplicate, "重复模式串：索引序列无重复项");

        engine.ApplyRulesOrder(idx);
        Check(engine.GetRules().size() == base + 2, "重复模式串：应用重排后规则数不丢失");
    }

    // 17. LoadBuiltinRules 幂等：重复调用不应重复累积内置规则
    //     构造函数已加载过一次，MainPresenter 又显式调用了一次；
    //     若不清空会导致内置规则翻倍（36+22 → 72+44），规则页条数与匹配开销都出错
    {
        RuleEngine engine;                                  // 构造时已加载一次
        const int afterCtor = engine.GetRules().size();
        engine.LoadBuiltinRules();                          // 再调一次
        const int afterSecond = engine.GetRules().size();
        Check(afterSecond == afterCtor,
              QString("LoadBuiltinRules 幂等: 重复调用后仍为 %1 条").arg(afterCtor));

        int cleanCount = 0;
        int keepCount = 0;
        for (const auto& r : engine.GetRules())
        {
            if (r.type == RuleType::Clean) { ++cleanCount; }
            else { ++keepCount; }
        }
        Check(cleanCount == 36, QString("内置清理规则 36 条（实际 %1）").arg(cleanCount));
        Check(keepCount == 22, QString("内置保留规则 22 条（实际 %1）").arg(keepCount));
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount == 0 ? 0 : 1;
}
