// 探针：FileCleaner 单文件删除
// 覆盖：普通文件删除 / 只读文件 / 不存在路径 / 空列表
// 依赖：Qt5::Core + Qt5::Widgets（FileCleaner 使用 QThread）
// 注意：FileCleaner 异步执行，需要事件循环等待信号

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <memory>

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

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== FileCleaner 单文件删除探针 ===" << std::endl;
    std::cout << std::endl;

    // 辅助函数：创建临时文件并填充内容
    auto createTempFile = [](const QString& dir, const QString& name) -> QString {
        QString path = dir + "/" + name;
        QFile file(path);
        if (file.open(QIODevice::WriteOnly))
        {
            file.write("probe test content", 18);
            file.close();
        }
        return path;
    };

    // 1. 正常文件删除
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString filePath = createTempFile(tempDir.path(), "normal_test.txt");
        Check(QFileInfo::exists(filePath), "临时文件创建成功");

        FileCleaner cleaner;
        bool finished = false;
        int deletedCount = 0;
        int failedCount = 0;

        QObject::connect(&cleaner, &FileCleaner::CleanFinished,
            [&](int ok, int fail)
            {
                deletedCount = ok;
                failedCount = fail;
                finished = true;
            });

        cleaner.SetTargetList(QStringList{filePath});
        cleaner.StartClean();

        // 等待事件循环处理（最多 5 秒）
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&cleaner, &FileCleaner::CleanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(5000);
        app.exec();

        Check(finished, "CleanFinished 信号已触发");
        Check(deletedCount == 1, QString("成功删除 %1 个文件").arg(deletedCount));
        Check(failedCount == 0, "无删除失败");
        Check(!QFileInfo::exists(filePath), "文件已不存在于磁盘");
    }

    // 2. 删除不存在的文件
    {
        FileCleaner cleaner;
        bool finished = false;
        int deletedCount = -1;
        int failedCount = -1;

        QObject::connect(&cleaner, &FileCleaner::CleanFinished,
            [&](int ok, int fail)
            {
                deletedCount = ok;
                failedCount = fail;
                finished = true;
            });

        QString nonexistent = "Z:/nonexistent_path_xyz123/test_file.txt";
        cleaner.SetTargetList(QStringList{nonexistent});
        cleaner.StartClean();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&cleaner, &FileCleaner::CleanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(5000);
        app.exec();

        Check(finished, "清理不存在的文件: CleanFinished 信号已触发");
        // 删除是幂等操作：目标已不存在即视为达成，记为成功。
        // 该语义与 DeleteDir 既有的「目录不存在直接返回成功」保持一致，
        // 且是必需的——扫描会同时产出目录目标与其内部文件目标，
        // 目录先被删除后，内部文件再单独删时必然已不存在。
        Check(deletedCount == 1, "清理不存在的文件: 记为成功（删除幂等）");
    }

    // 3. 删除只读文件
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString filePath = createTempFile(tempDir.path(), "readonly_test.txt");
        // 设置只读属性
        QFile::setPermissions(filePath, QFileDevice::ReadOwner);
        Check(!QFileInfo(filePath).isWritable(), "只读属性设置成功");

        FileCleaner cleaner;
        bool finished = false;
        int deletedCount = 0;

        QObject::connect(&cleaner, &FileCleaner::CleanFinished,
            [&](int ok, int)
            {
                deletedCount = ok;
                finished = true;
            });

        cleaner.SetTargetList(QStringList{filePath});
        cleaner.StartClean();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&cleaner, &FileCleaner::CleanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(5000);
        app.exec();

        Check(finished, "只读文件: CleanFinished 信号已触发");
        Check(deletedCount == 1, "只读文件: 自动取消只读后删除成功");
        Check(!QFileInfo::exists(filePath), "只读文件: 已不存在于磁盘");
    }

    // 4. 空列表 → 直接完成
    {
        FileCleaner cleaner;
        bool finished = false;

        QObject::connect(&cleaner, &FileCleaner::CleanFinished,
            [&](int, int) { finished = true; });

        cleaner.SetTargetList(QStringList{});
        cleaner.StartClean();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&cleaner, &FileCleaner::CleanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(3000);
        app.exec();

        Check(finished, "空列表: CleanFinished(0,0) 直接触发");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, "
              << g_failCount << " 失败 ===" << std::endl;

    return g_failCount > 0 ? 1 : 0;
}
