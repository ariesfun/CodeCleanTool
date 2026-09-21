@echo off
rem 本文件为 UTF-8 编码。cmd.exe 默认用 OEM 代码页读取批处理，
rem 会把中文字节误读并导致命令被截断，故先切到 65001(UTF-8)。
rem 这同时保证下方 echo 写入 sfx_config.txt 的是 UTF-8 字节，
rem 与配置头 ;!@Install@!UTF-8! 一致，安装包中文才不会乱码。
chcp 65001 >nul

rem CodeCleanTool 发布打包脚本
rem 1. 编译 Release 版本
rem 2. windeployqt 部署 Qt 依赖
rem 3. 生成 7z 自解压 exe
rem
rem 路径自动探测，无需手改。可用环境变量覆盖：
rem   QT_SDK_DIR      Qt SDK 根目录（其下的 bin 需含 windeployqt.exe）
rem   SEVENZIP_DIR    7-Zip 安装目录（需含 7z.exe 与 7z.sfx）

setlocal
set "PRODUCT_DIR=%~dp0product"
set "DIST_DIR=%~dp0dist"
set "RELEASE_DIR=%PRODUCT_DIR%\Release"
set "VERSION=V0.1.0"

rem ---- Qt 路径 ----
rem 优先级：环境变量 QT_SDK_DIR > 构建时记录的 qt_root.txt > CMake 缓存的
rem         CMAKE_PREFIX_PATH > PATH 中的 windeployqt
rem 注意：同一台机器可能装有多个 Qt 版本，直接按 PATH 查找极易选到版本不匹配的
rem       windeployqt，部署出的程序运行期会缺 DLL。故优先采用构建时实际使用的 Qt。
set "QT_BIN="
if defined QT_SDK_DIR set "QT_BIN=%QT_SDK_DIR%\bin"
if not defined QT_BIN if exist "%PRODUCT_DIR%\qt_root.txt" for /f "usebackq delims=" %%A in ("%PRODUCT_DIR%\qt_root.txt") do set "QT_BIN=%%A\bin"
if not defined QT_BIN if exist "%PRODUCT_DIR%\CMakeCache.txt" for /f "tokens=2 delims==" %%A in ('findstr /b /c:"CMAKE_PREFIX_PATH:" "%PRODUCT_DIR%\CMakeCache.txt"') do set "QT_BIN=%%A\bin"
rem PATH 兜底：先取 windeployqt.exe 全路径，再取其所在目录
if not defined QT_BIN for %%I in (windeployqt.exe) do set "QT_FALLBACK=%%~$PATH:I"
if not defined QT_BIN if defined QT_FALLBACK for %%I in ("%QT_FALLBACK%") do set "QT_BIN=%%~dpI"
rem 去掉末尾反斜杠，便于后续拼接
if defined QT_BIN if "%QT_BIN:~-1%"=="\" set "QT_BIN=%QT_BIN:~0,-1%"
if not defined QT_BIN (
    echo [错误] 未找到 Qt。请先执行一次 CMake 配置以生成 product\CMakeCache.txt，
    echo        或设置环境变量 QT_SDK_DIR 指向 Qt SDK 根目录。
    exit /b 1
)
if not exist "%QT_BIN%\windeployqt.exe" (
    echo [错误] 在 %QT_BIN% 下未找到 windeployqt.exe。
    echo        该目录可能不是有效的 Qt SDK，请检查配置或改设 QT_SDK_DIR。
    exit /b 1
)

rem ---- 7-Zip 路径：环境变量优先，其次常见安装位置，最后 PATH ----
set "SZ_PATH="
set "SFX_MODULE="
if defined SEVENZIP_DIR (
    set "SZ_PATH=%SEVENZIP_DIR%\7z.exe"
    set "SFX_MODULE=%SEVENZIP_DIR%\7z.sfx"
)
if not exist "%SZ_PATH%" if exist "C:\Program Files\7-Zip\7z.exe" (
    set "SZ_PATH=C:\Program Files\7-Zip\7z.exe"
    set "SFX_MODULE=C:\Program Files\7-Zip\7z.sfx"
)
if not exist "%SZ_PATH%" if exist "C:\Program Files (x86)\7-Zip\7z.exe" (
    set "SZ_PATH=C:\Program Files (x86)\7-Zip\7z.exe"
    set "SFX_MODULE=C:\Program Files (x86)\7-Zip\7z.sfx"
)
rem 7-Zip 安装路径也登记在注册表（Packager 运行时同样用此方式探测）
if not exist "%SZ_PATH%" (
    for /f "tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\7-Zip" /v Path 2^>nul') do set "SZ_PATH=%%B7z.exe"
)
if not exist "%SZ_PATH%" (
    for /f "tokens=2*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\7-Zip" /v Path 2^>nul') do set "SZ_PATH=%%B7z.exe"
)
if not exist "%SZ_PATH%" (
    for %%I in (7z.exe) do set "SZ_PATH=%%~$PATH:I"
)
rem 由 7z.exe 所在目录推导 7z.sfx
if not exist "%SFX_MODULE%" if exist "%SZ_PATH%" (
    for %%I in ("%SZ_PATH%") do set "SFX_MODULE=%%~dpI7z.sfx"
)
if not exist "%SZ_PATH%" (
    echo [错误] 未找到 7z.exe。
    echo        请设置环境变量 SEVENZIP_DIR 指向 7-Zip 安装目录，或将 7-Zip 加入 PATH。
    exit /b 1
)
if not exist "%SFX_MODULE%" (
    echo [错误] 未找到 7z.sfx: %SFX_MODULE%
    echo        该文件随 7-Zip 一同安装，请检查安装是否完整。
    exit /b 1
)

echo Qt bin : %QT_BIN%
echo 7-Zip  : %SZ_PATH%
echo.

echo [1/4] Building Release...
cmake --build "%PRODUCT_DIR%" --config Release --target CodeCleanTool
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [2/4] Deploying Qt DLLs...
"%QT_BIN%\windeployqt.exe" "%RELEASE_DIR%\CodeCleanTool.exe" --no-translations --no-compiler-runtime
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo [3/4] Creating dist directory...
if exist "%DIST_DIR%\CodeCleanTool" rd /s /q "%DIST_DIR%\CodeCleanTool"
mkdir "%DIST_DIR%\CodeCleanTool\platforms"
mkdir "%DIST_DIR%\CodeCleanTool\styles"
mkdir "%DIST_DIR%\CodeCleanTool\imageformats"
mkdir "%DIST_DIR%\CodeCleanTool\iconengines"

copy "%RELEASE_DIR%\CodeCleanTool.exe" "%DIST_DIR%\CodeCleanTool\" >nul
copy "%RELEASE_DIR%\Qt5Core.dll" "%DIST_DIR%\CodeCleanTool\" >nul
copy "%RELEASE_DIR%\Qt5Gui.dll" "%DIST_DIR%\CodeCleanTool\" >nul
copy "%RELEASE_DIR%\Qt5Widgets.dll" "%DIST_DIR%\CodeCleanTool\" >nul
copy "%RELEASE_DIR%\Qt5Svg.dll" "%DIST_DIR%\CodeCleanTool\" >nul
copy "%RELEASE_DIR%\libEGL.dll" "%DIST_DIR%\CodeCleanTool\" >nul
copy "%RELEASE_DIR%\libGLESv2.dll" "%DIST_DIR%\CodeCleanTool\" >nul
copy "%RELEASE_DIR%\D3Dcompiler_47.dll" "%DIST_DIR%\CodeCleanTool\" >nul
copy "%RELEASE_DIR%\opengl32sw.dll" "%DIST_DIR%\CodeCleanTool\" >nul
copy "%RELEASE_DIR%\platforms\qwindows.dll" "%DIST_DIR%\CodeCleanTool\platforms\" >nul
copy "%RELEASE_DIR%\styles\qwindowsvistastyle.dll" "%DIST_DIR%\CodeCleanTool\styles\" >nul
copy "%RELEASE_DIR%\imageformats\*.dll" "%DIST_DIR%\CodeCleanTool\imageformats\" >nul
copy "%RELEASE_DIR%\iconengines\*.dll" "%DIST_DIR%\CodeCleanTool\iconengines\" >nul

rem 复制使用手册
mkdir "%DIST_DIR%\CodeCleanTool\docs" 2>nul
copy "%~dp0docs\2026-06-09_CodeCleanTool_软件使用说明.md" "%DIST_DIR%\CodeCleanTool\docs\" >nul

echo [4/4] Creating 7z SFX package...

rem 生成 SFX 配置文件
rem 注意：本文件为 UTF-8 编码，且此处声明 ;!@Install@!UTF-8!，
rem       故下方 echo 输出的中文必须是 UTF-8 字节，切勿把本 bat 另存为 GBK
(
echo ;!@Install@!UTF-8!
echo Title="CodeCleanTool %VERSION% — 源代码清理与打包工具"
echo BeginPrompt="是否安装并运行 CodeCleanTool %VERSION%？"
echo RunProgram="CodeCleanTool.exe"
echo ;!@InstallEnd@!
) > "%TEMP%\sfx_config.txt"

rem 创建 7z 压缩
"%SZ_PATH%" a -mx5 "%TEMP%\CodeCleanTool.7z" "%DIST_DIR%\CodeCleanTool\*" -r >nul

rem 合并为自解压 exe
copy /b "%SFX_MODULE%" + "%TEMP%\sfx_config.txt" + "%TEMP%\CodeCleanTool.7z" "%DIST_DIR%\CodeCleanTool_%VERSION%.exe" >nul

del "%TEMP%\sfx_config.txt" "%TEMP%\CodeCleanTool.7z"

echo.
echo ============================================
echo  Done: %DIST_DIR%\CodeCleanTool_%VERSION%.exe
echo ============================================
endlocal
