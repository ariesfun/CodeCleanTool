#ifndef STATSWIDGET_H
#define STATSWIDGET_H

#include <QWidget>

// 环形图统计控件：显示原项目大小 vs 可清理大小的甜甜圈图
// 调用链：MainPresenter::StatsDataChanged → UpdateStats → repaint
// 线程约束：仅在 UI 主线程使用
class StatsWidget : public QWidget
{
    Q_OBJECT

public:
    // 构造：设置固定高度230px、最小宽度200px，初始隐藏（无数据时不占空间）
    explicit StatsWidget(QWidget* parent = nullptr);

    // UpdateStats: 更新统计数据并触发重绘
    // totalSize: 项目目录总大小（字节）
    // cleanableSize: 可清理文件总大小（字节）
    // 前置条件：totalSize > 0 时环形图有效；为0时百分比归零并隐藏控件
    void UpdateStats(qint64 totalSize, qint64 cleanableSize);

    // 返回建议尺寸 280x230，布局系统计算初始空间时使用
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    qint64 m_totalSize{0};      // 原项目总大小（字节）
    qint64 m_cleanableSize{0};  // 可清理大小（字节）
    int m_percentage{0};        // 可清理百分比 0-100
};

#endif // STATSWIDGET_H
