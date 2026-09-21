// 探针：扫描结果不受 .gitignore 影响
//
// 背景（用户报告复现）：早期版本会把「命中 .gitignore 的条目」直接从扫描结果中排除。
//   而 .gitignore 里列的恰恰是构建垃圾（.vs/、build/、*.obj、*.exe），
//   也正是本工具要清理的对象——按其排除等于把待清理项藏起来，与工具用途相反。
//   更严重的是解析器在 MainPresenter 中只创建一次并跨扫描复用：
//   项目 .gitignore 被删除后，旧规则仍残留在内存里，导致扫描结果与实际情况不符。
//
// 现已移除该排除语义，扫描范围只由内置规则与用户自定义规则决定。
// 本探针固化这一结论：目录中存在 .gitignore 时，命中清理规则的条目仍应被识别。
//
// 用例：
//   A 无 .gitignore（基线）            → 根 .vs 与其内 .exe 均应被识别
//   B 有 .gitignore（含 .vs/ 与 *.exe）→ 仍应被识别（旧版在此失败）
//   C 根 .vs 带隐藏属性 + .gitignore    → 仍应被识别（隐藏属性与 gitignore 叠加）
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

// 设置 Windows 隐藏属性（复刻 Visual Studio 生成的 .vs 目录）
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

// 扫描根目录；规则引擎含内置规则 + 模拟用户手动添加的 *.exe 清理规则
static ScanOutcome RunScan(const QString& root)
{
    RuleEngine ruleEngine;
    ruleEngine.AddCleanRule("*.exe");

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

static bool Hit(const QStringList& hits, const QString& target)
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

static void DumpHits(const QString& root, const ScanOutcome& out)
{
    QString r = QDir::fromNativeSeparators(root);
    r.replace('\\', '/');
    std::cout << "       命中 " << out.hitPaths.size() << " 项:";
    for (const auto& h : out.hitPaths)
    {
        QString a = QDir::fromNativeSeparators(h);
        a.replace('\\', '/');
        std::cout << " " << a.mid(r.size()).toStdString();
    }
    std::cout << std::endl;
}

// 建立统一测试目录：根 .vs（含一个 .exe）+ main.cpp + 可选 .gitignore
static bool BuildTree(const QString& root, const QString& gitIgnoreContent)
{
    QDir().mkpath(root + "/.vs");
    MakeFile(root + "/.vs/CodeCleanTool_V0.1.0.exe");
    MakeFile(root + "/.vs/1111");
    MakeFile(root + "/main.cpp");
    if (!gitIgnoreContent.isEmpty())
    {
        QFile f(root + "/.gitignore");
        if (!f.open(QIODevice::WriteOnly))
        {
            return false;
        }
        f.write(gitIgnoreContent.toUtf8());
        f.close();
    }
    return true;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // 可选用法：传入目录路径时，按程序实际行为对该目录做只读扫描并打印命中项
    if (argc > 1)
    {
        const QString target = QString::fromLocal8Bit(argv[1]);
        std::cout << "=== 只读扫描（模拟程序行为）===" << std::endl;
        std::cout << "  目标: " << target.toStdString() << std::endl;
        const ScanOutcome out = RunScan(target);
        std::cout << "  命中 " << out.hitPaths.size() << " 项:" << std::endl;
        QString r = QDir::fromNativeSeparators(target);
        r.replace('\\', '/');
        for (const auto& h : out.hitPaths)
        {
            QString a = QDir::fromNativeSeparators(h);
            a.replace('\\', '/');
            std::cout << "    " << a.mid(r.size()).toStdString() << std::endl;
        }
        return out.finished ? 0 : 1;
    }

    std::cout << "=== 扫描不受 .gitignore 影响探针 ===" << std::endl;
    std::cout << std::endl;

    // ---- A：无 .gitignore（基线）----
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "A: 临时目录创建成功");
        const QString root = tempDir.path();
        Check(BuildTree(root, QString()), "A: 建立测试目录（根 .vs + 其中 .exe + main.cpp）");

        const ScanOutcome out = RunScan(root);
        Check(out.finished, "A: 扫描完成");
        DumpHits(root, out);
        Check(Hit(out.hitPaths, root + "/.vs"), "A: 根目录 .vs 被识别为清理项");
        Check(Hit(out.hitPaths, root + "/.vs/CodeCleanTool_V0.1.0.exe"),
              "A: .vs 内的 .exe 命中自定义清理规则");
    }

    // ---- B：有 .gitignore 且含 .vs/ 与 *.exe（旧版在此被隐藏）----
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "B: 临时目录创建成功");
        const QString root = tempDir.path();
        Check(BuildTree(root, ".vs/\n*.exe\n"), "B: 写入含 .vs/ 与 *.exe 的 .gitignore");

        const ScanOutcome out = RunScan(root);
        Check(out.finished, "B: 扫描完成");
        DumpHits(root, out);
        Check(Hit(out.hitPaths, root + "/.vs"),
              "B: 有 .gitignore 时根目录 .vs 仍被识别（不再被排除语义隐藏）");
        Check(Hit(out.hitPaths, root + "/.vs/CodeCleanTool_V0.1.0.exe"),
              "B: 有 .gitignore 时 .exe 仍被识别");
    }

    // ---- C：.vs 带隐藏属性 + 有 .gitignore（两项叠加）----
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "C: 临时目录创建成功");
        const QString root = tempDir.path();
        BuildTree(root, ".vs/\n*.exe\n");
        Check(SetHiddenAttr(root + "/.vs"), "C: 已将 .vs 设为隐藏属性");

        const ScanOutcome out = RunScan(root);
        Check(out.finished, "C: 扫描完成");
        DumpHits(root, out);
        Check(Hit(out.hitPaths, root + "/.vs"), "C: 隐藏属性 + .gitignore 叠加时 .vs 仍被识别");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount == 0 ? 0 : 1;
}
