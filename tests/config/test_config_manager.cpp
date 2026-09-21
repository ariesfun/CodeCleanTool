// 探针：ConfigManager 配置读写
// 覆盖：默认值 / 加载/保存 INI / 成员变量同步 / 清空后回落默认值
// 依赖：Qt5::Core（+ Iniconfig 底层存储）

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <iostream>

#include "config/ConfigManager.h"

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

    std::cout << "=== ConfigManager 配置读写探针 ===" << std::endl;
    std::cout << std::endl;

    // 1. 默认配置值
    {
        ConfigManager cfg;
        Check(cfg.excludeVcsDirs == true, "默认: excludeVcsDirs == true");
        Check(cfg.autoPack == false, "默认: autoPack == false");
        Check(cfg.darkTheme == true, "默认: darkTheme == true");
        Check(cfg.outputDir.isEmpty(), "默认: outputDir 为空");
        Check(cfg.recentDirs.isEmpty(), "默认: recentDirs 为空列表");
    }

    // 2. 保存配置到临时文件
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString configPath = tempDir.path() + "/config.ini";

        ConfigManager cfg;
        cfg.outputDir = "D:/TestOutput";
        cfg.packageNamePattern = "TestProject_%DATE%_source";
        cfg.autoPack = true;
        cfg.darkTheme = false;

        bool saved = cfg.Save(configPath);
        Check(saved, "Save: 配置保存成功");
        Check(QFileInfo::exists(configPath), "Save: INI 文件已创建");
    }

    // 3. 从临时文件加载配置并验证
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString configPath = tempDir.path() + "/config.ini";

        // 先保存一份配置
        {
            ConfigManager wCfg;
            wCfg.outputDir = "D:/LoadTest";
            wCfg.autoPack = true;
            wCfg.Save(configPath);
        }

        // 再加载验证
        {
            ConfigManager rCfg;
            bool loaded = rCfg.Load(configPath);
            Check(loaded, "Load: 配置文件加载成功");
            Check(rCfg.outputDir == "D:/LoadTest",
                QString("Load: outputDir = %1").arg(rCfg.outputDir));
            Check(rCfg.autoPack == true, "Load: autoPack = true");
        }
    }

    // 3b. 往返覆盖：darkTheme / packageNamePattern / excludeVcsDirs / sevenZipPath
    //     这几项此前没有被往返验证；其中 darkTheme（主题偏好）与 packageNamePattern
    //     （包名模板）是本轮修复的功能所依赖的持久化项
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");
        const QString configPath = tempDir.path() + "/config.ini";

        {
            ConfigManager wCfg;
            wCfg.darkTheme = false;
            wCfg.packageNamePattern = "%Project_%YYYY%MM%DD_source";
            wCfg.excludeVcsDirs = false;
            wCfg.sevenZipPath = "C:/Program Files/7-Zip/7z.exe";
            Check(wCfg.Save(configPath), "往返: 保存成功");
        }
        {
            ConfigManager rCfg;
            Check(rCfg.Load(configPath), "往返: 加载成功");
            Check(rCfg.darkTheme == false, "往返: darkTheme = false");
            Check(rCfg.packageNamePattern == "%Project_%YYYY%MM%DD_source",
                  QString("往返: packageNamePattern = %1").arg(rCfg.packageNamePattern));
            Check(rCfg.excludeVcsDirs == false, "往返: excludeVcsDirs = false");
            Check(rCfg.sevenZipPath == "C:/Program Files/7-Zip/7z.exe",
                  QString("往返: sevenZipPath = %1").arg(rCfg.sevenZipPath));
        }
    }

    // 3c. DefaultConfigPath: 应指向可执行文件同级目录下的 config.ini
    //     修复配置持久化的前提是路径可确定，不随启动时的工作目录漂移
    {
        const QString p = ConfigManager::DefaultConfigPath();
        Check(p.endsWith("/config.ini") || p.endsWith("\\config.ini"),
              QString("DefaultConfigPath: 以 config.ini 结尾 -> %1").arg(p));
        const QString appDir = QCoreApplication::applicationDirPath();
        Check(!appDir.isEmpty(), "DefaultConfigPath: applicationDirPath 非空");
        Check(p.startsWith(appDir), "DefaultConfigPath: 位于可执行文件同级目录");
        Check(!p.contains("/../"), "DefaultConfigPath: 路径不含上跳片段");
    }

    // 4. 清空某项后再加载 → 回落到默认值（不是空串）
    // 依据：IniConfig::getString 在「值存在但为空」时返回 defaultVal。
    // 用户可见后果：在设置页把「包名模板」清空并保存，重启后会回到默认模板，
    //               而「打包输出目录」的默认值本身就是空串，所以清空后仍是空串。
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "清空回落: 临时目录创建成功");
        const QString path = tempDir.path() + "/config.ini";

        ConfigManager cfg;
        cfg.packageNamePattern = "";    // 用户清空了包名模板
        cfg.outputDir = "";             // 用户清空了输出目录
        Check(cfg.Save(path), "清空回落: 保存成功");

        ConfigManager cfg2;
        Check(cfg2.Load(path), "清空回落: 加载成功");
        Check(cfg2.packageNamePattern == "%Project_%YYYY%MM%DD_%HH%MM%SS_source",
              "清空包名模板后回落为默认模板（而非空串）");
        Check(cfg2.outputDir.isEmpty(),
              "清空输出目录后仍为空串（该项默认值本就是空串）");
    }

    // 5. 加载不存在的文件 → 使用默认值
    {
        ConfigManager cfg;
        // 先设置非默认值
        cfg.excludeVcsDirs = false;
        // 加载不存在文件
        bool loaded = cfg.Load("nonexistent_config_xyz456.ini");
        Check(!loaded, "Load: 不存在的文件返回 false");
        // 原值保持不变（Load 失败不覆盖）
        Check(cfg.excludeVcsDirs == false, "Load 失败: 原值不变");
    }

    // 6. customCleanRules 列表读写
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "临时目录创建成功");

        QString configPath = tempDir.path() + "/config.ini";

        ConfigManager wCfg;
        wCfg.customCleanRules = QStringList{"*.custom1", "*.custom2", "build_custom/"};
        wCfg.Save(configPath);

        ConfigManager rCfg;
        rCfg.Load(configPath);
        Check(rCfg.customCleanRules.size() == 3,
            QString("customCleanRules: 应有 3 条，实际 %1").arg(rCfg.customCleanRules.size()));
        Check(rCfg.customCleanRules.contains("*.custom1"), "customCleanRules: 包含 *.custom1");
        Check(rCfg.customCleanRules.contains("build_custom/"), "customCleanRules: 包含 build_custom/");
    }

    // 7. excludeVcsDirs 配置读写往返
    {
        QTemporaryDir tempDir;
        Check(tempDir.isValid(), "VCS配置: 临时目录创建成功");

        QString configPath = tempDir.path() + "/config.ini";

        // 写入：关闭 VCS 排除
        {
            ConfigManager wCfg;
            wCfg.excludeVcsDirs = false;
            wCfg.Save(configPath);
        }

        // 读回
        {
            ConfigManager rCfg;
            rCfg.Load(configPath);
            Check(rCfg.excludeVcsDirs == false, "VCS配置: excludeVcsDirs=false 保存后加载正确");
        }

        // 写入：开启 VCS 排除（默认值）
        {
            ConfigManager wCfg;
            wCfg.excludeVcsDirs = true;
            wCfg.Save(configPath);
        }

        // 读回
        {
            ConfigManager rCfg;
            rCfg.Load(configPath);
            Check(rCfg.excludeVcsDirs == true, "VCS配置: excludeVcsDirs=true 保存后加载正确");
        }
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, "
              << g_failCount << " 失败 ===" << std::endl;

    return g_failCount > 0 ? 1 : 0;
}
