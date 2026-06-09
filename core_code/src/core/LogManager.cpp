// 日志管理器实现：Logger 初始化 + 内存日志缓存 + UI 信号发射
// 业务层应使用 LOGMGR_* 宏（双写 Logger + 信号），或直接用 LOG_* 宏（仅写 Logger）
#include "LogManager.h"

#include <QFile>
#include <QTextStream>
#include <QDir>

LogManager::LogManager(QObject* parent)
    : QObject(parent)
{
}

void LogManager::Init(const QString& logDir, const QString& logName)
{
    // 防止重复初始化
    if (m_initialized)
    {
        return;
    }

    // 确保日志目录存在
    QDir dir;
    dir.mkpath(logDir);

    QString fullPath = logDir + "/" + logName;
    // 底层 Logger 为单例，DEBUG 级别 + 控制台输出
    Logger::instance().init(fullPath.toStdString(), LogLevel::DEBUG, true);
    m_initialized = true;

    // 直接用 LOG_INFO，避免走 LogManager wrapper 丢失真实调用位置
    LOG_INFO("[LogManager] 日志系统初始化完成");
    // 同时追加到 UI 缓存
    Append("INFO", "LogManager", "日志系统初始化完成");
}

void LogManager::Append(const QString& level, const QString& module, const QString& message)
{
    // 构造日志条目 → 追加到内存缓存 → 发射信号通知 UI
    LogEntry entry;
    entry.time = QDateTime::currentDateTime();
    entry.level = level;
    entry.module = module;
    entry.message = message;
    m_entries.append(entry);
    emit LogAdded(entry);
}

QList<LogEntry> LogManager::Entries() const
{
    return m_entries;
}

void LogManager::Clear()
{
    m_entries.clear();
}

bool LogManager::ExportToFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return false;
    }

    QTextStream stream(&file);
    // 格式：时间 [日志级别] [模块名] 日志内容
    for (const auto& entry : m_entries)
    {
        stream << entry.time.toString("yyyy-MM-dd hh:mm:ss") << " "
               << "[" << entry.level << "] "
               << "[" << entry.module << "] "
               << entry.message << "\n";
    }

    return true;
}
