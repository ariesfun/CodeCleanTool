@echo off
rem 运行全部探针测试（Release 配置）
rem 必须设置 Qt bin 到 PATH

set "QT_BIN=<Qt安装目录>\bin"
set "PATH=%QT_BIN%;%PATH%"

cd /d "%~dp0product"
ctest -C Release --output-on-failure --timeout 30
