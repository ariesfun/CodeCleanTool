// 探针：扫描结果表的排序
//
// 结果表有两层排序语义：
//   1) 类别优先 —— 编译产物、构建目录等大件排在前面（清理时优先处理体积大的）
//   2) 同类别内按点击的列排序
//
// 第 2 层容易被写坏：大小列在表格里显示的是 "1.5 MB" 这类字符串，
// 若排序时取的是显示文本，就变成字典序 —— "999 B" 会排在 "1.0 KB" 之后。
//
// 用例：
//   A 同类别内按大小升序，应按【字节数】而非显示字符串比较
//   B 同类别内按大小降序
//   C 类别优先不被破坏：优先级高的行即使更大也排在前面
//   D 按修改时间排序
//
// 依赖：Qt5::Core

#include <QCoreApplication>
#include <QDateTime>
#include <iostream>

#include "model/ResultModel.h"
#include "model/ResultSortModel.h"

static int g_passCount = 0;
static int g_failCount = 0;

static void Check(bool condition, const QString& description)
{
    if (condition)
    {
        std::cout << "[PASS] " << description.toStdString() << std::endl;
        ++g_passCount;
    }
    else
    {
        std::cout << "[FAIL] " << description.toStdString() << std::endl;
        ++g_failCount;
    }
}

// 造一条记录：类别相同（同 sortPriority），只有大小与时间不同
static FileItem MakeItem(const QString& name, qint64 size, int priority,
                         const QDateTime& modified)
{
    FileItem item;
    item.fileName = name;
    item.filePath = "C:/proj/" + name;
    item.fileSize = size;
    item.dateModified = modified;
    item.fileType = (priority == 0) ? "编译产物" : "IDE缓存";
    item.hitRule = "*.obj";
    item.sortPriority = priority;
    item.checked = true;
    item.isDir = false;
    return item;
}

// 按代理模型的当前行序取出各项的文件名与大小
static void ReadOrder(ResultSortModel& proxy, ResultModel& model,
                      QStringList& names, QList<qint64>& sizes)
{
    names.clear();
    sizes.clear();
    for (int row = 0; row < proxy.rowCount(); ++row)
    {
        const int srcRow = proxy.mapToSource(proxy.index(row, 0)).row();
        const FileItem item = model.GetFile(srcRow);
        names << item.fileName;
        sizes << item.fileSize;
    }
}

// 判断一列数是否单调不减
static bool IsAscending(const QList<qint64>& v)
{
    for (int i = 1; i < v.size(); ++i)
    {
        if (v[i - 1] > v[i]) { return false; }
    }
    return true;
}

// 判断一列数是否单调不增
static bool IsDescending(const QList<qint64>& v)
{
    for (int i = 1; i < v.size(); ++i)
    {
        if (v[i - 1] < v[i]) { return false; }
    }
    return true;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== 结果表排序探针 ===" << std::endl;
    std::cout << std::endl;

    const QDateTime base = QDateTime::fromString("2026-09-21T10:00:00", Qt::ISODate);

    // 三个同类别项，大小刻意选成「字典序与数值序相反」：
    //   显示文本分别是 "999 B"、"1.0 KB"、"1.5 MB"，字典序下 "1.0 KB" 排在 "999 B" 前面
    ResultModel model;
    model.AddFile(MakeItem("small.obj", 999, 0, base.addSecs(3600)));
    model.AddFile(MakeItem("mid.obj", 1024, 0, base));
    model.AddFile(MakeItem("big.obj", 1572864, 0, base.addSecs(7200)));

    Check(model.data(model.index(0, ResultModel::ColSize), Qt::DisplayRole).toString() == "999 B",
          "前置: 999 字节显示为 \"999 B\"");
    Check(model.data(model.index(1, ResultModel::ColSize), Qt::DisplayRole).toString() == "1.0 KB",
          "前置: 1024 字节显示为 \"1.0 KB\"（字典序小于 \"999 B\"）");

    ResultSortModel proxy;
    proxy.setSourceModel(&model);

    // ---- A：按大小升序 ----
    {
        proxy.sort(ResultModel::ColSize, Qt::AscendingOrder);
        QStringList names;
        QList<qint64> sizes;
        ReadOrder(proxy, model, names, sizes);
        std::cout << "       按大小升序: " << names.join(" | ").toStdString() << std::endl;
        Check(sizes.size() == 3, "A: 排序后行数不变");
        Check(IsAscending(sizes), "A: 同类别内按字节数升序（不是按 \"1.5 MB\" 这类显示文本的字典序）");
        Check(names.value(0) == "small.obj", "A: 最小项 small.obj(999 B) 排最前");
        Check(names.value(2) == "big.obj", "A: 最大项 big.obj(1.5 MB) 排最后");
    }

    // ---- B：按大小降序 ----
    {
        proxy.sort(ResultModel::ColSize, Qt::DescendingOrder);
        QStringList names;
        QList<qint64> sizes;
        ReadOrder(proxy, model, names, sizes);
        std::cout << "       按大小降序: " << names.join(" | ").toStdString() << std::endl;
        Check(IsDescending(sizes), "B: 同类别内按字节数降序");
        Check(names.value(0) == "big.obj", "B: 最大项排最前");
    }

    // ---- C：类别优先不被破坏 ----
    {
        // 再放一条 IDE缓存（优先级 3），大小故意取得很小
        ResultModel mixed;
        mixed.AddFile(MakeItem("cache.suo", 10, 3, base));
        mixed.AddFile(MakeItem("huge.obj", 99999999, 0, base));

        ResultSortModel p2;
        p2.setSourceModel(&mixed);
        p2.sort(ResultModel::ColSize, Qt::AscendingOrder);

        QStringList names;
        QList<qint64> sizes;
        ReadOrder(p2, mixed, names, sizes);
        std::cout << "       混合类别按大小升序: " << names.join(" | ").toStdString() << std::endl;
        Check(names.value(0) == "huge.obj",
              "C: 类别优先 —— 编译产物(1 亿字节)仍排在 IDE缓存(10 字节)之前");
    }

    // ---- D：按修改时间排序 ----
    {
        proxy.sort(ResultModel::ColDate, Qt::AscendingOrder);
        QStringList names;
        QList<qint64> sizes;
        ReadOrder(proxy, model, names, sizes);
        std::cout << "       按修改时间升序: " << names.join(" | ").toStdString() << std::endl;
        Check(names.value(0) == "mid.obj", "D: 时间最早项 mid.obj(10:00) 排最前");
        Check(names.value(1) == "small.obj", "D: 时间居中项 small.obj(11:00) 排中间");
        Check(names.value(2) == "big.obj", "D: 时间最晚项 big.obj(12:00) 排最后");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount > 0 ? 1 : 0;
}
