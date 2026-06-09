#ifndef LOGLISTMODEL_H
#define LOGLISTMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "LogManager.h"

// 日志列表模型：封装 LogManager 日志缓存，供 QTableView 渲染
// 调用链：LogManager::LogAdded → OnLogAdded 槽 → beginInsertRows → View 刷新
// 支持级别过滤（INFO/WARN/ERROR），重置过滤时重建全部行
class LogListModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        ColTime = 0,   // 时间
        ColLevel,       // 级别
        ColModule,      // 模块
        ColMessage,     // 内容
        ColCount
    };

    explicit LogListModel(QObject* parent = nullptr);

    // SetLogManager: 绑定 LogManager，获取已有条目 + 连接 LogAdded 信号
    void SetLogManager(LogManager* mgr);

    // QAbstractTableModel 接口
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // SetLevelFilter: 按级别过滤，"ALL" 显示全部，"INFO"/"WARN"/"ERROR" 仅显示对应级别
    void SetLevelFilter(const QString& level);

public slots:
    // OnLogAdded: LogManager::LogAdded 槽，按当前过滤条件决定是否追加到视图
    void OnLogAdded(const LogEntry& entry);

private:
    // RebuildFiltered: 清空并重建过滤列表
    void RebuildFiltered();

    LogManager* m_logMgr{nullptr};      // 日志管理器（非拥有）
    QString m_filterLevel{"ALL"};       // 当前过滤级别
    QList<int> m_filteredIndices;       // 过滤后的条目索引（指向 m_logMgr->Entries()）
};

#endif // LOGLISTMODEL_H
