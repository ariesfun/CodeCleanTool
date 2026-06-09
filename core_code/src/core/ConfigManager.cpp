// 配置管理器实现：Qt 接口层封装 common/config/IniConfig，QString ↔ std::string 转换
#include "ConfigManager.h"

#include "IniConfig.h"

ConfigManager::ConfigManager()
    : m_ini(new IniConfig())
{
}

ConfigManager::~ConfigManager()
{
    delete m_ini;
}

bool ConfigManager::Load(const QString& filePath)
{
    bool ok = m_ini->load(filePath.toStdString());
    if (ok)
    {
        // 加载成功后同步 INI 内容到 Qt 成员变量
        SyncFromStorage();
    }
    return ok;
}

bool ConfigManager::Save(const QString& filePath) const
{
    // 先同步成员变量到 INI 再保存
    const_cast<ConfigManager*>(this)->SyncToStorage();
    return m_ini->save(filePath.isEmpty() ? "" : filePath.toStdString());
}

void ConfigManager::SyncToStorage()
{
    // [General] 段：最近目录（逗号分隔）、输出目录
    m_ini->setString("General", "RecentDirs", recentDirs.join(",").toStdString());
    m_ini->setString("General", "OutputDir", outputDir.toStdString());

    // [Clean] 段：gitignore 开关、清理后自动打包、自定义清理规则
    m_ini->setBool("Clean", "EnableGitIgnore", enableGitIgnore);
    m_ini->setBool("Clean", "AutoPack", autoPack);
    m_ini->setString("Clean", "CustomCleanRules", customCleanRules.join(",").toStdString());

    // [Pack] 段：包名模板、7z 路径
    m_ini->setString("Pack", "PackageNamePattern", packageNamePattern.toStdString());
    m_ini->setString("Pack", "SevenZipPath", sevenZipPath.toStdString());

    // [UI] 段：主题偏好
    m_ini->setBool("UI", "DarkTheme", darkTheme);
}

void ConfigManager::SyncFromStorage()
{
    // 从 IniConfig 读取各段配置项并转换为 QString 成员变量
    recentDirs = QString::fromStdString(
        m_ini->getString("General", "RecentDirs", "")).split(",", Qt::SkipEmptyParts);
    outputDir = QString::fromStdString(
        m_ini->getString("General", "OutputDir", ""));

    enableGitIgnore = m_ini->getBool("Clean", "EnableGitIgnore", true);
    autoPack = m_ini->getBool("Clean", "AutoPack", false);
    customCleanRules = QString::fromStdString(
        m_ini->getString("Clean", "CustomCleanRules", "")).split(",", Qt::SkipEmptyParts);

    packageNamePattern = QString::fromStdString(
        m_ini->getString("Pack", "PackageNamePattern", "%Project_%YYYY%MM%DD_%HH%MM%SS_source"));
    sevenZipPath = QString::fromStdString(
        m_ini->getString("Pack", "SevenZipPath", ""));

    darkTheme = m_ini->getBool("UI", "DarkTheme", true);
}

void ConfigManager::SetValue(const QString& key, const QVariant& value)
{
    // 通用键值写入，存放在 [Custom] 段
    m_ini->setString("Custom", key.toStdString(), value.toString().toStdString());
}

QVariant ConfigManager::GetValue(const QString& key, const QVariant& defaultValue) const
{
    std::string v = m_ini->getString("Custom", key.toStdString(),
                                     defaultValue.toString().toStdString());
    return QString::fromStdString(v);
}
