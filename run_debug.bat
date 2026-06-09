@echo off
rem 启动 CodeCleanTool Debug 版本
rem 必须设置 Qt bin 到 PATH，否则提示找不到 Qt5Cored.dll

set "QT_BIN=<Qt安装目录>\bin"
set "PATH=%QT_BIN%;%PATH%"

cd /d "%~dp0product\Debug"
start "" CodeCleanTool.exe
