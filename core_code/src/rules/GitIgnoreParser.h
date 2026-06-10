#ifndef GITIGNOREPARSER_H
#define GITIGNOREPARSER_H

#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QList>

// 单条 gitignore 规则
struct GitIgnoreRule
{
    QString pattern;            // 原始模式文本
    QRegularExpression regex;   // 预编译正则
    bool isNegation;            // 是否为 ! 反向规则
    bool isDirOnly;             // 是否仅匹配目录（以 / 结尾）
    bool anchored;              // 是否含路径分隔符（锚定到 .gitignore 目录）
};

// .gitignore 解析器：读取并解析 .gitignore 规则，判断路径是否应排除
class GitIgnoreParser
{
public:
    GitIgnoreParser();

    // 从文件加载 .gitignore 规则
    bool LoadFromFile(const QString& filePath);

    // 解析规则文本行
    void ParseRules(const QString& text);

    // 判断相对路径是否应被忽略
    bool IsIgnored(const QString& relativePath, bool isDir = false) const;

    // 获取已加载的规则文本
    QStringList Rules() const;

    // 清空所有规则
    void Clear();

private:
    // 将单条 gitignore 规则编译为正则
    static GitIgnoreRule CompileRule(const QString& pattern);

    // 将 glob 模式转为正则（支持 **、*、?、[abc]）
    static QString GlobToRegex(const QString& glob);

    QList<GitIgnoreRule> m_rules;   // 规则列表（按加载顺序）
    QStringList m_rawRules;         // 原始规则文本
};

#endif // GITIGNOREPARSER_H
