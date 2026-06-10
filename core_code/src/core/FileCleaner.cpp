// 文件清理器实现：异步文件/目录批量删除，支持只读文件覆盖
#include "FileCleaner.h"

#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include "Logger.h"

// ======== CleanWorker 实现 ========

CleanWorker::CleanWorker(const QStringList& filePaths, QObject* parent)
    : QObject(parent)
    , m_targets(filePaths)
    , m_cancelled(0)
{
}

bool CleanWorker::DeleteFile(const QString& path)
{
    QFile file(path);

    // 只读文件先取消只读
    QFileInfo info(path);
    if (!info.isWritable())
    {
        file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }

    if (!file.remove())
    {
        emit CleanError(path, file.errorString());
        return false;
    }
    return true;
}

bool CleanWorker::DeleteDir(const QString& path)
{
    QDir dir(path);
    if (!dir.exists())
    {
        return true;
    }

    // 递归删除所有子项
    QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                     QDirIterator::Subdirectories);

    // 先收集所有条目（倒序删除：先文件后目录）
    QStringList entries;
    while (it.hasNext())
    {
        entries.append(it.next());
    }

    // 倒序删除（子目录先于父目录）
    for (int i = entries.size() - 1; i >= 0; --i)
    {
        QFileInfo info(entries[i]);
        if (info.isDir())
        {
            QDir(info.absoluteFilePath()).rmdir(info.absoluteFilePath());
        }
        else
        {
            QFile::remove(info.absoluteFilePath());
        }
    }

    // 删除目录本身
    return dir.rmdir(path);
}

void CleanWorker::DoClean()
{
    int total = m_targets.size();
    LOG_INFO("[CleanWorker] 开始清理, 共 %d 个目标", total);

    for (int i = 0; i < m_targets.size(); ++i)
    {
        if (m_cancelled.load())
        {
            break;
        }

        const QString& path = m_targets[i];
        QFileInfo info(path);
        bool ok = false;

        if (info.isDir())
        {
            ok = DeleteDir(path);
        }
        else
        {
            ok = DeleteFile(path);
        }

        if (ok)
        {
            ++m_deletedCount;
        }
        else
        {
            ++m_failedCount;
            LOG_ERROR("[CleanWorker] 删除失败: %s", path.toStdString().c_str());
            emit CleanError(path, "删除失败，可能被占用或无权限");
        }

        emit CleanProgress(i + 1, total);
    }

    emit CleanFinished(m_deletedCount, m_failedCount);
    LOG_INFO("[CleanWorker] 清理完成, 成功: %d, 失败: %d", m_deletedCount, m_failedCount);
}

// ======== FileCleaner 实现 ========

FileCleaner::FileCleaner(QObject* parent)
    : QObject(parent)
{
}

FileCleaner::~FileCleaner()
{
    CancelClean();
}

void FileCleaner::SetTargetList(const QStringList& filePaths)
{
    m_targets = filePaths;
}

void FileCleaner::StartClean()
{
    if (m_targets.isEmpty())
    {
        LOG_INFO("[FileCleaner] 目标列表为空，跳过清理");
        emit CleanFinished(0, 0);
        return;
    }

    CancelClean();

    LOG_INFO("[FileCleaner] 创建清理工作线程, 目标数: %d", m_targets.size());
    m_workerThread = new QThread(this);
    m_worker = new CleanWorker(m_targets);
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &CleanWorker::DoClean);
    connect(m_worker, &CleanWorker::CleanProgress, this, &FileCleaner::CleanProgress);
    connect(m_worker, &CleanWorker::CleanError, this, &FileCleaner::CleanError);
    connect(m_worker, &CleanWorker::CleanFinished, this, [this](int ok, int fail)
    {
        LOG_INFO("[FileCleaner] 清理线程完成, 成功: %d, 失败: %d", ok, fail);
        emit CleanFinished(ok, fail);
        CleanupThread();
    });

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_workerThread->start();
}

void FileCleaner::CancelClean()
{
    if (m_worker)
    {
        LOG_INFO("[FileCleaner] 取消清理");
        m_worker->m_cancelled.store(1);
    }
    CleanupThread();
}

void FileCleaner::CleanupThread()
{
    if (m_workerThread)
    {
        LOG_INFO("[FileCleaner] 清理工作线程");
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
        m_worker = nullptr;
    }
}
