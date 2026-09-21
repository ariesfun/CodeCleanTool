// 探针：结果模型的大小统计语义
//
// 起因：清理完成后要刷新瘦身统计环形图，需要知道「本次释放了多少字节」。
//       MainPresenter 原先用 `项目总大小 - 剩余项大小` 计算，但两个数不是一回事：
//         - 项目总大小：扫描时统计的【全部文件】体积（含源码）
//         - 剩余项大小：清理后模型里剩下的【待清理项】体积
//       二者相减得不到「已释放字节数」。
//
// 本探针把模型能提供的三个量固定下来，明确「已释放」只能由删除前后之差得到。
//
// 用例：
//   A TotalSize 累加全部行
//   B RemoveFile 之后 TotalSize 随之减少
//   C 已释放字节数 = 删除前 TotalSize - 删除后 TotalSize
//   D 对照：用「项目总大小 - 剩余项大小」会得到错误答案
//   E SortValueRole 给出可比较的原始值
//
// 依赖：Qt5::Core

#include <QCoreApplication>
#include <QDateTime>
#include <iostream>

#include "model/ResultModel.h"

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

static FileItem MakeItem(const QString& name, qint64 size, bool checked)
{
    FileItem item;
    item.fileName = name;
    item.filePath = "C:/proj/" + name;
    item.fileSize = size;
    item.dateModified = QDateTime::fromString("2026-09-21T10:00:00", Qt::ISODate);
    item.fileType = "编译产物";
    item.hitRule = "*.obj";
    item.sortPriority = 0;
    item.checked = checked;
    item.isDir = false;
    return item;
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== 结果模型大小统计探针 ===" << std::endl;
    std::cout << std::endl;

    // ---- A：TotalSize 累加全部行 ----
    ResultModel model;
    model.AddFile(MakeItem("a.obj", 1000, true));    // 已勾选：待清理
    model.AddFile(MakeItem("b.obj", 3000, false));   // 未勾选：不清理
    model.AddFile(MakeItem("c.obj", 6000, true));    // 已勾选：待清理

    Check(model.TotalCount() == 3, "A: 共 3 行");
    Check(model.TotalSize() == 10000, "A: TotalSize 累加全部行 = 10000");
    Check(model.CheckedCount() == 2, "A: 勾选数为 2");

    // ---- B/C：清理后模型只剩未勾选项，已释放字节数由删除前后之差得出 ----
    const qint64 sizeBeforeRemoval = model.TotalSize();     // 10000

    // 倒序遍历移除勾选行（与 MainPresenter::OnClean 的处理顺序一致）
    for (int i = model.TotalCount() - 1; i >= 0; --i)
    {
        if (model.GetFile(i).checked)
        {
            model.RemoveFile(i);
        }
    }

    const qint64 remainSize = model.TotalSize();
    const qint64 deletedSize = sizeBeforeRemoval - remainSize;

    Check(model.TotalCount() == 1, "B: 移除后只剩 1 行");
    Check(remainSize == 3000, "B: 移除后 TotalSize = 3000");
    Check(deletedSize == 7000, "C: 已释放字节数 = 删除前 10000 - 删除后 3000 = 7000");

    // ---- D：对照 —— 「项目总大小 - 剩余项大小」得不出已释放字节数 ----
    // 项目总大小含源码，必然 >= 模型里所有清理项之和，故这个减法恒偏大
    {
        const qint64 projectTotal = 100000;   // 扫描时统计的全项目体积（含源码）
        const qint64 naiveFreed = projectTotal - remainSize;
        std::cout << "       项目总大小 100000 - 剩余项 3000 = " << naiveFreed
                  << "（真实已释放 " << deletedSize << "）" << std::endl;
        Check(naiveFreed != deletedSize,
              "D: 「项目总大小 - 剩余项大小」不等于已释放字节数（该算法错误）");

        // 全部勾选是最常见的情形：剩余项为 0，错误算法会得出「一点没释放」
        ResultModel allChecked;
        allChecked.AddFile(MakeItem("x.obj", 7000, true));
        allChecked.RemoveFile(0);
        const qint64 allFreed = projectTotal - allChecked.TotalSize();
        std::cout << "       全部勾选时：错误算法得 " << allFreed
                  << "，应为 " << (projectTotal - 7000) << std::endl;
        Check(allFreed == projectTotal,
              "D: 全部勾选时错误算法仍得出项目体积 100000（真实应为 93000）—— 症状是"
              "「清理全部垃圾后环形图纹丝不动」");
    }

    // ---- E：SortValueRole 给出可比较的原始值 ----
    {
        ResultModel m;
        m.AddFile(MakeItem("s.obj", 1024, true));

        const QVariant sizeVal = m.data(m.index(0, ResultModel::ColSize), ResultModel::SortValueRole);
        Check(sizeVal.type() == QVariant::LongLong,
              QString("E: 大小列的 SortValueRole 是数值（实际 %1）").arg(sizeVal.typeName()));
        Check(sizeVal.toLongLong() == 1024, "E: 大小列的原始值 = 字节数 1024");
        Check(m.data(m.index(0, ResultModel::ColSize), Qt::DisplayRole).toString() == "1.0 KB",
              "E: 同一格的显示文本仍是 \"1.0 KB\"（原始值与显示文本分离）");

        const QVariant dateVal = m.data(m.index(0, ResultModel::ColDate), ResultModel::SortValueRole);
        Check(dateVal.type() == QVariant::DateTime,
              QString("E: 修改时间列的 SortValueRole 是 QDateTime（实际 %1）").arg(dateVal.typeName()));

        // 其余列没有专门的原始形态，回落到显示文本
        const QVariant nameVal = m.data(m.index(0, ResultModel::ColName), ResultModel::SortValueRole);
        Check(nameVal.toString() == "s.obj", "E: 文件名列的 SortValueRole 回落到显示文本");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount > 0 ? 1 : 0;
}
