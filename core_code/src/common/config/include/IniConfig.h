#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// IniConfig.h
// 轻量级跨平台 INI 配置文件读写模块
// 兼容 VS2015 C++14 / 麒麟V10 GCC
//
// 支持格式：
//   [section]          ; 节名
//   key = value        ; 键值对
//   # 注释 / ; 注释    ; 行注释
//   key =              ; 空值（返回默认值）
//
// 用法示例：
//   IniConfig cfg;
//   cfg.load("config/app.ini");
//
//   std::string host = cfg.getString("network", "host", "127.0.0.1");
//   int port         = cfg.getInt("network", "port", 8080);
//   bool debug       = cfg.getBool("log", "debug_mode", false);
//
//   cfg.setString("network", "host", "192.168.1.1");
//   cfg.save("config/app.ini");
// ─────────────────────────────────────────────────────────────────────────────

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

class IniConfig
{
public:
    IniConfig()  = default;
    ~IniConfig() = default;

    // ── 加载与保存 ────────────────────────────────────────────────────────────

    // 从文件加载 INI，成功返回 true
    bool load(const std::string& filePath);

    // 将当前配置写回文件，成功返回 true
    bool save(const std::string& filePath = "") const;

    // ── 读取接口 ──────────────────────────────────────────────────────────────

    // 读取字符串值，找不到时返回 defaultVal
    std::string getString(const std::string& section,
                          const std::string& key,
                          const std::string& defaultVal = "") const;

    // 读取整数值
    int         getInt(const std::string& section,
                       const std::string& key,
                       int defaultVal = 0) const;

    // 读取浮点值
    double      getDouble(const std::string& section,
                          const std::string& key,
                          double defaultVal = 0.0) const;

    // 读取布尔值（"true"/"1"/"yes"/"on" → true，其余 → false）
    bool        getBool(const std::string& section,
                        const std::string& key,
                        bool defaultVal = false) const;

    // ── 写入接口 ──────────────────────────────────────────────────────────────

    void setString(const std::string& section,
                   const std::string& key,
                   const std::string& value);

    void setInt(const std::string& section,
                const std::string& key,
                int value);

    void setDouble(const std::string& section,
                   const std::string& key,
                   double value);

    void setBool(const std::string& section,
                 const std::string& key,
                 bool value);

    // ── 查询接口 ──────────────────────────────────────────────────────────────

    // 检查某个 section 是否存在
    bool hasSection(const std::string& section) const;

    // 检查某个 key 是否存在
    bool hasKey(const std::string& section, const std::string& key) const;

    // 获取所有 section 名称
    std::vector<std::string> sections() const;

    // 获取某 section 下所有 key 名称
    std::vector<std::string> keys(const std::string& section) const;

private:
    // 去除字符串首尾空白
    static std::string trim(const std::string& s);

    // 判断字符串是否代表布尔 true
    static bool strToBool(const std::string& s, bool defaultVal);

private:
    // 数据结构：section → { key → value }
    // 使用有序的 vector 保留 section / key 的写入顺序（方便 save 时顺序输出）
    std::vector<std::string>                                       m_sectionOrder;
    std::unordered_map<std::string,
        std::vector<std::string>>                                  m_keyOrder;
    std::unordered_map<std::string,
        std::unordered_map<std::string, std::string>>              m_data;

    mutable std::mutex  m_mutex;        // 保证多线程读写安全
    std::string         m_loadedPath;   // 最后加载的文件路径，save() 默认使用
};
