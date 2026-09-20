#ifndef PACKAGER_H
#define PACKAGER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QDateTime>

// 打包器：调用 7z CLI 生成纯源码 .7z 压缩包
// 调用链：MainWindow::OnPack() → StartPack() → QProcess 启动 7z → OnProcessFinished/OnProcessError
// QProcess 双侧打日志，Find7zPath 按已知路径→注册表→PATH 查找
class Packager : public QObject
{
    Q_OBJECT

public:
    explicit Packager(QObject* parent = nullptr);

    // SetSourceDir: 设置源码目录（必填，打包前调用）
    void SetSourceDir(const QString& sourceDir);
    // SetOutputDir: 设置输出目录（选填，默认 sourceDir/../output）
    void SetOutputDir(const QString& outputDir);
    // SetOutputName: 设置包名不含扩展名（选填，默认 项目名_YYYYMMDD_HHMMSS_source）
    void SetOutputName(const QString& name);
    // SetFileList: 设置待打包文件列表（选填，空列表则打包整个源码目录）
    void SetFileList(const QStringList& files);
    // Set7zPath: 设置 7z.exe 路径（选填，空则自动检测）
    void Set7zPath(const QString& path);
    // SetExcludeVcsDirs: 设置是否在 7z 命令行排除 .git/.svn
    void SetExcludeVcsDirs(bool exclude);

    // StartPack: 检查前置条件 → 确认 7z 路径 → 构建参数 → 启动 QProcess
    // 前置条件：待设置 SetSourceDir 且系统已安装 7-Zip
    void StartPack();
    // CancelPack: 若 7z 进程运行中则 kill 并等待结束
    void CancelPack();

    // Find7zPath: 按已知路径 → 注册表(HKLM\SOFTWARE\7-Zip) → PATH 顺序查找 7z.exe
    static QString Find7zPath();

    // FormatOutputName: 按模板展开包名（不含扩展名）
    // pattern:   模板串，支持 %Project %YYYY %MM %DD %HH %MM %SS 占位符；
    //            %MM 按位置消歧 —— 出现 %HH 之前解析为「月」，之后解析为「分」；
    //            未知的 %XXX 原样保留
    // sourceDir: 源码目录，用于推导项目名（取叶子目录名，如 C:/work/MyApp → MyApp）
    // now:       时间基准，默认取当前时间（测试可注入固定值保证可重现）
    // 返回:      展开后的包名；pattern 为空时返回空串，由调用方回退到默认命名
    static QString FormatOutputName(const QString& pattern,
                                    const QString& sourceDir,
                                    const QDateTime& now = QDateTime::currentDateTime());

signals:
    void PackProgress(int percent);                    // 打包进度 0-100
    void PackFinished(const QString& outputPath, qint64 sizeBytes); // 打包完成
    void PackError(const QString& errorMsg);           // 打包出错

private slots:
    void OnProcessFinished(int exitCode, QProcess::ExitStatus status);
    void OnProcessError(QProcess::ProcessError error);

private:
    QString m_sourceDir;        // 源码目录
    QString m_outputDir;        // 输出目录
    QString m_outputName;       // 包名
    QStringList m_fileList;     // 文件列表（如为空则打包整个源码目录）
    QString m_sevenZipPath;     // 用户指定的 7z 路径（空则自动检测）
    bool m_excludeVcsDirs{true}; // 打包时排除 .git/.svn
    QProcess* m_process{nullptr};
};

#endif // PACKAGER_H
