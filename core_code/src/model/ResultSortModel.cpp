// 结果表排序代理实现：类别优先级优先，同类别内按排序列比较
#include "ResultSortModel.h"

#include "ResultModel.h"

ResultSortModel::ResultSortModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    // 排序角色取原始值而非显示文本。基类的 lessThan 正是用排序角色取值的：
    //   取显示文本 → 大小列按 "1.5 MB" 这类字符串的字典序排（"999 B" 落在 "1.0 KB" 之后）
    //   取类别优先级 → 同类别内所有项相等，「点击表头排序」完全失效
    setSortRole(ResultModel::SortValueRole);
}

bool ResultSortModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    // 第一层：类别优先级（编译产物/构建目录等大件靠前）
    const int leftPrio = sourceModel()->data(left, ResultModel::SortPriorityRole).toInt();
    const int rightPrio = sourceModel()->data(right, ResultModel::SortPriorityRole).toInt();
    if (leftPrio != rightPrio)
    {
        return leftPrio < rightPrio;
    }

    // 第二层：同类别内按当前排序列的原始值比较（交由基类按 sortRole 取值）
    return QSortFilterProxyModel::lessThan(left, right);
}
