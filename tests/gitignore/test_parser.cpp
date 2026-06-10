// 探针：GitIgnoreParser 规则解析
// 覆盖：ParseRules 注释行/空行/简单模式/!反向规则/目录规则/锚定规则
// 不依赖文件 I/O，纯内存规则解析验证
// 依赖：Qt5::Core

#include <QCoreApplication>
#include <iostream>

#include "rules/GitIgnoreParser.h"

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

    std::cout << "=== GitIgnoreParser 规则解析探针 ===" << std::endl;
    std::cout << std::endl;

    // 1. 空规则集：任何路径都不被忽略
    {
        GitIgnoreParser parser;
        Check(!parser.IsIgnored("anything.cpp"), "空规则集: anything.cpp 不被忽略");
        Check(!parser.IsIgnored("build/output.exe"), "空规则集: build/output.exe 不被忽略");
        Check(!parser.IsIgnored(".git/config"), "空规则集: .git/config 不被忽略");
    }

    // 2. 简单文件通配符：*.o 忽略当前目录的 .o 文件
    {
        GitIgnoreParser parser;
        parser.ParseRules("*.o\n");
        Check(parser.IsIgnored("main.o"), "*.o: main.o 应被忽略");
        Check(!parser.IsIgnored("main.cpp"), "*.o: main.cpp 不应被忽略");
    }

    // 3. 双星通配符：**/*.o 忽略任意深度的 .o 文件
    {
        GitIgnoreParser parser;
        parser.ParseRules("**/*.o\n");
        Check(parser.IsIgnored("subdir/test.o"), "**/*.o: subdir/test.o 应被忽略");
        Check(parser.IsIgnored("a/b/c/test.o"), "**/*.o: a/b/c/test.o 应被忽略");
    }

    // 4. 目录规则：build/ 忽略匹配前缀的目录
    {
        GitIgnoreParser parser;
        parser.ParseRules("build/\n");
        Check(parser.IsIgnored("build", true), "build/: build 目录应被忽略");
    }

    // 4. ! 反向规则：排除特定文件
    {
        GitIgnoreParser parser;
        parser.ParseRules("*.log\n!important.log\n");
        Check(parser.IsIgnored("debug.log"), "*.log + !important.log: debug.log 应被忽略");
        Check(!parser.IsIgnored("important.log"), "*.log + !important.log: important.log 不应被忽略");
        Check(parser.IsIgnored("error.log"), "*.log + !important.log: error.log 应被忽略");
    }

    // 5. 注释行和空行
    {
        GitIgnoreParser parser;
        parser.ParseRules("# 这是一个注释\n*.tmp\n\n# 另一个注释\n*.cache\n");
        Check(parser.IsIgnored("test.tmp"), "注释行: test.tmp 应被忽略（规则有效）");
        Check(parser.IsIgnored("test.cache"), "注释行: test.cache 应被忽略（规则有效）");
    }

    // 6. 多规则组合
    {
        GitIgnoreParser parser;
        parser.ParseRules("*.obj\n*.pdb\nbuild/\n");
        Check(parser.IsIgnored("file.obj"), "组合规则: file.obj 应被忽略");
        Check(parser.IsIgnored("vc143.pdb"), "组合规则: vc143.pdb 应被忽略");
        Check(parser.IsIgnored("build", true), "组合规则: build 目录应被忽略");
        Check(!parser.IsIgnored("source.cpp"), "组合规则: source.cpp 不应被忽略");
    }

    // 7. Rules() 返回原始规则文本
    {
        GitIgnoreParser parser;
        parser.ParseRules("*.obj\n*.pdb\n");
        auto rules = parser.Rules();
        Check(rules.size() == 2, QString("Rules(): 应有 2 条规则，实际 %1 条").arg(rules.size()));
        Check(rules.contains("*.obj"), "Rules(): 包含 *.obj");
        Check(rules.contains("*.pdb"), "Rules(): 包含 *.pdb");
    }

    // 8. Clear() 清空规则
    {
        GitIgnoreParser parser;
        parser.ParseRules("*.tmp\n");
        parser.Clear();
        Check(!parser.IsIgnored("test.tmp"), "Clear 后 test.tmp 不再被忽略");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, "
              << g_failCount << " 失败 ===" << std::endl;

    return g_failCount > 0 ? 1 : 0;
}
