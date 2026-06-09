@echo off
rem 启动 CodeCleanTool Release 版本（Ela 框架推荐 Release x64 以获得最佳性能）
rem 必须设置 Qt bin 到 PATH，否则提示找不到 Qt5Core.dll

set "QT_BIN=<Qt安装目录>\bin"
set "PATH=%QT_BIN%;%PATH%"

cd /d "%~dp0product\Release"
start "" CodeCleanTool.exe
