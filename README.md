# CodeCleanTool

源代码清理与打包工具。面向 C++/Qt/VS/CMake 开发者，在交付、归档、外发前对工程目录扫描、筛选、清理，只保留核心源码与必要资源，自动生成仅含源码的 7z 压缩包。

**版本：** V1.0.0 | **构建：** CMake 编译日期自动嵌入

## 功能

- **智能扫描** — 内置 36 条清理规则 + 22 条保留规则（含 *.sln），覆盖 IDE 缓存、编译产物、调试文件、临时文件、构建目录；可配置排除 .git/.svn 版本控制目录；扫描完成后展示瘦身统计环形图（原项目大小 / 可清理大小 / 清理后剩余）
- **.gitignore 联动** — 读取并应用项目根目录的 .gitignore 排除规则，支持 `**/*` 通配、`!` 反向规则，设置页可开关
- **规则管理** — 两区段分组展示（清理规则上 / 保留规则下），标题显示规则条数；自定义添加/移除规则，拖拽排序（同步到引擎优先级），导入/导出 tab 分隔 .txt，规则类型颜色标识（清理=红色，保留=绿色）
- **类型颜色标签** — 文件表格新增"类型"列，6 色分类标识（IDE缓存/编译产物/调试文件/临时文件/构建目录/其他），排序优先清理目标
- **预览确认** — 扫描结果以 6 列表格展示（名称/路径/大小/时间/类型/命中规则），支持勾选/全选/反选；删除前弹出主题确认框；右侧详情面板随选中项展示完整字段
- **一键打包** — 调用 7z 生成纯源码 .7z 压缩包，7z 路径自动检测（已知路径 + 注册表 + PATH），设置页可指定 7z 路径、打包输出目录与包名模板（`%Project`/`%YYYY`/`%MM`/`%DD`/`%HH`/`%MM`/`%SS` 占位符）
- **清理后自动打包** — 可选"清理完成后自动打包"开关，打包后自动打开输出目录
- **瘦身统计环形图** — 扫描完成后展示甜甜圈环形图，直观对比原项目大小、可释放空间、清理后剩余，中心显示可瘦身百分比
- **操作完成通知** — 扫描/清理/打包完成后顶部 ElaMessageBar 通知条（自动消失），清理确认用 ElaContentDialog 主题弹窗
- **日志记录** — 级别过滤（INFO/WARN/ERROR），ERROR 红色 WARN 橙色着色，导出/清空
- **深色/浅色主题** — ElaWidgetTools Fluent UI 风格，qApp 全局样式表覆盖原生 Qt 控件（表格/编辑框/按钮/标签等），Ela 自绘控件自动跟随 ElaTheme
- **设置持久化** — 输出目录、包名模板、7z 路径、各类开关与主题偏好统一保存到**程序目录的 `config.ini`**，启动时自动加载；便携式分发下配置随程序目录走，删除该文件即可恢复默认值
- **关于软件** — 设置页显示版本号(Build 日期)、开发者、功能简介、技术栈、许可证
- **控制台无黑窗** — WIN32 子系统，纯 GUI 应用

## 已知限制

| 能力 | 现状 |
|------|------|
| 文件列表搜索 | 尚未实现，扫描结果表格仅支持点击表头排序 |

其余已知限制（GitIgnoreParser 未完全对齐 git 语义、FileCleaner 将目录计为 1 项、多 `exec()` 调用非最佳实践等）见[软件测试报告](docs/产品文档/2026-06-09_CodeCleanTool_软件测试报告.md)第七节。

## 技术栈

C++17 · Qt 5.15.2 · ElaWidgetTools (Fluent UI) · CMake 3.16+ · MSVC 143 · 7z CLI

## 快速开始

### 构建

```bash
mkdir -p product && cd product
cmake ../core_code -G "Visual Studio 17 2022" \
    -DCMAKE_PREFIX_PATH=<Qt安装目录>/5.15.2/msvc2019_64
cmake --build . --config Release
```

### 部署

```bash
"<Qt安装目录>/5.15.2/msvc2019_64/bin/windeployqt.exe" Release/CodeCleanTool.exe --no-translations --no-compiler-runtime
```

### 运行

双击 `Release/CodeCleanTool.exe` 或执行 `run_release.bat`

### 打包发布

```bash
make_dist.bat     # 一键编译 → 部署 → 生成 7z SFX 自解压 exe
```

### 测试

```bash
run_tests.bat                   # 运行全部探针
cd product && ctest -C Release --output-on-failure --timeout 30
ctest -R RuleEngine -C Release  # 运行单个模块探针
```

## 探针测试

| 模块 | 断言 | 状态 |
|------|------|------|
| RuleEngine | 26 | ✅ |
| RuleOrder | 32 | ✅ |
| GitIgnoreParser | 21 | ✅ |
| GitIgnoreMatcher | 14 | ✅ |
| FileCleaner_File | 14 | ✅ |
| FileCleaner_Dir | 18 | ✅ |
| ConfigManager | 37 | ✅ |
| ScanManager | 59 | ✅ |
| Packager | 13（另 1 项无 7z 时跳过） | ✅ |
| OutputName | 18 | ✅ |
| **合计** | **252** | **100%** |

## 项目结构

```
CodeCleanTool-Repo/
├── core_code/src/
│   ├── app/              # View + Presenter 层 (MainWindow / MainPresenter)
│   ├── widgets/          # 可复用控件（StatsWidget 环形图）
│   ├── scanner/          # 异步目录扫描 (ScanManager)
│   ├── rules/            # 规则引擎 + .gitignore (RuleEngine / GitIgnoreParser)
│   ├── cleaner/          # 异步文件清理 (FileCleaner)
│   ├── packager/         # 7z CLI 打包 (Packager)
│   ├── model/            # 数据模型 (ResultModel / LogListModel)
│   ├── config/           # 配置管理 (ConfigManager)
│   ├── log/              # 日志管理 (LogManager)
│   └── common/           # 基础设施（Logger / IniConfig）
├── tests/                # 探针测试（10 模块）
├── dist/                 # 发布包 (CodeCleanTool_V1.0.0.exe + CodeCleanTool/ 便携目录)
├── docs/                 # 产品文档 + 使用手册
├── make_dist.bat         # 一键打包脚本
├── run_release.bat       # 启动脚本
├── run_tests.bat         # 测试脚本
├── LICENSE               # MIT + 第三方依赖许可
└── README.md
```

## 文档

- [PRD + 技术方案](docs/产品文档/2026-06-09_CodeCleanTool_PRD.md)
- [架构设计](docs/产品文档/2026-06-09_CodeCleanTool_架构设计.md)
- [软件使用说明](docs/2026-06-09_CodeCleanTool_软件使用说明.md)
- [软件技术报告](docs/产品文档/2026-06-09_CodeCleanTool_软件技术报告.md)
- [软件测试报告](docs/产品文档/2026-06-09_CodeCleanTool_软件测试报告.md)
- [Ela框架使用及关键技术报告](docs/产品文档/2026-06-09_CodeCleanTool_Ela框架使用及关键技术报告.md)
- [Qt5 核心技术调研](docs/产品文档/2026-06-09_CodeCleanTool_Qt5技术调研报告.md)
- [产品功能现状](docs/产品文档/2026-06-09_CodeCleanTool_产品功能现状.md)
- [开发需求](docs/2026-06-09_开发需求.md)

## 许可证

本项目采用 MIT License。第三方依赖：
- **ElaWidgetTools** — MIT License (Copyright 2024 Liniyous)
- **Qt 5.15.2** — LGPL v3（动态链接）
- **7-Zip** — LGPL（外部 CLI 调用）

详见 [LICENSE](LICENSE)
