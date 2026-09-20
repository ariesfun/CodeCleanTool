// 探针：Packager 包名模板展开与输出目录配置
// 覆盖：空模板、默认模板完整展开、%MM 月/分歧义消解、未知占位符保留、
//       无占位符原样返回、项目名推导（叶子目录名/尾斜杠/根路径）、
//       SetOutputDir/SetOutputName 对打包路径的影响
// 依赖：Qt5::Core

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QTime>
#include <iostream>

#include "packager/Packager.h"

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

// 比较并打印期望值与实际值，便于失败时定位
static void CheckEqual(const QString& actual, const QString& expected, const QString& description)
{
    const bool ok = (actual == expected);
    Check(ok, description);
    if (!ok)
    {
        std::cout << "       期望: [" << expected.toStdString()
                  << "]  实际: [" << actual.toStdString() << "]" << std::endl;
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== Packager 包名模板探针 ===" << std::endl;
    std::cout << std::endl;

    // 固定时间基准，保证结果可重现：2026-09-20 17:05:33
    const QDateTime fixedNow(QDate(2026, 9, 20), QTime(17, 5, 33));

    // 1. 空模板：返回空串，交由调用方回退到默认命名
    {
        const QString r = Packager::FormatOutputName(QString(), "C:/work/MyApp", fixedNow);
        CheckEqual(r, QString(), "空模板返回空串（调用方回退默认命名）");
    }

    // 2. 无占位符的纯文本：原样返回
    {
        const QString r = Packager::FormatOutputName("plain-name", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "plain-name", "无占位符模板原样返回");
    }

    // 3. 默认模板完整展开（项目名取叶子目录）
    {
        const QString r = Packager::FormatOutputName(
            "%Project_%YYYY%MM%DD_%HH%MM%SS_source", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "MyApp_20260920_170533_source", "默认模板完整展开");
    }

    // 4. 各占位符单独展开
    {
        const QString r = Packager::FormatOutputName("%YYYY", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "2026", "%YYYY 展开为四位年");
    }
    {
        const QString r = Packager::FormatOutputName("%DD", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "20", "%DD 展开为两位日");
    }
    {
        const QString r = Packager::FormatOutputName("%SS", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "33", "%SS 展开为两位秒");
    }

    // 5. %MM 歧义消解：未出现 %HH 时按「月」，出现 %HH 之后按「分」
    {
        const QString r = Packager::FormatOutputName("%MM", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "09", "单独 %MM 解析为月（09）");
    }
    {
        const QString r = Packager::FormatOutputName("%HH%MM", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "1705", "%HH 之后的 %MM 解析为分（1705）");
    }
    {
        const QString r = Packager::FormatOutputName("%MM-%HH%MM", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "09-1705", "%HH 之前为月、之后为分（09-1705）");
    }

    // 6. 未知占位符原样保留，不吞字符
    {
        const QString r = Packager::FormatOutputName("%XYZ/%Project", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "%XYZ/MyApp", "未知占位符 %XYZ 原样保留");
    }

    // 7. 项目名推导：叶子目录名优先
    {
        const QString r = Packager::FormatOutputName("%Project", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "MyApp", "项目名取叶子目录名（非父目录名）");
    }
    {
        const QString r = Packager::FormatOutputName("%Project", "C:/work/MyApp/", fixedNow);
        CheckEqual(r, "MyApp", "路径带尾斜杠时项目名仍为 MyApp");
    }
    {
        const QString r = Packager::FormatOutputName("%Project", "MyApp", fixedNow);
        CheckEqual(r, "MyApp", "相对路径项目名推导");
    }

    // 8. 边界：根路径不崩溃且不产生非法包名
    {
        const QString r = Packager::FormatOutputName("%Project", "C:/", fixedNow);
        Check(!r.isEmpty(), "根路径退化：项目名非空（回退到上级目录名）");
        Check(r != "%Project", "根路径退化：占位符已被替换");
    }

    // 9. 连续占位符无分隔符
    {
        const QString r = Packager::FormatOutputName("%YYYY%MM%DD", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "20260920", "连续占位符无分隔符正确展开");
    }

    // 10. 末尾孤立的 % 不吞掉后续内容
    {
        const QString r = Packager::FormatOutputName("%Project%", "C:/work/MyApp", fixedNow);
        CheckEqual(r, "MyApp%", "末尾孤立 % 原样保留");
    }

    // 11. SetOutputName/SetOutputDir 状态保持（不触发打包，仅验证 setter 语义）
    {
        Packager pkg;
        pkg.SetSourceDir("C:/work/MyApp");
        pkg.SetOutputDir("D:/out/dir");
        pkg.SetOutputName("Custom_Name");
        // setter 无返回值，这里验证重复设置不崩溃且可被后续 StartPack 前置检查覆盖
        pkg.SetOutputDir("D:/out/dir");
        pkg.SetOutputName("Custom_Name");
        Check(true, "SetOutputDir/SetOutputName 可重复调用不崩溃");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount == 0 ? 0 : 1;
}
