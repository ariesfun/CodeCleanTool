#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QMap>

// 配置管理器：Qt 接口层封装 common/config/IniConfig
// 配置项以类型化成员变量的形式对外暴露（见下方「配置项」），
// 读写在 Load/Save 里经 SyncFromStorage/SyncToStorage 两个方向同步到 INI
class ConfigManager
{
public:
    ConfigManager();
    ~ConfigManager();

    // 加载/保存配置（底层使用 IniConfig INI 格式）
    bool Load(const QString& filePath);
    bool Save(const QString& filePath = "") const;

    // DefaultConfigPath: 默认配置文件路径 —— 可执行文件同级目录下的 config.ini
    // 便携式分发（SFX 自解压）下配置随程序目录走，不依赖启动时的工作目录
    static QString DefaultConfigPath();

    // 配置项
    QStringList recentDirs;          // 最近目录列表
    bool excludeVcsDirs{true};       // 扫描和打包时是否排除 .git/.svn 等版本控制目录
    QStringList customCleanRules;    // 自定义清理规则
    QString outputDir;               // 打包输出目录
    QString packageNamePattern;      // 包名规则
    bool autoPack{false};            // 清理后自动打包
    bool darkTheme{true};            // 深色主题
    QString sevenZipPath;            // 7z.exe 路径（用户自定义，空则自动检测）

    // 同步：将成员变量写回 IniConfig / 从 IniConfig 读取到成员变量
    void SyncToStorage();
    void SyncFromStorage();

private:
    class IniConfig* m_ini;          // 底层 INI 引擎（不透明指针，避免头文件依赖）
};

#endif // CONFIGMANAGER_H
