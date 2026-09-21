// 探针：FileCleaner 目录删除
// 覆盖：递归目录删除 / 空目录 / 多级嵌套目录
// 依赖：Qt5::Core + Qt5::Widgets（FileCleaner 使用 QThread）

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

#include "cleaner/FileCleaner.h"

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

// 在临时目录中创建多级嵌套结构
// 返回最外层目录路径
static QString createNestedDir(const QString& basePath)
{
    QString root = basePath + "/nested_root";
    QDir().mkdir(root);
    QDir().mkdir(root + "/sub1");
    QDir().mkdir(root + "/sub1/sub1a");
    QDir().mkdir(root + "/sub2");

    // 写入测试文件
    QStringList files = {
        root + "/root_file.txt",
        root + "/sub1/file_a.txt",
        root + "/sub1/file_b.txt",
        root + "/sub1/sub1a/deep_file.txt",
        root + "/sub2/file_c.txt"
    };
    for (const auto& f : files)
    {
        QFile file(f);
        file.open(QIODevice::WriteOnly);
        file.write("nested probe test", 17);
        file.close();
    }

    return root;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== FileCleaner 目录删除探针 ===" << std::endl;
    std::cout << std::endl;

    // 1. 空目录删除
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString emptyDir = tempDir.path() + "/empty_dir";
        QDir().mkdir(emptyDir);
        Check(QFileInfo::exists(emptyDir), "空目录创建成功");

        FileCleaner cleaner;
        bool finished = false;
        int deletedCount = 0;

        QObject::connect(&cleaner, &FileCleaner::CleanFinished,
            [&](int ok, int) { deletedCount = ok; finished = true; });

        cleaner.SetTargetList(QStringList{emptyDir});
        cleaner.StartClean();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&cleaner, &FileCleaner::CleanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(5000);
        app.exec();

        Check(finished, "空目录: CleanFinished 信号已触发");
        Check(deletedCount == 1, "空目录: 成功删除");
        Check(!QFileInfo::exists(emptyDir), "空目录: 已不存在于磁盘");
    }

    // 2. 多级嵌套目录递归删除
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString nestedRoot = createNestedDir(tempDir.path());
        Check(QFileInfo::exists(nestedRoot), "嵌套目录创建成功");
        Check(QFileInfo::exists(nestedRoot + "/sub1/sub1a/deep_file.txt"), "深层文件存在");

        FileCleaner cleaner;
        bool finished = false;
        int deletedCount = 0;

        QObject::connect(&cleaner, &FileCleaner::CleanFinished,
            [&](int ok, int) { deletedCount = ok; finished = true; });

        cleaner.SetTargetList(QStringList{nestedRoot});
        cleaner.StartClean();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&cleaner, &FileCleaner::CleanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(5000);
        app.exec();

        Check(finished, "嵌套目录: CleanFinished 信号已触发");
        Check(deletedCount == 1, "嵌套目录: 成功删除（顶层目录计为 1）");
        Check(!QFileInfo::exists(nestedRoot), "嵌套目录: 已不存在于磁盘");
        Check(!QFileInfo::exists(nestedRoot + "/sub1"), "子目录 sub1 已删除");
        Check(!QFileInfo::exists(nestedRoot + "/sub1/sub1a/deep_file.txt"), "深层文件已删除");
    }

    // 3. 多个目录同时删除
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString dirA = tempDir.path() + "/multi_a";
        QString dirB = tempDir.path() + "/multi_b";
        QDir().mkdir(dirA);
        QDir().mkdir(dirB);
        QFile(dirA + "/a.txt").open(QIODevice::WriteOnly);
        QFile(dirB + "/b.txt").open(QIODevice::WriteOnly);

        FileCleaner cleaner;
        bool finished = false;
        int deletedCount = 0;

        QObject::connect(&cleaner, &FileCleaner::CleanFinished,
            [&](int ok, int) { deletedCount = ok; finished = true; });

        cleaner.SetTargetList(QStringList{dirA, dirB});
        cleaner.StartClean();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&cleaner, &FileCleaner::CleanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(5000);
        app.exec();

        Check(finished, "多目录: CleanFinished 信号已触发");
        Check(deletedCount >= 2, QString("多目录: 成功数 >= 2，实际 %1").arg(deletedCount));
    }

    // 4. 不存在的目录
    {
        FileCleaner cleaner;
        bool finished = false;
        int deletedCount = -1;

        QObject::connect(&cleaner, &FileCleaner::CleanFinished,
            [&](int ok, int) { deletedCount = ok; finished = true; });

        cleaner.SetTargetList(QStringList{"Z:/nonexistent_dir_xyz789"});
        cleaner.StartClean();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&cleaner, &FileCleaner::CleanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(5000);
        app.exec();

        Check(finished, "不存在的目录: CleanFinished 信号已触发");
        // 注意：路径不存在时 QFileInfo::isDir() 为 false，实际会走 DeleteFile 分支。
        // 删除幂等语义下记为成功，与 DeleteDir 对「目录不存在」的处理保持一致。
        Check(deletedCount == 1, "不存在的目录: 记为成功（删除幂等）");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, "
              << g_failCount << " 失败 ===" << std::endl;

    return g_failCount > 0 ? 1 : 0;
}
