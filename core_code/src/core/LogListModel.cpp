// 日志列表模型实现：LogManager 缓存 → QAbstractTableModel 渲染
// 架构角色：适配器层，将 LogManager 的线性日志缓存转为 Qt Model/View 可消费的表格数据
// 过滤策略：按级别增量过滤，OnLogAdded 实时判断是否追加，避免全量重建开销
#include "LogListModel.h"
#include <QColor>

// ========== 构造与析构 ==========

LogListModel::LogListModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

// ========== 数据绑定 ==========

void LogListModel::SetLogManager(LogManager* mgr)
{
    // 绑定外部 LogManager（非拥有），m_logMgr 为 nullptr 时清空视图
    m_logMgr = mgr;
    if (!m_logMgr)
    {
        return;
    }

    // 先加载 LogManager 中已有条目到过滤列表
    RebuildFiltered();

    // 订阅 LogAdded 信号，后续新日志实时追加，无需全量刷新
    connect(m_logMgr, &LogManager::LogAdded, this, &LogListModel::OnLogAdded);
}

// ========== 过滤控制 ==========

void LogListModel::SetLevelFilter(const QString& level)
{
    // 切换过滤级别时全量重建，因为需要重新判断每个条目的可见性
    m_filterLevel = level;
    RebuildFiltered();
}

// ========== 实时日志追加 ==========

void LogListModel::OnLogAdded(const LogEntry& entry)
{
    if (!m_logMgr)
    {
        return;
    }

    // 增量过滤：不符合当前级别条件的条目不追加到视图
    if (m_filterLevel != "ALL" && entry.level != m_filterLevel)
    {
        return;
    }

    // 追加到视图末尾，存储指向 LogManager 原始列表的索引
    // 不存储 LogEntry 副本，避免内存膨胀
    int row = m_filteredIndices.size();
    int totalEntries = m_logMgr->Entries().size();
    beginInsertRows(QModelIndex(), row, row);
    m_filteredIndices.append(totalEntries - 1);
    endInsertRows();
}

// ========== 过滤列表重建 ==========

void LogListModel::RebuildFiltered()
{
    // beginResetModel/endResetModel 通知 View 即将全量替换数据，避免逐行通知的性能开销
    beginResetModel();
    m_filteredIndices.clear();

    if (!m_logMgr)
    {
        endResetModel();
        return;
    }

    // 遍历全部日志条目，按当前过滤级别筛选
    const auto& entries = m_logMgr->Entries();
    for (int i = 0; i < entries.size(); ++i)
    {
        if (m_filterLevel == "ALL" || entries[i].level == m_filterLevel)
        {
            m_filteredIndices.append(i);
        }
    }
    endResetModel();
}

// ========== QAbstractTableModel 接口 ==========

int LogListModel::rowCount(const QModelIndex& parent) const
{
    // Qt 树模型约定：parent 有效表示子节点查询，本模型为扁平列表无子节点
    if (parent.isValid())
    {
        return 0;
    }
    // 返回过滤后可见条目数，非 LogManager 全部条目数
    return m_filteredIndices.size();
}

int LogListModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return ColCount;
}

QVariant LogListModel::data(const QModelIndex& index, int role) const
{
    // 越界保护：QModelIndex 无效或 LogManager 未绑定时返回空
    if (!index.isValid() || !m_logMgr)
    {
        return {};
    }

    // 通过过滤索引映射到 LogManager 原始条目
    int realIdx = m_filteredIndices.at(index.row());
    const auto& entry = m_logMgr->Entries().at(realIdx);

    // 显示角色：按列返回对应字段
    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case ColTime: return entry.time.toString("hh:mm:ss");
        case ColLevel: return entry.level;
        case ColModule: return entry.module;
        case ColMessage: return entry.message;
        }
    }
    // 前景色角色：仅级别列着色，ERROR 红色 WARN 橙色，INFO 使用默认颜色
    else if (role == Qt::ForegroundRole && index.column() == ColLevel)
    {
        if (entry.level == "ERROR") { return QColor(220, 50, 50); }
        if (entry.level == "WARN") { return QColor(200, 140, 0); }
    }

    return {};
}

QVariant LogListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // 仅处理水平表头 + 显示角色，垂直表头不显示
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }
    switch (section)
    {
    case ColTime: return "时间";
    case ColLevel: return "级别";
    case ColModule: return "模块";
    case ColMessage: return "内容";
    }
    return {};
}
