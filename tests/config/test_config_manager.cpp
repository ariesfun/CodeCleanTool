// 探针：ConfigManager 配置读写
// 覆盖：默认值 / 加载/保存 INI / 成员变量同步 / SetValue/GetValue
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
        Check(cfg.enableGitIgnore == true, "默认: enableGitIgnore == true");
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
        cfg.enableGitIgnore = false;
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
            wCfg.enableGitIgnore = false;
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
            Check(rCfg.enableGitIgnore == false, "Load: enableGitIgnore = false");
            Check(rCfg.autoPack == true, "Load: autoPack = true");
        }
    }

    // 4. SetValue / GetValue 通用读写
    {
        ConfigManager cfg;
        cfg.SetValue("custom_key", QString("hello"));
        QVariant val = cfg.GetValue("custom_key");
        Check(val.toString() == "hello",
            QString("SetValue/GetValue: %1").arg(val.toString()));

        cfg.SetValue("int_key", 42);
        Check(cfg.GetValue("int_key").toInt() == 42, "SetValue/GetValue: int 值");

        // 默认值
        QVariant def = cfg.GetValue("nonexistent", QString("default"));
        Check(def.toString() == "default", "GetValue: 默认值生效");
    }

    // 5. 加载不存在的文件 → 使用默认值
    {
        ConfigManager cfg;
        // 先设置非默认值
        cfg.enableGitIgnore = false;
        // 加载不存在文件
        bool loaded = cfg.Load("nonexistent_config_xyz456.ini");
        Check(!loaded, "Load: 不存在的文件返回 false");
        // 原值保持不变（Load 失败不覆盖）
        Check(cfg.enableGitIgnore == false, "Load 失败: 原值不变");
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
