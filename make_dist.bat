@echo off
rem CodeCleanTool 发布打包脚本
rem 1. 编译 Release 版本
rem 2. windeployqt 部署 Qt 依赖
rem 3. 生成 7z 自解压 exe

setlocal
set "PRODUCT_DIR=%~dp0product"
set "DIST_DIR=%~dp0dist"
set "RELEASE_DIR=%PRODUCT_DIR%\Release"
set "QT_BIN=<Qt安装目录>\bin"
set "SZ_PATH=<7-Zip安装目录>\7z.exe"
set "SFX_MODULE=<7-Zip安装目录>\7z.sfx"
set "VERSION=V1.0.0"

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
