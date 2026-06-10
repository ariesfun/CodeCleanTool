// 瘦身统计环形图控件实现：QPainter 绘制甜甜圈图 + 中心百分比文字 + 底部三行图例
// 调用链：MainPresenter::StatsDataChanged → UpdateStats → update() → paintEvent
// 主题感知：通过 eTheme 实时查询深色/浅色模式，重绘时动态适配颜色方案
#include "StatsWidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

#include "ElaTheme.h"

// 将字节数转为人类可读的大小字符串（B/KB/MB/GB），保留一位小数
static QString FormatSize(qint64 bytes)
{
    if (bytes < 1024)
    {
        return QString::number(bytes) + " B";
    }
    else if (bytes < 1024LL * 1024)
    {
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    }
    else if (bytes < 1024LL * 1024 * 1024)
    {
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    }
    else
    {
        return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
    }
}

// 判断当前是否为 ElaTheme 深色模式，用于选择文字/背景色系
static bool IsDarkTheme()
{
    return eTheme->getThemeMode() == ElaThemeType::Dark;
}

StatsWidget::StatsWidget(QWidget* parent)
    : QWidget(parent)
{
    // 固定高度 230px 确保布局稳定，初始隐藏避免无数据时显示空白环形图区域
    setFixedHeight(230);
    setVisible(false);
    setMinimumWidth(200);

    // 主题切换时触发重绘，避免缓存的 QPainter 颜色与当前主题不匹配
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this](ElaThemeType::ThemeMode)
    {
        update();
    });
}

// sizeHint 返回 280x230，布局系统计算右侧 Dock 初始宽度时使用
QSize StatsWidget::sizeHint() const
{
    return {280, 230};
}

// UpdateStats: 接收扫描/清理完成后的统计数据，更新内部状态并触发重绘
void StatsWidget::UpdateStats(qint64 totalSize, qint64 cleanableSize)
{
    m_totalSize = totalSize;
    m_cleanableSize = cleanableSize;
    // 百分比取整，totalSize 为 0 时默认 0 避免除零
    m_percentage = totalSize > 0 ? static_cast<int>(cleanableSize * 100 / totalSize) : 0;
    setVisible(true);
    update();  // 触发 paintEvent 重新绘制环形图
}

void StatsWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    // 抗锯齿确保环形图边缘平滑，无像素锯齿感
    painter.setRenderHint(QPainter::Antialiasing);

    bool dark = IsDarkTheme();

    // 主题感知色：深色主题用浅色文字+深灰底环；浅色主题用深色文字+浅灰底环
    QColor bgRingColor    = dark ? QColor("#404040") : QColor("#E8E8E8");
    QColor textColor      = dark ? QColor("#E0E0E0") : QColor("#333333");
    QColor labelColor     = dark ? QColor("#AAAAAA") : QColor("#666666");
    QColor subLabelColor  = dark ? QColor("#909090") : QColor("#888888");

    int w = width();
    int h = height();

    // 环形图区域（上部居中）：尺寸取控件高度的 55%，保证下方三行图例有足够空间
    int ringSize = qMin(100, h * 55 / 100);
    int ringX = (w - ringSize) / 2;
    int ringY = 8;
    // 环厚度 12px 适配窄面板（280px），与 100px 环形直径比例约 1:8
    int ringThickness = 12;
    QRectF ringRect(ringX, ringY, ringSize, ringSize);

    // 先绘制完整底环表示"保留部分"视觉占位，再覆盖橙红色可清理弧段
    painter.setPen(QPen(bgRingColor, ringThickness, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(ringRect, 0, 360 * 16);

    if (m_totalSize > 0 && m_cleanableSize > 0)
    {
        // 从12点方向(90°)顺时针绘制可清理弧段：QPainter 正角度为逆时针，取负值转为顺时针
        int spanAngle = static_cast<int>(m_cleanableSize * 360 / m_totalSize * 16);
        painter.setPen(QPen(QColor("#E67E22"), ringThickness, Qt::SolidLine, Qt::RoundCap));
        painter.drawArc(ringRect, 90 * 16, -spanAngle);
    }

    // 环形中心空白区叠加百分比数值 + "可瘦身"说明文字
    QRectF centerRect = ringRect.adjusted(ringThickness, ringThickness,
                                          -ringThickness, -ringThickness);
    painter.setPen(textColor);
    QFont pctFont = painter.font();
    pctFont.setPointSize(18);
    pctFont.setBold(true);
    painter.setFont(pctFont);
    painter.drawText(centerRect.adjusted(0, -4, 0, 0), Qt::AlignHCenter | Qt::AlignBottom,
                     QString("%1%").arg(m_percentage));

    QFont subFont = painter.font();
    subFont.setPointSize(8);
    subFont.setBold(false);
    painter.setFont(subFont);
    painter.setPen(subLabelColor);
    painter.drawText(centerRect.adjusted(0, 4, 0, 0), Qt::AlignHCenter | Qt::AlignTop,
                     "可瘦身");

    // 图例区域（环形图下方三行紧凑排列）：每行 = 色块 + 标签 + 格式化数值
    int legendY = ringY + ringSize + 10;
    int lineH = 28;
    int legendX = 8;

    auto drawLegendRow = [&](int y, const QColor& color, const QString& label,
                              qint64 size)
    {
        // 10x10 实心色块，与环形图颜色对应建立视觉关联
        painter.fillRect(legendX, y + 4, 10, 10, color);

        // 标签用 8px 浅色，与数值形成字体大小和粗细的二级层次
        QFont f = painter.font();
        f.setPointSize(8);
        painter.setFont(f);
        painter.setPen(labelColor);
        painter.drawText(legendX + 16, y, label);

        // 数值用 10px 加粗深色，突出可读性
        f.setBold(true);
        f.setPointSize(10);
        painter.setFont(f);
        painter.setPen(textColor);
        painter.drawText(legendX + 16, y + 13, FormatSize(size));
    };

    drawLegendRow(legendY, QColor("#E67E22"), "原项目大小", m_totalSize);
    // 剩余大小 = 项目总大小 - 可清理大小，取 max(0) 防止意外负值显示
    qint64 remaining = qMax(0LL, m_totalSize - m_cleanableSize);
    drawLegendRow(legendY + lineH, QColor("#27AE60"), "可释放空间", m_cleanableSize);
    drawLegendRow(legendY + lineH * 2, QColor("#3498DB"), "清理后剩余", remaining);
}
