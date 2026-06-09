// 探针：Packager 打包
// 覆盖：Find7zPath 路径检测 / 包名生成 / 前置条件检查 / 打包流程
// 依赖：Qt5::Core（如需实际打包还需要 7z CLI）
// 注意：实际打包测试需要 7z CLI，若无则跳过并标记为 PASS（非阻塞）

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

#include "core/Packager.h"

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

// 辅助：在指定目录下创建文件
static bool CreateFile(const QString& dir, const QString& name)
{
    QFile file(dir + "/" + name);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write("test content for packaging probe", 31);
        file.close();
        return true;
    }
    return false;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== Packager 打包探针 ===" << std::endl;
    std::cout << std::endl;

    // 1. Find7zPath 检测（至少返回非空字符串，或明确告知未找到）
    {
        QString sevenZip = Packager::Find7zPath();
        if (sevenZip.isEmpty())
        {
            Skip("未找到 7z CLI，跳过实际打包测试");
        }
        else
        {
            Check(QFileInfo::exists(sevenZip),
                QString("Find7zPath: 7z 路径存在 -> %1").arg(sevenZip));
        }
    }

    // 2. 未设置 sourceDir → 触发 PackError
    {
        Packager packager;
        bool errorReceived = false;
        QString errorMsg;
        QObject::connect(&packager, &Packager::PackError,
            [&](const QString& msg)
            {
                errorReceived = true;
                errorMsg = msg;
            });

        packager.StartPack();

        QTimer::singleShot(500, &app, &QCoreApplication::quit);
        app.exec();

        Check(errorReceived, "无 sourceDir: PackError 信号已触发");
        Check(errorMsg.contains("源码"), "无 sourceDir: 错误消息含'源码'");
    }

    // 3. 设置了 sourceDir 但无 7z → 触发 PackError
    {
        // 暂时覆盖 Find7zPath 不可行（静态方法），直接测试未找到 7z 的场景
        // 若 Find7zPath 有结果则此测试不适用，改为验证 setSourceDir 不崩溃
        QTemporaryDir tempDir;
        if (tempDir.isValid())
        {
            Packager packager;
            packager.SetSourceDir(tempDir.path());
            packager.SetOutputDir(tempDir.path() + "/output");

            QString sevenZip = Packager::Find7zPath();
            if (sevenZip.isEmpty())
            {
                bool errorReceived = false;
                QObject::connect(&packager, &Packager::PackError,
                    [&](const QString&) { errorReceived = true; });

                packager.StartPack();

                QTimer::singleShot(500, &app, &QCoreApplication::quit);
                app.exec();

                Check(errorReceived, "无 7z: PackError 信号已触发");
            }
            else
            {
                Skip("7z 可用，跳过无 7z 错误测试");
            }
        }
    }

    // 4. 实际打包测试（需要 7z CLI）
    {
        QString sevenZip = Packager::Find7zPath();
        if (!sevenZip.isEmpty())
        {
            QTemporaryDir tempDir;
            Check(tempDir.isValid(), "打包测试: 临时目录创建成功");

            // 创建测试文件
            Check(CreateFile(tempDir.path(), "main.cpp"), "打包测试: 创建 main.cpp");
            Check(CreateFile(tempDir.path(), "utils.h"), "打包测试: 创建 utils.h");
            Check(CreateFile(tempDir.path(), "CMakeLists.txt"), "打包测试: 创建 CMakeLists.txt");

            QString outputDir = tempDir.path() + "/output";

            Packager packager;
            packager.SetSourceDir(tempDir.path());
            packager.SetOutputDir(outputDir);

            bool finished = false;
            bool error = false;
            QString outputPath;
            qint64 outputSize = 0;
            QObject::connect(&packager, &Packager::PackFinished,
                [&](const QString& path, qint64 size)
                {
                    finished = true;
                    outputPath = path;
                    outputSize = size;
                });
            QObject::connect(&packager, &Packager::PackError,
                [&](const QString&) { error = true; });

            packager.StartPack();

            // 等待打包完成（最多 15 秒）
            QTimer timeoutTimer;
            timeoutTimer.setSingleShot(true);
            QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
            QObject::connect(&packager, &Packager::PackFinished, &app, &QCoreApplication::quit);
            QObject::connect(&packager, &Packager::PackError, &app, &QCoreApplication::quit);
            timeoutTimer.start(15000);
            app.exec();

            if (finished)
            {
                Check(!outputPath.isEmpty(), "打包测试: 输出路径非空");
                Check(outputSize > 0, QString("打包测试: 输出大小 > 0 (%1 bytes)").arg(outputSize));
                Check(QFileInfo::exists(outputPath), "打包测试: .7z 文件已创建");
                Check(outputPath.endsWith(".7z"), "打包测试: 输出为 .7z 文件");
                Check(outputPath.contains("_source"), "打包测试: 包名含 '_source'");
            }
            else if (error)
            {
                Check(false, "打包测试: 7z 进程执行失败");
            }
            else
            {
                Check(false, "打包测试: 超时未完成");
            }
        }
        else
        {
            Skip("无 7z CLI，跳过实际打包测试");
        }
    }

    // 5. CancelPack 不崩溃
    {
        QString sevenZip = Packager::Find7zPath();
        if (!sevenZip.isEmpty())
        {
            QTemporaryDir tempDir;
            if (tempDir.isValid())
            {
                CreateFile(tempDir.path(), "large_test.cpp");

                Packager packager;
                packager.SetSourceDir(tempDir.path());
                packager.StartPack();

                // 立即取消
                packager.CancelPack();

                QTimer::singleShot(500, &app, &QCoreApplication::quit);
                app.exec();

                Check(true, "CancelPack: 未崩溃");
            }
        }
        else
        {
            Skip("无 7z CLI，跳过 CancelPack 测试");
        }
    }

    std::cout << std::endl;
    int total = g_passCount + g_failCount + g_skipCount;
    std::cout << "=== 结果: " << g_passCount << " 通过, "
              << g_failCount << " 失败, "
              << g_skipCount << " 跳过 (共 " << total << " 项) ===" << std::endl;

    return g_failCount > 0 ? 1 : 0;
}
