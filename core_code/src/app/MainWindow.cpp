// 主窗口实现：UI 布局初始化 + 信号槽绑定 + 扫描/清理/打包操作入口
#include "MainWindow.h"

#include "ElaApplication.h"
#include "ElaDockWidget.h"
#include "ElaMenu.h"
#include "ElaProgressBar.h"
#include "ElaStatusBar.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToolButton.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

#include "core/ResultModel.h"
#include "core/ScanManager.h"
#include "core/LogManager.h"
#include "Logger.h"

MainWindow::MainWindow(QWidget* parent)
    : ElaWindow(parent)
{
    InitWindow();
    InitAppBar();
    InitNavBar();
    InitCentral();
    InitDetailPanel();
    InitStatusBar();
    InitConnections();
}

MainWindow::~MainWindow()
{
}

void MainWindow::InitWindow()
{
    // 窗口基础尺寸和元信息
    resize(1200, 740);
    setWindowTitle("CodeCleanTool");
    setUserInfoCardTitle("CodeCleanTool");
    setUserInfoCardSubTitle("源代码清理与打包工具");
    setUserInfoCardVisible(true);
    setIsNavigationBarEnable(true);
}

void MainWindow::InitAppBar()
{
    // AppBar 自定义菜单 — 主题切换按钮
    auto* appBarMenu = new ElaMenu(this);
    appBarMenu->setMenuItemHeight(27);

    connect(appBarMenu->addElaIconAction(ElaIconType::MoonStars, "切换深色/浅色主题"),
            &QAction::triggered, this, &MainWindow::ToggleTheme);

    setCustomMenu(appBarMenu);
}

void MainWindow::ToggleTheme()
{
    // 禁用更新跳过 ElaThemeAnimationWidget 颜色渐变动画，避免主题切换时界面卡顿
    setUpdatesEnabled(false);

    ElaThemeType::ThemeMode current = eTheme->getThemeMode();
    eTheme->setThemeMode(current == ElaThemeType::Light
                         ? ElaThemeType::Dark
                         : ElaThemeType::Light);

    setUpdatesEnabled(true);
}

void MainWindow::InitNavBar()
{
    // 扫描页 — 主操作区
    auto* scanPage = new QWidget(this);
    auto* scanLayout = new QVBoxLayout(scanPage);
    scanLayout->setContentsMargins(10, 10, 10, 10);

    // 路径选择栏
    auto* pathBar = new QHBoxLayout();
    m_pathEdit = new QLineEdit(scanPage);
    m_pathEdit->setPlaceholderText("选择或输入工程根目录...");
    m_pathEdit->setMinimumHeight(32);
    m_browseBtn = new QPushButton("浏览", scanPage);
    m_browseBtn->setMinimumHeight(32);
    m_browseBtn->setFixedWidth(80);
    pathBar->addWidget(m_pathEdit, 1);
    pathBar->addWidget(m_browseBtn);
    scanLayout->addLayout(pathBar);

    // 文件表格
    m_fileTable = new QTableView(scanPage);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileTable->horizontalHeader()->setStretchLastSection(true);
    m_fileTable->setAlternatingRowColors(true);
    scanLayout->addWidget(m_fileTable, 1);

    addPageNode("扫描", scanPage, ElaIconType::MagnifyingGlass);
    m_scanPageKey = scanPage->property("ElaPageKey").toString();

    // 规则页 — 规则配置
    auto* rulesPage = new QWidget(this);
    auto* rulesLayout = new QVBoxLayout(rulesPage);
    auto* rulesLabel = new QLabel("清理规则配置（待实现）", rulesPage);
    rulesLabel->setAlignment(Qt::AlignCenter);
    rulesLayout->addWidget(rulesLabel);
    addPageNode("规则", rulesPage, ElaIconType::Filter);
    m_rulesPageKey = rulesPage->property("ElaPageKey").toString();

    // 日志页 — 操作日志
    auto* logPage = new QWidget(this);
    auto* logLayout = new QVBoxLayout(logPage);
    auto* logLabel = new QLabel("操作日志（待实现）", logPage);
    logLabel->setAlignment(Qt::AlignCenter);
    logLayout->addWidget(logLabel);
    addPageNode("日志", logPage, ElaIconType::ListCheck);
    m_logPageKey = logPage->property("ElaPageKey").toString();

    // 设置页脚节点
    addFooterNode("设置", nullptr, m_settingsPageKey, 0, ElaIconType::GearComplex);
}

void MainWindow::InitCentral()
{
    // 中心区域已在 InitNavBar 中通过 addPageNode 设置
}

void MainWindow::InitDetailPanel()
{
    // 右侧详情停靠面板，默认 220px 宽
    m_detailDock = new ElaDockWidget("文件详情", this);
    m_detailLabel = new QLabel("选择文件查看详情", m_detailDock);
    m_detailLabel->setAlignment(Qt::AlignCenter);
    m_detailLabel->setWordWrap(true);
    m_detailDock->setWidget(m_detailLabel);
    addDockWidget(Qt::RightDockWidgetArea, m_detailDock);
    resizeDocks({m_detailDock}, {220}, Qt::Horizontal);
}

void MainWindow::InitStatusBar()
{
    m_statusBar = new ElaStatusBar(this);

    // 左侧信息区容器 — 用 HBoxLayout 统一管理间距
    auto* infoWidget = new QWidget(this);
    auto* infoLayout = new QHBoxLayout(infoWidget);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(12);

    m_statusText = new ElaText("就绪", this);
    m_statusText->setTextPixelSize(13);
    infoLayout->addWidget(m_statusText);

    // 进度条（初始隐藏）
    m_progressBar = new ElaProgressBar(this);
    m_progressBar->setFixedSize(150, 16);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setVisible(false);
    infoLayout->addWidget(m_progressBar);

    // 统计信息
    m_statText = new ElaText("共 0 项 | 待清理 0 项 | 可释放 0 B", this);
    m_statText->setTextPixelSize(13);
    infoLayout->addWidget(m_statText);

    m_statusBar->addWidget(infoWidget, 1);

    // 操作按钮 — addPermanentWidget 靠右
    auto* scanBtn = new ElaToolButton(this);
    scanBtn->setElaIcon(ElaIconType::MagnifyingGlass);
    scanBtn->setText("扫描");
    scanBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    scanBtn->setFixedHeight(28);
    connect(scanBtn, &ElaToolButton::clicked, this, &MainWindow::OnScan);
    m_statusBar->addPermanentWidget(scanBtn);

    auto* cleanBtn = new ElaToolButton(this);
    cleanBtn->setElaIcon(ElaIconType::Trash);
    cleanBtn->setText("清理");
    cleanBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    cleanBtn->setFixedHeight(28);
    connect(cleanBtn, &ElaToolButton::clicked, this, &MainWindow::OnClean);
    m_statusBar->addPermanentWidget(cleanBtn);

    auto* packBtn = new ElaToolButton(this);
    packBtn->setElaIcon(ElaIconType::BoxArchive);
    packBtn->setText("打包");
    packBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    packBtn->setFixedHeight(28);
    connect(packBtn, &ElaToolButton::clicked, this, &MainWindow::OnPack);
    m_statusBar->addPermanentWidget(packBtn);

    setStatusBar(m_statusBar);
}

void MainWindow::InitConnections()
{
    // 浏览按钮 — 打开目录选择对话框，将选中路径填入输入框
    connect(m_browseBtn, &QPushButton::clicked, this, [this]()
    {
        QString dir = QFileDialog::getExistingDirectory(this, "选择工程根目录", m_pathEdit->text());
        if (!dir.isEmpty())
        {
            m_pathEdit->setText(dir);
        }
    });
}

void MainWindow::OnScan()
{
    LOG_INFO("[MainWindow] 用户触发扫描, 目录: %s", m_pathEdit->text().toStdString().c_str());
    // 更新状态栏 — 进度条可见、统计文本切换
    m_statusText->setText("正在扫描...");
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    m_statText->setText("扫描中...");
    // TODO: 启动 ScanManager 异步扫描
}

void MainWindow::OnClean()
{
    LOG_INFO("[MainWindow] 用户触发清理");
    // 更新状态栏 — 提示准备清理
    m_statusText->setText("准备清理...");
    m_statText->setText("待清理中...");
    // TODO: 启动 FileCleaner 异步清理
}

void MainWindow::OnPack()
{
    LOG_INFO("[MainWindow] 用户触发打包");
    // 更新状态栏 — 提示准备打包
    m_statusText->setText("准备打包...");
    m_statText->setText("打包中...");
    // TODO: 启动 Packager 异步打包
}
