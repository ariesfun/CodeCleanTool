// Logger.cpp — 跨平台日志系统实现（C++14，兼容 VS2015 MSVC 和麒麟V10 GCC）
//
// 核心职责：提供线程安全的日志写入，支持按日期自动切换文件、级别过滤、
//           控制台彩色输出、Windows 调试窗口输出。
// 日志格式：[时间][级别][文件:行][函数] 消息内容
// 被调用：LOG_INFO/LOG_ERROR/LOG_WARN 等宏（定义在 Logger.h），全应用统一使用
// 线程安全：所有公开方法通过 std::mutex 保护，log() 内部持有锁期间做 I/O 写入

#include "Logger.h"

#include <cstring>
#include <cstring>      // strrchr
#include <iostream>
#include <stdexcept>
#include <sys/stat.h>   // stat / mkdir


// Windows 专有头文件（仅在 Windows 编译时引入）
#ifdef _WIN32
    // 避免宏重定义警告（如果命令行已定义则不再重复）
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    // 强制取消 min/max 宏，防止污染标准库（即使 NOMINMAX 已定义也不影响）
    #undef min
    #undef max
    #include <direct.h>
#else
    #include <unistd.h>   // POSIX mkdir
#endif

// ============================================================================
// UTF-8 路径 → 宽字符路径（仅 Windows）
// ============================================================================

// 必要性：MSVC 的窄字符文件接口（std::ofstream::open(const std::string&)、
//  _mkdir(const char*)）按【当前 ANSI 代码页】解释文件名，而不是按 UTF-8 解释。
//  本项目的路径统一是 UTF-8（由 QString::toStdString() 而来），直接传进去时，
//  路径里只要有中文就会被误读 —— 表现为 std::ofstream 打不开文件、_mkdir 建到
//  乱码名下。程序与配置都定位在 exe 同级，用户解压到「D:\工具\CodeCleanTool\」
//  这类目录即会命中，届时日志一条都写不出来。
//  故 Windows 下先把 UTF-8 转成宽字符，再走宽字符版文件接口。
#ifdef _WIN32
static std::wstring Utf8PathToWide(const std::string& utf8Path)
{
    if (utf8Path.empty())
    {
        return std::wstring();
    }
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(),
                                        static_cast<int>(utf8Path.size()), nullptr, 0);
    if (len <= 0)
    {
        return std::wstring();
    }
    std::wstring wide(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Path.c_str(),
                        static_cast<int>(utf8Path.size()), &wide[0], len);
    return wide;
}
#endif

// ============================================================================
// 跨平台递归创建目录 — 确保日志文件所在目录树存在
// ============================================================================

// 从根目录开始逐级创建，已存在的目录跳过（不报错）
// 返回 true 表示目录已存在或创建成功
static bool mkdirRecursive(const std::string& path)
{
    if (path.empty())
    {
        return true;
    }

    std::string tmp = path;
    // 统一路径分隔符为 /，简化后续查找逻辑
    for (auto& c : tmp)
    {
        if (c == '\\')
        {
            c = '/';
        }
    }

    // 逐级创建：对路径中每个 / 分隔的目录调用 mkdir
    size_t pos = 0;
    while ((pos = tmp.find('/', pos + 1)) != std::string::npos)
    {
        std::string sub = tmp.substr(0, pos);
        if (sub.empty() || sub == ".")
        {
            continue;
        }
#ifdef _WIN32
        // 走宽字符版：_mkdir 按 ANSI 代码页解释路径，含中文的目录建不出来
        _wmkdir(Utf8PathToWide(sub).c_str());
#else
        mkdir(sub.c_str(), 0755);
#endif
    }
    // 创建最后一级目录，EEXIST 表示已存在（正常情况）
#ifdef _WIN32
    return _wmkdir(Utf8PathToWide(tmp).c_str()) == 0 || errno == EEXIST;
#else
    return mkdir(tmp.c_str(), 0755) == 0 || errno == EEXIST;
#endif
}

// ============================================================================
// Logger 实现 — 单例模式 + 线程安全日志写入
// ============================================================================

// 获取全局唯一 Logger 实例（C++11 局部静态变量保证线程安全初始化，无需双重检查锁）
Logger& Logger::instance()
{
    // C++11 保证局部静态变量线程安全初始化（VS2015 已支持）
    static Logger s_instance;
// 首次调用时设置控制台输出为 UTF-8（仅 Windows）
#ifdef _WIN32
    static bool consoleUtf8Set = false;
    if (!consoleUtf8Set) {
        SetConsoleOutputCP(CP_UTF8);
        consoleUtf8Set = true;
    }
#endif    
    return s_instance;
}

Logger::Logger()
    : m_minLevel(LogLevel::DEBUG)
    , m_enableConsole(true)
    , m_initialized(false)
{
}

Logger::~Logger()
{
    flush();  // 析构前将缓冲区残留日志刷盘
    if (m_file.is_open())
    {
        m_file.close();
    }
}

// 初始化日志系统：设定输出路径、最低级别、是否输出到控制台
// 必须在首次 log() 调用前执行，否则日志仅输出到 cerr
void Logger::init(const std::string& logFilePath,
                  LogLevel minLevel,
                  bool enableConsole)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    m_baseFilePath  = logFilePath;
    m_minLevel      = minLevel;
    m_enableConsole = enableConsole;
    m_initialized   = true;

    // 确保日志目录存在（递归创建）
    size_t slashPos = logFilePath.find_last_of("/\\");
    if (slashPos != std::string::npos)
    {
        std::string dir = logFilePath.substr(0, slashPos);
        mkdirRecursive(dir);
    }

    openLogFile();
}

// 按日期打开/切换日志文件：检测当前日期与上次打开日期是否一致，
// 若跨天则关闭旧文件、创建新文件（文件名含日期后缀，如 macinsight_2026-05-31.log）
// 调用方已持有 m_mutex，此函数内部不再加锁
void Logger::openLogFile()
{
    std::string today = currentDateStr();
    if (today == m_currentDate && m_file.is_open())
    {
        return;
    }

    if (m_file.is_open())
    {
        m_file.close();
    }

    // 构造带日期的文件名：在扩展名前插入 _YYYY-MM-DD 后缀
    std::string finalPath;
    size_t dotPos = m_baseFilePath.find_last_of('.');
    if (dotPos != std::string::npos)
    {
        finalPath = m_baseFilePath.substr(0, dotPos)
                  + "_" + today
                  + m_baseFilePath.substr(dotPos);
    }
    else
    {
        finalPath = m_baseFilePath + "_" + today;
    }

    // 追加模式打开：同一天多次启动时不清空旧日志
#ifdef _WIN32
    // 宽字符重载是 MSVC 扩展：窄字符重载按 ANSI 代码页解释路径，含中文时打不开文件
    m_file.open(Utf8PathToWide(finalPath).c_str(), std::ios::out | std::ios::app);
#else
    m_file.open(finalPath, std::ios::out | std::ios::app);
#endif
    if (!m_file.is_open())
    {
        std::cerr << "[Logger] 无法打开日志文件: " << finalPath << std::endl;
    }
    m_currentDate = today;
}

// 核心日志写入方法：格式化 → 拼接完整行 → 写文件 + 控制台 + 调试器
// 这是 LOG_INFO/LOG_ERROR 等宏最终调用的唯一入口
void Logger::log(LogLevel level,
                 const char* file,
                 int line,
                 const char* func,
                 const char* fmt, ...)
{
    // 低于当前日志级别的消息直接丢弃
    if (level < m_minLevel)
    {
        return;
    }

    // 1. 格式化可变参数用户消息（vsnprintf → msgBuf，最大 4096 字节）
    char msgBuf[4096] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(msgBuf, sizeof(msgBuf) - 1, fmt, args);
    va_end(args);

    // 2. 拼接完整日志行：[时间][级别][文件:行][函数] 消息
    std::string timeStr  = currentTimeStr();
    std::string fileStr  = shortFileName(file);
    const char* levelStr = levelToStr(level);

    char lineBuf[5120] = {0};
    snprintf(lineBuf, sizeof(lineBuf) - 1,
             "[%s][%s][%s:%d][%s] %s\n",
             timeStr.c_str(), levelStr,
             fileStr.c_str(), line, func,
             msgBuf);

    // 3. 线程安全写入：锁保护文件 + 控制台 + 调试器三路输出
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized)
    {
        // 未初始化时仅输出到 cerr，避免日志丢失且不崩溃
        std::cerr << "[Logger 未初始化] " << lineBuf;
        return;
    }

    // 检查是否跨天需切换文件（在锁内做，保证原子性）
    openLogFile();

    // 写入日志文件，每条消息 flush 确保立即落盘（面向调试场景）
    if (m_file.is_open())
    {
        m_file << lineBuf;
        m_file.flush();
    }

    // 控制台输出：WARN+ 级别走 stderr，INFO/DEBUG 走 stdout
    if (m_enableConsole)
    {
        if (level >= LogLevel::WARN)
        {
            std::cerr << lineBuf;
        }
        else
        {
            std::cout << lineBuf;
        }
    }

#ifdef _WIN32
    // Windows 额外输出到调试器窗口（VS/CLion 调试时无需查看文件即可看到日志）
    OutputDebugStringA(lineBuf);
#endif
}

// 运行时修改日志级别（无需重启程序）
void Logger::setLevel(LogLevel level)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_minLevel = level;
}

LogLevel Logger::getLevel() const
{
    return m_minLevel;
}

// 将文件缓冲区残留数据刷入磁盘
void Logger::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open())
    {
        m_file.flush();
    }
}

// ============================================================================
// 私有静态工具函数 — 日志级别→字符串、时间戳生成、文件名截短
// ============================================================================

// 将 LogLevel 枚举转为固定宽度的 5 字符标签（右补空格对齐）
const char* Logger::levelToStr(LogLevel level)
{
    switch (level)
    {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO ";
        case LogLevel::WARN:  return "WARN ";
        case LogLevel::ERROR_: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "?????";
    }
}

// 生成当前时间字符串：格式 YYYY-MM-DD HH:MM:SS，线程安全（平台适配版）
std::string Logger::currentTimeStr()
{
    // 格式：2024-01-01 12:00:00
    time_t now = time(nullptr);
    struct tm tmInfo;

#ifdef _WIN32
    localtime_s(&tmInfo, &now);     // VS2015 线程安全版
#else
    localtime_r(&now, &tmInfo);     // POSIX 线程安全版
#endif

    char buf[64] = {0};             // 更宽松的安全空间
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             tmInfo.tm_year + 1900,
             tmInfo.tm_mon  + 1,
             tmInfo.tm_mday,
             tmInfo.tm_hour,
             tmInfo.tm_min,
             tmInfo.tm_sec);
    return std::string(buf);
}

std::string Logger::currentDateStr()
{
    time_t now = time(nullptr);
    struct tm tmInfo;
#ifdef _WIN32
    localtime_s(&tmInfo, &now);
#else
    localtime_r(&now, &tmInfo);
#endif

    char buf[64] = {0};
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
             tmInfo.tm_year + 1900,
             tmInfo.tm_mon  + 1,
             tmInfo.tm_mday);
    return std::string(buf);
}

// 从完整路径中提取文件名部分（去掉目录前缀）
// 先查 / 再查 \ ，兼容 Windows 和 POSIX 路径
std::string Logger::shortFileName(const char* path)
{
    if (!path)
    {
        return "";
    }
    const char* p = strrchr(path, '/');
    if (!p)
    {
        p = strrchr(path, '\\');
    }
    return p ? (p + 1) : path;  // 找到分隔符 → 返回文件名；否则整串就是文件名
}
