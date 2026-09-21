@echo off
rem 本文件为 UTF-8 编码，含中文提示，故先切到 65001(UTF-8) 代码页。
rem cmd.exe 默认按 OEM 代码页读取批处理，会把中文字节误读并截断命令。
chcp 65001 >nul

rem 启动 CodeCleanTool Release 版本（Ela 框架推荐 Release x64 以获得最佳性能）
rem
rem 正常情况下 windeployqt 已把 Qt DLL 放到 exe 同级，加载时优先命中那一份；
rem 但若尚未部署，就会回落到 PATH 上碰到的第一个 Qt —— 本机装有多版本时
rem 会加载到版本不匹配的 Qt5Core.dll，程序以 0xc0000139(找不到入口点) 退出。
rem 故这里按构建时实际使用的 Qt 显式指定 PATH，行为可预期。
rem
rem 路径自动探测，无需手改。可用环境变量 QT_SDK_DIR 覆盖。

setlocal
set "PRODUCT_DIR=%~dp0product"
set "RELEASE_DIR=%PRODUCT_DIR%\Release"
set "EXE_PATH=%RELEASE_DIR%\CodeCleanTool.exe"

if not exist "%EXE_PATH%" (
    echo [错误] 未找到 %EXE_PATH%
    echo        请先编译 Release：
    echo            mkdir product ^&^& cd product
    echo            cmake ..\core_code -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH=^<Qt安装目录^>
    echo            cmake --build . --config Release
    exit /b 1
)

rem ---- Qt 路径 ----
rem 优先级：环境变量 QT_SDK_DIR > 构建时记录的 qt_root.txt > CMake 缓存的 CMAKE_PREFIX_PATH
rem 与 make_dist.bat / run_tests.bat 保持一致。
set "QT_BIN="
if defined QT_SDK_DIR set "QT_BIN=%QT_SDK_DIR%\bin"
if not defined QT_BIN if exist "%PRODUCT_DIR%\qt_root.txt" for /f "usebackq delims=" %%A in ("%PRODUCT_DIR%\qt_root.txt") do set "QT_BIN=%%A\bin"
if not defined QT_BIN if exist "%PRODUCT_DIR%\CMakeCache.txt" for /f "tokens=2 delims==" %%A in ('findstr /b /c:"CMAKE_PREFIX_PATH:" "%PRODUCT_DIR%\CMakeCache.txt"') do set "QT_BIN=%%A\bin"
rem 去掉末尾反斜杠，便于后续拼接
if defined QT_BIN if "%QT_BIN:~-1%"=="\" set "QT_BIN=%QT_BIN:~0,-1%"

rem 找不到 Qt 不算致命：exe 同级若已部署 Qt DLL，双击也能跑起来，故只提示不拦截
if defined QT_BIN (
    set "PATH=%QT_BIN%;%PATH%"
    echo Qt bin : %QT_BIN%
) else (
    echo [提示] 未定位到 Qt 根目录，将依赖 exe 同级的 Qt DLL。
    echo        若启动报缺 DLL，请先跑 windeployqt 或设置环境变量 QT_SDK_DIR。
)

cd /d "%RELEASE_DIR%"
start "" CodeCleanTool.exe
