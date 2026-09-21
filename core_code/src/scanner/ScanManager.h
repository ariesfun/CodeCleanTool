#ifndef SCANMANAGER_H
#define SCANMANAGER_H

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QAtomicInt>

class RuleEngine;
class ResultModel;
struct FileItem;

// 扫描工作线程：在后台执行目录遍历，不阻塞 UI
// 调用链：ScanManager::StartScan() 创建 → moveToThread → QThread::started 触发 DoScan
// 线程安全：m_cancelled 为 QAtomicInt，主线程写入、工作线程读取
// 生命周期：随 ScanManager::CleanupThread() 通过 deleteLater 释放
class ScanWorker : public QObject
{
    Q_OBJECT

public:
    // rootPath: 扫描根目录（绝对路径）
    // ruleEngine: 规则引擎（非拥有，生命周期由调用方管理）
    // excludeVcsDirs: 是否跳过 .git/.svn 目录（由用户设置控制）
    explicit ScanWorker(const QString& rootPath,
                        RuleEngine* ruleEngine,
                        bool excludeVcsDirs,
                        QObject* parent = nullptr);

public slots:
    // DoScan: 两遍扫描（计数 → 收集），每 100 项发射进度，命中清理规则的文件通过 FileFound 信号发射
    void DoScan();

signals:
    void ProgressUpdate(int percent);               // 进度 0-100
    void FileFound(const QString& filePath, qint64 size,
                   const QDateTime& modTime, const QString& rule); // 找到文件
    void ScanFinished(int totalFiles, qint64 totalProjectSize); // 扫描完成（含项目总大小）
    void ScanError(const QString& errorMsg);         // 扫描出错

private:
    QString m_rootPath;             // 根目录
    RuleEngine* m_ruleEngine;       // 规则引擎（非拥有）
    bool m_excludeVcsDirs{true};    // 跳过 .git/.svn 版本控制目录
    QAtomicInt m_cancelled;         // 取消标志（原子操作）

    friend class ScanManager;
};

// 扫描管理器：管理 ScanWorker 线程生命周期，对外提供简洁接口
// 调用链：MainWindow::OnScan() → StartScan() → 创建 QThread + ScanWorker
// 线程安全：Signal-slot 跨线程通信，CancelScan 通过 QAtomicInt 通知取消
class ScanManager : public QObject
{
    Q_OBJECT

public:
    explicit ScanManager(QObject* parent = nullptr);
    ~ScanManager() override;

    // SetRootPath: 设置扫描根目录，调用方确保路径有效
    void SetRootPath(const QString& path);
    // SetRuleEngine: 注入规则引擎实例（非拥有，生命周期由调用方管理）
    void SetRuleEngine(RuleEngine* engine);
    // SetResultModel: 注入结果数据模型，扫描结果将填充到此模型中
    void SetResultModel(ResultModel* model);
    // SetExcludeVcsDirs: 设置是否跳过 .git/.svn 版本控制目录（默认开启）
    void SetExcludeVcsDirs(bool exclude);

    // StartScan: 取消进行中的扫描 → 清空模型 → 创建工作线程 → 连接信号链 → 启动
    // 前置条件：需先调用 SetRootPath，否则 emit ScanError
    void StartScan();
    // CancelScan: 设置取消标志 + 等待线程结束，无副作用可多次调用
    void CancelScan();

signals:
    void ScanProgress(int percent);          // 扫描进度 0-100
    void ScanFinished(int totalFiles, qint64 totalProjectSize);       // 扫描完成（含项目总大小）
    void ScanError(const QString& errorMsg); // 扫描出错

private:
    // CleanupThread: quit + wait + deleteLater 线程清理，将 m_worker/m_workerThread 置空
    void CleanupThread();

    QString m_rootPath;             // 根目录
    RuleEngine* m_ruleEngine{nullptr};
    ResultModel* m_resultModel{nullptr};
    bool m_excludeVcsDirs{true};     // 跳过 .git/.svn 版本控制目录
    QThread* m_workerThread{nullptr};
    ScanWorker* m_worker{nullptr};
};

#endif // SCANMANAGER_H
