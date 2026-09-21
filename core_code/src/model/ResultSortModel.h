#ifndef RESULTSORTMODEL_H
#define RESULTSORTMODEL_H

#include <QSortFilterProxyModel>

// 结果表排序代理：先按清理目标类别分组（编译产物、构建目录等大件靠前），
// 同一类别内再按用户点击的列排序。
//
// 调用链：MainWindow::InitPresenter 创建并 setSourceModel(ResultModel)
//        → QTableView::setSortingEnabled(true) 点击表头 → sort() → lessThan()
//
// 放在 model/ 而非 View 层：它只做数据比较、不触碰任何 Widgets，
// 且无 GUI 的探针需要直接验证排序结果。
//
// 关键约束：同类别内的比较必须取 SortValueRole（原始值）。
//   若直接委托给基类而排序角色仍是 SortPriorityRole，同类别内所有项相等，
//   点击"大小"表头不会有任何效果；若改用 DisplayRole，大小列又会按
//   "1.5 MB" 这类字符串的字典序排（"999 B" 排在 "1.0 KB" 之后）。
class ResultSortModel : public QSortFilterProxyModel
{
public:
    // parent: Qt 父对象，用于生命周期管理
    explicit ResultSortModel(QObject* parent = nullptr);

protected:
    // lessThan: 类别优先级不同时按优先级升序；相同时按当前排序列的原始值比较
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};

#endif // RESULTSORTMODEL_H
