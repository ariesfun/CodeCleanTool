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
            if (r == ".vs")  { hasVs = true; }
            if (r == ".db")  { hasDb = true; }
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

    // 总结
    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, "
              << g_failCount << " 失败 ===" << std::endl;

    return g_failCount > 0 ? 1 : 0;
}
