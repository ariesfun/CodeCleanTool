// gitignore 解析器实现：glib → 正则编译 + 路径匹配，语义对齐 git 官方规则
#include "GitIgnoreParser.h"

#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QDir>

GitIgnoreParser::GitIgnoreParser()
{
}

bool GitIgnoreParser::LoadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }

    QTextStream stream(&file);
    QString text = stream.readAll();
    // 将全部文本交由 ParseRules 逐行解析
    ParseRules(text);
    return true;
}

// 将 gitignore glob 语法转为正则
// 支持：**（跨目录）、*（不含分隔符）、?（单字符非分隔符）、[abc]（字符集）
QString GitIgnoreParser::GlobToRegex(const QString& glob)
{
    QString result;
    result.reserve(glob.size() * 2 + 2);
    result += '^';

    int i = 0;
    while (i < glob.size())
    {
        QChar c = glob[i];

        // ** 双星：匹配任意路径（含 /）
        if (c == '*' && i + 1 < glob.size() && glob[i + 1] == '*')
        {
            // 处理 **/xxx 和 xxx/** 和 **
            if (i + 2 < glob.size() && glob[i + 2] == '/')
            {
                result += "(.*/)?";
                i += 3;
                continue;
            }
            else if (i == 0 && i + 2 == glob.size())
            {
                result += ".*";
                break;
            }
            else if (i + 2 == glob.size())
            {
                result += "/.*";
                break;
            }
        }

        // * 单星：匹配不含 / 的任意字符序列
        if (c == '*')
        {
            result += "[^/]*";
            ++i;
            continue;
        }

        // ? 问号：匹配单个非 / 字符
        if (c == '?')
        {
            result += "[^/]";
            ++i;
            continue;
        }

        // [abc] 字符集：直接保留
        if (c == '[')
        {
            int end = glob.indexOf(']', i);
            if (end != -1)
            {
                result += glob.mid(i, end - i + 1);
                i = end + 1;
                continue;
            }
        }

        // 转义正则特殊字符
        if (QString(".^$+{}()|\\").contains(c))
        {
            result += '\\';
            result += c;
        }
        else
        {
            result += c;
        }
        ++i;
    }

    // 目录规则：匹配前缀即可
    result += ".*$";

    return result;
}

GitIgnoreRule GitIgnoreParser::CompileRule(const QString& pattern)
{
    GitIgnoreRule rule;
    rule.pattern = pattern;
    rule.isNegation = false;
    rule.isDirOnly = false;
    rule.anchored = false;

    QString p = pattern.trimmed();

    if (p.isEmpty())
    {
        return rule;
    }

    // 反向规则 ! 开头
    if (p.startsWith('!'))
    {
        rule.isNegation = true;
        p = p.mid(1);
    }

    // 不以 / 开头的模式匹配任意层级
    rule.anchored = p.contains('/');

    // 去掉首尾 /
    if (p.startsWith('/'))
    {
        p = p.mid(1);
    }

    // 目录规则（/ 结尾）
    if (p.endsWith('/'))
    {
        rule.isDirOnly = true;
        p.chop(1);
    }

    // 移除尾部空格（gitignore 语义）
    p = p.trimmed();

    QString regexStr = GlobToRegex(p);
    rule.regex = QRegularExpression(regexStr);
    rule.regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);

    return rule;
}

void GitIgnoreParser::ParseRules(const QString& text)
{
    QStringList lines = text.split('\n', Qt::SkipEmptyParts);

    for (const QString& line : lines)
    {
        QString trimmed = line.trimmed();

        // 跳过空行和注释行（# 开头）
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
        {
            continue;
        }

        // 同时保存原始文本和编译后的规则
        m_rawRules.append(trimmed);
        m_rules.append(CompileRule(trimmed));
    }
}

bool GitIgnoreParser::IsIgnored(const QString& relativePath, bool isDir) const
{
    // gitignore 语义：最后匹配的规则生效（遍历全部规则，不提前返回）
    bool ignored = false;

    for (const auto& rule : m_rules)
    {
        // 仅匹配目录的规则在检查文件时跳过
        if (rule.isDirOnly && !isDir)
        {
            continue;
        }

        if (rule.regex.match(relativePath).hasMatch())
        {
            // ! 反向规则反转忽略状态
            ignored = !rule.isNegation;
        }
    }

    return ignored;
}

QStringList GitIgnoreParser::Rules() const
{
    return m_rawRules;
}

void GitIgnoreParser::Clear()
{
    m_rules.clear();
    m_rawRules.clear();
}
