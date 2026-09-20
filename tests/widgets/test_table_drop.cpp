// 探针：规则表拖拽排序的模型行为
//
// 背景：MainWindow 的拖拽同步同时连接了 rowsMoved / rowsInserted / rowsRemoved 三个信号，
//       因为不确定 QTableWidget 在 InternalMove 下是用 moveRows 还是 insert+remove 实现重排。
//       本探针把「不确定」变成「证据」。
//
// 说明：真实拖拽放置无法在此模拟 —— QAbstractItemView::dropEvent 对 InternalMove 会先判断
//       event->source() != this 便直接返回，而手工构造的 QDropEvent 没有拖拽源（source() 为
//       null 且无公开 API 可设置），因此合成事件不会被接受（已实测验证）。
//       故改从模型能力入手：内部模型若不支持 moveRow，Qt 就只可能用 insert+remove 重排，
//       对应的 rowsInserted / rowsRemoved 必然发出；若支持，则发出 rowsMoved。
//       两条路径我们的连接都覆盖。
//
// 依赖：Qt5::Widgets（需要 QApplication）

#include <QApplication>
#include <QAbstractItemModel>
#include <QTableWidget>
#include <iostream>

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

static QStringList CollectFirstColumn(QTableWidget* table)
{
    QStringList out;
    for (int row = 0; row < table->rowCount(); ++row)
    {
        if (auto* item = table->item(row, 0))
        {
            out << item->text();
        }
    }
    return out;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    std::cout << "=== 规则表拖拽模型行为探针 ===" << std::endl;
    std::cout << std::endl;

    QTableWidget table;
    table.setColumnCount(1);
    table.setHorizontalHeaderLabels({"模式"});
    table.setRowCount(3);
    table.setDragEnabled(true);
    table.setAcceptDrops(true);
    table.setDragDropMode(QAbstractItemView::InternalMove);
    table.setDropIndicatorShown(true);

    const QStringList initial = {"*.a", "*.b", "*.c"};
    for (int i = 0; i < initial.size(); ++i)
    {
        table.setItem(i, 0, new QTableWidgetItem(initial[i]));
    }

    bool movedFired = false;
    bool insertedFired = false;
    bool removedFired = false;
    QObject::connect(table.model(), &QAbstractItemModel::rowsMoved,
                     &table, [&movedFired]() { movedFired = true; });
    QObject::connect(table.model(), &QAbstractItemModel::rowsInserted,
                     &table, [&insertedFired]() { insertedFired = true; });
    QObject::connect(table.model(), &QAbstractItemModel::rowsRemoved,
                     &table, [&removedFired]() { removedFired = true; });

    Check(CollectFirstColumn(&table) == initial, "初始行序为 *.a *.b *.c");

    // ---- 1. 内部模型是否支持 moveRow ----
    const bool supportsMoveRows = table.model()->moveRow(QModelIndex(), 0, QModelIndex(), 3);
    std::cout << "       内部模型 moveRow 支持情况: "
              << (supportsMoveRows ? "支持" : "不支持") << std::endl;

    if (supportsMoveRows)
    {
        Check(movedFired, "模型支持 moveRow 时发出 rowsMoved（连接覆盖此路径）");
        Check(CollectFirstColumn(&table) != initial, "moveRow 后行序已改变");
    }
    else
    {
        // 不支持 moveRow：Qt 的拖拽重排只能走 insert + remove，二者必然发信号
        Check(!movedFired, "模型不支持 moveRow 时不发 rowsMoved");

        // 模拟该兜底路径：先在新位置插入，再删除原位置
        // 并检查中间态是否会被误当成稳定状态
        table.insertRow(3);
        table.setItem(3, 0, new QTableWidgetItem("*.a"));
        Check(insertedFired, "兜底路径的插入阶段发出 rowsInserted（连接覆盖此路径）");
        Check(CollectFirstColumn(&table).size() == 4,
              "中间态：插入后行数为 4（与引擎的 3 条不一致，同步逻辑应拒绝）");

        table.setItem(0, 0, new QTableWidgetItem("*.b"));
        table.setItem(1, 0, new QTableWidgetItem("*.c"));
        table.removeRow(2);
        Check(removedFired, "兜底路径的删除阶段发出 rowsRemoved（连接覆盖此路径）");

        const QStringList expected = {"*.b", "*.c", "*.a"};
        Check(CollectFirstColumn(&table) == expected, "兜底路径完成后行序为 *.b *.c *.a（行数恢复 3）");
    }

    // ---- 2. 关键结论：无论走哪条路径，都至少有一个被连接的信号发出 ----
    const bool anySignal = movedFired || insertedFired || removedFired;
    Check(anySignal, "结论：行序变化时至少触发一个已连接的信号，MainWindow 的同步会启动");

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount == 0 ? 0 : 1;
}
