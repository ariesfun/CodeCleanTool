// 探针：ScanManager 目录扫描
// 覆盖：空目录扫描 / 匹配清理规则 / 保留规则过滤 / 不存在路径 / 取消中断
// 依赖：Qt5::Core
// 注意：扫描为异步 QThread，需事件循环等待信号

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

#include "core/ScanManager.h"
#include "core/RuleEngine.h"
#include "core/GitIgnoreParser.h"
#include "core/ResultModel.h"

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

// 辅助：在指定目录下创建文件并填充内容
static bool CreateFile(const QString& dir, const QString& name)
{
    QFile file(dir + "/" + name);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write("probe", 5);
        file.close();
        return true;
    }
    return false;
}

// 辅助：在指定目录下创建子目录
static bool CreateDir(const QString& parentDir, const QString& name)
{
    return QDir(parentDir).mkdir(name);
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== ScanManager 目录扫描探针 ===" << std::endl;
    std::cout << std::endl;

    // 1. 空目录扫描 → 0 个文件
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "空目录: 临时目录创建成功");

        RuleEngine ruleEngine;
        GitIgnoreParser gitIgnore;
        ResultModel resultModel;

        ScanManager scanManager;
        scanManager.SetRootPath(tempDir.path());
        scanManager.SetRuleEngine(&ruleEngine);
        scanManager.SetGitIgnoreParser(&gitIgnore);
        scanManager.SetResultModel(&resultModel);

        bool finished = false;
        int totalFiles = -1;
        QObject::connect(&scanManager, &ScanManager::ScanFinished,
            [&](int total, qint64)
            {
                finished = true;
                totalFiles = total;
            });

        scanManager.StartScan();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&scanManager, &ScanManager::ScanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(10000);
        app.exec();

        Check(finished, "空目录: ScanFinished 信号已触发");
        Check(totalFiles == 0, QString("空目录: 找到 %1 个待清理项，应为 0").arg(totalFiles));
        Check(resultModel.TotalCount() == 0, "空目录: ResultModel 为空");
    }

    // 2. 有匹配清理规则的文件 → 应被扫描到并放入 ResultModel
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "清理匹配: 临时目录创建成功");

        // 创建命中清理规则的文件
        Check(CreateFile(tempDir.path(), "test.obj"), "清理匹配: 创建 test.obj");
        Check(CreateFile(tempDir.path(), "test.pdb"), "清理匹配: 创建 test.pdb");
        Check(CreateFile(tempDir.path(), "test.tmp"), "清理匹配: 创建 test.tmp");
        // 创建保留的文件
        Check(CreateFile(tempDir.path(), "main.cpp"), "清理匹配: 创建 main.cpp");

        // 创建命中清理规则的目录
        Check(CreateDir(tempDir.path(), "build"), "清理匹配: 创建 build/ 目录");
        Check(CreateFile(tempDir.path() + "/build", "output.lib"), "清理匹配: 创建 build/output.lib");

        RuleEngine ruleEngine;
        GitIgnoreParser gitIgnore;
        ResultModel resultModel;

        ScanManager scanManager;
        scanManager.SetRootPath(tempDir.path());
        scanManager.SetRuleEngine(&ruleEngine);
        scanManager.SetGitIgnoreParser(&gitIgnore);
        scanManager.SetResultModel(&resultModel);

        bool finished = false;
        int totalFiles = -1;
        QObject::connect(&scanManager, &ScanManager::ScanFinished,
            [&](int total, qint64)
            {
                finished = true;
                totalFiles = total;
            });

        scanManager.StartScan();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&scanManager, &ScanManager::ScanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(10000);
        app.exec();

        Check(finished, "清理匹配: ScanFinished 信号已触发");
        Check(totalFiles > 0, QString("清理匹配: 找到 %1 个待清理项").arg(totalFiles));
        Check(resultModel.TotalCount() > 0,
            QString("清理匹配: ResultModel 有 %1 项").arg(resultModel.TotalCount()));

        // 验证清理项中存在 .obj 文件
        bool hasObj = false;
        bool hasBuild = false;
        bool hasCpp = false;
        for (int i = 0; i < resultModel.TotalCount(); ++i)
        {
            auto item = resultModel.GetFile(i);
            if (item.fileName == "test.obj") { hasObj = true; }
            if (item.filePath.contains("build")) { hasBuild = true; }
            if (item.fileName == "main.cpp") { hasCpp = true; }
        }
        Check(hasObj, "清理匹配: test.obj 命中清理规则");
        Check(hasBuild, "清理匹配: build 目录命中清理规则");
        Check(!hasCpp, "清理匹配: main.cpp 被保留规则过滤");
    }

    // 3. 不存在的目录 → ScanError 信号
    {
        RuleEngine ruleEngine;
        GitIgnoreParser gitIgnore;
        ResultModel resultModel;

        ScanManager scanManager;
        scanManager.SetRootPath("Z:/nonexistent_path_xyz_123456/");
        scanManager.SetRuleEngine(&ruleEngine);
        scanManager.SetGitIgnoreParser(&gitIgnore);
        scanManager.SetResultModel(&resultModel);

        bool errorReceived = false;
        QString errorMsg;
        QObject::connect(&scanManager, &ScanManager::ScanError,
            [&](const QString& msg)
            {
                errorReceived = true;
                errorMsg = msg;
            });

        scanManager.StartScan();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&scanManager, &ScanManager::ScanError, &app, &QCoreApplication::quit);
        timeoutTimer.start(5000);
        app.exec();

        Check(errorReceived, "不存在目录: ScanError 信号已触发");
        Check(!errorMsg.isEmpty(), "不存在目录: 错误消息非空");
    }

    // 4. 无规则引擎 / 无 gitignore → 不应崩溃
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "空规则: 临时目录创建成功");
        Check(CreateFile(tempDir.path(), "anyfile.txt"), "空规则: 创建 anyfile.txt");

        ResultModel resultModel;

        ScanManager scanManager;
        scanManager.SetRootPath(tempDir.path());
        scanManager.SetResultModel(&resultModel);
        // 不设置 RuleEngine 和 GitIgnoreParser

        bool finished = false;
        int totalFiles = -1;
        QObject::connect(&scanManager, &ScanManager::ScanFinished,
            [&](int total, qint64)
            {
                finished = true;
                totalFiles = total;
            });

        scanManager.StartScan();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&scanManager, &ScanManager::ScanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(10000);
        app.exec();

        Check(finished, "空规则: ScanFinished 信号已触发（未崩溃）");
    }

    // 5. 取消扫描
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "取消扫描: 临时目录创建成功");

        // 创建较多文件以增加扫描时间
        for (int i = 0; i < 200; ++i)
        {
            CreateFile(tempDir.path(), QString("file_%1.tmp").arg(i));
        }

        RuleEngine ruleEngine;
        GitIgnoreParser gitIgnore;
        ResultModel resultModel;

        ScanManager scanManager;
        scanManager.SetRootPath(tempDir.path());
        scanManager.SetRuleEngine(&ruleEngine);
        scanManager.SetGitIgnoreParser(&gitIgnore);
        scanManager.SetResultModel(&resultModel);

        bool finished = false;
        QObject::connect(&scanManager, &ScanManager::ScanFinished,
            [&](int, qint64) { finished = true; });

        scanManager.StartScan();

        // 立即取消
        scanManager.CancelScan();

        // 等待清理完成
        QTimer::singleShot(1000, &app, &QCoreApplication::quit);
        app.exec();

        // 取消后 finished 可能已触发（扫描在取消前完成）或未触发
        // 关键是不崩溃
        Check(true, "取消扫描: 未崩溃");
    }

    // 6. VCS 目录跳过：.git/ 和 .svn/ 中的文件不应出现在扫描结果中
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "VCS跳过: 临时目录创建成功");

        // 创建正常命中清理规则的文件
        Check(CreateFile(tempDir.path(), "normal.obj"), "VCS跳过: 创建 normal.obj");

        // 创建 .git 目录并在其中放置清理规则命中文件
        Check(CreateDir(tempDir.path(), ".git"), "VCS跳过: 创建 .git/ 目录");
        Check(CreateFile(tempDir.path() + "/.git", "should_skip.obj"), "VCS跳过: 创建 .git/should_skip.obj");
        Check(CreateFile(tempDir.path() + "/.git", "should_skip.tmp"), "VCS跳过: 创建 .git/should_skip.tmp");

        // 创建 .svn 目录并在其中放置清理规则命中文件
        Check(CreateDir(tempDir.path(), ".svn"), "VCS跳过: 创建 .svn/ 目录");
        Check(CreateFile(tempDir.path() + "/.svn", "should_skip.obj"), "VCS跳过: 创建 .svn/should_skip.obj");

        RuleEngine ruleEngine;
        GitIgnoreParser gitIgnore;
        ResultModel resultModel;

        ScanManager scanManager;
        scanManager.SetRootPath(tempDir.path());
        scanManager.SetRuleEngine(&ruleEngine);
        scanManager.SetGitIgnoreParser(&gitIgnore);
        scanManager.SetResultModel(&resultModel);
        scanManager.SetExcludeVcsDirs(true);

        bool finished = false;
        int totalFiles = -1;
        QObject::connect(&scanManager, &ScanManager::ScanFinished,
            [&](int total) { finished = true; totalFiles = total; });

        scanManager.StartScan();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&scanManager, &ScanManager::ScanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(10000);
        app.exec();

        Check(finished, "VCS跳过: ScanFinished 信号已触发");
        // 只应找到 normal.obj，.git/ 和 .svn/ 中的文件应被跳过
        Check(totalFiles == 1,
            QString("VCS跳过: 找到 %1 个待清理项，应为 1（仅 normal.obj）").arg(totalFiles));

        bool hasNormal = false;
        bool hasGitFile = false;
        bool hasSvnFile = false;
        for (int i = 0; i < resultModel.TotalCount(); ++i)
        {
            auto item = resultModel.GetFile(i);
            if (item.fileName == "normal.obj") { hasNormal = true; }
            if (item.filePath.contains("/.git/")) { hasGitFile = true; }
            if (item.filePath.contains("/.svn/")) { hasSvnFile = true; }
        }
        Check(hasNormal, "VCS跳过: normal.obj 在结果中");
        Check(!hasGitFile, "VCS跳过: .git/ 中的文件不在结果中");
        Check(!hasSvnFile, "VCS跳过: .svn/ 中的文件不在结果中");
    }

    // 7. VCS 目录不跳过：关闭 excludeVcsDirs 时 .git 中的文件应出现
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "VCS不跳过: 临时目录创建成功");

        Check(CreateFile(tempDir.path(), "top.obj"), "VCS不跳过: 创建 top.obj");
        Check(CreateDir(tempDir.path(), ".git"), "VCS不跳过: 创建 .git/ 目录");
        Check(CreateFile(tempDir.path() + "/.git", "inside.obj"), "VCS不跳过: 创建 .git/inside.obj");

        RuleEngine ruleEngine;
        GitIgnoreParser gitIgnore;
        ResultModel resultModel;

        ScanManager scanManager;
        scanManager.SetRootPath(tempDir.path());
        scanManager.SetRuleEngine(&ruleEngine);
        scanManager.SetGitIgnoreParser(&gitIgnore);
        scanManager.SetResultModel(&resultModel);
        scanManager.SetExcludeVcsDirs(false);  // 关闭 VCS 跳过

        bool finished = false;
        int totalFiles = -1;
        QObject::connect(&scanManager, &ScanManager::ScanFinished,
            [&](int total) { finished = true; totalFiles = total; });

        scanManager.StartScan();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&scanManager, &ScanManager::ScanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(10000);
        app.exec();

        Check(finished, "VCS不跳过: ScanFinished 信号已触发");
        // 关闭 VCS 跳过后，.git/ 内的 .obj 也应被扫描到
        Check(totalFiles == 2,
            QString("VCS不跳过: 找到 %1 个待清理项，应为 2（top.obj + inside.obj）").arg(totalFiles));
    }

    // 8. 子目录中的 .vs 应被扫描到并标记为清理目标
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), ".vs子目录: 临时目录创建成功");

        // 创建子目录结构：root/subdir/.vs/
        Check(CreateDir(tempDir.path(), "subdir"), ".vs子目录: 创建 subdir/");
        Check(CreateDir(tempDir.path() + "/subdir", ".vs"), ".vs子目录: 创建 subdir/.vs/");
        Check(CreateFile(tempDir.path() + "/subdir/.vs", ".suo"), ".vs子目录: 创建 subdir/.vs/.suo");
        Check(CreateFile(tempDir.path() + "/subdir/.vs", "Browse.VC.db"), ".vs子目录: 创建 Browse.VC.db");
        // 也创建一个普通源码文件
        Check(CreateFile(tempDir.path(), "main.cpp"), ".vs子目录: 创建 main.cpp");

        RuleEngine ruleEngine;
        GitIgnoreParser gitIgnore;
        ResultModel resultModel;

        ScanManager scanManager;
        scanManager.SetRootPath(tempDir.path());
        scanManager.SetRuleEngine(&ruleEngine);
        scanManager.SetGitIgnoreParser(&gitIgnore);
        scanManager.SetResultModel(&resultModel);

        bool finished = false;
        int totalFiles = -1;
        QObject::connect(&scanManager, &ScanManager::ScanFinished,
            [&](int total) { finished = true; totalFiles = total; });

        scanManager.StartScan();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&scanManager, &ScanManager::ScanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(10000);
        app.exec();

        Check(finished, ".vs子目录: ScanFinished 信号已触发");

        // 应该在结果中找到 .vs 目录和 .suo / Browse.VC.db 文件
        bool hasVsDir = false;
        bool hasSuoFile = false;
        bool hasDbFile = false;
        for (int i = 0; i < resultModel.TotalCount(); ++i)
        {
            auto item = resultModel.GetFile(i);
            if (item.filePath.endsWith("/.vs") || item.filePath.endsWith("\\.vs"))
            {
                hasVsDir = true;
            }
            if (item.fileName == ".suo")
            {
                hasSuoFile = true;
            }
            if (item.fileName == "Browse.VC.db")
            {
                hasDbFile = true;
            }
        }
        Check(hasVsDir,
            QString(".vs子目录: .vs 目录在扫描结果中 (共 %1 项)").arg(resultModel.TotalCount()));
        Check(hasSuoFile,
            QString(".vs子目录: .suo 文件在扫描结果中 (共 %1 项)").arg(resultModel.TotalCount()));
        Check(hasDbFile,
            QString(".vs子目录: Browse.VC.db 命中 *.db 规则 (共 %1 项, hasSuo=%2)")
                .arg(resultModel.TotalCount()).arg(hasSuoFile));
    }

    // 9. 清理目录规则（.vs/ build/ debug/ 等）的目录自身应作为清理目标
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "清理目录自身: 临时目录创建成功");

        // 创建多个IDE缓存目录
        Check(CreateDir(tempDir.path(), ".vs"), "清理目录自身: 创建 .vs/");
        Check(CreateDir(tempDir.path(), ".idea"), "清理目录自身: 创建 .idea/");
        Check(CreateDir(tempDir.path(), "build"), "清理目录自身: 创建 build/");
        Check(CreateDir(tempDir.path(), "debug"), "清理目录自身: 创建 debug/");

        RuleEngine ruleEngine;
        GitIgnoreParser gitIgnore;
        ResultModel resultModel;

        ScanManager scanManager;
        scanManager.SetRootPath(tempDir.path());
        scanManager.SetRuleEngine(&ruleEngine);
        scanManager.SetGitIgnoreParser(&gitIgnore);
        scanManager.SetResultModel(&resultModel);

        bool finished = false;
        int totalFiles = -1;
        QObject::connect(&scanManager, &ScanManager::ScanFinished,
            [&](int total) { finished = true; totalFiles = total; });

        scanManager.StartScan();

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QObject::connect(&timeoutTimer, &QTimer::timeout, &app, &QCoreApplication::quit);
        QObject::connect(&scanManager, &ScanManager::ScanFinished, &app, &QCoreApplication::quit);
        timeoutTimer.start(10000);
        app.exec();

        Check(finished, "清理目录自身: ScanFinished 信号已触发");

        // 应该找到 .vs, .idea, build, debug 四个目录作为清理目标
        int dirCount = 0;
        for (int i = 0; i < resultModel.TotalCount(); ++i)
        {
            auto item = resultModel.GetFile(i);
            QString fn = item.fileName;
            if (fn == ".vs" || fn == ".idea" || fn == "build" || fn == "debug")
            {
                ++dirCount;
            }
        }
        Check(dirCount == 4,
            QString("清理目录自身: 找到 %1 个清理目标目录，应为 4（当前共 %2 项）")
                .arg(dirCount).arg(resultModel.TotalCount()));
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, "
              << g_failCount << " 失败 ===" << std::endl;

    return g_failCount > 0 ? 1 : 0;
}
