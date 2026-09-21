// 探针：打包的输入输出边界
//
// 三个问题在此一并验证：
//
// 1) 输出包已存在时能否正常结束
//    Packager 组装 7z 命令时没有传 -y。曾怀疑 7z 会停下来问「是否覆盖」，
//    而 QProcess 的 stdin 是永不写入的管道，会让打包永久卡住。
//    实测结论：7z 的 a(添加) 命令对已存在档案是更新语义，不询问，不会卡。
//
// 2) 输出目录落在源码目录内时，压缩包会不会把上一版包打进去
//    args 里传的是「源码目录/*」，若输出目录在源码目录内，上一次的 .7z
//    也在其中，会被一并收进新包，包体积逐次膨胀。
//
// 3) SetExcludeList 走 @listfile 时的两个副作用
//    - 列表文件必须用 UTF-8 写出：QTextStream 默认 codecForLocale（中文 Windows 为 GBK），
//      而 7-Zip 24.09 按 UTF-8 解读列表文件 —— 编码不对时 7z 直接报
//      "Incorrect item in listfile" 并拒绝执行整个打包（不是漏文件，是整个失败）
//    - 列表文件是运行期临时文件，用后需清理，且不能落在输出目录里
//
// 依赖：Qt5::Core + 7z CLI

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>
#include <iostream>

#include "packager/Packager.h"

static int g_passCount = 0;
static int g_failCount = 0;
static int g_skipCount = 0;

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

static void Skip(const QString& description)
{
    std::cout << "[SKIP] " << description.toStdString() << std::endl;
    ++g_skipCount;
}

static bool CreateFile(const QString& dir, const QString& name)
{
    QDir().mkpath(dir);
    QFile file(dir + "/" + name);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write("probe content", 13);
        file.close();
        return true;
    }
    return false;
}

// 列出压缩包内的条目名；-sccUTF-8 让 7z 以 UTF-8 输出，便于比对中文名
static QStringList ListArchive(const QString& archivePath)
{
    QStringList names;
    QProcess proc;
    proc.start(Packager::Find7zPath(),
               {"l", "-sccUTF-8", "-ba", "-slt", archivePath});
    if (!proc.waitForFinished(20000))
    {
        proc.kill();
        return names;
    }
    // -slt 输出形如 "Path = xxx"，逐行取 Path 字段
    const QString out = QString::fromUtf8(proc.readAllStandardOutput());
    for (const QString& line : out.split('\n'))
    {
        if (line.startsWith("Path = "))
        {
            names << line.mid(7).trimmed();
        }
    }
    return names;
}

enum class PackOutcome { Finished, Error, Timeout };

static PackOutcome RunPack(Packager& packager, int timeoutMs, qint64* outSize)
{
    PackOutcome outcome = PackOutcome::Timeout;
    bool finished = false;
    bool errored = false;

    QObject::connect(&packager, &Packager::PackFinished,
                     [&](const QString&, qint64 size)
                     {
                         finished = true;
                         if (outSize) { *outSize = size; }
                     });
    QObject::connect(&packager, &Packager::PackError,
                     [&](const QString&) { errored = true; });

    QEventLoop loop;
    QObject::connect(&packager, &Packager::PackFinished, &loop, &QEventLoop::quit);
    QObject::connect(&packager, &Packager::PackError, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    packager.StartPack();
    loop.exec();

    if (finished) { outcome = PackOutcome::Finished; }
    else if (errored) { outcome = PackOutcome::Error; }
    else { packager.CancelPack(); }   // 超时：回收 7z 进程，不留给后续用例

    return outcome;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== 打包输入输出边界探针 ===" << std::endl;
    std::cout << std::endl;

    if (Packager::Find7zPath().isEmpty())
    {
        Skip("未找到 7z CLI，整个探针跳过");
        std::cout << std::endl;
        std::cout << "=== 结果: 0 通过, 0 失败, 1 跳过 ===" << std::endl;
        return 0;
    }

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
    {
        Check(false, "临时目录创建成功");
        std::cout << "=== 结果: 0 通过, 1 失败 ===" << std::endl;
        return 1;
    }

    CreateFile(tempDir.path(), "main.cpp");
    CreateFile(tempDir.path(), "CMakeLists.txt");

    // ---------- 1：同名包重复打包 ----------
    {
        const QString outputDir = tempDir.path() + "/output1";
        const QString fixedName = "SameName_source";
        const QString archivePath = outputDir + "/" + fixedName + ".7z";

        Packager first;
        first.SetSourceDir(tempDir.path());
        first.SetOutputDir(outputDir);
        first.SetOutputName(fixedName);
        qint64 size1 = 0;
        Check(RunPack(first, 20000, &size1) == PackOutcome::Finished, "1: 首次打包完成");
        Check(QFileInfo::exists(archivePath), "1: 压缩包已生成");

        // 同名再打一次：7z 的 a 命令是更新语义，不会停下来等覆盖确认
        Packager second;
        second.SetSourceDir(tempDir.path());
        second.SetOutputDir(outputDir);
        second.SetOutputName(fixedName);
        qint64 size2 = 0;
        const PackOutcome outcome = RunPack(second, 15000, &size2);
        Check(outcome != PackOutcome::Timeout,
              "1: 目标包已存在时再次打包不会卡住（7z 的 a 命令不询问覆盖）");
        Check(outcome == PackOutcome::Finished, "1: 再次打包正常完成");
    }

    // ---------- 2：输出目录在源码目录内 ----------
    // 模拟用户在设置页把输出目录填成项目内的子目录
    {
        const QString srcDir = tempDir.path() + "/proj_inner";
        const QString outDir = srcDir + "/out";
        CreateFile(srcDir, "main.cpp");
        CreateFile(srcDir, "utils.cpp");

        Packager p1;
        p1.SetSourceDir(srcDir);
        p1.SetOutputDir(outDir);
        p1.SetOutputName("Inner_source");
        Check(RunPack(p1, 20000, nullptr) == PackOutcome::Finished, "2: 首次打包完成");

        Packager p2;
        p2.SetSourceDir(srcDir);
        p2.SetOutputDir(outDir);
        p2.SetOutputName("Inner_source");
        Check(RunPack(p2, 20000, nullptr) == PackOutcome::Finished, "2: 再次打包完成");

        const QStringList entries = ListArchive(outDir + "/Inner_source.7z");
        // 条目名带目录前缀且用反斜杠（out\Inner_source.7z），故按后缀比对
        bool selfIncluded = false;
        for (const QString& e : entries)
        {
            if (e.endsWith("Inner_source.7z")) { selfIncluded = true; }
        }
        std::cout << "       第二次包的条目: " << entries.join(" | ").toStdString() << std::endl;
        Check(!selfIncluded, "2: 压缩包不含它自己（输出目录在源码目录内时不得自包含）");
    }

    // ---------- 3：排除列表（勾选的待清理项）----------
    // 排除项里刻意放一个中文名文件：列表文件若按本地编码（中文 Windows 为 GBK）写出，
    // 7z 按 UTF-8 解读时会直接报 "Incorrect item in listfile" 并拒绝执行整个打包
    {
        const QString srcDir = tempDir.path() + "/proj_list";
        const QString outDir = tempDir.path() + "/output3";
        CreateFile(srcDir, "keep_a.cpp");
        CreateFile(srcDir, "keep_中文.cpp");
        CreateFile(srcDir, "中文垃圾.obj");
        CreateFile(srcDir + "/build", "y.obj");

        Packager p;
        p.SetSourceDir(srcDir);
        p.SetOutputDir(outDir);
        p.SetOutputName("List_source");
        p.SetExcludeList({srcDir + "/中文垃圾.obj", srcDir + "/build"});
        Check(RunPack(p, 20000, nullptr) == PackOutcome::Finished, "3: 带排除列表的打包完成");

        const QString archivePath = outDir + "/List_source.7z";
        Check(QFileInfo::exists(archivePath), "3: 压缩包已生成");

        // 列表文件写在系统临时目录，输出目录里不应出现任何运行期临时文件
        const QStringList leftovers =
            QDir(outDir).entryList(QStringList{"_packlist.tmp"}, QDir::Files);
        Check(leftovers.isEmpty(), "3: 输出目录无 _packlist.tmp 残留");

        QStringList entries = ListArchive(archivePath);
        for (QString& e : entries) { e.replace('\\', '/'); }
        std::cout << "       包内条目: " << entries.join(" | ").toStdString() << std::endl;

        Check(entries.contains("keep_a.cpp"), "3: 未排除的源码 keep_a.cpp 已入包");
        Check(entries.contains("keep_中文.cpp"), "3: 中文名源码 keep_中文.cpp 已入包");
        Check(!entries.contains("中文垃圾.obj"),
              "3: 排除列表里的中文名项已被排除（列表文件编码正确）");
        Check(!entries.contains("build"), "3: 排除列表里的目录已被排除");
        Check(!entries.contains("build/y.obj"), "3: 被排除目录下的内容未入包");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, "
              << g_failCount << " 失败, " << g_skipCount << " 跳过 ===" << std::endl;
    return g_failCount > 0 ? 1 : 0;
}
