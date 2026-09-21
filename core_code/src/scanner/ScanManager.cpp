// 扫描管理器实现：异步目录遍历 + 规则过滤 + 结果填充
#include "ScanManager.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QDir>

#include "rules/RuleEngine.h"
#include "model/ResultModel.h"
#include "Logger.h"

// ======== ScanWorker 实现 ========

ScanWorker::ScanWorker(const QString& rootPath,
                       const QStringList& extensions,
                       RuleEngine* ruleEngine,
                       bool excludeVcsDirs,
                       QObject* parent)
    : QObject(parent)
    , m_rootPath(rootPath)
    , m_extensions(extensions)
    , m_ruleEngine(ruleEngine)
    , m_excludeVcsDirs(excludeVcsDirs)
    , m_cancelled(0)
{
}

void ScanWorker::DoScan()
{
    LOG_INFO("[ScanWorker] 开始扫描: %s", m_rootPath.toStdString().c_str());
    QDir rootDir(m_rootPath);
    if (!rootDir.exists())
    {
        LOG_ERROR("[ScanWorker] 目录不存在: %s", m_rootPath.toStdString().c_str());
        emit ScanError("目录不存在: " + m_rootPath);
        return;
    }

    // 第一遍：计数（用于进度估算）
    int totalItems = 0;
    {
        // 必须带 QDir::Hidden：Visual Studio 生成的 .vs 目录带隐藏属性，
        // 不带该标志会被整个跳过，导致 IDE 缓存永远扫不到
        QDirIterator countIt(m_rootPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                              QDirIterator::Subdirectories);
        while (countIt.hasNext())
        {
            countIt.next();
            if (m_cancelled.load())
            {
                LOG_INFO("[ScanWorker] 扫描被取消(计数阶段)");
                emit ScanFinished(0, 0);
                return;
            }
            // 跳过 VCS 目录内容，使进度估算更准确（由用户设置控制）
            if (m_excludeVcsDirs)
            {
                QFileInfo fi = countIt.fileInfo();
                QString absPath = fi.absoluteFilePath();
                absPath.replace('\\', '/');
                if ((fi.isDir() && (fi.fileName() == ".git" || fi.fileName() == ".svn")) ||
                    absPath.contains("/.git/") || absPath.endsWith("/.git") || absPath.contains("/.svn/") || absPath.endsWith("/.svn"))
                {
                    continue;
                }
            }
            ++totalItems;
        }
    }

    // 第二遍：实际收集
    int processed = 0;
    int foundFiles = 0;
    qint64 totalProjectSize = 0;  // 项目总大小（含所有非跳过文件）
    // 与计数阶段保持一致，同样需要 QDir::Hidden
    QDirIterator it(m_rootPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                     QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        it.next();

        if (m_cancelled.load())
        {
            LOG_INFO("[ScanWorker] 扫描被取消(收集阶段), 已找到 %d 个文件", foundFiles);
            emit ScanFinished(foundFiles, totalProjectSize);
            return;
        }

        QFileInfo info = it.fileInfo();

        // 跳过 VCS 版本控制目录及其内容（.git/.svn），由用户设置控制
        if (m_excludeVcsDirs)
        {
            QString absPath = info.absoluteFilePath();
            absPath.replace('\\', '/');
            if ((info.isDir() && (info.fileName() == ".git" || info.fileName() == ".svn")) ||
                absPath.contains("/.git/") || absPath.endsWith("/.git") || absPath.contains("/.svn/") || absPath.endsWith("/.svn"))
            {
                continue;
            }
        }

        // 说明：此处原有一段「命中 .gitignore 即跳过」的逻辑，已移除。
        // 原因：.gitignore 里列的正是构建垃圾（.vs/、build/、*.obj、*.exe），
        //       也正是本工具要清理的对象；按其排除等于把待清理项藏起来，与该工具的用途相反。
        //       更严重的是解析器在 MainPresenter 中只创建一次并跨扫描复用，
        //       项目 .gitignore 被删除后旧规则仍残留在内存里，导致扫描结果与实际情况不符。
        // 现在扫描范围只由内置规则与用户自定义规则决定。

        // 扩展名过滤
        if (!m_extensions.isEmpty() && info.isFile())
        {
            bool matched = false;
            for (const auto& ext : m_extensions)
            {
                if (info.fileName().endsWith(ext, Qt::CaseInsensitive))
                {
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                ++processed;
                continue;
            }
        }

        // 累加项目总大小（所有非跳过、非排除的文件和目录）
        totalProjectSize += info.size();

        // 规则匹配
        RuleMatch match;
        if (m_ruleEngine)
        {
            if (info.isDir())
            {
                match = m_ruleEngine->MatchDir(info.absoluteFilePath());
            }
            else
            {
                match = m_ruleEngine->MatchFile(info.absoluteFilePath());
            }
        }

        // 仅输出命中清理规则的文件
        if (match.isCleanTarget)
        {
            emit FileFound(info.absoluteFilePath(), info.size(),
                           info.lastModified(), match.ruleName);
            ++foundFiles;
        }

        ++processed;

        // 每 100 项发送一次进度
        if (processed % 100 == 0 || totalItems <= 100)
        {
            int pct = totalItems > 0 ? (processed * 100 / totalItems) : 0;
            emit ProgressUpdate(pct);
        }
    }

    emit ProgressUpdate(100);
    LOG_INFO("[ScanWorker] 扫描完成, 共找到 %d 个待清理项, 项目总大小 %lld bytes", foundFiles, totalProjectSize);
    emit ScanFinished(foundFiles, totalProjectSize);
}

// ======== ScanManager 实现 ========

ScanManager::ScanManager(QObject* parent)
    : QObject(parent)
{
}

ScanManager::~ScanManager()
{
    CancelScan();
}

void ScanManager::SetRootPath(const QString& path)
{
    m_rootPath = path;
}

void ScanManager::SetExtensions(const QStringList& extensions)
{
    m_extensions = extensions;
}

void ScanManager::SetRuleEngine(RuleEngine* engine)
{
    m_ruleEngine = engine;
}

void ScanManager::SetResultModel(ResultModel* model)
{
    m_resultModel = model;
}

void ScanManager::SetExcludeVcsDirs(bool exclude)
{
    m_excludeVcsDirs = exclude;
}

void ScanManager::StartScan()
{
    if (m_rootPath.isEmpty())
    {
        LOG_ERROR("[ScanManager] 扫描目录为空");
        emit ScanError("未设置扫描目录");
        return;
    }

    CancelScan();

    // 清空模型
    if (m_resultModel)
    {
        m_resultModel->Clear();
    }

    LOG_INFO("[ScanManager] 创建扫描工作线程, 目录: %s", m_rootPath.toStdString().c_str());
    m_workerThread = new QThread(this);
    m_worker = new ScanWorker(m_rootPath, m_extensions, m_ruleEngine, m_excludeVcsDirs);
    m_worker->moveToThread(m_workerThread);

    // 连接信号 — 跨线程信号链：worker → manager → UI
    connect(m_workerThread, &QThread::started, m_worker, &ScanWorker::DoScan);
    connect(m_worker, &ScanWorker::ProgressUpdate, this, &ScanManager::ScanProgress);
    connect(m_worker, &ScanWorker::ScanFinished, this, [this](int total, qint64 totalSize)
    {
        LOG_INFO("[ScanManager] 扫描线程完成, 结果: %d 个文件, 项目总大小: %lld", total, totalSize);
        emit ScanFinished(total, totalSize);
        CleanupThread();
    });
    connect(m_worker, &ScanWorker::ScanError, this, [this](const QString& msg)
    {
        LOG_ERROR("[ScanManager] 扫描线程出错: %s", msg.toStdString().c_str());
        emit ScanError(msg);
        CleanupThread();
    });

    // 文件发现 → 添加到数据模型
    connect(m_worker, &ScanWorker::FileFound, this, [this](
        const QString& path, qint64 size, const QDateTime& modTime, const QString& rule)
    {
        if (m_resultModel)
        {
            QFileInfo info(path);
            FileItem item;
            item.fileName = info.fileName();
            item.filePath = path;
            item.fileSize = size;
            item.dateModified = modTime;
            item.fileType = RuleEngine::GetCategory(rule);
            item.hitRule = rule;
            item.sortPriority = RuleEngine::GetCategoryPriority(item.fileType);
            item.checked = true;
            item.isDir = info.isDir();      // 供视图绘制文件夹/文件图标
            m_resultModel->AddFile(item);
        }
    });

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    m_workerThread->start();
}

void ScanManager::CancelScan()
{
    if (m_worker)
    {
        LOG_INFO("[ScanManager] 取消扫描");
        m_worker->m_cancelled.store(1);
    }
    CleanupThread();
}

QString ScanManager::RootPath() const
{
    return m_rootPath;
}

void ScanManager::CleanupThread()
{
    if (m_workerThread)
    {
        LOG_INFO("[ScanManager] 清理工作线程");
        m_workerThread->quit();
        m_workerThread->wait();
        m_workerThread->deleteLater();
        m_workerThread = nullptr;
        m_worker = nullptr;
    }
}
