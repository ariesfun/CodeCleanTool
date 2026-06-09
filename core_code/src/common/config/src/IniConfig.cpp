// IniConfig.cpp — INI 配置文件读写实现（C++14，无第三方依赖）
//
// 数据结构：三重嵌套 map——section 名 → key 名 → value 字符串。
// 同时用 m_sectionOrder / m_keyOrder 两个 vector 保留写入顺序，确保 save() 输出
// 与用户手工编辑的顺序一致，避免迷惑 diff。
//
// 线程安全：所有公开方法均通过 std::lock_guard<std::mutex> 加锁，
// 允许跨线程读/写同一 IniConfig 实例而无需外部同步。
// 依赖：IniConfig.h（头文件中定义完整接口），标准库 <fstream>/<sstream>/<algorithm>

#include "IniConfig.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>

// ============================================================================
// 加载 — 从 INI 文本文件解析到内存数据结构
// ============================================================================

// 解析流程（逐行状态机）：
//   1. 逐行读取，trim 去掉首尾空白
//   2. 跳过空行和注释行（支持 ; 和 # 两种 INI 注释前缀）
//   3. 遇到 [xxx] 行 → 切换当前 section（重复的 section 合并到同一分组）
//   4. 遇到 key = value → 存入当前 section；value 中行尾 ; 注释会被切除
//   5. 同名 key 后值覆盖前值（符合标准 INI 行为）
bool IniConfig::load(const std::string& filePath)
{
    std::ifstream ifs(filePath);
    if (!ifs.is_open())
    {
        // 文件不存在或无法打开时静默返回 false，让调用方按需报错
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // 清空上次加载的全部数据，避免新旧配置混合
    m_sectionOrder.clear();
    m_keyOrder.clear();
    m_data.clear();
    m_loadedPath = filePath;

    std::string currentSection;
    std::string line;

    while (std::getline(ifs, line))
    {
        std::string s = trim(line);

        // 跳过空行和注释行（; 与 # 均为标准 INI 注释前缀）
        if (s.empty() || s[0] == ';' || s[0] == '#')
        {
            continue;
        }

        // 解析节名：匹配 [xxx] 格式，提取括号内的 section 标识
        if (s.front() == '[' && s.back() == ']')
        {
            currentSection = trim(s.substr(1, s.size() - 2));

            // 只注册首次出现的 section（后续同名 section 合并到已有分组）
            if (m_data.find(currentSection) == m_data.end())
            {
                m_sectionOrder.push_back(currentSection);
                m_data[currentSection] = {};
                m_keyOrder[currentSection] = {};
            }
            continue;
        }

        // 解析键值对：定位第一个 = 号，左为 key 右为 value
        size_t eqPos = s.find('=');
        if (eqPos == std::string::npos)
        {
            continue;   // 不含 = 的行不是合法键值对，跳过
        }

        std::string key   = trim(s.substr(0, eqPos));
        std::string value = trim(s.substr(eqPos + 1));

        // 切除 value 中的行尾分号注释（仅处理无引号包围的简单情况）
        size_t commentPos = value.find(';');
        if (commentPos != std::string::npos)
        {
            value = trim(value.substr(0, commentPos));
        }

        // key 为空或 section 未确定时无法归类该键值对
        if (key.empty() || currentSection.empty())
        {
            continue;
        }

        // 首次出现的 key 记录写入顺序，后续复用已有位置
        auto& keyMap = m_data[currentSection];
        if (keyMap.find(key) == keyMap.end())
        {
            m_keyOrder[currentSection].push_back(key);
        }
        keyMap[key] = value;  // 后值覆盖前值（标准 INI 行为）
    }

    return true;
}

// ============================================================================
// 保存 — 将内存配置写回 INI 文本文件
// ============================================================================

// 输出格式：按 section/key 写入顺序逐节写出，节之间以空行分隔。
// 若调用未传 filePath 则使用上次 load 时的路径；若从未 load 过返回 false。
bool IniConfig::save(const std::string& filePath) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 确定写入目标：显式路径优先，否则回退到 load 路径
    const std::string& path = filePath.empty() ? m_loadedPath : filePath;
    if (path.empty())
    {
        return false;  // 既无显式路径也无历史 load 路径，无法保存
    }

    std::ofstream ofs(path, std::ios::out | std::ios::trunc);
    if (!ofs.is_open())
    {
        return false;
    }

    // 按写入顺序逐节输出，保持与用户手工编辑 .ini 文件一致的字段排列
    for (const auto& sec : m_sectionOrder)
    {
        ofs << "[" << sec << "]\n";

        auto it = m_keyOrder.find(sec);
        if (it != m_keyOrder.end())
        {
            for (const auto& key : it->second)
            {
                auto& keyMap = m_data.at(sec);
                auto  kv     = keyMap.find(key);
                if (kv != keyMap.end())
                {
                    ofs << key << " = " << kv->second << "\n";
                }
            }
        }
        ofs << "\n";  // 节间空行提升可读性
    }

    return true;
}

// ============================================================================
// 读取接口 — 从已解析的配置中查询值，缺失时返回默认值
// ============================================================================

// getString：从 section/key 读取字符串值
// 注意：显式写了 key=（空值）也会返回 defaultVal 而非空串
std::string IniConfig::getString(const std::string& section,
                                 const std::string& key,
                                 const std::string& defaultVal) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto secIt = m_data.find(section);
    if (secIt == m_data.end()) return defaultVal;

    auto keyIt = secIt->second.find(key);
    if (keyIt == secIt->second.end()) return defaultVal;

    return keyIt->second.empty() ? defaultVal : keyIt->second;
}

// getInt：解析为 int，格式非法时静默回退到 defaultVal（不抛异常）
int IniConfig::getInt(const std::string& section,
                      const std::string& key,
                      int defaultVal) const
{
    std::string v = getString(section, key, "");
    if (v.empty()) return defaultVal;
    try {
        return std::stoi(v);
    } catch (...)
    {
        return defaultVal;  // 非数字字符串退回默认值，保证不中断调用方
    }
}

// getDouble：解析为 double，容错策略同 getInt
double IniConfig::getDouble(const std::string& section,
                            const std::string& key,
                            double defaultVal) const
{
    std::string v = getString(section, key, "");
    if (v.empty()) return defaultVal;
    try {
        return std::stod(v);
    } catch (...)
    {
        return defaultVal;
    }
}

// getBool：委托 strToBool 做大小写不敏感的布尔字符串解析
bool IniConfig::getBool(const std::string& section,
                        const std::string& key,
                        bool defaultVal) const
{
    std::string v = getString(section, key, "");
    if (v.empty()) return defaultVal;
    return strToBool(v, defaultVal);
}

// ============================================================================
// 写入接口 — 修改内存中的配置值（不会自动持久化，需显式调 save）
// ============================================================================

// setString：写入字符串值；section 或 key 不存在时自动创建并记录顺序
void IniConfig::setString(const std::string& section,
                          const std::string& key,
                          const std::string& value)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 新 section：追加到顺序表末尾，初始化空的 key 映射
    if (m_data.find(section) == m_data.end())
    {
        m_sectionOrder.push_back(section);
        m_data[section]    = {};
        m_keyOrder[section] = {};
    }

    // 新 key：追加到该 section 的 key 顺序表
    auto& keyMap = m_data[section];
    if (keyMap.find(key) == keyMap.end())
    {
        m_keyOrder[section].push_back(key);
    }
    keyMap[key] = value;  // 已存在的 key 直接覆盖
}

void IniConfig::setInt(const std::string& section,
                       const std::string& key,
                       int value)
{
    setString(section, key, std::to_string(value));
}

void IniConfig::setDouble(const std::string& section,
                          const std::string& key,
                          double value)
{
    std::ostringstream oss;
    oss << value;
    setString(section, key, oss.str());
}

// setBool：统一写小写 "true"/"false"，保持文件内格式一致性
void IniConfig::setBool(const std::string& section,
                        const std::string& key,
                        bool value)
{
    setString(section, key, value ? "true" : "false");
}

// ============================================================================
// 查询接口 — 遍历 section/key 结构、判断存在性
// ============================================================================

bool IniConfig::hasSection(const std::string& section) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data.find(section) != m_data.end();
}

bool IniConfig::hasKey(const std::string& section, const std::string& key) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto secIt = m_data.find(section);
    if (secIt == m_data.end()) return false;
    return secIt->second.find(key) != secIt->second.end();
}

// sections：返回所有 section 标识，顺序与首次 load/set 时一致
std::vector<std::string> IniConfig::sections() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sectionOrder;
}

// keys：返回指定 section 下的所有 key 标识，顺序与首次创建一致
std::vector<std::string> IniConfig::keys(const std::string& section) const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_keyOrder.find(section);
    if (it == m_keyOrder.end()) return {};
    return it->second;
}

// ============================================================================
// 工具函数 — 纯函数，不依赖成员状态，仅被上述公开方法内部调用
// ============================================================================

// trim：删除字符串首尾的空白字符（空格、制表、回车、换行）
std::string IniConfig::trim(const std::string& s)
{
    const std::string whitespace = " \t\r\n";
    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(whitespace);
    return s.substr(start, end - start + 1);
}

// strToBool：大小写不敏感的字符串→布尔转换
//   真值：true / 1 / yes / on（不区分大小写）
//   假值：false / 0 / no / off（不区分大小写）
//   不匹配以上任一值时退回到 defaultVal（由调用方传入，如 getBool 的第三个参数）
bool IniConfig::strToBool(const std::string& s, bool defaultVal)
{
    // 统一转小写后做精确匹配，消除 True/TRUE/On 等变体无法识别的问题
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    if (lower == "true" || lower == "1" || lower == "yes" || lower == "on")
    {
        return true;
    }
    if (lower == "false" || lower == "0" || lower == "no" || lower == "off")
    {
        return false;
    }
    return defaultVal;
}
