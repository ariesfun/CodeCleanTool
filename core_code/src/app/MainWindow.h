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
class MainPresenter;
class LogManager;
class LogListModel;

// 主窗口（View 层）：继承 ElaWindow，负责布局渲染 + 转发用户操作给 MainPresenter
// 调用链：main() → show() → 按钮点击 → MainPresenter 槽 → Service → UI 信号回传
// 生命周期：随 QApplication 一起销毁，析构时 Qt 自动回收子控件
// 约束：不直接持有 Service 引用，不包含业务逻辑
class MainWindow : public ElaWindow
{
    Q_OBJECT

public:
    // logMgr: 日志管理器指针（非拥有），供日志页连接 LogAdded 信号
    explicit MainWindow(LogManager* logMgr, QWidget* parent = nullptr);
    ~MainWindow() override;

private:
    // InitWindow: 设置窗口尺寸、标题、用户信息卡片、导航栏开关
    void InitWindow();
    // InitAppBar: 创建标题栏自定义菜单，含主题切换 MoonStars 按钮
    void InitAppBar();
    // InitNavBar: 创建左侧导航页（扫描/规则/日志）+ 底部设置页脚节点
    void InitNavBar();
    // InitCentral: 中心区域占位，实际内容由 NavBar 的 addPageNode 填充
    void InitCentral();
    // InitDetailPanel: 创建右侧停靠详情面板，默认 220px 宽
    void InitDetailPanel();
    // InitStatusBar: 创建底部状态栏（状态文本 + 进度条 + 统计 + 扫描/清理/打包按钮）
    void InitStatusBar();
    // InitConnections: 绑定浏览按钮等 View 内部信号槽
    void InitConnections();
    // InitPresenter: 创建 MainPresenter → 绑定 ResultModel 到 QTableView → 连接 Presenter UI 信号 → View 控件
    void InitPresenter();
    // InitLogPage: 替换日志页占位为 QTableView + LogListModel + 过滤栏 + 导出/清空按钮
    void InitLogPage();
    // InitRulesPage: 替换规则页占位为规则列表 + 添加/删除按钮
    void InitRulesPage();
    // InitSettingsPage: 替换设置页占位为配置表单，绑定 ConfigManager
    void InitSettingsPage();

    // OnScan: 转发到 MainPresenter::OnScan
    void OnScan();
    // OnClean: 转发到 MainPresenter::OnClean
    void OnClean();
    // OnPack: 转发到 MainPresenter::OnPack
    void OnPack();
    // ToggleTheme: 切换浅色/深色主题，setUpdatesEnabled(false) 跳过动画过渡
    void ToggleTheme();

    // 基础设施
    LogManager* m_logMgr{nullptr};            // 日志管理器（非拥有，main.cpp 创建）
    // 控制层
    MainPresenter* m_presenter{nullptr};     // Presenter（协调 View ↔ Service）

    // 顶部路径栏
    QLineEdit* m_pathEdit{nullptr};          // 路径输入框
    QPushButton* m_browseBtn{nullptr};       // 浏览按钮

    // 中间表格
    QTableView* m_fileTable{nullptr};        // 文件列表表格

    // 右侧详情
    ElaDockWidget* m_detailDock{nullptr};    // 详情停靠面板
    QLabel* m_detailLabel{nullptr};          // 详情占位

    // 底部状态栏
    ElaStatusBar* m_statusBar{nullptr};      // 状态栏
    ElaText* m_statusText{nullptr};          // 状态文本
    ElaProgressBar* m_progressBar{nullptr};  // 进度条
    ElaText* m_statText{nullptr};            // 统计信息

    // 导航页键
    QString m_scanPageKey;                   // 扫描页
    QString m_rulesPageKey;                  // 规则页
    QString m_logPageKey;                    // 日志页
    QString m_settingsPageKey;               // 设置页

    // 页面控件指针（用于 Init*Page 方法填充内容）
    QWidget* m_rulesPageWidget{nullptr};     // 规则页容器
    QWidget* m_settingsPageWidget{nullptr};  // 设置页容器
};

#endif // MAINWINDOW_H
