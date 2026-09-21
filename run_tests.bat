@echo off
rem 本文件为 UTF-8 编码，含中文提示，故先切到 65001(UTF-8) 代码页。
rem cmd.exe 默认按 OEM 代码页读取批处理，会把中文字节误读并截断命令。
chcp 65001 >nul

rem 运行全部探针测试（Release 配置）
rem
rem 探针链接的是构建时使用的那个 Qt，必须把对应版本的 bin 放到 PATH 最前面。
rem 本机若装有多个 Qt 版本而任由 PATH 解析，会加载到版本不匹配的 Qt5Core.dll，
rem 表现为每个探针都以 0xc0000139（找不到入口点）退出。
rem
rem 路径自动探测，无需手改。可用环境变量 QT_SDK_DIR 覆盖。

setlocal
set "PRODUCT_DIR=%~dp0product"

rem ---- Qt 路径 ----
rem 优先级：环境变量 QT_SDK_DIR > 构建时记录的 qt_root.txt > CMake 缓存的 CMAKE_PREFIX_PATH
rem 与 make_dist.bat 保持一致：优先采用构建时实际使用的 Qt，而非 PATH 里碰到的那个。
set "QT_BIN="
if defined QT_SDK_DIR set "QT_BIN=%QT_SDK_DIR%\bin"
if not defined QT_BIN if exist "%PRODUCT_DIR%\qt_root.txt" for /f "usebackq delims=" %%A in ("%PRODUCT_DIR%\qt_root.txt") do set "QT_BIN=%%A\bin"
if not defined QT_BIN if exist "%PRODUCT_DIR%\CMakeCache.txt" for /f "tokens=2 delims==" %%A in ('findstr /b /c:"CMAKE_PREFIX_PATH:" "%PRODUCT_DIR%\CMakeCache.txt"') do set "QT_BIN=%%A\bin"
rem 去掉末尾反斜杠，便于后续拼接
if defined QT_BIN if "%QT_BIN:~-1%"=="\" set "QT_BIN=%QT_BIN:~0,-1%"

if not defined QT_BIN (
    echo [错误] 未找到 Qt。请先执行一次 CMake 配置以生成 product\CMakeCache.txt，
    echo        或设置环境变量 QT_SDK_DIR 指向 Qt SDK 根目录。
    exit /b 1
)
if not exist "%QT_BIN%\Qt5Core.dll" (
    echo [错误] 在 %QT_BIN% 下未找到 Qt5Core.dll。
    echo        该目录可能不是有效的 Qt SDK，请检查配置或改设 QT_SDK_DIR。
    exit /b 1
)

echo Qt bin : %QT_BIN%
set "PATH=%QT_BIN%;%PATH%"

cd /d "%PRODUCT_DIR%"
rem 超时给到 60 秒：打包探针内部对 7z 有 15~20 秒的等待上限，30 秒会误判为超时
ctest -C Release --output-on-failure --timeout 60
