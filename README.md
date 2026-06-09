# CodeCleanTool

源代码清理与打包工具。面向 C++/Qt/VS/CMake 开发者，在交付、归档、外发前对工程目录扫描、筛选、清理，只保留核心源码与必要资源，并自动生成仅含源码的 7z 压缩包。

## 功能

- **规则扫描** — 内置 IDE 缓存、编译产物、调试文件、临时文件等清理规则
- **.gitignore 联动** — 读取并应用项目 .gitignore 排除规则
- **预览确认** — 删除前展示完整文件列表，支持勾选/全选/反选
- **一键打包** — 清理后自动调用 7z 生成纯源码压缩包
- **深色/浅色主题** — ElaWidgetTools Fluent UI 风格

## 技术栈

C++17 · Qt 5.15.2 · ElaWidgetTools · CMake · MSVC 143 · 7z CLI

## 构建

```bash
mkdir -p product && cd product
cmake ../core_code -G "Visual Studio 17 2022" \
    -DCMAKE_PREFIX_PATH=<Qt安装目录>
cmake --build . --config Debug
```

## 文档

- [PRD + 技术方案](docs/项目%20PRD%20+%20技术方案版.md)
- [开发需求](docs/开发需求.md)
