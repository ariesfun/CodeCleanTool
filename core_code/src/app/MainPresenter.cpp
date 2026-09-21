// 控制层实现：Service 生命周期管理 + 信号链编排 + 按钮事件处理
// 架构角色：View 与 Service 之间的协调层，MainWindow 通过此层访问所有业务能力
// 线程约束：全部在 UI 主线程执行，Service 内部自行创建工作线程，通过信号回传结果
// 信号方向：View 按钮 → Presenter 槽 → Service 方法 → Service 信号 → Presenter lambda → UI 信号 → View 控件
#include "MainPresenter.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QWidget>

#include "ElaMessageBar.h"

#include "config/ConfigManager.h"
#include "cleaner/FileCleaner.h"
#include "log/LogManager.h"
#include "packager/Packager.h"
#include "model/ResultModel.h"
#include "rules/RuleEngine.h"
#include "scanner/ScanManager.h"
#include "Logger.h"

// ========== 构造与析构 ==========

MainPresenter::MainPresenter(LogManager* logMgr, QObject* parent)
    : QObject(parent)
    , m_logMgr(logMgr)
    , m_parentWidget(qobject_cast<QWidget*>(parent))
{
}

MainPresenter::~MainPresenter()
{
    LOGMGR_INFO((*m_logMgr), "MainPresenter", "销毁");
}

// ========== 初始化：创建 Service 实例 + 连接信号链 ==========

void MainPresenter::Init()
{
    LOGMGR_INFO((*m_logMgr), "MainPresenter", "初始化开始");

    // --- Service 实例创建 ---

    m_configManager = new ConfigManager();          // 配置读写（INI 格式）
    // 从 exe 同级目录的 config.ini 加载用户配置；文件不存在时保持默认值
    {
        const QString configPath = ConfigManager::DefaultConfigPath();
        const bool cfgLoaded = m_configManager->Load(configPath);
        LOGMGR_INFO((*m_logMgr), "MainPresenter", "ConfigManager 已创建, 配置加载%s: %s",
                    cfgLoaded ? "成功" : "失败(沿用默认值)", configPath.toStdString().c_str());
    }

    m_ruleEngine = new RuleEngine();                // 规则匹配引擎
    m_ruleEngine->LoadBuiltinRules();               // 加载 36 条清理规则 + 22 条保留规则（幂等，重复调用不累积）
    LOGMGR_INFO((*m_logMgr), "MainPresenter", "RuleEngine 已创建, 内置规则已加载");

    m_resultModel = new ResultModel(this);          // 文件扫描结果模型（QAbstractTableModel）

    m_scanManager = new ScanManager(this);          // 异步目录扫描器（QThread 内部线程）
    m_scanManager->SetRuleEngine(m_ruleEngine);
    m_scanManager->SetResultModel(m_resultModel);
    m_scanManager->SetExcludeVcsDirs(m_configManager->excludeVcsDirs);

    m_fileCleaner = new FileCleaner(this);          // 异步文件清理器（QThread 内部线程）
    LOGMGR_INFO((*m_logMgr), "MainPresenter", "FileCleaner 已创建");

    m_packager = new Packager(this);                // 异步 7z 打包器（QProcess）
    LOGMGR_INFO((*m_logMgr), "MainPresenter", "Packager 已创建");
    m_packager->SetExcludeVcsDirs(m_configManager->excludeVcsDirs);
    // 从配置加载用户自定义的 7z 路径（若已配置）
    if (!m_configManager->sevenZipPath.isEmpty())
    {
        m_packager->Set7zPath(m_configManager->sevenZipPath);
    }

    // --- 信号链：Service 信号 → Presenter 处理 → UI 信号发射 ---
    // 每个 connect 将底层 Service 的进度/完成/错误信号转换为统一的 UI 更新信号
    // UI 信号（ProgressChanged/StatusChanged/StatsChanged）由 MainWindow 接收更新控件

    // 扫描进度 → 进度条更新
    connect(m_scanManager, &ScanManager::ScanProgress, this, [this](int percent)
    {
        emit ProgressChanged(percent, true);
    });

    // 扫描完成 → 隐藏进度条 + 更新统计 + ElaMessageBar通知 + 发射统计图表数据
    connect(m_scanManager, &ScanManager::ScanFinished, this, [this](int total, qint64 totalProjectSize)
    {
        emit ProgressChanged(100, false);
        emit StatusChanged("就绪");
        qint64 cleanableSize = m_resultModel->TotalSize();
        m_lastTotalProjectSize = totalProjectSize;  // 存储供清理后更新使用
        emit StatsChanged(QString("共 %1 项 | 待清理 %1 项 | 可释放 %2")
            .arg(total).arg(FormatFileSize(cleanableSize)));
        emit StatsDataChanged(totalProjectSize, cleanableSize);

        LOGMGR_INFO((*m_logMgr), "MainPresenter", "扫描完成: %d 个待清理项, 项目总大小 %s, 可释放 %s",
                    total,
                    FormatFileSize(totalProjectSize).toStdString().c_str(),
                    FormatFileSize(cleanableSize).toStdString().c_str());

        ElaMessageBar::success(ElaMessageBarType::Top, "扫描完成",
            QString("共找到 %1 个待清理项，可释放 %2")
                .arg(total).arg(FormatFileSize(cleanableSize)), 3000, m_parentWidget);
    });

    // 扫描出错 → 隐藏进度条 + 显示错误
    connect(m_scanManager, &ScanManager::ScanError, this, [this](const QString& msg)
    {
        LOGMGR_ERROR((*m_logMgr), "MainPresenter", "扫描失败: %s", msg.toStdString().c_str());
        emit ProgressChanged(0, false);
        emit StatusChanged("扫描失败: " + msg);
    });

    // 清理进度 → 百分比进度条更新
    connect(m_fileCleaner, &FileCleaner::CleanProgress, this, [this](int current, int total)
    {
        int pct = total > 0 ? current * 100 / total : 0;
        emit ProgressChanged(pct, true);
    });

    // 清理完成 → 从模型移除已删行（倒序遍历避免索引偏移）+ 刷新统计 + 弹窗通知
    connect(m_fileCleaner, &FileCleaner::CleanFinished, this, [this](int ok, int fail)
    {
        emit ProgressChanged(100, false);
        emit StatusChanged(QString("清理完成：成功 %1，失败 %2").arg(ok).arg(fail));

        // 倒序遍历：removeAt 后后续索引前移，倒序可保证未处理项索引不受影响
        for (int i = m_resultModel->TotalCount() - 1; i >= 0; --i)
        {
            if (m_resultModel->GetFile(i).checked)
            {
                m_resultModel->RemoveFile(i);
            }
        }

        // 清理后重新统计剩余项
        int remain = m_resultModel->TotalCount();
        qint64 remainSize = m_resultModel->TotalSize();
        m_lastTotalProjectSize = qMax(0LL, m_lastTotalProjectSize - remainSize);
        emit StatsChanged(QString("共 %1 项 | 待清理 %1 项 | 可释放 %2")
            .arg(remain).arg(FormatFileSize(remainSize)));
        emit StatsDataChanged(m_lastTotalProjectSize, remainSize);

        LOGMGR_INFO((*m_logMgr), "MainPresenter", "清理完成: 成功 %d, 失败 %d, 剩余 %d 项",
                    ok, fail, m_resultModel->TotalCount());

        ElaMessageBar::success(ElaMessageBarType::Top, "清理完成",
            QString("成功 %1 项，失败 %2 项").arg(ok).arg(fail), 3000, m_parentWidget);

        // 若用户启用了"清理完成后自动打包"，清理成功后自动触发打包
        if (m_configManager->autoPack && !m_lastSourceDir.isEmpty())
        {
            LOGMGR_INFO((*m_logMgr), "MainPresenter", "自动打包触发, 目录: %s", m_lastSourceDir.toStdString().c_str());
            OnPack(m_lastSourceDir);
        }
    });

    // 清理出错 → 显示错误信息
    connect(m_fileCleaner, &FileCleaner::CleanError, this, [this](const QString& path, const QString& msg)
    {
        LOGMGR_ERROR((*m_logMgr), "MainPresenter", "清理失败: 路径=%s, 原因=%s",
                     path.toStdString().c_str(), msg.toStdString().c_str());
        emit StatusChanged("清理失败: " + msg);
    });

    // 打包进度 → 进度条更新
    connect(m_packager, &Packager::PackProgress, this, [this](int percent)
    {
        emit ProgressChanged(percent, true);
    });

    // 打包完成 → 弹窗通知 + 自动打开输出目录
    connect(m_packager, &Packager::PackFinished, this, [this](const QString& path, qint64 size)
    {
        emit ProgressChanged(100, false);
        emit StatusChanged(QString("打包完成: %1 (%2)").arg(path).arg(FormatFileSize(size)));

        LOGMGR_INFO((*m_logMgr), "MainPresenter", "打包完成: 输出=%s, 大小=%s",
                    path.toStdString().c_str(),
                    FormatFileSize(size).toStdString().c_str());

        ElaMessageBar::success(ElaMessageBarType::Top, "打包完成",
            QString("输出：%1（%2）").arg(path).arg(FormatFileSize(size)), 4000, m_parentWidget);

        // 自动打开压缩包所在目录
        QFileInfo fi(path);
        QDir outDir = fi.absoluteDir();
        QDesktopServices::openUrl(QUrl::fromLocalFile(outDir.absolutePath()));
    });

    // 打包出错 → 隐藏进度条 + 显示错误
    connect(m_packager, &Packager::PackError, this, [this](const QString& msg)
    {
        LOGMGR_ERROR((*m_logMgr), "MainPresenter", "打包失败: %s", msg.toStdString().c_str());
        emit ProgressChanged(0, false);
        emit StatusChanged("打包失败: " + msg);
    });

    LOGMGR_INFO((*m_logMgr), "MainPresenter", "初始化完成, 所有Service已就绪");
}

// ========== 访问器 ==========

ResultModel* MainPresenter::GetResultModel() const
{
    return m_resultModel;
}

RuleEngine* MainPresenter::GetRuleEngine() const
{
    return m_ruleEngine;
}

ConfigManager* MainPresenter::GetConfigManager() const
{
    return m_configManager;
}

// ========== 扫描流程 ==========

void MainPresenter::OnScan(const QString& dir)
{
    // 输入校验：路径非空且目录存在
    QString path = dir.trimmed();
    if (path.isEmpty())
    {
        LOGMGR_WARN((*m_logMgr), "MainPresenter", "扫描请求被拒绝: 路径为空");
        emit StatusChanged("请先选择工程根目录");
        return;
    }
    QDir rootDir(path);
    if (!rootDir.exists())
    {
        LOGMGR_ERROR((*m_logMgr), "MainPresenter", "扫描请求被拒绝: 目录不存在 %s", path.toStdString().c_str());
        emit StatusChanged("目录不存在: " + path);
        return;
    }

    // 记录最后扫描目录，供清理后自动打包使用
    m_lastSourceDir = path;

    // 启动异步扫描：设置根目录后 StartScan 在工作线程执行，UI 不阻塞
    LOGMGR_INFO((*m_logMgr), "MainPresenter", "用户触发扫描, 目录: %s", path.toStdString().c_str());
    emit StatusChanged("正在扫描...");
    emit ProgressChanged(0, true);
    emit StatsChanged("扫描中...");
    m_scanManager->SetRootPath(path);
    m_scanManager->SetExcludeVcsDirs(m_configManager->excludeVcsDirs);  // 同步最新配置
    m_scanManager->StartScan();
}

// ========== 清理流程 ==========

void MainPresenter::OnClean()
{
    // 第一步：收集 ResultModel 中所有勾选项的文件路径
    QStringList targets;
    for (int i = 0; i < m_resultModel->TotalCount(); ++i)
    {
        auto item = m_resultModel->GetFile(i);
        if (item.checked)
        {
            targets << item.filePath;
        }
    }

    // 第二步：无勾选项时提前返回，不执行清理
    if (targets.isEmpty())
    {
        LOGMGR_WARN((*m_logMgr), "MainPresenter", "清理请求被拒绝: 无勾选待清理项");
        ElaMessageBar::warning(ElaMessageBarType::Top, "提示", "没有勾选待清理项", 2000, m_parentWidget);
        return;
    }

    // 第三步：启动异步清理（确认已在 View 层完成）
    LOGMGR_INFO((*m_logMgr), "MainPresenter", "用户触发清理, 目标数: %d", targets.size());
    emit StatusChanged("正在清理...");
    emit ProgressChanged(0, true);
    m_fileCleaner->SetTargetList(targets);
    m_fileCleaner->StartClean();
}

// ========== 打包流程 ==========

void MainPresenter::OnPack(const QString& dir)
{
    // 输入校验：路径非空且目录存在
    QString path = dir.trimmed();
    if (path.isEmpty())
    {
        LOGMGR_WARN((*m_logMgr), "MainPresenter", "打包请求被拒绝: 路径为空");
        emit StatusChanged("请先选择要打包的源码目录");
        return;
    }
    QDir rootDir(path);
    if (!rootDir.exists())
    {
        LOGMGR_ERROR((*m_logMgr), "MainPresenter", "打包请求被拒绝: 目录不存在 %s", path.toStdString().c_str());
        emit StatusChanged("目录不存在: " + path);
        return;
    }

    LOGMGR_INFO((*m_logMgr), "MainPresenter", "用户触发打包, 目录: %s", path.toStdString().c_str());
    m_packager->SetSourceDir(path);
    m_packager->SetExcludeVcsDirs(m_configManager->excludeVcsDirs);  // 同步最新配置

    // 同步打包输出目录：配置留空时传空串，由 Packager 回退到 源码目录/../output
    m_packager->SetOutputDir(m_configManager->outputDir);

    // 按设置页的包名模板展开；模板留空时传空串，由 Packager 回退到默认命名
    const QString outputName = Packager::FormatOutputName(m_configManager->packageNamePattern, path);
    m_packager->SetOutputName(outputName);

    const QString outDirText = m_configManager->outputDir.isEmpty()
        ? QString("(默认 源码目录/../output)") : m_configManager->outputDir;
    const QString nameText = outputName.isEmpty()
        ? QString("(默认 项目名_时间戳_source)") : outputName;
    LOGMGR_INFO((*m_logMgr), "MainPresenter", "打包参数: 输出目录 %s, 包名 %s",
                outDirText.toStdString().c_str(), nameText.toStdString().c_str());

    // 语义约定：未勾选项 = 保留项 = 需要打包的文件
    // 如果之前执行过扫描，将未勾选（保留）的文件列表传给 Packager 作为打包白名单
    if (m_resultModel->TotalCount() > 0)
    {
        QStringList keepFiles;
        for (int i = 0; i < m_resultModel->TotalCount(); ++i)
        {
            auto item = m_resultModel->GetFile(i);
            if (!item.checked)
            {
                keepFiles << item.filePath;
            }
        }
        if (!keepFiles.isEmpty())
        {
            m_packager->SetFileList(keepFiles);
        }
    }

    // 启动异步打包
    emit StatusChanged("正在打包...");
    emit ProgressChanged(0, true);
    m_packager->StartPack();
}

// ========== 工具方法 ==========

// FormatFileSize: 将字节数转为人类可读的大小字符串（B/KB/MB/GB），保留一位小数
QString MainPresenter::FormatFileSize(qint64 bytes)
{
    if (bytes < 1024)
    {
        return QString::number(bytes) + " B";
    }
    else if (bytes < 1024 * 1024)
    {
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    }
    else if (bytes < 1024LL * 1024 * 1024)
    {
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    }
    else
    {
        return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
    }
}
