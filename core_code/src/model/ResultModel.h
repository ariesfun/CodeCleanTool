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
    QString fileType;       // 文件类型分类（IDE缓存/编译产物/调试文件/临时文件/构建目录/其他）
    QString hitRule;        // 命中规则
    int sortPriority{0};    // 排序优先级（越小越靠前，清理目标优先）
    bool checked{true};     // 勾选状态
    bool isDir{false};      // 是否为目录（视图据此绘制文件夹/文件图标）
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
        ColType,        // 文件类型分类（带颜色标签）
        ColRule,        // 命中规则名
        ColCount        // 列数占位
    };

    enum
    {
        SortPriorityRole = Qt::UserRole + 1,  // 排序优先级（int），清理目标优先
        // IsDirRole：该项是否为目录（bool）。视图据此在名称列绘制文件夹/文件图标。
        // 之所以由视图取该角色自行绘制，而非模型直接返回 QIcon：
        // 生成系统标准图标要用 QStyle（Qt Widgets），而本模型只依赖 Qt Core/Gui，
        // 保持模型层不引入 Widgets 依赖。
        IsDirRole = Qt::UserRole + 2,
        // SortValueRole：按列返回可直接比较的原始值（大小给字节数、时间给 QDateTime，
        // 其余列回落到显示文本）。排序代理在同一类别内比较时必须用它，不能用
        // DisplayRole —— 大小列显示的是 "1.5 MB" 这类字符串，字典序与数值序不一致
        // （"999 B" 会排在 "1.0 KB" 之后）。
        SortValueRole = Qt::UserRole + 3
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
    // RemoveFile: 移除指定行，触发 beginRemoveRows/endRemoveRows，越界无操作
    void RemoveFile(int row);

private:
    QList<FileItem> m_files;   // 文件列表
};

#endif // RESULTMODEL_H
