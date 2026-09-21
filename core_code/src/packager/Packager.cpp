// 打包器实现：调用 7z CLI 生成 .7z 纯源码压缩包
#include "Packager.h"

#include <QFileInfo>
#include <QCoreApplication>
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

void Packager::SetExcludeList(const QStringList& paths)
{
    m_excludeList = paths;
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

    // 输出目录默认取源码目录的上一级：放在源码目录内会被一并打进包里
    // （cleanPath 消掉 "proj/../output" 里的 ".."，否则日志与提示里会显示带 ".." 的路径）
    const QString outputDir = m_outputDir.isEmpty()
        ? QDir::cleanPath(m_sourceDir + "/../output")
        : m_outputDir;
    QDir().mkpath(outputDir);

    // 包名规则：未显式指定时按默认模板展开（项目名_yyyyMMdd_hhmmss_source）
    // 与设置页模板走同一展开逻辑，避免两处规则漂移
    const QString outputName = m_outputName.isEmpty()
        ? FormatOutputName("%Project_%YYYY%MM%DD_%HH%MM%SS_source", m_sourceDir)
        : m_outputName;

    m_outputPath = outputDir + "/" + outputName + ".7z";

    // 排除项写进 @listfile：勾选项可能上百条，直接拼进命令行会超出长度上限
    m_listFilePath = WriteExcludeListFile(m_outputPath);

    // 构建 7z 命令
    // 7z a -t7z -mx5 <output> -x@<排除列表> <source>
    LOG_INFO("[Packager] 启动 7z, 输出: %s, 源码: %s, 排除项: %d",
             m_outputPath.toStdString().c_str(), m_sourceDir.toStdString().c_str(),
             m_excludeList.size());
    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Packager::OnProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &Packager::OnProcessError);

    QStringList args;
    args << "a" << "-t7z" << "-mx5" << m_outputPath;
    // 由用户设置控制：是否排除 VCS 版本控制目录
    if (m_excludeVcsDirs)
    {
        args << "-xr!.git" << "-xr!.svn";
    }
    if (!m_listFilePath.isEmpty())
    {
        args << "-x@" + m_listFilePath;
    }
    // 打包整个源码目录
    args << m_sourceDir + "/*";

    m_process->setWorkingDirectory(m_sourceDir);
    m_process->start(sevenZip, args);

    if (!m_process->waitForStarted(5000))
    {
        LOG_ERROR("[Packager] 无法启动 7z 进程");
        emit PackError("无法启动 7z 进程");
        delete m_process;
        m_process = nullptr;
        RemoveListFile();
    }
}

QString Packager::WriteExcludeListFile(const QString& outputPath)
{
    QStringList relPaths;

    // 用户勾选的待清理项：换算成相对源码目录的路径。
    // 7z 的排除模式按【压缩包内相对路径】精确匹配，故这里必须给相对路径
    // （给绝对路径不会匹配到任何条目，排除会静默失效）
    for (const auto& path : m_excludeList)
    {
        const QString rel = QDir(m_sourceDir).relativeFilePath(path);
        if (!rel.isEmpty() && !rel.startsWith("..") && !rel.contains(':'))
        {
            relPaths << rel;
        }
    }

    // 输出包自身：输出目录落在源码目录内时，上一次打出的包也在「源码目录/*」范围内，
    // 不排除就会被收进新包 —— 表现为包体积逐次膨胀，且里面裹着上一版的自己
    const QString relOutput = QDir(m_sourceDir).relativeFilePath(outputPath);
    if (!relOutput.isEmpty() && !relOutput.startsWith("..") && !relOutput.contains(':'))
    {
        relPaths << relOutput;
    }

    if (relPaths.isEmpty())
    {
        return QString();
    }

    // 列表文件写在系统临时目录而非输出目录：写在输出目录里的话，
    // 若输出目录恰好也在源码目录内，它自己同样会被打进包里
    const QString listFile = QDir::tempPath()
        + QString("/codecleantool_packlist_%1.tmp").arg(QCoreApplication::applicationPid());

    QFile lf(listFile);
    if (!lf.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        LOG_ERROR("[Packager] 无法写入排除列表: %s", listFile.toStdString().c_str());
        return QString();
    }

    QTextStream ts(&lf);
    // 必须显式 UTF-8：QTextStream 默认走 codecForLocale（中文 Windows 为 GBK），
    // 而 7z 按 UTF-8 解读列表文件，路径含中文时会直接报
    // "Incorrect item in listfile" 并拒绝执行整个打包
    ts.setCodec("UTF-8");
    for (const auto& rel : relPaths)
    {
        ts << rel << "\n";
    }
    lf.close();
    return listFile;
}

void Packager::RemoveListFile()
{
    if (m_listFilePath.isEmpty())
    {
        return;
    }
    // 7z 进程已结束，列表文件用完即删。留在 %TEMP% 里虽不致命，
    // 但每次打包留一个文件属于运行期垃圾，正是本工具要清理的东西
    if (QFile::exists(m_listFilePath))
    {
        QFile::remove(m_listFilePath);
    }
    m_listFilePath.clear();
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
        RemoveListFile();
        return;
    }

    QFileInfo fi(m_outputPath);

    if (fi.exists())
    {
        LOG_INFO("[Packager] 打包完成, 输出: %s, 大小: %lld bytes",
                 m_outputPath.toStdString().c_str(), fi.size());
        emit PackProgress(100);
        emit PackFinished(m_outputPath, fi.size());
    }
    else
    {
        QString err = QString::fromLocal8Bit(m_process->readAllStandardError());
        LOG_ERROR("[Packager] 打包失败: %s", err.toStdString().c_str());
        emit PackError("打包失败: " + err);
    }

    m_process->deleteLater();
    m_process = nullptr;
    RemoveListFile();
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
    RemoveListFile();
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
    RemoveListFile();
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
