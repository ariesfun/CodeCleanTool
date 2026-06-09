#ifndef RESULTMODEL_H
#define RESULTMODEL_H

#include <QAbstractTableModel>
#include <QDateTime>
#include <QFileInfo>
#include <QList>

// 文件项数据结构
struct FileItem
{
    QString fileName;       // 文件名
    QString filePath;       // 完整路径
    qint64 fileSize;        // 大小（字节）
    QDateTime dateModified; // 修改时间
    QString hitRule;        // 命中规则
    bool checked{true};     // 勾选状态
};

// 结果数据模型：文件列表的 Qt Model，供 QTableView 使用
// 调用链：ScanManager 扫描完成 → AddFile/AddFiles 填充 → QTableView 通过 data/headerData 渲染
// 线程约束：仅主线程操作（QAbstractTableModel 非线程安全）
// 关键约束：修改数据须通过 beginInsertRows/endInsertRows 通知 View
class ResultModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        ColName = 0,   // 文件名（首列支持复选框）
        ColPath,        // 完整路径
        ColSize,        // 文件大小（字节）
        ColDate,        // 修改时间（yyyy-MM-dd hh:mm:ss）
        ColRule,        // 命中规则名
        ColCount        // 列数占位
    };

    explicit ResultModel(QObject* parent = nullptr);

    // QAbstractTableModel 接口 — 提供 QTableView 所需数据
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    // setData: 仅处理 ColName 的 CheckStateRole，切换复选框状态
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    // AddFile: 追加单条文件记录，触发 beginInsertRows/endInsertRows
    void AddFile(const FileItem& item);
    // AddFiles: 批量追加，空列表直接返回
    void AddFiles(const QList<FileItem>& items);
    // Clear: 清空所有行，触发 beginResetModel/endResetModel
    void Clear();
    // GetFile: 获取指定行数据，越界返回空 FileItem
    FileItem GetFile(int row) const;
    // TotalCount: 返回文件总数
    int TotalCount() const;
    // TotalSize: 遍历累加所有文件大小（字节）
    qint64 TotalSize() const;
    // CheckedCount: 返回已勾选的文件数
    int CheckedCount() const;

private:
    QList<FileItem> m_files;   // 文件列表
};

#endif // RESULTMODEL_H
