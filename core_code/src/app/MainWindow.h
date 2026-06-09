#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "ElaWindow.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class ElaStatusBar;
class ElaDockWidget;
class ElaText;
class ElaProgressBar;
class ScanManager;
class ResultModel;

// 主窗口：继承 ElaWindow，使用 Fluent UI 导航 + 停靠布局
// 调用链：main() 创建实例并 show() → ElaWindow 管理导航/AppBar/状态栏
// 生命周期：随 QApplication 一起销毁，析构时 Qt 自动回收子控件
class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    // InitWindow: 设置窗口尺寸、标题、用户信息卡片、导航栏开关，无副作用
    void InitWindow();
    // InitAppBar: 创建标题栏自定义菜单，含主题切换 MoonStars 按钮
    void InitAppBar();
    // InitNavBar: 创建左侧导航页（扫描/规则/日志）+ 底部设置页脚节点
    void InitNavBar();
    // InitCentral: 中心区域占位，实际内容由 NavBar 的 addPageNode 填充
    void InitCentral();
    // InitDetailPanel: 创建右侧停靠详情面板，默认 220px 宽，内容为 QLabel
    void InitDetailPanel();
    // InitStatusBar: 创建底部状态栏（状态文本 + 进度条 + 统计 + 扫描/清理/打包按钮）
    void InitStatusBar();
    // InitConnections: 绑定浏览按钮等零散信号槽
    void InitConnections();

    // OnScan: 用户点击扫描按钮，更新状态栏提示，后续启动 ScanManager 异步扫描
    void OnScan();
    // OnClean: 用户点击清理按钮，更新状态栏提示，后续启动 FileCleaner 异步清理
    void OnClean();
    // OnPack: 用户点击打包按钮，更新状态栏提示，后续启动 Packager 异步打包
    void OnPack();
    // ToggleTheme: 切换浅色/深色主题，setUpdatesEnabled(false) 跳过动画过渡避免卡顿
    void ToggleTheme();

    // 核心模块
    ScanManager* m_scanManager{nullptr};   // 扫描管理器
    ResultModel* m_resultModel{nullptr};   // 结果数据模型

    // 顶部路径栏
    QLineEdit* m_pathEdit{nullptr};        // 路径输入框
    QPushButton* m_browseBtn{nullptr};     // 浏览按钮

    // 中间表格
    QTableView* m_fileTable{nullptr};      // 文件列表表格

    // 右侧详情
    ElaDockWidget* m_detailDock{nullptr};  // 详情停靠面板
    QLabel* m_detailLabel{nullptr};        // 详情占位

    // 底部状态栏
    ElaStatusBar* m_statusBar{nullptr};    // 状态栏
    ElaText* m_statusText{nullptr};        // 状态文本
    ElaProgressBar* m_progressBar{nullptr}; // 进度条
    ElaText* m_statText{nullptr};          // 统计信息

    // 导航页键
    QString m_scanPageKey;                 // 扫描页
    QString m_rulesPageKey;                // 规则页
    QString m_logPageKey;                  // 日志页
    QString m_settingsPageKey;             // 设置页
};

#endif // MAINWINDOW_H
