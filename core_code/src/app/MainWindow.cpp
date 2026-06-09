// 主窗口实现（View 层）：UI 布局 + 信号转发给 MainPresenter
#include "MainWindow.h"
#include "MainPresenter.h"

#include "ElaApplication.h"
#include "ElaDockWidget.h"
#include "ElaMenu.h"
#include "ElaProgressBar.h"
#include "ElaStatusBar.h"
#include "ElaText.h"
#include "ElaTheme.h"
#include "ElaToolButton.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QTableView>
#include <QVBoxLayout>

#include "core/ConfigManager.h"
#include "core/LogListModel.h"
#include "core/LogManager.h"
#include "core/ResultModel.h"
#include "core/RuleEngine.h"
#include "Logger.h"

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
    auto* selectAllBtn = new QPushButton("全选", scanPage);
    selectAllBtn->setMinimumHeight(32);
    selectAllBtn->setFixedWidth(50);
    auto* deselectAllBtn = new QPushButton("反选", scanPage);
    deselectAllBtn->setMinimumHeight(32);
    deselectAllBtn->setFixedWidth(50);
    pathBar->addWidget(m_pathEdit, 1);
    pathBar->addWidget(m_browseBtn);
    pathBar->addWidget(selectAllBtn);
    pathBar->addWidget(deselectAllBtn);
    scanLayout->addLayout(pathBar);

    // 全选/反选 — lambda 延迟访问模型（InitPresenter 才设置）
    connect(selectAllBtn, &QPushButton::clicked, this, [this]()
    {
        auto* model = qobject_cast<ResultModel*>(m_fileTable->model());
        if (!model) { return; }
        for (int i = 0; i < model->TotalCount(); ++i)
            model->setData(model->index(i, ResultModel::ColName), Qt::Checked, Qt::CheckStateRole);
    });
    connect(deselectAllBtn, &QPushButton::clicked, this, [this]()
    {
        auto* model = qobject_cast<ResultModel*>(m_fileTable->model());
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
        }
    });
}

void MainWindow::InitPresenter()
{
    // 创建控制层
    m_presenter = new MainPresenter(this);
    m_presenter->Init();

    // 绑定数据模型到表格
    m_fileTable->setModel(m_presenter->GetResultModel());
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_fileTable->setColumnWidth(0, 180);
    m_fileTable->setColumnWidth(1, 300);
    m_fileTable->setColumnWidth(2, 80);
    m_fileTable->setColumnWidth(3, 150);
    m_fileTable->setColumnWidth(4, 120);

    // Presenter UI 信号 → View 控件更新
    connect(m_presenter, &MainPresenter::StatusChanged, m_statusText, &ElaText::setText);
    connect(m_presenter, &MainPresenter::StatsChanged, m_statText, &ElaText::setText);
    connect(m_presenter, &MainPresenter::ProgressChanged, this, [this](int percent, bool visible)
    {
        m_progressBar->setValue(percent);
        m_progressBar->setVisible(visible);
    });
}

void MainWindow::OnScan()
{
    m_presenter->OnScan(m_pathEdit->text());
}

void MainWindow::OnClean()
{
    m_presenter->OnClean();
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
    auto makeFilterBtn = [&](const QString& text, int w = 50) {
        auto* btn = new QPushButton(text, foundPage);
        btn->setFixedSize(w, 26);
        filterBar->addWidget(btn);
        return btn;
    };
    auto* allBtn = makeFilterBtn("全部");
    auto* infoBtn = makeFilterBtn("INFO");
    auto* warnBtn = makeFilterBtn("WARN");
    auto* errorBtn = makeFilterBtn("ERROR", 55);
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
    });

    // 导出按钮
    connect(exportBtn, &QPushButton::clicked, this, [this]()
    {
        QString path = QFileDialog::getSaveFileName(this, "导出日志", "codecleantool_export.log",
                                                     "日志文件 (*.log *.txt)");
        if (!path.isEmpty())
        {
            m_logMgr->ExportToFile(path);
        }
    });
}

void MainWindow::InitRulesPage()
{
    if (!m_rulesPageWidget)
    {
        return;
    }

    auto* layout = new QVBoxLayout(m_rulesPageWidget);
    layout->setContentsMargins(8, 8, 8, 8);

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
    addBtn->setFixedSize(60, 30);
    addBar->addWidget(patternEdit, 1);
    addBar->addWidget(typeCombo);
    addBar->addWidget(addBtn);
    layout->addLayout(addBar);

    // 规则表格
    auto* ruleTable = new QTableWidget(m_rulesPageWidget);
    ruleTable->setColumnCount(4);
    ruleTable->setHorizontalHeaderLabels({"模式", "类型", "目录规则", "操作"});
    ruleTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ruleTable->setAlternatingRowColors(true);
    ruleTable->horizontalHeader()->setStretchLastSection(true);
    ruleTable->setColumnWidth(0, 200);
    ruleTable->setColumnWidth(1, 80);
    ruleTable->setColumnWidth(2, 70);
    ruleTable->verticalHeader()->setVisible(false);
    ruleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(ruleTable, 1);

    auto* engine = m_presenter->GetRuleEngine();

    // 刷新规则表格
    auto refreshTable = [ruleTable, engine]()
    {
        ruleTable->setRowCount(0);
        if (!engine) { return; }
        auto rules = engine->GetRules();
        for (int i = 0; i < rules.size(); ++i)
        {
            const auto& r = rules[i];
            ruleTable->insertRow(i);
            ruleTable->setItem(i, 0, new QTableWidgetItem(r.pattern));
            ruleTable->setItem(i, 1, new QTableWidgetItem(r.type == RuleType::Clean ? "清理" : "保留"));
            ruleTable->setItem(i, 2, new QTableWidgetItem(r.isDirRule ? "是" : "否"));

            auto* delBtn = new QPushButton("删除");
            delBtn->setFixedSize(45, 24);
            ruleTable->setCellWidget(i, 3, delBtn);

            // 删除按钮：捕获 delBtn 指针定位行，移除后直接 removeRow
            QObject::connect(delBtn, &QPushButton::clicked, [ruleTable, engine, delBtn]()
            {
                for (int row = 0; row < ruleTable->rowCount(); ++row)
                {
                    if (ruleTable->cellWidget(row, 3) == delBtn)
                    {
                        engine->RemoveRule(row);
                        ruleTable->removeRow(row);
                        break;
                    }
                }
            });
        }
    };
    refreshTable();

    // 添加按钮
    QObject::connect(addBtn, &QPushButton::clicked, [patternEdit, typeCombo, engine, refreshTable]()
    {
        QString pattern = patternEdit->text().trimmed();
        if (pattern.isEmpty()) return;
        auto type = static_cast<RuleType>(typeCombo->currentData().toInt());
        if (type == RuleType::Clean)
            engine->AddCleanRule(pattern);
        else
            engine->AddKeepRule(pattern);
        patternEdit->clear();
        refreshTable();
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

    // 自动打包开关
    auto* packCheck = new QCheckBox("清理完成后自动打包", m_settingsPageWidget);
    packCheck->setChecked(cfg->autoPack);
    form->addRow("", packCheck);

    layout->addLayout(form);

    // 保存按钮
    auto* saveBtn = new QPushButton("保存设置", m_settingsPageWidget);
    saveBtn->setFixedHeight(34);
    layout->addWidget(saveBtn);

    // 状态提示
    auto* tipLabel = new QLabel(m_settingsPageWidget);
    tipLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(tipLabel);
    layout->addStretch();

    connect(saveBtn, &QPushButton::clicked, this, [cfg, outputEdit, nameEdit, gitCheck, packCheck, tipLabel]()
    {
        cfg->outputDir = outputEdit->text().trimmed();
        cfg->packageNamePattern = nameEdit->text().trimmed();
        cfg->enableGitIgnore = gitCheck->isChecked();
        cfg->autoPack = packCheck->isChecked();
        tipLabel->setText("设置已保存");
    });
}
