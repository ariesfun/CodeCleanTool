// 结果数据模型实现：QAbstractTableModel 五个列（文件名/路径/大小/时间/规则），首列支持复选框
#include "ResultModel.h"

ResultModel::ResultModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int ResultModel::rowCount(const QModelIndex& parent) const
{
    // 顶层节点的行数 = 文件列表大小，子节点始终为 0
    if (parent.isValid())
    {
        return 0;
    }
    return m_files.size();
}

int ResultModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }
    return ColCount;
}

QVariant ResultModel::data(const QModelIndex& index, int role) const
{
    // 边界检查和越界保护
    if (!index.isValid() || index.row() >= m_files.size())
    {
        return {};
    }

    const auto& file = m_files.at(index.row());

    if (role == Qt::DisplayRole)
    {
        // 按列返回对应字段，大小和时间为特殊格式化
        switch (index.column())
        {
        case ColName: return file.fileName;
        case ColPath: return file.filePath;
        case ColSize:
        {
            // 格式化带单位：B / KB / MB / GB
            qint64 bytes = file.fileSize;
            if (bytes < 1024) { return QString::number(bytes) + " B"; }
            if (bytes < 1024 * 1024) { return QString::number(bytes / 1024.0, 'f', 1) + " KB"; }
            if (bytes < 1024LL * 1024 * 1024) { return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB"; }
            return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
        }
        case ColDate: return file.dateModified.toString("yyyy-MM-dd hh:mm:ss");
        case ColRule: return file.hitRule;
        }
    }
    else if (role == Qt::TextAlignmentRole && index.column() == ColSize)
    {
        // 大小列右对齐
        return int(Qt::AlignRight | Qt::AlignVCenter);
    }
    else if (role == Qt::CheckStateRole && index.column() == ColName)
    {
        // 首列（文件名）显示复选框，状态由 FileItem::checked 控制
        return file.checked ? Qt::Checked : Qt::Unchecked;
    }

    return {};
}

QVariant ResultModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    // 仅处理水平表头 + 显示角色
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
    {
        return {};
    }

    // 5 列表头为中文
    switch (section)
    {
    case ColName: return "文件名";
    case ColPath: return "路径";
    case ColSize: return "大小";
    case ColDate: return "修改时间";
    case ColRule: return "命中规则";
    }
    return {};
}

Qt::ItemFlags ResultModel::flags(const QModelIndex& index) const
{
    auto flags = QAbstractTableModel::flags(index);
    // 首列附加可勾选标志
    if (index.column() == ColName)
    {
        flags |= Qt::ItemIsUserCheckable;
    }
    return flags;
}

bool ResultModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    // 仅处理首列的复选框状态变更
    if (!index.isValid() || index.row() >= m_files.size())
    {
        return false;
    }

    if (role == Qt::CheckStateRole && index.column() == ColName)
    {
        m_files[index.row()].checked = (value.toInt() == Qt::Checked);
        emit dataChanged(index, index, {role});
        return true;
    }

    return false;
}

void ResultModel::AddFile(const FileItem& item)
{
    // 通知 View 即将在末尾插入一行
    beginInsertRows(QModelIndex(), m_files.size(), m_files.size());
    m_files.append(item);
    endInsertRows();
}

void ResultModel::AddFiles(const QList<FileItem>& items)
{
    if (items.isEmpty())
    {
        return;
    }
    // 通知 View 即将在末尾批量插入多行
    beginInsertRows(QModelIndex(), m_files.size(), m_files.size() + items.size() - 1);
    m_files.append(items);
    endInsertRows();
}

void ResultModel::Clear()
{
    // beginResetModel/endResetModel 通知 View 重建全部行
    beginResetModel();
    m_files.clear();
    endResetModel();
}

FileItem ResultModel::GetFile(int row) const
{
    if (row >= 0 && row < m_files.size())
    {
        return m_files.at(row);
    }
    // 越界返回空 FileItem
    return {};
}

int ResultModel::TotalCount() const
{
    return m_files.size();
}

qint64 ResultModel::TotalSize() const
{
    qint64 total = 0;
    for (const auto& f : m_files)
    {
        total += f.fileSize;
    }
    return total;
}

int ResultModel::CheckedCount() const
{
    int count = 0;
    for (const auto& f : m_files)
    {
        if (f.checked)
        {
            ++count;
        }
    }
    return count;
}

void ResultModel::RemoveFile(int row)
{
    if (row < 0 || row >= m_files.size())
    {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_files.removeAt(row);
    endRemoveRows();
}
