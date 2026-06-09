#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>

// 配置管理器：Qt 接口层封装 common/config/IniConfig
// 将 std::string 接口转为 QString，同时支持 QVariant 通用读写
class ConfigManager
{
public:
    ConfigManager();
    ~ConfigManager();

    // 加载/保存配置（底层使用 IniConfig INI 格式）
    bool Load(const QString& filePath);
    bool Save(const QString& filePath = "") const;

    // 配置项
    QStringList recentDirs;          // 最近目录列表
    bool enableGitIgnore{true};      // 是否启用 .gitignore
    QStringList customCleanRules;    // 自定义清理规则
    QString outputDir;               // 打包输出目录
    QString packageNamePattern;      // 包名规则
    bool autoPack{false};            // 清理后自动打包
    bool darkTheme{true};            // 深色主题
    QString sevenZipPath;            // 7z.exe 路径（用户自定义，空则自动检测）

    // 同步：将成员变量写回 IniConfig / 从 IniConfig 读取到成员变量
    void SyncToStorage();
    void SyncFromStorage();

    // 通用键值读写
    void SetValue(const QString& key, const QVariant& value);
    QVariant GetValue(const QString& key, const QVariant& defaultValue = {}) const;

private:
    class IniConfig* m_ini;          // 底层 INI 引擎（不透明指针，避免头文件依赖）
};

#endif // CONFIGMANAGER_H
