// 探针：RuleEngine 规则匹配
// 覆盖：内置规则加载 / 清理规则列表 / 保留规则列表 / 自定义规则
// 不依赖 Qt 完整框架，仅链接 Qt5::Core

#include <QCoreApplication>
#include <QDebug>
#include <iostream>

#include "core/RuleEngine.h"

// 每个断言打印 [PASS] 或 [FAIL]
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

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== RuleEngine 探针测试 ===" << std::endl;
    std::cout << std::endl;

    // 1. 内置清理规则加载
    {
        RuleEngine engine;
        auto cleanRules = engine.CleanRules();
        auto keepRules = engine.KeepRules();
        Check(!cleanRules.isEmpty(), QString("内置清理规则已加载: %1 条").arg(cleanRules.size()));
        Check(!keepRules.isEmpty(), QString("内置保留规则已加载: %1 条").arg(keepRules.size()));
    }

    // 2. 内置清理规则包含 IDE 缓存
    {
        RuleEngine engine;
        auto rules = engine.CleanRules();
        bool hasVs = false;
        bool hasDb = false;
        bool hasObj = false;
        for (const auto& r : rules)
        {
            if (r == ".vs/")  { hasVs = true; }
            if (r == ".db/")  { hasDb = true; }
            if (r == "*.obj") { hasObj = true; }
        }
        Check(hasVs,  "清理规则包含 .vs (IDE 缓存)");
        Check(hasDb,  "清理规则包含 .db (调试数据库)");
        Check(hasObj, "清理规则包含 *.obj (编译产物)");
    }

    // 3. 内置保留规则包含源码文件
    {
        RuleEngine engine;
        auto rules = engine.KeepRules();
        bool hasCpp = false;
        bool hasH = false;
        bool hasUi = false;
        for (const auto& r : rules)
        {
            if (r == "*.cpp") { hasCpp = true; }
            if (r == "*.h")   { hasH = true; }
            if (r == "*.ui")  { hasUi = true; }
        }
        Check(hasCpp, "保留规则包含 *.cpp (C++ 源码)");
        Check(hasH,   "保留规则包含 *.h (C++ 头文件)");
        Check(hasUi,  "保留规则包含 *.ui (Qt 界面文件)");
    }

    // 4. 自定义规则添加
    {
        RuleEngine engine;
        int beforeCount = engine.CleanRules().size();
        engine.AddCleanRule("*.custom");
        int afterCount = engine.CleanRules().size();
        Check(afterCount == beforeCount + 1, "添加自定义清理规则后数量 +1");

        int keepBefore = engine.KeepRules().size();
        engine.AddKeepRule("*.important");
        int keepAfter = engine.KeepRules().size();
        Check(keepAfter == keepBefore + 1, "添加自定义保留规则后数量 +1");
    }

    // 5. ruleMatch 空匹配测试（骨架返回值）
    {
        RuleEngine engine;
        auto result = engine.MatchFile("test.cpp");
        Check(!result.isCleanTarget, "test.cpp 默认不命中清理 (骨架)");
    }

    // 6. RemoveRule 按索引删除
    {
        RuleEngine engine;
        int before = engine.GetRules().size();
        engine.AddCleanRule("*.test_remove");
        int afterAdd = engine.GetRules().size();
        Check(afterAdd == before + 1, QString("RemoveRule: 添加后规则数 %1").arg(afterAdd));

        engine.RemoveRule(afterAdd - 1);  // 删除最后一条（刚添加的）
        int afterRemove = engine.GetRules().size();
        Check(afterRemove == before, QString("RemoveRule: 删除后规则数恢复 %1").arg(afterRemove));
    }

    // 7. RemoveRule 边界（删除末位和中间）
    {
        RuleEngine engine;
        engine.AddCleanRule("*.aaa");
        engine.AddCleanRule("*.bbb");
        engine.AddCleanRule("*.ccc");
        int initial = engine.GetRules().size();

        // 删除中间元素 *.bbb（自定义规则追加在末尾，故位置为 initial-2）
        engine.RemoveRule(initial - 2);
        auto rules = engine.GetRules();
        bool hasBbb = false;
        for (const auto& r : rules) { if (r.pattern == "*.bbb") { hasBbb = true; } }
        Check(!hasBbb, "RemoveRule: 中间元素 *.bbb 已删除");

        // 找到 *.aaa 的实际索引后删除
        int aaaIdx = -1;
        for (int i = 0; i < rules.size(); ++i) { if (rules[i].pattern == "*.aaa") { aaaIdx = i; break; } }
        Check(aaaIdx >= 0, "RemoveRule: 找到 *.aaa 的索引");
        engine.RemoveRule(aaaIdx);
        rules = engine.GetRules();
        bool hasAaa = false;
        for (const auto& r : rules) { if (r.pattern == "*.aaa") { hasAaa = true; } }
        Check(!hasAaa, "RemoveRule: *.aaa 已通过索引删除");

        Check(rules.size() == initial - 2, QString("RemoveRule: 剩余 %1 条").arg(rules.size()));
    }

    // 8. GetRules 返回类型和模式字段
    {
        RuleEngine engine;
        engine.AddCleanRule("*.custom_clean");
        engine.AddKeepRule("*.custom_keep");

        auto rules = engine.GetRules();
        bool foundClean = false, foundKeep = false;
        for (const auto& r : rules)
        {
            if (r.pattern == "*.custom_clean" && r.type == RuleType::Clean) { foundClean = true; }
            if (r.pattern == "*.custom_keep" && r.type == RuleType::Keep) { foundKeep = true; }
        }
        Check(foundClean, "GetRules: 返回自定义清理规则且类型正确");
        Check(foundKeep, "GetRules: 返回自定义保留规则且类型正确");
    }

    // 9. MoveRule 单步移动
    {
        RuleEngine engine;
        engine.AddCleanRule("*.move_test");
        int lastIdx = engine.GetRules().size() - 1;
        int targetIdx = 0;

        engine.MoveRule(lastIdx, targetIdx);
        auto rules = engine.GetRules();
        Check(rules[targetIdx].pattern == "*.move_test", "MoveRule: 规则移到首位");
    }

    // 10. MatchDir 测试：.vs / .idea / build 等目录规则在不同层级都能匹配
    {
        RuleEngine engine;

        auto m1 = engine.MatchDir("D:/Project/.vs");
        Check(m1.isCleanTarget && m1.ruleName == ".vs/",
            QString("MatchDir: 根目录 .vs 命中清理规则 (rule=%1, target=%2)")
                .arg(m1.ruleName).arg(m1.isCleanTarget));

        auto m2 = engine.MatchDir("D:/Project/subdir/.vs");
        Check(m2.isCleanTarget && m2.ruleName == ".vs/",
            QString("MatchDir: 子目录 .vs 命中清理规则 (rule=%1, target=%2)")
                .arg(m2.ruleName).arg(m2.isCleanTarget));

        auto m3 = engine.MatchDir("D:/Project/a/b/c/.vs");
        Check(m3.isCleanTarget && m3.ruleName == ".vs/",
            QString("MatchDir: 深层子目录 .vs 命中清理规则 (rule=%1, target=%2)")
                .arg(m3.ruleName).arg(m3.isCleanTarget));

        auto m4 = engine.MatchDir("D:/Project/build");
        Check(m4.isCleanTarget,
            QString("MatchDir: build 目录命中清理规则 (rule=%1)").arg(m4.ruleName));

        auto m5 = engine.MatchDir("D:/Project/debug");
        Check(m5.isCleanTarget,
            QString("MatchDir: debug 目录命中清理规则 (rule=%1)").arg(m5.ruleName));

        // .vs 目录带尾部斜杠也应正确匹配
        auto m6 = engine.MatchDir("D:/Project/module/.vs/");
        Check(m6.isCleanTarget && m6.ruleName == ".vs/",
            QString("MatchDir: 带尾斜杠 .vs/ 命中清理规则 (rule=%1)").arg(m6.ruleName));
    }

    // 总结
    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, "
              << g_failCount << " 失败 ===" << std::endl;

    return g_failCount > 0 ? 1 : 0;
}
