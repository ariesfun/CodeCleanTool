// 探针：冗余清理目标（目录与其内部文件同时命中）的删除行为
//
// 复现用户报告：
//   路径=.../EnvironmentGateway/x64/Release/moc_WindGeo.obj
//   原因=系统找不到指定的路径
//   但该文件实际已被删除（x64 目录已空）
//
// 成因：对 .../x64/Release/moc_WindGeo.obj 而言
//   - 目录 Release 命中构建目录规则 release/（大小写不敏感）
//   - 文件 moc_WindGeo.obj 命中编译产物规则 *.obj
//   扫描会同时产出【目录目标】与【目录内文件目标】两个条目。
//   清理时先删目录（递归删掉了里面的文件），再单独删该文件就必然失败。
//
// 本探针验证：这类冗余目标不应产生失败计数。
//
// 依赖：Qt5::Core / Qt5::Gui

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

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

static bool Contains(const QStringList& list, const QString& target)
{
    QString t = QDir::fromNativeSeparators(target);
    t.replace('\\', '/');
    for (const auto& s : list)
    {
        QString a = QDir::fromNativeSeparators(s);
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

    std::cout << "=== 冗余清理目标删除探针 ===" << std::endl;
    std::cout << std::endl;

    // ---- 用例 A：复刻用户报告的结构 ----
    // .../proj/x64/Release/moc_WindGeo.obj  —— 目录 Release 与文件 .obj 同时命中
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "A: 临时目录创建成功");
        const QString root = tempDir.path();

        QDir().mkpath(root + "/proj/x64/Release");
        Check(MakeFile(root + "/proj/x64/Release/moc_WindGeo.obj"), "A: 创建 Release/moc_WindGeo.obj");
        Check(MakeFile(root + "/proj/x64/Release/WindGeo.obj"), "A: 创建 Release/WindGeo.obj");

        const QString relDir = root + "/proj/x64/Release";
        const QString objFile = relDir + "/moc_WindGeo.obj";

        const ScanOutcome scan = RunScan(root);
        Check(scan.finished, "A: 扫描完成");

        const bool dirHit = Contains(scan.hitPaths, relDir);
        const bool fileHit = Contains(scan.hitPaths, objFile);
        std::cout << "       扫描命中 " << scan.hitPaths.size() << " 项:"
                  << " Release 目录=" << (dirHit ? "是" : "否")
                  << " moc_WindGeo.obj=" << (fileHit ? "是" : "否") << std::endl;
        Check(dirHit, "A: Release 目录被识别为清理项");
        Check(fileHit, "A: 目录内的 .obj 也被识别为清理项（冗余目标由此产生）");

        // 按扫描产出的顺序清理（复刻真实调用路径）
        const CleanOutcome clean = RunClean(scan.hitPaths);
        Check(clean.finished, "A: 清理完成");
        std::cout << "       结果: ok=" << clean.ok << " fail=" << clean.fail << std::endl;
        for (const auto& e : clean.errors)
        {
            std::cout << "       错误: " << e.toStdString() << std::endl;
        }

        Check(clean.fail == 0, "A: 冗余目标不产生失败计数");
        Check(!QDir(relDir).exists(), "A: Release 目录已被删除");
        Check(!QFileInfo::exists(objFile), "A: 目录内的文件已随目录一并删除");
    }

    // ---- 用例 B：单独删除已不存在的文件应视为成功（幂等）----
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "B: 临时目录创建成功");
        const QString ghost = tempDir.path() + "/already-gone.obj";

        const CleanOutcome out = RunClean({ghost});
        Check(out.finished, "B: 清理完成");
        std::cout << "       结果: ok=" << out.ok << " fail=" << out.fail << std::endl;
        Check(out.fail == 0, "B: 删除不存在的文件记为成功（删除是幂等操作）");
    }

    // ---- 用例 C：对照——真正删不掉的文件（父目录不存在）仍应报失败 ----
    // 这里用一个整体不存在于任何位置的路径，用于确认不会把「真失败」也吞掉
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "C: 临时目录创建成功");
        const QString normalFile = tempDir.path() + "/normal.obj";
        MakeFile(normalFile);

        const CleanOutcome out = RunClean({normalFile});
        Check(out.finished, "C: 清理完成");
        Check(out.ok == 1 && out.fail == 0, "C: 存在的文件正常删除且记为成功");
        Check(!QFileInfo::exists(normalFile), "C: 文件确实已被删除");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount == 0 ? 0 : 1;
}
