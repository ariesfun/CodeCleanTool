// 主窗口实现（View 层）：UI 布局 + 信号转发给 MainPresenter
#include "MainWindow.h"
#include "MainPresenter.h"

#include "ElaApplication.h"
#include "ElaContentDialog.h"
#include "ElaDockWidget.h"
#include "ElaMenu.h"
#include "ElaMessageBar.h"
#include "ElaProgressBar.h"
#include "ElaStatusBar.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToolButton.h"

#include <QApplication>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTableWidget>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>

#include "core/ConfigManager.h"
#include "core/LogListModel.h"
#include "core/LogManager.h"
#include "core/ResultModel.h"
#include "core/RuleEngine.h"
#include "StatsWidget.h"
#include "Logger.h"

// 排序代理模型：优先按清理目标类型排序（编译产物/构建目录排最前），同类型再按点击列排序
class ResultSortModel : public QSortFilterProxyModel
{
public:
    explicit ResultSortModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {}

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
    {
        int leftPrio = sourceModel()->data(left, ResultModel::SortPriorityRole).toInt();
        int rightPrio = sourceModel()->data(right, ResultModel::SortPriorityRole).toInt();
        if (leftPrio != rightPrio)
        {
            return leftPrio < rightPrio;
        }
        return QSortFilterProxyModel::lessThan(left, right);
    }
};

MainWindow::MainWindow(LogManager* logMgr, QWidget* parent)
    : ElaWindow(parent)
    , m_logMgr(logMgr)
{
    InitWindow();
    InitAppBar();
    InitNavBar();
    InitCentral();
    InitDetailPanel();
    InitStatusBar();
    InitConnections();
    InitPresenter();
    InitLogPage();
    InitRulesPage();
    InitSettingsPage();

    // 深色主题样式表：仅覆盖原生 Qt 控件（QTableView / QTableWidget / QLabel / QLineEdit / QPushButton / QGroupBox / QCheckBox / QDialog / QMessageBox 等）
    // 不使用 QApplication::setPalette()，因其会破坏 ElaWidgetTools 自绘控件的调色板且无法在切换回浅色时正确复原
    // Qt 样式表与 QPalette 相互独立，不影响 ElaWidgetTools 控件的 palette 渲染
    static const char* kDarkStyleSheet = R"(
        QTableView, QTableWidget {
            background-color: #1E1E1E;
            color: #E0E0E0;
            gridline-color: #3A3A3A;
            alternate-background-color: #2A2A2A;
        }
        QTableView::item:selected, QTableWidget::item:selected {
            background-color: #264F78;
            color: #FFFFFF;
        }
        QHeaderView::section {
            background-color: #2D2D2D;
            color: #CCCCCC;
            border: 1px solid #3A3A3A;
            padding: 4px 6px;
        }
        QLabel {
            color: #E0E0E0;
            background: transparent;
        }
        QLineEdit {
            background-color: #2D2D2D;
            color: #E0E0E0;
            border: 1px solid #3A3A3A;
            padding: 2px 6px;
        }
        QGroupBox {
            color: #CCCCCC;
            border: 1px solid #3A3A3A;
            margin-top: 12px;
            padding-top: 10px;
        }
        QGroupBox::title {
            color: #CCCCCC;
            subcontrol-origin: margin;
            padding: 0 4px;
        }
        QPushButton {
            background-color: #3D3D3D;
            color: #E0E0E0;
            border: 1px solid #4A4A4A;
            padding: 4px 8px;
        }
        QPushButton:hover {
            background-color: #4A4A4A;
        }
        QPushButton:pressed {
            background-color: #353535;
        }
        QCheckBox {
            color: #E0E0E0;
        }
        QComboBox {
            background-color: #2D2D2D;
            color: #E0E0E0;
            border: 1px solid #3A3A3A;
            padding: 2px 6px;
        }
        QComboBox::drop-down {
            background-color: #3D3D3D;
        }
        QComboBox QAbstractItemView {
            background-color: #2D2D2D;
            color: #E0E0E0;
            selection-background-color: #264F78;
        }
        QTextEdit, QPlainTextEdit {
            background-color: #1E1E1E;
            color: #E0E0E0;
        }
        QFrame[frameShape="4"] {
            color: #3A3A3A;
        }
        QDialog, QMessageBox {
            background-color: #2D2D2D;
        }
    )";

    auto applyDarkStyleSheet = [](bool dark)
    {
        qApp->setStyleSheet(dark ? QString::fromLatin1(kDarkStyleSheet) : QString());
    };

    applyDarkStyleSheet(eTheme->getThemeMode() == ElaThemeType::Dark);
    connect(eTheme, &ElaTheme::themeModeChanged, this,
            [applyDarkStyleSheet](ElaThemeType::ThemeMode mode)
            { applyDarkStyleSheet(mode == ElaThemeType::Dark); });
}

MainWindow::~MainWindow()
{
}

void MainWindow::InitWindow()
{
    resize(1200, 740);
    setWindowTitle("CodeCleanTool");
    setUserInfoCardTitle("CodeCleanTool");
    setUserInfoCardSubTitle("源代码清理与打包工具");
    setUserInfoCardVisible(true);
    setIsNavigationBarEnable(true);
}

void MainWindow::InitAppBar()
{
    auto* appBarMenu = new ElaMenu(this);
    appBarMenu->setMenuItemHeight(27);

    connect(appBarMenu->addElaIconAction(ElaIconType::MoonStars, "切换深色/浅色主题"),
            &QAction::triggered, this, &MainWindow::ToggleTheme);

    setCustomMenu(appBarMenu);
}

void MainWindow::ToggleTheme()
{
    setUpdatesEnabled(false);

    ElaThemeType::ThemeMode current = eTheme->getThemeMode();
    ElaThemeType::ThemeMode next = (current == ElaThemeType::Light)
                                   ? ElaThemeType::Dark
                                   : ElaThemeType::Light;
    eTheme->setThemeMode(next);

    LOGMGR_INFO((*m_logMgr), "MainWindow", "主题切换: %s",
                next == ElaThemeType::Dark ? "深色" : "浅色");

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
    auto* selectAllBtn = new QPushButton("全选", scanPage);
    selectAllBtn->setMinimumHeight(32);
    selectAllBtn->setFixedWidth(56);
    auto* deselectAllBtn = new QPushButton("反选", scanPage);
    deselectAllBtn->setMinimumHeight(32);
    deselectAllBtn->setFixedWidth(56);
    pathBar->addWidget(m_pathEdit, 1);
    pathBar->addWidget(m_browseBtn);
    pathBar->addWidget(selectAllBtn);
    pathBar->addWidget(deselectAllBtn);
    scanLayout->addLayout(pathBar);

    // 全选/反选 — 通过排序代理访问源模型
    connect(selectAllBtn, &QPushButton::clicked, this, [this]()
    {
        auto* sortModel = qobject_cast<QSortFilterProxyModel*>(m_fileTable->model());
        if (!sortModel) { return; }
        auto* model = qobject_cast<ResultModel*>(sortModel->sourceModel());
        if (!model) { return; }
        for (int i = 0; i < model->TotalCount(); ++i)
            model->setData(model->index(i, ResultModel::ColName), Qt::Checked, Qt::CheckStateRole);
    });
    connect(deselectAllBtn, &QPushButton::clicked, this, [this]()
    {
        auto* sortModel = qobject_cast<QSortFilterProxyModel*>(m_fileTable->model());
        if (!sortModel) { return; }
        auto* model = qobject_cast<ResultModel*>(sortModel->sourceModel());
        if (!model) { return; }
        for (int i = 0; i < model->TotalCount(); ++i)
            model->setData(model->index(i, ResultModel::ColName), Qt::Unchecked, Qt::CheckStateRole);
    });

    // 文件表格
    m_fileTable = new QTableView(scanPage);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileTable->horizontalHeader()->setStretchLastSection(true);
    m_fileTable->setAlternatingRowColors(true);
    m_fileTable->setSortingEnabled(true);
    scanLayout->addWidget(m_fileTable, 1);

    addPageNode("扫描", scanPage, ElaIconType::MagnifyingGlass);
    m_scanPageKey = scanPage->property("ElaPageKey").toString();

    // 规则页 — 控件在 InitRulesPage 中构建
    m_rulesPageWidget = new QWidget(this);
    addPageNode("规则", m_rulesPageWidget, ElaIconType::Filter);
    m_rulesPageKey = m_rulesPageWidget->property("ElaPageKey").toString();

    // 日志页 — 控件在 InitLogPage 中构建
    auto* logPage = new QWidget(this);
    addPageNode("日志", logPage, ElaIconType::ListCheck);
    m_logPageKey = logPage->property("ElaPageKey").toString();

    // 设置页脚节点 — 控件在 InitSettingsPage 中构建
    m_settingsPageWidget = new QWidget(this);
    addFooterNode("设置", m_settingsPageWidget, m_settingsPageKey, 0, ElaIconType::GearComplex);
}

void MainWindow::InitCentral()
{
}

void MainWindow::InitDetailPanel()
{
    // 文件详情 dock（右上）
    m_detailDock = new ElaDockWidget("文件详情", this);
    m_detailLabel = new QLabel("选择文件查看详情", m_detailDock);
    m_detailLabel->setAlignment(Qt::AlignCenter);
    m_detailLabel->setWordWrap(true);
    m_detailDock->setWidget(m_detailLabel);
    addDockWidget(Qt::RightDockWidgetArea, m_detailDock);

    // 瘦身统计 dock（右下），独立停靠面板
    m_statsDock = new ElaDockWidget("瘦身统计", this);
    // StatsWidget 在此创建，主题感知由 StatsWidget 内部处理
    m_statsWidget = new StatsWidget(m_statsDock);
    m_statsDock->setWidget(m_statsWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_statsDock);

    // 两个 dock 等宽 280px
    resizeDocks({m_detailDock, m_statsDock}, {280, 280}, Qt::Horizontal);
}

void MainWindow::InitStatusBar()
{
    m_statusBar = new ElaStatusBar(this);

    auto* infoWidget = new QWidget(this);
    auto* infoLayout = new QHBoxLayout(infoWidget);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    infoLayout->setSpacing(12);

    m_statusText = new ElaText("就绪", this);
    m_statusText->setTextPixelSize(13);
    infoLayout->addWidget(m_statusText);

    m_progressBar = new ElaProgressBar(this);
    m_progressBar->setFixedSize(150, 16);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setVisible(false);
    infoLayout->addWidget(m_progressBar);

    m_statText = new ElaText("共 0 项 | 待清理 0 项 | 可释放 0 B", this);
    m_statText->setTextPixelSize(13);
    infoLayout->addWidget(m_statText);

    m_statusBar->addWidget(infoWidget, 1);

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
    connect(m_browseBtn, &QPushButton::clicked, this, [this]()
    {
        QString dir = QFileDialog::getExistingDirectory(this, "选择工程根目录", m_pathEdit->text());
        if (!dir.isEmpty())
        {
            m_pathEdit->setText(dir);
            OnScan();
        }
    });
}

void MainWindow::InitPresenter()
{
    // 创建控制层，传入 LogManager 用于关键操作日志双写(文件+UI面板)
    m_presenter = new MainPresenter(m_logMgr, this);
    m_presenter->Init();

    // 包装排序代理模型：优先按清理目标类型排序
    auto* sortModel = new ResultSortModel(this);
    sortModel->setSourceModel(m_presenter->GetResultModel());
    sortModel->setSortRole(ResultModel::SortPriorityRole);
    m_fileTable->setModel(sortModel);
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    m_fileTable->setColumnWidth(0, 180);
    m_fileTable->setColumnWidth(1, 280);
    m_fileTable->setColumnWidth(2, 80);
    m_fileTable->setColumnWidth(3, 140);
    m_fileTable->setColumnWidth(4, 80);
    m_fileTable->setColumnWidth(5, 110);

    // Presenter UI 信号 → View 控件更新
    connect(m_presenter, &MainPresenter::StatusChanged, m_statusText, &ElaText::setText);
    connect(m_presenter, &MainPresenter::StatsChanged, m_statText, &ElaText::setText);
    connect(m_presenter, &MainPresenter::ProgressChanged, this, [this](int percent, bool visible)
    {
        m_progressBar->setValue(percent);
        m_progressBar->setVisible(visible);
    });

    // 瘦身统计数据 → 环形图更新
    connect(m_presenter, &MainPresenter::StatsDataChanged,
            m_statsWidget, &StatsWidget::UpdateStats);

    // 表格选中行变化 → 更新右侧详情面板
    connect(m_fileTable->selectionModel(), &QItemSelectionModel::currentChanged,
        this, [this](const QModelIndex& current, const QModelIndex&)
    {
        if (!current.isValid())
        {
            m_detailLabel->setText("选择文件查看详情");
            return;
        }
        // 通过排序代理获取源模型行
        auto* sortModel = qobject_cast<QSortFilterProxyModel*>(m_fileTable->model());
        if (!sortModel) { return; }
        QModelIndex sourceIdx = sortModel->mapToSource(current);
        auto* model = qobject_cast<ResultModel*>(sortModel->sourceModel());
        if (!model) { return; }

        auto item = model->GetFile(sourceIdx.row());
        QString detail;
        detail += QString("文件名: %1\n\n").arg(item.fileName);
        detail += QString("完整路径: %1\n\n").arg(item.filePath);
        detail += QString("文件大小: %1\n\n").arg(m_presenter->FormatFileSize(item.fileSize));
        detail += QString("修改时间: %1\n\n").arg(item.dateModified.toString("yyyy-MM-dd hh:mm:ss"));
        detail += QString("文件类型: %1\n\n").arg(item.fileType);
        detail += QString("命中规则: %1").arg(item.hitRule);
        m_detailLabel->setText(detail);
        m_detailLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    });
}

void MainWindow::OnScan()
{
    m_presenter->OnScan(m_pathEdit->text());
}

void MainWindow::OnClean()
{
    // 统计当前勾选的待清理项数量
    auto* model = m_presenter->GetResultModel();
    int targetCount = 0;
    for (int i = 0; i < model->TotalCount(); ++i)
    {
        if (model->GetFile(i).checked) { ++targetCount; }
    }

    if (targetCount == 0)
    {
        ElaMessageBar::warning(ElaMessageBarType::Top, "提示", "没有勾选待清理项", 2000, this);
        return;
    }

    // 懒创建清理确认弹窗（Ela 主题，自动跟随亮/暗）
    if (!m_cleanConfirmDialog)
    {
        m_cleanConfirmDialog = new ElaContentDialog(this);
        m_cleanConfirmDialog->setLeftButtonText("取消");
        m_cleanConfirmDialog->setMiddleButtonText("");
        m_cleanConfirmDialog->setRightButtonText("确认清理");
    }

    // 每次更新确认内容（文件数量可能变化）
    auto* content = new QWidget();
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(15, 25, 15, 10);
    auto* title = new ElaText("确认清理", content);
    title->setTextStyle(ElaTextType::Title);
    auto* subTitle = new ElaText(QString("将删除 %1 个文件/目录，此操作不可恢复，确定继续？").arg(targetCount), content);
    subTitle->setTextStyle(ElaTextType::Body);
    contentLayout->addWidget(title);
    contentLayout->addSpacing(2);
    contentLayout->addWidget(subTitle);
    contentLayout->addStretch();
    m_cleanConfirmDialog->setCentralWidget(content);

    // 断开上次连接后重连，避免多次 exec 后重复触发
    disconnect(m_cleanConfirmDialog, &ElaContentDialog::rightButtonClicked, nullptr, nullptr);
    connect(m_cleanConfirmDialog, &ElaContentDialog::rightButtonClicked, this, [this]()
    {
        m_presenter->OnClean();
    });

    m_cleanConfirmDialog->exec();
}

void MainWindow::OnPack()
{
    m_presenter->OnPack(m_pathEdit->text());
}

void MainWindow::InitLogPage()
{
    // 查找日志页 widget（ElaWindow 自动设置了 ElaPageKey 属性）
    QWidget* foundPage = nullptr;
    for (auto* w : this->findChildren<QWidget*>())
    {
        if (w->property("ElaPageKey").toString() == m_logPageKey)
        {
            foundPage = w;
            break;
        }
    }
    if (!foundPage)
    {
        return;
    }

    auto* layout = new QVBoxLayout(foundPage);
    layout->setContentsMargins(8, 8, 8, 8);

    // 过滤按钮栏
    auto* filterBar = new QHBoxLayout();
    auto makeFilterBtn = [&](const QString& text, int w = 64) {
        auto* btn = new QPushButton(text, foundPage);
        btn->setFixedSize(w, 28);
        filterBar->addWidget(btn);
        return btn;
    };
    auto* allBtn = makeFilterBtn("全部");
    auto* infoBtn = makeFilterBtn("INFO");
    auto* warnBtn = makeFilterBtn("WARN");
    auto* errorBtn = makeFilterBtn("ERROR", 72);
    filterBar->addStretch();

    auto* exportBtn = makeFilterBtn("导出");
    auto* clearBtn = makeFilterBtn("清空");
    layout->addLayout(filterBar);

    // 日志表格
    auto* logTable = new QTableView(foundPage);
    logTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    logTable->setAlternatingRowColors(true);
    logTable->horizontalHeader()->setStretchLastSection(true);
    logTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logTable->verticalHeader()->setVisible(false);
    layout->addWidget(logTable, 1);

    // 创建模型并绑定 LogManager
    auto* logModel = new LogListModel(this);
    logModel->SetLogManager(m_logMgr);
    logTable->setModel(logModel);
    logTable->setColumnWidth(LogListModel::ColTime, 80);
    logTable->setColumnWidth(LogListModel::ColLevel, 50);
    logTable->setColumnWidth(LogListModel::ColModule, 100);

    // 过滤按钮连接
    connect(allBtn, &QPushButton::clicked, this, [logModel]() { logModel->SetLevelFilter("ALL"); });
    connect(infoBtn, &QPushButton::clicked, this, [logModel]() { logModel->SetLevelFilter("INFO"); });
    connect(warnBtn, &QPushButton::clicked, this, [logModel]() { logModel->SetLevelFilter("WARN"); });
    connect(errorBtn, &QPushButton::clicked, this, [logModel]() { logModel->SetLevelFilter("ERROR"); });

    // 清空按钮 — Clear 是普通方法非 slot，用 lambda
    connect(clearBtn, &QPushButton::clicked, this, [logModel, this]()
    {
        m_logMgr->Clear();
        logModel->SetLevelFilter("ALL");
        LOGMGR_INFO((*m_logMgr), "MainWindow", "日志已清空");
    });

    // 导出按钮
    connect(exportBtn, &QPushButton::clicked, this, [this]()
    {
        QString path = QFileDialog::getSaveFileName(this, "导出日志", "codecleantool_export.log",
                                                     "日志文件 (*.log *.txt)");
        if (!path.isEmpty())
        {
            m_logMgr->ExportToFile(path);
            LOGMGR_INFO((*m_logMgr), "MainWindow", "日志已导出到: %s", path.toStdString().c_str());
        }
    });
}

// 创建规则表格（清理/保留各有独立表格，避免表头重复的 QTableWidget 工厂）
static QTableWidget* CreateRuleSubTable(QWidget* parent)
{
    auto* t = new QTableWidget(parent);
    t->setColumnCount(3);
    t->setHorizontalHeaderLabels({"模式", "目录规则", "操作"});
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setAlternatingRowColors(true);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setColumnWidth(0, 280);
    t->setColumnWidth(1, 70);
    t->verticalHeader()->setVisible(false);
    t->setEditTriggers(QAbstractItemView::DoubleClicked);
    t->setDragEnabled(true);
    t->setAcceptDrops(true);
    t->setDragDropMode(QAbstractItemView::InternalMove);
    t->setDropIndicatorShown(true);
    t->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    return t;
}

void MainWindow::InitRulesPage()
{
    if (!m_rulesPageWidget)
    {
        return;
    }

    auto* layout = new QVBoxLayout(m_rulesPageWidget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 添加规则栏
    auto* addBar = new QHBoxLayout();
    auto* patternEdit = new QLineEdit(m_rulesPageWidget);
    patternEdit->setPlaceholderText("输入规则模式（如 *.tmp, build/）...");
    patternEdit->setMinimumHeight(30);
    auto* typeCombo = new QComboBox(m_rulesPageWidget);
    typeCombo->addItem("清理规则", static_cast<int>(RuleType::Clean));
    typeCombo->addItem("保留规则", static_cast<int>(RuleType::Keep));
    typeCombo->setFixedWidth(90);
    auto* addBtn = new QPushButton("添加", m_rulesPageWidget);
    addBtn->setFixedSize(68, 30);
    auto* exportBtn = new QPushButton("导出", m_rulesPageWidget);
    exportBtn->setFixedSize(68, 30);
    auto* importBtn = new QPushButton("导入", m_rulesPageWidget);
    importBtn->setFixedSize(68, 30);
    addBar->addWidget(patternEdit, 1);
    addBar->addWidget(typeCombo);
    addBar->addWidget(addBtn);
    addBar->addWidget(exportBtn);
    addBar->addWidget(importBtn);
    layout->addLayout(addBar);

    // 清理规则区域
    auto* cleanGroup = new QGroupBox("清理规则", m_rulesPageWidget);
    auto* cleanLayout = new QVBoxLayout(cleanGroup);
    cleanLayout->setContentsMargins(4, 4, 4, 4);
    auto* cleanTable = CreateRuleSubTable(cleanGroup);
    cleanLayout->addWidget(cleanTable);
    layout->addWidget(cleanGroup, 1);

    // 保留规则区域
    auto* keepGroup = new QGroupBox("保留规则", m_rulesPageWidget);
    auto* keepLayout = new QVBoxLayout(keepGroup);
    keepLayout->setContentsMargins(4, 4, 4, 4);
    auto* keepTable = CreateRuleSubTable(keepGroup);
    keepLayout->addWidget(keepTable);
    layout->addWidget(keepGroup, 1);

    auto* engine = m_presenter->GetRuleEngine();

    // 更新分组标题中的规则条数
    auto updateRuleCounts = [cleanGroup, keepGroup, engine]()
    {
        auto rules = engine->GetRules();
        int cleanCount = 0, keepCount = 0;
        for (const auto& r : rules)
        {
            if (r.type == RuleType::Clean) { ++cleanCount; }
            else { ++keepCount; }
        }
        cleanGroup->setTitle(QString("清理规则（%1 条）").arg(cleanCount));
        keepGroup->setTitle(QString("保留规则（%1 条）").arg(keepCount));
    };

    // 填充子表格并内联绑定删除按钮 — 删除只移除单行，不触发全量重建
    LogManager* logMgrPtr = m_logMgr;  // 按值捕获指针避免 MSVC C3494
    auto populateSubTable = [engine, updateRuleCounts, logMgrPtr](QTableWidget* table, RuleType rtype)
    {
        table->setRowCount(0);
        table->disconnect(SIGNAL(itemChanged(QTableWidgetItem*)));

        auto rules = engine->GetRules();
        // 筛选出当前类型的规则
        QList<RuleEntry> subRules;
        for (const auto& r : rules)
        {
            if (r.type == rtype) { subRules.append(r); }
        }

        for (int i = 0; i < subRules.size(); ++i)
        {
            const auto& r = subRules[i];
            table->insertRow(i);

            auto* patternItem = new QTableWidgetItem(r.pattern);
            patternItem->setFlags(patternItem->flags() | Qt::ItemIsEditable);
            table->setItem(i, 0, patternItem);

            auto* dirItem = new QTableWidgetItem(r.isDirRule ? "是" : "否");
            dirItem->setFlags(dirItem->flags() & ~Qt::ItemIsEditable);
            table->setItem(i, 1, dirItem);

            auto* delBtn = new QPushButton("移除规则");
            delBtn->setFixedSize(74, 24);
            table->setCellWidget(i, 2, delBtn);

            // 内联删除逻辑：从引擎移除 → 仅移除当前行 + 更新计数
            int localRow = i;
            QObject::connect(delBtn, &QPushButton::clicked, delBtn, [engine, table, rtype, localRow, updateRuleCounts, logMgrPtr]()
            {
                auto allRules = engine->GetRules();
                int globalIdx = -1;
                int count = 0;
                for (int gi = 0; gi < allRules.size(); ++gi)
                {
                    if (allRules[gi].type == rtype)
                    {
                        if (count == localRow) { globalIdx = gi; break; }
                        ++count;
                    }
                }
                if (globalIdx >= 0)
                {
                    QString pattern = allRules[globalIdx].pattern;
                    engine->RemoveRule(globalIdx);
                    LOGMGR_INFO((*logMgrPtr), "MainWindow", "移除规则: %s", pattern.toStdString().c_str());
                }
                table->removeRow(localRow);
                updateRuleCounts();
            });
        }
    };

    // 绑定子表格编辑信号
    auto connectSubTableEdit = [engine](QTableWidget* table, RuleType rtype)
    {
        QObject::connect(table, &QTableWidget::itemChanged, table, [engine, rtype](QTableWidgetItem* item)
        {
            if (!item || item->column() != 0) { return; }
            QString newPattern = item->text().trimmed();
            if (newPattern.isEmpty()) { return; }
            auto rules = engine->GetRules();
            int localRow = item->row();
            for (int i = 0; i < rules.size(); ++i)
            {
                if (rules[i].type == rtype)
                {
                    if (localRow == 0)
                    {
                        engine->RemoveRule(i);
                        if (rtype == RuleType::Clean) { engine->AddCleanRule(newPattern); }
                        else { engine->AddKeepRule(newPattern); }
                        return;
                    }
                    --localRow;
                }
            }
        });
    };

    // 刷新全部：两个子表格均重建
    // 注意：按值捕获，不可用 &refreshAll（它是指向栈上 std::function 的引用，InitRulesPage 返回后悬空）
    auto refreshAll = [=]()
    {
        if (!engine) { return; }

        populateSubTable(cleanTable, RuleType::Clean);
        populateSubTable(keepTable, RuleType::Keep);

        connectSubTableEdit(cleanTable, RuleType::Clean);
        connectSubTableEdit(keepTable, RuleType::Keep);

        updateRuleCounts();
    };

    refreshAll();

    // 添加按钮
    QObject::connect(addBtn, &QPushButton::clicked, [patternEdit, typeCombo, engine, refreshAll, this]()
    {
        QString pattern = patternEdit->text().trimmed();
        if (pattern.isEmpty()) { return; }
        auto type = static_cast<RuleType>(typeCombo->currentData().toInt());
        if (type == RuleType::Clean)
        {
            engine->AddCleanRule(pattern);
            LOGMGR_INFO((*m_logMgr), "MainWindow", "添加清理规则: %s", pattern.toStdString().c_str());
        }
        else
        {
            engine->AddKeepRule(pattern);
            LOGMGR_INFO((*m_logMgr), "MainWindow", "添加保留规则: %s", pattern.toStdString().c_str());
        }
        patternEdit->clear();
        refreshAll();
    });

    // 导出按钮
    QObject::connect(exportBtn, &QPushButton::clicked, [engine, this]()
    {
        QString path = QFileDialog::getSaveFileName(nullptr, "导出规则", "rules_export.txt",
                                                     "文本文件 (*.txt)");
        if (path.isEmpty()) { return; }
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream ts(&file);
            auto rules = engine->GetRules();
            for (const auto& r : rules)
            {
                QString typeStr = (r.type == RuleType::Clean) ? "清理" : "保留";
                ts << r.pattern << "\t" << typeStr << "\n";
            }
        }
        LOGMGR_INFO((*m_logMgr), "MainWindow", "导出规则到: %s", path.toStdString().c_str());
    });

    // 导入按钮
    QObject::connect(importBtn, &QPushButton::clicked, [engine, refreshAll, this]()
    {
        QString path = QFileDialog::getOpenFileName(nullptr, "导入规则", "",
                                                     "文本文件 (*.txt)");
        if (path.isEmpty()) { return; }
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream ts(&file);
            int importedCount = 0;
            while (!ts.atEnd())
            {
                QString line = ts.readLine().trimmed();
                if (line.isEmpty() || line.startsWith('#')) { continue; }
                QStringList parts = line.split('\t');
                if (parts.size() >= 1)
                {
                    QString pattern = parts[0].trimmed();
                    if (pattern.isEmpty()) { continue; }
                    QString typeStr = (parts.size() >= 2) ? parts[1].trimmed() : "清理";
                    if (typeStr == "保留") { engine->AddKeepRule(pattern); }
                    else { engine->AddCleanRule(pattern); }
                    ++importedCount;
                }
            }
            LOGMGR_INFO((*m_logMgr), "MainWindow", "导入规则: %s, 共 %d 条", path.toStdString().c_str(), importedCount);
            refreshAll();
        }
    });
}

void MainWindow::InitSettingsPage()
{
    if (!m_settingsPageWidget)
    {
        return;
    }

    auto* layout = new QVBoxLayout(m_settingsPageWidget);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* form = new QFormLayout();
    form->setSpacing(12);

    auto* cfg = m_presenter->GetConfigManager();

    // 输出目录
    auto* outputEdit = new QLineEdit(m_settingsPageWidget);
    outputEdit->setText(cfg->outputDir);
    outputEdit->setPlaceholderText("默认: 源码目录/../output");
    outputEdit->setMinimumHeight(30);
    form->addRow("打包输出目录:", outputEdit);

    // 包名模板
    auto* nameEdit = new QLineEdit(m_settingsPageWidget);
    nameEdit->setText(cfg->packageNamePattern);
    nameEdit->setPlaceholderText("默认: 项目名_时间戳_source");
    nameEdit->setMinimumHeight(30);
    form->addRow("包名模板:", nameEdit);

    // gitignore 开关
    auto* gitCheck = new QCheckBox("启用 .gitignore 规则", m_settingsPageWidget);
    gitCheck->setChecked(cfg->enableGitIgnore);
    form->addRow("", gitCheck);

    // 排除 VCS 目录开关
    auto* vcsCheck = new QCheckBox("排除版本控制目录 (.git / .svn)", m_settingsPageWidget);
    vcsCheck->setChecked(cfg->excludeVcsDirs);
    form->addRow("", vcsCheck);

    // 自动打包开关
    auto* packCheck = new QCheckBox("清理完成后自动打包", m_settingsPageWidget);
    packCheck->setChecked(cfg->autoPack);
    form->addRow("", packCheck);

    // 7z 路径
    auto* sevenZipLayout = new QHBoxLayout();
    auto* sevenZipEdit = new QLineEdit(m_settingsPageWidget);
    sevenZipEdit->setText(cfg->sevenZipPath);
    sevenZipEdit->setPlaceholderText("自动检测（<7-Zip安装目录>/7z.exe 等）");
    sevenZipEdit->setMinimumHeight(30);
    auto* browse7zBtn = new QPushButton("浏览", m_settingsPageWidget);
    browse7zBtn->setFixedSize(68, 30);
    sevenZipLayout->addWidget(sevenZipEdit, 1);
    sevenZipLayout->addWidget(browse7zBtn);
    form->addRow("7z 路径:", sevenZipLayout);

    layout->addLayout(form);

    // 保存按钮
    auto* saveBtn = new QPushButton("保存设置", m_settingsPageWidget);
    saveBtn->setFixedHeight(34);
    layout->addWidget(saveBtn);

    // 状态提示
    auto* tipLabel = new QLabel(m_settingsPageWidget);
    tipLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(tipLabel);

    // 分隔线
    auto* sepLine = new QFrame(m_settingsPageWidget);
    sepLine->setFrameShape(QFrame::HLine);
    sepLine->setFrameShadow(QFrame::Sunken);
    layout->addWidget(sepLine);

    // 关于软件
    auto* aboutGroup = new QGroupBox("关于软件", m_settingsPageWidget);
    auto* aboutLayout = new QVBoxLayout(aboutGroup);
    aboutLayout->setSpacing(6);

    QString versionStr = QString("版本: V%1 (Build %2)")
        .arg(APP_VERSION, BUILD_DATE);
    auto* verLabel = new QLabel(versionStr, m_settingsPageWidget);
    verLabel->setStyleSheet("font-weight:bold; font-size:13px;");
    aboutLayout->addWidget(verLabel);

    auto* devLabel = new QLabel("开发者: ariesfun", m_settingsPageWidget);
    aboutLayout->addWidget(devLabel);

    auto* descLabel = new QLabel(m_settingsPageWidget);
    descLabel->setWordWrap(true);
    descLabel->setText(QString(
        "CodeCleanTool 是一款面向 C++/Qt/VS/CMake 开发者的源代码清理与打包工具。\n"
        "在交付、归档、外发工程前，自动识别并清理编译产物、IDE 缓存、临时文件等无关内容，"
        "保留核心源码和必要资源，一键生成仅含源码的 7z 压缩包。\n\n"
        "技术栈: C++17 + Qt 5.15.2 + ElaWidgetTools (Fluent UI) + 7z CLI\n"
        "许可证: MIT License"));
    aboutLayout->addWidget(descLabel);

    layout->addWidget(aboutGroup);
    layout->addStretch();

    // 浏览 7z 按钮
    connect(browse7zBtn, &QPushButton::clicked, this, [sevenZipEdit, this]()
    {
        QString path = QFileDialog::getOpenFileName(this, "选择 7z.exe",
            sevenZipEdit->text(), "7z 可执行文件 (7z.exe)");
        if (!path.isEmpty())
        {
            sevenZipEdit->setText(path);
        }
    });

    connect(saveBtn, &QPushButton::clicked, this, [cfg, outputEdit, nameEdit, gitCheck, vcsCheck, packCheck, sevenZipEdit, tipLabel, this]()
    {
        cfg->outputDir = outputEdit->text().trimmed();
        cfg->packageNamePattern = nameEdit->text().trimmed();
        cfg->enableGitIgnore = gitCheck->isChecked();
        cfg->excludeVcsDirs = vcsCheck->isChecked();
        cfg->autoPack = packCheck->isChecked();
        cfg->sevenZipPath = sevenZipEdit->text().trimmed();
        tipLabel->setText("设置已保存");
        LOGMGR_INFO((*m_logMgr), "MainWindow", "用户保存设置: gitignore=%s, vcs排除=%s, 自动打包=%s",
                    cfg->enableGitIgnore ? "开" : "关",
                    cfg->excludeVcsDirs ? "开" : "关",
                    cfg->autoPack ? "开" : "关");
    });
}
