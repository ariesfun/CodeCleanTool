#ifndef MAINPRESENTER_H
#define MAINPRESENTER_H

#include <QObject>

class ConfigManager;
class RuleEngine;
class GitIgnoreParser;
class ScanManager;
class FileCleaner;
class Packager;
class ResultModel;
class LogManager;
class QWidget;

// 控制层：协调 View 与 Service，管理异步任务状态
// 调用链：MainWindow 按钮点击 → Presenter 槽 → Service → 信号回传 → UI 信号发射
// 生命周期：由 MainWindow 创建和拥有，随 MainWindow 销毁
// 线程约束：全部在 UI 主线程执行（Service 内部自行创建工作线程）
class MainPresenter : public QObject
{
    Q_OBJECT

public:
    // logMgr: 日志管理器指针（非拥有），用于 LOGMGR_* 宏双写文件+UI面板
    explicit MainPresenter(LogManager* logMgr, QObject* parent = nullptr);
    ~MainPresenter() override;

    // Init: 创建所有 Service 实例 + 注入依赖 + 连接 Service → UI 信号链
    void Init();
    // GetResultModel: View 通过此方法获取模型绑定到 QTableView
    ResultModel* GetResultModel() const;
    // GetRuleEngine: 规则页通过此方法获取规则引擎
    RuleEngine* GetRuleEngine() const;
    // GetConfigManager: 设置页通过此方法读写配置
    ConfigManager* GetConfigManager() const;
    // FormatFileSize: 将字节数转为可读字符串（B/KB/MB/GB），供 View 层使用
    static QString FormatFileSize(qint64 bytes);

public slots:
    // OnScan: 校验目录 → 配置 ScanManager → 启动异步扫描，更新 UI 状态
    void OnScan(const QString& dir);
    // OnClean: 收集勾选项 → 启动异步清理（确认在 View 层完成）
    void OnClean();
    // OnPack: 校验目录 → 收集保留项 → 启动异步打包
    void OnPack(const QString& dir);

signals:
    void StatusChanged(const QString& text);            // 状态栏文字更新
    void ProgressChanged(int percent, bool visible);    // 进度条更新
    void StatsChanged(const QString& text);             // 统计文本更新
    void StatsDataChanged(qint64 totalSize, qint64 cleanableSize); // 瘦身统计（原项目大小、可清理大小）

private:
    // 日志管理器（非拥有，main.cpp 创建）
    LogManager* m_logMgr{nullptr};
    // 父窗口指针，供 ElaMessageBar 通知条定位（非拥有）
    QWidget* m_parentWidget{nullptr};
    // Service 实例（Presenter 拥有）
    ConfigManager* m_configManager{nullptr};
    RuleEngine* m_ruleEngine{nullptr};
    GitIgnoreParser* m_gitIgnore{nullptr};
    ScanManager* m_scanManager{nullptr};
    FileCleaner* m_fileCleaner{nullptr};
    Packager* m_packager{nullptr};
    ResultModel* m_resultModel{nullptr};
    QString m_lastSourceDir;         // 最近一次扫描的源码目录，供自动打包使用
    qint64 m_lastTotalProjectSize{0}; // 上次扫描的项目总大小，清理后更新用
};

#endif // MAINPRESENTER_H
