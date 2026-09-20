// 打包器实现：调用 7z CLI 生成 .7z 纯源码压缩包
#include "Packager.h"

#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

#include "Logger.h"

Packager::Packager(QObject* parent)
    : QObject(parent)
{
}

void Packager::SetSourceDir(const QString& sourceDir)
{
    m_sourceDir = sourceDir;
}

void Packager::SetOutputDir(const QString& outputDir)
{
    m_outputDir = outputDir;
}

void Packager::SetOutputName(const QString& name)
{
    m_outputName = name;
}

void Packager::SetFileList(const QStringList& files)
{
    m_fileList = files;
}

void Packager::Set7zPath(const QString& path)
{
    m_sevenZipPath = path;
}

void Packager::SetExcludeVcsDirs(bool exclude)
{
    m_excludeVcsDirs = exclude;
}

void Packager::StartPack()
{
    if (m_sourceDir.isEmpty())
    {
        LOG_ERROR("[Packager] 未设置源码目录");
        emit PackError("未设置源码目录");
        return;
    }

    // 优先使用用户指定的 7z 路径，否则自动检测
    QString sevenZip = m_sevenZipPath.isEmpty() ? Find7zPath() : m_sevenZipPath;
    if (!sevenZip.isEmpty() && !QFileInfo::exists(sevenZip))
    {
        LOG_ERROR("[Packager] 指定的 7z 路径无效: %s", sevenZip.toStdString().c_str());
        emit PackError("7z 路径不存在: " + sevenZip);
        return;
    }
    if (sevenZip.isEmpty())
    {
        LOG_ERROR("[Packager] 未找到 7z.exe");
        emit PackError("未找到 7z.exe，请安装 7-Zip");
        return;
    }

    // 输出目录默认同源目录
    if (m_outputDir.isEmpty())
    {
        m_outputDir = m_sourceDir + "/../output";
    }
    QDir().mkpath(m_outputDir);

    // 包名规则：未显式指定时按默认模板展开（项目名_yyyyMMdd_hhmmss_source）
    // 与设置页模板走同一展开逻辑，避免两处规则漂移
    if (m_outputName.isEmpty())
    {
        m_outputName = FormatOutputName("%Project_%YYYY%MM%DD_%HH%MM%SS_source", m_sourceDir);
    }

    QString outputPath = m_outputDir + "/" + m_outputName + ".7z";

    // 构建 7z 命令
    // 7z a -t7z -mx5 <output> <source>
    LOG_INFO("[Packager] 启动 7z, 输出: %s, 源码: %s", outputPath.toStdString().c_str(), m_sourceDir.toStdString().c_str());
    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Packager::OnProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &Packager::OnProcessError);

    QStringList args;
    args << "a" << "-t7z" << "-mx5" << outputPath;
    // 由用户设置控制：是否排除 VCS 版本控制目录
    if (m_excludeVcsDirs)
    {
        args << "-xr!.git" << "-xr!.svn";
    }

    if (!m_fileList.isEmpty())
    {
        // 有显式文件列表：通过 @listfile 传入
        QString listFile = m_outputDir + "/_packlist.tmp";
        QFile lf(listFile);
        if (lf.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream ts(&lf);
            for (const auto& f : m_fileList)
            {
                // 转为相对路径
                QString rel = QDir(m_sourceDir).relativeFilePath(f);
                ts << rel << "\n";
            }
        }
        args << "@" + listFile;
    }
    else
    {
        // 打包整个源码目录
        args << m_sourceDir + "/*";
    }

    m_process->setWorkingDirectory(m_sourceDir);
    m_process->start(sevenZip, args);

    if (!m_process->waitForStarted(5000))
    {
        LOG_ERROR("[Packager] 无法启动 7z 进程");
        emit PackError("无法启动 7z 进程");
        delete m_process;
        m_process = nullptr;
    }
}

void Packager::OnProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    Q_UNUSED(exitCode);

    // 进程被 kill（取消打包），不读标准错误，直接清理
    if (status == QProcess::CrashExit || !m_process)
    {
        if (m_process)
        {
            m_process->deleteLater();
            m_process = nullptr;
        }
        return;
    }

    QString outputPath = m_outputDir + "/" + m_outputName + ".7z";
    QFileInfo fi(outputPath);

    if (fi.exists())
    {
        LOG_INFO("[Packager] 打包完成, 输出: %s, 大小: %lld bytes", outputPath.toStdString().c_str(), fi.size());
        emit PackProgress(100);
        emit PackFinished(outputPath, fi.size());
    }
    else
    {
        QString err = QString::fromLocal8Bit(m_process->readAllStandardError());
        LOG_ERROR("[Packager] 打包失败: %s", err.toStdString().c_str());
        emit PackError("打包失败: " + err);
    }

    m_process->deleteLater();
    m_process = nullptr;
}

void Packager::OnProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    // 若 m_process 已在 CancelPack 中置空，直接返回
    if (!m_process)
    {
        return;
    }
    LOG_ERROR("[Packager] 7z 进程错误: %s", m_process->errorString().toStdString().c_str());
    emit PackError("7z 进程错误: " + m_process->errorString());
    m_process->deleteLater();
    m_process = nullptr;
}

void Packager::CancelPack()
{
    if (m_process && m_process->state() == QProcess::Running)
    {
        LOG_INFO("[Packager] 取消打包");
        // 断开所有信号以避免 kill 后异步回调访问已销毁状态
        m_process->disconnect();
        m_process->kill();
        m_process->waitForFinished(3000);
        m_process->deleteLater();
        m_process = nullptr;
    }
}

QString Packager::Find7zPath()
{
    // 1. 检查已知安装路径
    QStringList knownPaths;
    knownPaths << "C:/Program Files/7-Zip/7z.exe"
               << "C:/Program Files (x86)/7-Zip/7z.exe";

    for (const auto& path : knownPaths)
    {
        if (QFileInfo::exists(path))
        {
            return path;
        }
    }

    // 2. 检查 Windows 注册表
    QSettings reg("HKEY_LOCAL_MACHINE\\SOFTWARE\\7-Zip", QSettings::NativeFormat);
    QString regPath = reg.value("Path").toString();
    if (!regPath.isEmpty())
    {
        QString exe = regPath + "/7z.exe";
        if (QFileInfo::exists(exe))
        {
            return exe;
        }
    }

    // 3. 检查 PATH 环境变量
    return QStandardPaths::findExecutable("7z");
}

QString Packager::FormatOutputName(const QString& pattern, const QString& sourceDir, const QDateTime& now)
{
    // 空模板：返回空串，由调用方决定回退策略（保留 Packager 内部默认命名）
    if (pattern.isEmpty())
    {
        return QString();
    }

    // 项目名推导：优先取叶子目录名（C:/work/MyApp → MyApp）
    // 根路径等取不到名字的情况退化为上级目录名，仍为空则用 project 兜底，保证包名非空
    QFileInfo srcInfo(sourceDir);
    QString projectName = srcInfo.fileName();
    if (projectName.isEmpty())
    {
        projectName = srcInfo.dir().dirName();
    }
    if (projectName.isEmpty())
    {
        projectName = "project";
    }

    QString result;
    result.reserve(pattern.size() + 16);

    // hourSeen: %MM 歧义消解依据 —— 未出现 %HH 时视为「月」，出现之后视为「分」
    bool hourSeen = false;
    int i = 0;
    while (i < pattern.size())
    {
        if (pattern.at(i) != '%')
        {
            result.append(pattern.at(i));
            ++i;
            continue;
        }

        // 占位符按名字匹配；匹配成功则连同 '%' 一起跳过
        const QString rest = pattern.mid(i + 1);
        if (rest.startsWith("Project"))
        {
            result.append(projectName);
            i += 8;                 // '%' + "Project"
        }
        else if (rest.startsWith("YYYY"))
        {
            result.append(now.toString("yyyy"));
            i += 5;                 // '%' + "YYYY"
        }
        else if (rest.startsWith("HH"))
        {
            result.append(now.toString("hh"));
            hourSeen = true;
            i += 3;                 // '%' + "HH"
        }
        else if (rest.startsWith("MM"))
        {
            result.append(hourSeen ? now.toString("mm") : now.toString("MM"));
            i += 3;                 // '%' + "MM"
        }
        else if (rest.startsWith("DD"))
        {
            result.append(now.toString("dd"));
            i += 3;                 // '%' + "DD"
        }
        else if (rest.startsWith("SS"))
        {
            result.append(now.toString("ss"));
            i += 3;                 // '%' + "SS"
        }
        else
        {
            // 未知占位符：原样保留 '%'，后续字符留到下一轮按普通字符处理
            result.append('%');
            ++i;
        }
    }

    return result;
}
