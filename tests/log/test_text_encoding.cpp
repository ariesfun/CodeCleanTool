// 探针：文本文件读写的编码
//
// 覆盖两个因 QTextStream 未显式指定编码而产生的问题：
//   1) LogManager::ExportToFile 导出的日志应为合法 UTF-8。
//      QTextStream 默认使用 codecForLocale()，中文 Windows 上即 GBK，
//      导出文件拿到其它工具/编辑器里会乱码。
//   2) GitIgnoreParser::LoadFromFile 应以 UTF-8 解析 .gitignore。
//      git 规范要求 .gitignore 为 UTF-8；按 GBK 解读会让含中文的规则匹配错误。
//
// 依赖：Qt5::Core

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextCodec>
#include <iostream>

#include "log/LogManager.h"
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

// 以「严格 UTF-8」解码；含非法字节序列时抛异常
static bool TryDecodeStrictUtf8(const QByteArray& bytes, QString& out)
{
    QTextCodec* codec = QTextCodec::codecForName("UTF-8");
    QTextCodec::ConverterState state;
    out = codec->toUnicode(bytes.constData(), bytes.size(), &state);
    return state.invalidChars == 0;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== 文本文件编码探针 ===" << std::endl;
    std::cout << std::endl;

    // ---- 用例 A：导出日志应为合法 UTF-8 ----
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "A: 临时目录创建成功");

        LogManager mgr;
        mgr.Append("INFO", "测试模块", "中文内容：路径 C:/临时/文件.txt");
        mgr.Append("WARN", "测试模块", "警告：包含中文标点，、。");

        const QString outPath = tempDir.path() + "/export.log";
        Check(mgr.ExportToFile(outPath), "A: 导出成功");
        Check(QFileInfo::exists(outPath), "A: 导出文件已生成");

        QFile f(outPath);
        Check(f.open(QIODevice::ReadOnly), "A: 可读取导出文件");
        const QByteArray bytes = f.readAll();
        f.close();

        QString decoded;
        const bool validUtf8 = TryDecodeStrictUtf8(bytes, decoded);
        Check(validUtf8, "A: 导出文件是合法 UTF-8（其它工具可直接读取）");

        if (validUtf8)
        {
            Check(decoded.contains("中文内容：路径 C:/临时/文件.txt"),
                  "A: 中文内容按 UTF-8 解码正确");
            Check(decoded.contains("警告：包含中文标点，、。"),
                  "A: 中文标点按 UTF-8 解码正确");
        }
        else
        {
            Check(false, "A: 中文内容未能按 UTF-8 解码");
        }
    }

    // ---- 用例 B：.gitignore 按 UTF-8 解析 ----
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "B: 临时目录创建成功");
        const QString giPath = tempDir.path() + "/.gitignore";

        // 以 UTF-8 写入含中文的规则（git 规范要求 .gitignore 为 UTF-8）
        QFile f(giPath);
        Check(f.open(QIODevice::WriteOnly), "B: 可写入 .gitignore");
        f.write(QString("临时目录/\n*.备份\n中文文件.txt\n").toUtf8());
        f.close();

        GitIgnoreParser parser;
        Check(parser.LoadFromFile(giPath), "B: .gitignore 加载成功");
        Check(parser.Rules().size() == 3,
              QString("B: 解析出 3 条规则（实际 %1）").arg(parser.Rules().size()));

        Check(parser.IsIgnored("临时目录/a.txt", true), "B: 中文目录规则命中");
        Check(parser.IsIgnored("data.备份"), "B: 中文扩展名规则命中");
        Check(parser.IsIgnored("中文文件.txt"), "B: 中文文件名规则命中");
        Check(!parser.IsIgnored("main.cpp"), "B: 未命中规则的文件不被忽略");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount == 0 ? 0 : 1;
}
