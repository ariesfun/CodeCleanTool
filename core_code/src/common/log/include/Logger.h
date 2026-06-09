#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// Logger.h
// 跨平台日志模块（支持 Windows VS2015 + 国防版麒麟V10 GCC C++14）
//
// 特性：
//   - 单例模式，全局唯一日志实例
//   - 线程安全（std::mutex）
//   - 支持日志级别过滤：DEBUG / INFO / WARN / ERROR / FATAL
//   - 同时输出到文件（按日期自动切割）和控制台
//   - Windows 下额外输出到 OutputDebugString（CLion/VS 调试窗口可见）
//   - 通过宏调用，自动携带文件名、行号、函数名
//
// 用法：
//   // 在 main 或初始化处调用一次
//   Logger::instance().init("logs/app.log", LogLevel::DEBUG);
//
//   LOG_INFO("系统启动成功");
//   LOG_DEBUG("变量值 x={}", 42);   // 支持 printf 风格格式化 ： Logger 不支持 {} 格式化 TODO待优化
//   LOG_ERROR_("加载失败: {}", errMsg);
//   日志宏调用（任何 .cpp 文件中，引入头文件即可用）：
//   LOG_DEBUG("变量 x=%d", x);
//   LOG_INFO("模块初始化完成");
//   LOG_WARN("配置文件不存在，使用默认值");
//   LOG_ERROR_("连接失败，错误码=%d", errCode);
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <mutex>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstdarg>
#include <ctime>

// ── 日志级别枚举 ──────────────────────────────────────────────────────────────
enum class LogLevel : int
{
    DEBUG = 0,  // 调试信息（开发阶段详细输出）
    INFO  = 1,  // 普通信息
    WARN  = 2,  // 警告（程序可继续运行）
    ERROR_ = 3,  // 错误（功能受损但不崩溃）
    FATAL = 4,  // 致命错误（程序即将退出）
    OFF   = 5   // 关闭所有日志输出
};

// ── 便捷宏定义（自动注入文件名/行号/函数名） ─────────────────────────────────
#define LOG_DEBUG(fmt, ...) \
    Logger::instance().log(LogLevel::DEBUG, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    Logger::instance().log(LogLevel::INFO,  __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    Logger::instance().log(LogLevel::WARN,  __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    Logger::instance().log(LogLevel::ERROR_, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...) \
    Logger::instance().log(LogLevel::FATAL, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__)

// ─────────────────────────────────────────────────────────────────────────────
// Logger 类
// ─────────────────────────────────────────────────────────────────────────────
class Logger
{
public:
    // 获取全局单例
    static Logger& instance();

    // ── 初始化 ────────────────────────────────────────────────────────────────
    // logFilePath : 日志文件路径，如 "logs/app.log"
    //               目录不存在时自动创建
    //               实际写入文件为 "logs/app_2024-01-01.log"（按日期切割）
    // minLevel    : 最低输出级别，低于此级别的日志会被丢弃
    // enableConsole: 是否同时输出到标准输出（控制台）
    void init(const std::string& logFilePath,
              LogLevel minLevel     = LogLevel::DEBUG,
              bool enableConsole    = true);

    // ── 核心写日志接口（不建议直接调用，请使用宏） ───────────────────────────
    void log(LogLevel level,
             const char* file,
             int line,
             const char* func,
             const char* fmt, ...);

    // ── 运行时动态调整日志级别 ────────────────────────────────────────────────
    void setLevel(LogLevel level);
    LogLevel getLevel() const;

    // ── 手动刷盘（一般不需要，析构时自动 flush） ─────────────────────────────
    void flush();

    // 禁止拷贝
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();

    // 打开/切换日志文件（自动按日期命名）
    void openLogFile();

    // 将日志级别枚举转为字符串标签
    static const char* levelToStr(LogLevel level);

    // 格式化当前时间为字符串
    static std::string currentTimeStr();

    // 获取当前日期字符串（用于文件切割判断）
    static std::string currentDateStr();

    // 从完整路径中提取文件名（减少日志长度）
    static std::string shortFileName(const char* path);

private:
    std::mutex   m_mutex;           // 保证多线程写入安全
    std::ofstream m_file;           // 日志文件流
    std::string  m_baseFilePath;    // 基础路径（不含日期后缀）
    std::string  m_currentDate;     // 当前已打开文件的日期，用于切割判断
    LogLevel     m_minLevel;        // 最低日志级别
    bool         m_enableConsole;   // 是否控制台同步输出
    bool         m_initialized;     // 是否已初始化
};
