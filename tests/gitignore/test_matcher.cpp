// 探针：GitIgnoreParser 文件加载 + 综合匹配
// 覆盖：LoadFromFile 加载 / 综合规则匹配 / 目录与文件结合
// 使用临时文件模拟 .gitignore
// 依赖：Qt5::Core

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
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

    std::cout << "=== GitIgnoreParser 文件加载 + 综合匹配探针 ===" << std::endl;
    std::cout << std::endl;

    // 1. 加载不存在的文件 → 返回 false
    {
        GitIgnoreParser parser;
        bool loaded = parser.LoadFromFile("nonexistent_gitignore_xyz123");
        Check(!loaded, "LoadFromFile: 不存在的文件返回 false");
    }

    // 2. 创建临时 .gitignore 并加载
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString gitignorePath = tempDir.path() + "/.gitignore";
        QFile file(gitignorePath);
        Check(file.open(QIODevice::WriteOnly | QIODevice::Text), "临时 .gitignore 文件创建成功");

        QTextStream stream(&file);
        stream << "# 编译产物\n";
        stream << "*.obj\n";
        stream << "*.pdb\n";
        stream << "\n";
        stream << "# 构建目录\n";
        stream << "build/\n";
        stream << "release/\n";
        file.close();

        GitIgnoreParser parser;
        bool loaded = parser.LoadFromFile(gitignorePath);
        Check(loaded, "LoadFromFile: 加载临时 .gitignore 成功");

        // 验证规则条数（注释和空行跳过）
        auto rules = parser.Rules();
        Check(rules.size() == 4, QString("加载后应有 4 条规则，实际 %1 条").arg(rules.size()));

        // 验证文件匹配
        Check(parser.IsIgnored("main.obj"), "文件匹配: main.obj 应被忽略");
        Check(!parser.IsIgnored("main.cpp"), "文件匹配: main.cpp 不应被忽略");

        // 验证目录匹配
        Check(parser.IsIgnored("build", true), "目录匹配: build 应被忽略");
        Check(parser.IsIgnored("release", true), "目录匹配: release 应被忽略");
    }

    // 3. 空文件加载
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString gitignorePath = tempDir.path() + "/.gitignore";
        QFile file(gitignorePath);
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        file.close();

        GitIgnoreParser parser;
        parser.LoadFromFile(gitignorePath);
        Check(parser.Rules().isEmpty(), "空 .gitignore: 规则列表为空");
        Check(!parser.IsIgnored("test.cpp"), "空 .gitignore: test.cpp 不被忽略");
    }

    // 4. 纯注释文件加载
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString gitignorePath = tempDir.path() + "/.gitignore";
        QFile file(gitignorePath);
        file.open(QIODevice::WriteOnly | QIODevice::Text);
        QTextStream stream(&file);
        stream << "# 全部是注释\n";
        stream << "# 没有实际规则\n";
        stream << "#   缩进注释\n";
        file.close();

        GitIgnoreParser parser;
        parser.LoadFromFile(gitignorePath);
        Check(parser.Rules().isEmpty(), "纯注释 .gitignore: 规则列表为空");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, "
              << g_failCount << " 失败 ===" << std::endl;

    return g_failCount > 0 ? 1 : 0;
}
