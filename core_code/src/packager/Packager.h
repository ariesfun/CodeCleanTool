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
    // SetExcludeList: 设置打包时要【排除】的路径（选填，空列表则不排除任何项）
    // paths: 绝对路径列表，内部换算成相对源码目录的路径再交给 7z 的 -x（排除）
    // 语义说明：传进来的是「判定为冗余、准备清理的项」，排除掉它们才得到纯源码包。
    //           与「传待打包文件白名单」相反 —— 结果模型里只有清理目标，
    //           拿它当白名单会打出只剩垃圾的包。
    void SetExcludeList(const QStringList& paths);
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
    // WriteExcludeListFile: 把排除项写成 7z 的 @listfile，返回文件路径；无需排除时返回空串
    // outputPath: 本次要写出的压缩包路径，若它落在源码目录内则一并排除（否则会自己打包自己）
    QString WriteExcludeListFile(const QString& outputPath);
    // RemoveListFile: 删除 WriteExcludeListFile 写出的临时文件
    void RemoveListFile();

    QString m_sourceDir;        // 源码目录
    QString m_outputDir;        // 输出目录（空则由 StartPack 回退到源码目录的上一级）
    QString m_outputName;       // 包名（不含扩展名，空则由 StartPack 回退到默认模板）
    QString m_outputPath;       // 本次打包实际写出的 .7z 路径（StartPack 解析后填，完成回调回读）
    QStringList m_excludeList;  // 待排除的绝对路径（勾选的待清理项）
    QString m_listFilePath;     // @listfile 临时文件路径（进程结束后清理）
    QString m_sevenZipPath;     // 用户指定的 7z 路径（空则自动检测）
    bool m_excludeVcsDirs{true}; // 打包时排除 .git/.svn
    QProcess* m_process{nullptr};
};

#endif // PACKAGER_H
