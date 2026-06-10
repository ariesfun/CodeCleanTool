#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QObject>
#include <QDateTime>
#include <QList>
#include <QString>

#include "Logger.h"

// 日志条目
struct LogEntry
{
    QDateTime time;        // 时间
    QString level;         // 级别: INFO, WARN, ERROR
    QString module;        // 模块名
    QString message;       // 日志内容
};

// 日志管理器：Qt 信号层封装 common/log/Logger
// 同时写入底层 Logger（文件/控制台/DebugOutput）并发射 Qt 信号供 UI 日志面板实时展示
// 推荐使用下方的 LOGMGR_* 宏，它们会在真实的调用位置展开 LOG_INFO，确保日志定位准确
class LogManager : public QObject
{
    Q_OBJECT

public:
    explicit LogManager(QObject* parent = nullptr);

    // 初始化底层 Logger（调用一次即可）
    void Init(const QString& logDir = "logs", const QString& logName = "code-clean-tool.log");

    // 追加日志条目到内存缓存并发射 LogAdded 信号（不写底层 Logger）
    // 通常不直接调用，使用下方的 LOGMGR_* 宏自动完成双写
    void Append(const QString& level, const QString& module, const QString& message);

    // 获取已缓存的日志条目
    QList<LogEntry> Entries() const;

    // 清空缓存 / 导出到文件
    void Clear();
    bool ExportToFile(const QString& filePath) const;

signals:
    void LogAdded(const LogEntry& entry);   // 新日志条目通知 UI

private:
    QList<LogEntry> m_entries;              // 内存日志缓存（UI 展示用）
    bool m_initialized{false};              // 底层 Logger 是否已初始化
};

// ── LogManager 便捷宏 ──────────────────────────────────────────────────
// 在调用处展开 LOG_INFO + Append，确保日志文件中的 __FILE__:__LINE__:__FUNCTION__
// 指向真实的业务代码调用点，而不是 LogManager 内部
// 用法：LOGMGR_INFO(mgr, "模块名", "消息 %s", arg);
//        LOGMGR_WARN(mgr, "模块名", "警告内容");
//        LOGMGR_ERROR(mgr, "模块名", "错误内容");
#define LOGMGR_INFO(mgr, module, fmt, ...) \
    do { \
        QString _mgrMsg = QString::asprintf(fmt, ##__VA_ARGS__); \
        LOG_INFO("[%s] %s", module, _mgrMsg.toStdString().c_str()); \
        (mgr).Append("INFO", module, _mgrMsg); \
    } while(0)

#define LOGMGR_WARN(mgr, module, fmt, ...) \
    do { \
        QString _mgrMsg = QString::asprintf(fmt, ##__VA_ARGS__); \
        LOG_WARN("[%s] %s", module, _mgrMsg.toStdString().c_str()); \
        (mgr).Append("WARN", module, _mgrMsg); \
    } while(0)

#define LOGMGR_ERROR(mgr, module, fmt, ...) \
    do { \
        QString _mgrMsg = QString::asprintf(fmt, ##__VA_ARGS__); \
        LOG_ERROR("[%s] %s", module, _mgrMsg.toStdString().c_str()); \
        (mgr).Append("ERROR", module, _mgrMsg); \
    } while(0)

#endif // LOGMANAGER_H
