// 探针：扫描结果集的边界 —— 结果模型里到底装了什么
//
// 起因：MainPresenter::OnPack 拿「未勾选项」当打包白名单，注释写着
//       「语义约定：未勾选项 = 保留项 = 需要打包的文件」。
//       这条约定成立的前提是：模型里既有待清理项、也有保留项，
//       于是「未勾选」等于「用户决定留下的源码」。
//
// 本探针不讨论该约定是否合理，只验证它的前提在代码里是否成立：
// 扫描结果模型装的是「全部文件」还是「仅命中的清理目标」。
//
// 用例：
//   A 同一目录里既有命中保留规则的源码，也有命中清理规则的垃圾
//   B 模型里每一项的命中规则都指向清理规则（没有一项是「保留项」）
//   C 结论对照：把模型全部反选后，所谓「未勾选集合」依然全是清理目标
//
// 依赖：Qt5::Core + Qt5::Gui（ResultModel 使用 QColor）

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

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

static bool CreateFile(const QString& dir, const QString& name)
{
    QDir().mkpath(dir);
    QFile file(dir + "/" + name);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write("probe", 5);
        file.close();
        return true;
    }
    return false;
}

// 同步跑一轮扫描：发起后等 ScanFinished，返回结果模型中的文件名清单
static QStringList RunScan(ScanManager& scanManager, ResultModel& model)
{
    QEventLoop loop;
    QObject::connect(&scanManager, &ScanManager::ScanFinished, &loop, &QEventLoop::quit);
    QObject::connect(&scanManager, &ScanManager::ScanError, &loop, &QEventLoop::quit);
    QTimer::singleShot(20000, &loop, &QEventLoop::quit);

    scanManager.StartScan();
    loop.exec();

    QStringList names;
    for (int i = 0; i < model.TotalCount(); ++i)
    {
        names << model.GetFile(i).fileName;
    }
    return names;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== 扫描结果集边界探针 ===" << std::endl;
    std::cout << std::endl;

    QTemporaryDir tempDir;
    Check(tempDir.isValid(), "临时目录创建成功");
    if (!tempDir.isValid())
    {
        std::cout << "=== 结果: 0 通过, 1 失败 ===" << std::endl;
        return 1;
    }
    const QString root = tempDir.path();

    // 一个典型工程目录：源码命中保留规则，垃圾命中清理规则
    CreateFile(root, "main.cpp");                       // 保留：*.cpp
    CreateFile(root, "utils.h");                        // 保留：*.h
    CreateFile(root, "CMakeLists.txt");                 // 保留：完整文件名
    CreateFile(root, "a.obj");                          // 清理：*.obj
    CreateFile(root, "junk.tmp");                       // 清理：*.tmp
    CreateFile(root + "/build", "y.obj");               // 清理：build/ 与其下的 *.obj
    CreateFile(root + "/.vs", "z.suo");                 // 清理：.vs/ 与其下的 *.suo

    RuleEngine engine;
    ResultModel model;
    ScanManager scanManager;
    scanManager.SetRootPath(root);
    scanManager.SetRuleEngine(&engine);
    scanManager.SetResultModel(&model);

    const QStringList names = RunScan(scanManager, model);
    std::cout << "       扫描结果共 " << names.size() << " 项: "
              << names.join(" | ").toStdString() << std::endl;

    // ---- A：结果集里应当只有清理目标，源码不在其中 ----
    Check(names.contains("a.obj"), "A: *.obj 命中清理规则，进入结果集");
    Check(names.contains("junk.tmp"), "A: *.tmp 命中清理规则，进入结果集");
    Check(names.contains("build"), "A: build/ 命中目录规则，进入结果集");
    Check(names.contains(".vs"), "A: .vs/ 命中目录规则，进入结果集");

    Check(!names.contains("main.cpp"),
          "A: main.cpp 命中的是保留规则，【不在】结果集里");
    Check(!names.contains("utils.h"),
          "A: utils.h 命中的是保留规则，【不在】结果集里");
    Check(!names.contains("CMakeLists.txt"),
          "A: CMakeLists.txt 命中的是保留规则，【不在】结果集里");

    // ---- B：结果集里每一项都是清理目标，没有任何一项是保留项 ----
    bool anyKeepItem = false;
    for (int i = 0; i < model.TotalCount(); ++i)
    {
        const FileItem item = model.GetFile(i);
        if (item.hitRule.isEmpty() || item.hitRule == "未匹配")
        {
            anyKeepItem = true;
            std::cout << "       结果集中出现非清理目标: " << item.fileName.toStdString()
                      << " (命中规则: " << item.hitRule.toStdString() << ")" << std::endl;
        }
    }
    Check(!anyKeepItem, "B: 结果集每一项都是清理目标，不存在「保留项」这种行");

    // ---- C：把结果集全部反选，得到的「未勾选集合」依然全是清理目标 ----
    // 打包白名单若取「未勾选项」，它拿到的就是这些 —— 恰恰是要清掉的东西
    for (int i = 0; i < model.TotalCount(); ++i)
    {
        model.setData(model.index(i, ResultModel::ColName), Qt::Unchecked, Qt::CheckStateRole);
    }

    QStringList uncheckedNames;
    for (int i = 0; i < model.TotalCount(); ++i)
    {
        if (!model.GetFile(i).checked) { uncheckedNames << model.GetFile(i).fileName; }
    }

    std::cout << "       全部反选后的「未勾选集合」共 " << uncheckedNames.size() << " 项: "
              << uncheckedNames.join(" | ").toStdString() << std::endl;

    Check(uncheckedNames.size() == model.TotalCount(),
          "C: 全部反选后，未勾选集合等于整个结果集");
    Check(uncheckedNames.contains("a.obj") && uncheckedNames.contains("build")
              && uncheckedNames.contains(".vs"),
          "C: 该集合里全是清理目标（.obj / .tmp / build/ / .vs/），"
          "没有一项是要保留的源码");
    Check(!uncheckedNames.contains("main.cpp"),
          "C: 该集合里【不存在】main.cpp —— 因此「未勾选 = 保留的源码」这一前提不成立");

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount > 0 ? 1 : 0;
}
