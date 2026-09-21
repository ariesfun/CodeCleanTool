// 探针：.vs 目录的扫描识别与删除（复现「.vs 清理不掉」）
//
// 背景：用户报告 <proj>\<proj>\.vs\<proj>\v14\.suo 始终清理不掉。
//       实测该目录最长路径 156 字符（远低于 MAX_PATH），故排除长路径因素；
//       实测 .suo 的属性为 Hidden+Archive，并非只读，故也排除「只读未清」这一路径。
//
// 本探针分两段取证：
//   第一段：扫描 —— 验证 .vs 是否被识别为清理项
//   第二段：删除 —— 用 FileCleaner 真实删除，并与首段结论对照，定位失败发生在哪一环
// 第二段会复刻真实文件的 Hidden 属性（Qt 无公开 API，直接调 Win32 SetFileAttributes）。
//
// 依赖：Qt5::Core / Qt5::Gui

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "scanner/ScanManager.h"
#include "cleaner/FileCleaner.h"
#include "rules/RuleEngine.h"
#include "model/ResultModel.h"

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

static bool MakeDirs(const QString& base, const QString& rel)
{
    return QDir(base).mkpath(rel);
}

static bool MakeFile(const QString& fullPath)
{
    QFile f(fullPath);
    if (!f.open(QIODevice::WriteOnly))
    {
        return false;
    }
    f.write("x", 1);
    f.close();
    return true;
}

// 设置 Windows 隐藏属性（VS 生成的 .suo 即为此属性）
static bool SetHiddenAttr(const QString& path)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(path);
    DWORD attrs = GetFileAttributesW(reinterpret_cast<const wchar_t*>(native.utf16()));
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }
    return SetFileAttributesW(reinterpret_cast<const wchar_t*>(native.utf16()),
                              attrs | FILE_ATTRIBUTE_HIDDEN) != 0;
#else
    Q_UNUSED(path);
    return false;
#endif
}

struct ScanOutcome
{
    bool finished{false};
    int total{0};
    QStringList hitPaths;
};

static ScanOutcome RunScan(const QString& root)
{
    RuleEngine ruleEngine;
    ResultModel resultModel;

    ScanManager scanManager;
    scanManager.SetRootPath(root);
    scanManager.SetRuleEngine(&ruleEngine);
    scanManager.SetResultModel(&resultModel);
    scanManager.SetExcludeVcsDirs(true);

    ScanOutcome out;
    QObject::connect(&scanManager, &ScanManager::ScanFinished, [&out](int total, qint64)
    {
        out.finished = true;
        out.total = total;
    });

    scanManager.StartScan();

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, qApp, &QCoreApplication::quit);
    QObject::connect(&scanManager, &ScanManager::ScanFinished, qApp, &QCoreApplication::quit);
    timeoutTimer.start(15000);
    qApp->exec();

    for (int i = 0; i < resultModel.TotalCount(); ++i)
    {
        out.hitPaths << resultModel.GetFile(i).filePath;
    }
    return out;
}

struct CleanOutcome
{
    bool finished{false};
    int ok{0};
    int fail{0};
    QStringList errors;
};

static CleanOutcome RunClean(const QStringList& targets)
{
    FileCleaner cleaner;
    cleaner.SetTargetList(targets);

    CleanOutcome out;
    QObject::connect(&cleaner, &FileCleaner::CleanFinished, [&out](int ok, int fail)
    {
        out.finished = true;
        out.ok = ok;
        out.fail = fail;
    });
    QObject::connect(&cleaner, &FileCleaner::CleanError, [&out](const QString& path, const QString& msg)
    {
        out.errors << QString("%1 | %2").arg(path, msg);
    });

    cleaner.StartClean();

    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    QObject::connect(&timeoutTimer, &QTimer::timeout, qApp, &QCoreApplication::quit);
    QObject::connect(&cleaner, &FileCleaner::CleanFinished, qApp, &QCoreApplication::quit);
    timeoutTimer.start(15000);
    qApp->exec();

    return out;
}

static bool HitContains(const QStringList& hits, const QString& target)
{
    QString t = QDir::fromNativeSeparators(target);
    t.replace('\\', '/');
    for (const auto& h : hits)
    {
        QString a = QDir::fromNativeSeparators(h);
        a.replace('\\', '/');
        if (a.compare(t, Qt::CaseInsensitive) == 0)
        {
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // 可选用法：传入一个目录路径时，只对该目录做一次只读扫描并打印命中项，
    // 用于对真实工程做排查（只扫描、不删除）。不传参数时跑下方的完整回归用例。
    if (argc > 1)
    {
        const QString target = QString::fromLocal8Bit(argv[1]);
        std::cout << "=== 只读扫描指定目录 ===" << std::endl;
        std::cout << "  目标: " << target.toStdString() << std::endl;
        const ScanOutcome out = RunScan(target);
        std::cout << "  扫描完成: " << (out.finished ? "是" : "否")
                  << "  命中: " << out.hitPaths.size() << " 项  total=" << out.total << std::endl;
        for (const auto& h : out.hitPaths)
        {
            std::cout << "    " << QDir::fromNativeSeparators(h).toStdString() << std::endl;
        }
        return out.finished ? 0 : 1;
    }

    std::cout << "=== .vs 目录扫描与删除探针 ===" << std::endl;
    std::cout << std::endl;

    // ================= 第一段：扫描识别 =================
    std::cout << "---- 第一段：扫描 ----" << std::endl;

    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "A: 临时目录创建成功");
        const QString root = tempDir.path();

        Check(MakeDirs(root, "PluginTest0904/PluginTest0904/.vs/PluginTest0904/v14"),
              "A: 创建 PluginTest0904/PluginTest0904/.vs/PluginTest0904/v14");
        Check(MakeFile(root + "/PluginTest0904/PluginTest0904/.vs/PluginTest0904/v14/.suo"),
              "A: 创建 .suo");
        Check(MakeFile(root + "/PluginTest0904/PluginTest0904/main.cpp"),
              "A: 创建同级的 main.cpp（应保留）");

        const ScanOutcome out = RunScan(root);
        Check(out.finished, "A: 扫描完成");
        Check(HitContains(out.hitPaths, root + "/PluginTest0904/PluginTest0904/.vs"),
              "A: .vs 目录被识别为清理项");
    }

    // 用例 D：.vs 是空目录
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "D: 临时目录创建成功");
        Check(MakeDirs(tempDir.path(), ".vs"), "D: 创建空 .vs 目录");

        const ScanOutcome out = RunScan(tempDir.path());
        Check(out.finished, "D: 扫描完成");
        Check(HitContains(out.hitPaths, tempDir.path() + "/.vs"), "D: 空 .vs 目录被识别为清理项");
    }

    // 用例 I：.vs 目录带【隐藏】属性 —— 复刻 Visual Studio 生成的真实 .vs
    // VS 会给 .vs 目录设置 Hidden 属性，若扫描迭代器未开启 QDir::Hidden，该目录会被整个跳过
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "I: 临时目录创建成功");
        const QString root = tempDir.path();
        const QString vsDir = root + "/PluginTest0904/PluginTest0904/.vs";
        MakeDirs(root, "PluginTest0904/PluginTest0904/.vs/PluginTest0904/v14");
        MakeFile(vsDir + "/PluginTest0904/v14/.suo");
        SetHiddenAttr(vsDir + "/PluginTest0904/v14/.suo");
        Check(SetHiddenAttr(vsDir), "I: 已将 .vs 目录设为隐藏（复刻 VS 真实行为）");
        Check(SetHiddenAttr(root + "/PluginTest0904/PluginTest0904/.vs"), "I: 隐藏属性设置成功");

        const ScanOutcome out = RunScan(root);
        Check(out.finished, "I: 扫描完成");
        std::cout << "       命中 " << out.hitPaths.size() << " 项, total=" << out.total << std::endl;
        for (const auto& h : out.hitPaths)
        {
            std::cout << "         " << QDir::fromNativeSeparators(h).toStdString() << std::endl;
        }
        Check(HitContains(out.hitPaths, vsDir), "I: 【隐藏的】.vs 目录被识别为清理项");
    }

    // ================= 第二段：真实删除 =================
    std::cout << std::endl << "---- 第二段：删除 ----" << std::endl;

    // E：普通 .vs（文件非隐藏）
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "E: 临时目录创建成功");
        const QString root = tempDir.path();
        const QString vsDir = root + "/proj/.vs";
        MakeDirs(root, "proj/.vs/proj/v14");
        MakeFile(vsDir + "/proj/v14/.suo");

        const CleanOutcome out = RunClean({vsDir});
        Check(out.finished, "E: 清理完成");
        std::cout << "       结果: ok=" << out.ok << " fail=" << out.fail << std::endl;
        for (const auto& e : out.errors) { std::cout << "       错误: " << e.toStdString() << std::endl; }
        Check(!QDir(vsDir).exists(), "E: .vs 目录已被删除");
    }

    // F：.vs 内含【隐藏】文件（复刻真实 .suo 的 Hidden 属性）
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "F: 临时目录创建成功");
        const QString root = tempDir.path();
        const QString vsDir = root + "/proj/.vs";
        const QString suoPath = vsDir + "/proj/v14/.suo";
        MakeDirs(root, "proj/.vs/proj/v14");
        MakeFile(suoPath);
        Check(SetHiddenAttr(suoPath), "F: 已将 .suo 设为隐藏属性（复刻真实环境）");

        const CleanOutcome out = RunClean({vsDir});
        Check(out.finished, "F: 清理完成");
        std::cout << "       结果: ok=" << out.ok << " fail=" << out.fail << std::endl;
        for (const auto& e : out.errors) { std::cout << "       错误: " << e.toStdString() << std::endl; }
        Check(!QDir(vsDir).exists(), "F: 含隐藏文件的 .vs 目录已被删除");
    }

    // G：.vs 内含【只读】文件
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "G: 临时目录创建成功");
        const QString root = tempDir.path();
        const QString vsDir = root + "/proj/.vs";
        const QString suoPath = vsDir + "/proj/v14/.suo";
        MakeDirs(root, "proj/.vs/proj/v14");
        MakeFile(suoPath);
        QFile(suoPath).setPermissions(QFileDevice::ReadOwner);   // 只读
        Check(QFileInfo(suoPath).isWritable() == false, "G: 已将 .suo 设为只读");

        const CleanOutcome out = RunClean({vsDir});
        Check(out.finished, "G: 清理完成");
        std::cout << "       结果: ok=" << out.ok << " fail=" << out.fail << std::endl;
        for (const auto& e : out.errors) { std::cout << "       错误: " << e.toStdString() << std::endl; }
        Check(!QDir(vsDir).exists(), "G: 含只读文件的 .vs 目录已被删除");
    }

    // H：单独删除隐藏文件（走 DeleteFile 单文件路径，用于对照）
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "H: 临时目录创建成功");
        const QString f = tempDir.path() + "/hidden.suo";
        MakeFile(f);
        SetHiddenAttr(f);

        const CleanOutcome out = RunClean({f});
        Check(out.finished, "H: 清理完成");
        std::cout << "       结果: ok=" << out.ok << " fail=" << out.fail << std::endl;
        Check(!QFileInfo::exists(f), "H: 隐藏文件可被单独删除（对照）");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount == 0 ? 0 : 1;
}
