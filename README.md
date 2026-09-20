# CodeCleanTool

源代码清理与打包工具。面向 C++/Qt/VS/CMake 开发者，在交付、归档、外发前扫描工程目录、清理冗余文件，并自动生成仅含源码的 7z 压缩包。

C++17 · Qt 5.15.2 · ElaWidgetTools (Fluent UI) · CMake 3.16+ · MSVC 143 · 7z CLI

## 功能

- **扫描清理** — 内置 36 条清理规则 + 22 条保留规则，覆盖 IDE 缓存、编译产物、调试文件、临时文件、构建目录；可选排除 `.git`/`.svn`
- **.gitignore 联动** — 读取工程根目录 `.gitignore`，支持 `**/*` 通配与 `!` 反向规则，设置页可开关
- **规则管理** — 清理/保留两区段分组，自定义增删、拖拽排序（同步至匹配优先级）、导入导出
- **预览确认** — 扫描结果按 6 列展示（名称/路径/大小/时间/类型/命中规则），勾选/全选/反选；删除前确认；右侧详情面板
- **一键打包** — 调用 7z 生成纯源码包，自动检测 7z 路径（已知路径 → 注册表 → PATH），可配置输出目录与包名模板
- **瘦身统计** — 环形图对比原项目大小 / 可释放空间 / 清理后剩余
- **设置持久化** — 全部配置与主题偏好保存到程序目录 `config.ini`，启动自动加载
- **深色/浅色主题** — 全局样式表覆盖原生 Qt 控件，Ela 自绘控件跟随主题引擎

## 已知限制

文件列表暂不支持搜索，仅支持点击表头排序。

## 快速开始

```bash
mkdir product && cd product
cmake ../core_code -G "Visual Studio 17 2022" \
    -DCMAKE_PREFIX_PATH=<Qt安装目录>/5.15.2/msvc2019_64
cmake --build . --config Release
"<Qt安装目录>/5.15.2/msvc2019_64/bin/windeployqt.exe" Release/CodeCleanTool.exe --no-translations --no-compiler-runtime
```

首次构建后需运行 `windeployqt` 部署 Qt 依赖，之后双击 `Release/CodeCleanTool.exe` 即可运行。
`make_dist.bat` 可一键完成编译 → 部署 → 生成 7z SFX 自解压 exe。

## 测试

11 个独立探针、259 条断言，覆盖规则匹配、扫描、清理、打包、配置等模块；不启动完整 GUI 即可验证业务逻辑。

```bash
run_tests.bat
cd product && ctest -C Release --output-on-failure --timeout 30
```

## 项目结构

```
core_code/src/
├── app/         View + Presenter（MainWindow / MainPresenter）
├── widgets/     可复用控件（StatsWidget 环形图）
├── scanner/     异步目录扫描
├── rules/       规则引擎 + .gitignore 解析
├── cleaner/     异步文件清理
├── packager/    7z CLI 打包
├── model/       数据模型（ResultModel / LogListModel）
├── config/      配置管理
├── log/         日志管理
└── common/      基础设施（Logger / IniConfig）
```

## 文档

[软件使用说明](docs/2026-06-09_CodeCleanTool_软件使用说明.md) — 安装、界面说明、操作流程与常见问题

## 许可证

MIT License。第三方依赖：ElaWidgetTools (MIT)、Qt 5.15.2 (LGPL v3，动态链接)、7-Zip (LGPL，外部 CLI 调用)。详见 [LICENSE](LICENSE)。
