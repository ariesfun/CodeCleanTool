#ifndef FILECLEANER_H
#define FILECLEANER_H

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QAtomicInt>

// 清理工作线程：在后台执行文件/目录批量删除
// 调用链：FileCleaner::StartClean() 创建 → moveToThread → QThread::started 触发 DoClean
// 线程安全：m_cancelled 为 QAtomicInt，主线程写入、工作线程读取
// 生命周期：随 FileCleaner::CleanupThread() 通过 deleteLater 释放
class CleanWorker : public QObject
{
    Q_OBJECT

public:
    // filePaths: 待删除的文件/目录绝对路径列表
    explicit CleanWorker(const QStringList& filePaths, QObject* parent = nullptr);

public slots:
    // DoClean: 遍历目标列表逐项删除，自动处理只读文件，中途可通过 m_cancelled 中断
    void DoClean();

signals:
    void CleanProgress(int current, int total);        // 清理进度（当前/总数）
    void CleanFinished(int deletedCount, int failedCount); // 清理完成（成功数/失败数）
    void CleanError(const QString& path, const QString& errorMsg); // 单文件失败

private:
    // DeleteFile: 删除单文件，只读文件先取消只读属性
    bool DeleteFile(const QString& path);
    // DeleteDir: 递归删除目录，先收集所有子项再倒序删除（子目录先于父目录）
    bool DeleteDir(const QString& path);

    QStringList m_targets;      // 待删除的文件/目录路径
    QAtomicInt m_cancelled;     // 取消标志
    int m_deletedCount{0};      // 成功删除数
    int m_failedCount{0};       // 失败数

    friend class FileCleaner;
};

// 文件清理器：管理 CleanWorker 线程生命周期，对外提供异步删除接口
// 调用链：MainWindow::OnClean() → StartClean() → 创建 QThread + CleanWorker
class FileCleaner : public QObject
{
    Q_OBJECT

public:
    explicit FileCleaner(QObject* parent = nullptr);
    ~FileCleaner() override;

    // SetTargetList: 设置待删除文件/目录路径列表
    void SetTargetList(const QStringList& filePaths);
    // StartClean: 取消进行中的清理 → 创建工作线程 → 连接信号链 → 启动，空列表直接 emit CleanFinished(0,0)
    void StartClean();
    // CancelClean: 设置取消标志 + 等待线程结束
    void CancelClean();

signals:
    void CleanProgress(int current, int total);
    void CleanFinished(int deletedCount, int failedCount);
    void CleanError(const QString& path, const QString& errorMsg);

private:
    // CleanupThread: quit + wait + deleteLater 线程清理
    void CleanupThread();

    QStringList m_targets;              // 待删除列表
    QThread* m_workerThread{nullptr};
    CleanWorker* m_worker{nullptr};
};

#endif // FILECLEANER_H
