// 程序入口：初始化 Qt 应用、ElaWidgetTools 主题引擎、日志系统，启动主窗口
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>

#include "ElaApplication.h"
#include "app/MainWindow.h"
#include "log/LogManager.h"

int main(int argc, char* argv[])
{
    // 高 DPI 适配（Qt 5.6 ~ 5.14 兼容）
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    QGuiApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#endif

    QApplication app(argc, argv);

    // 初始化 Fluent UI 主题引擎（必须在窗口创建前调用）
    eApp->init();

    // 初始化日志系统（文件输出 + 控制台 + DebugOutput + UI 信号）
    LogManager logMgr;
    logMgr.Init("logs", "code-clean-tool.log");
    // 使用 LOGMGR_INFO 宏确保日志定位到真实的 main.cpp 调用点
    LOGMGR_INFO(logMgr, "main", "CodeCleanTool 启动");

    // 创建并显示主窗口，传入 LogManager 供日志页使用
    MainWindow window(&logMgr);
    window.show();
    LOGMGR_INFO(logMgr, "main", "主窗口已显示");

    // 进入 Qt 事件循环
    int ret = app.exec();
    LOGMGR_INFO(logMgr, "main", "CodeCleanTool 退出, 返回码: %d", ret);
    return ret;
}
