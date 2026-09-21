// 探针：程序装在含中文的路径下时，配置与日志还能不能正常落盘
//
// 背景：ConfigManager 与 Logger 都在内部把 QString 转成 std::string（UTF-8 字节），
//       再交给 std::ofstream / std::ifstream / _mkdir 这些窄字符 C 接口。
//       MSVC 的窄字符文件接口按【当前 ANSI 代码页】解释文件名，不按 UTF-8，
//       所以路径里的中文会被误读，表现为「文件没写到预期位置」。
//
// 影响面：config.ini 与 logs 目录都定位在 exe 同级，用户把程序解压到
//         「D:\工具\CodeCleanTool\」这类含中文的目录下即会命中。
//
// 用例：
//   A 日志目录含中文时，日志文件确实落在该目录下
//   B 配置路径含中文时，config.ini 确实落在该路径
//   C 且写出的配置能读回来（同一路径往返一致）
//
// 依赖：Qt5::Core
//
// 注：运行后 Qt 会打印 "QTemporaryDir: Unable to remove ..." 警告 ——
//     原因是 Logger 为单例、进程存活期间一直持有日志文件句柄，临时目录删不掉。
//     属预期现象，不影响用例结论。

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <iostream>

#include "config/ConfigManager.h"
#include "log/LogManager.h"

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

// 列出目录下的文件名，用于呈现「文件实际写到哪儿去了」
static QString ShowEntries(const QString& dir)
{
    return QDir(dir).entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).join(" | ");
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== 含中文路径下的落盘探针 ===" << std::endl;
    std::cout << std::endl;

    QTemporaryDir tempDir;
    Check(tempDir.isValid(), "临时目录创建成功");
    if (!tempDir.isValid())
    {
        std::cout << "=== 结果: 0 通过, 1 失败 ===" << std::endl;
        return 1;
    }

    // 模拟用户的安装目录：路径里带中文
    const QString cnRoot = tempDir.path() + "/工具集";
    QDir().mkpath(cnRoot);
    Check(QDir(cnRoot).exists(), "含中文的目录已创建（用 Qt 的宽字符接口）");

    // ---- A：日志目录含中文 ----
    {
        const QString logDir = cnRoot + "/logs";
        LogManager mgr;
        mgr.Init(logDir, "code-clean-tool.log");
        mgr.Append("INFO", "探针", "中文路径下的日志写入");

        const QStringList files = QDir(logDir).entryList(QStringList{"*.log"}, QDir::Files);
        std::cout << "       日志目录内容: " << ShowEntries(logDir).toStdString() << std::endl;
        Check(!files.isEmpty(),
              QString("A: 日志目录下确实生成了 .log 文件（实际 %1 个）").arg(files.size()));
        Check(QDir(cnRoot).exists(), "A: 含中文的目录未被误建/误删");
    }

    // ---- B/C：配置路径含中文 ----
    {
        const QString cfgPath = cnRoot + "/config.ini";

        ConfigManager cfg;
        cfg.outputDir = "C:/out";
        cfg.packageNamePattern = "%Project_source";
        cfg.excludeVcsDirs = false;
        cfg.autoPack = true;

        const bool saved = cfg.Save(cfgPath);
        std::cout << "       工具集目录内容: " << ShowEntries(cnRoot).toStdString() << std::endl;
        Check(saved, "B: Save 返回成功");
        Check(QFileInfo::exists(cfgPath),
              "B: config.ini 确实落在含中文的路径下");

        // 往返：换一个实例从同一路径读回
        ConfigManager cfg2;
        const bool loaded = cfg2.Load(cfgPath);
        Check(loaded, "C: 从含中文的路径 Load 成功");
        Check(cfg2.outputDir == "C:/out", "C: 输出目录往返一致");
        Check(cfg2.packageNamePattern == "%Project_source", "C: 包名模板往返一致");
        Check(cfg2.autoPack == true, "C: 自动打包开关往返一致");
        Check(cfg2.excludeVcsDirs == false, "C: VCS 排除开关往返一致");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount > 0 ? 1 : 0;
}
