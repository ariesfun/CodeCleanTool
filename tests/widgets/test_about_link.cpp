// 探针：深色主题样式表下，QLabel 里的超链接还能不能看出是链接
//
// 背景：设置页「关于软件」要展示项目地址，做成可点击超链接。
//       但全局深色样式表里有一条 `QLabel { color: #E0E0E0; }`，
//       它会作用于所有 QLabel —— 超链接文本会不会被一并染成同色、
//       从而"看着不像链接"，靠读代码推断不出来，必须实测。
//
//   本探针只复现【那条会干扰的规则】，不搬整套样式表：
//   要回答的是「样式表设了 color 之后，锚点自身的颜色还生效吗」，
//   与其它规则无关。
//
// 用例（三种写法各渲染一次，统计像素里有没有链接色）：
//   A 锚点内联 style="color:#4A9EFF"
//   B 锚点不指定颜色（走 QPalette::Link）
//   C 控件级样式表 QLabel{color:#4A9EFF} + 普通锚点
//
// 依赖：Qt5::Widgets（需要 QApplication 才能渲染）

#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QPalette>
#include <QPixmap>
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

// 把控件渲染成图片（QWidget::grab 不需要控件已显示）
static QImage Render(QLabel& label)
{
    label.adjustSize();
    return label.grab().toImage();
}

// 统计与目标色接近的像素数（抗锯齿会让字符边缘产生渐变，故用容差而非全等）
static int CountPixelsNear(const QImage& img, const QColor& target, int tolerance = 40)
{
    int count = 0;
    for (int y = 0; y < img.height(); ++y)
    {
        for (int x = 0; x < img.width(); ++x)
        {
            const QColor c = img.pixelColor(x, y);
            if (c.alpha() < 128) { continue; }   // 透明背景不计
            if (qAbs(c.red()   - target.red())   <= tolerance &&
                qAbs(c.green() - target.green()) <= tolerance &&
                qAbs(c.blue()  - target.blue())  <= tolerance)
            {
                ++count;
            }
        }
    }
    return count;
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    std::cout << "=== 深色主题下超链接可辨识性探针 ===" << std::endl;
    std::cout << std::endl;

    // 复现会干扰的那一条规则
    qApp->setStyleSheet("QLabel { color: #E0E0E0; background: transparent; }");
    const QColor kDarkText(0xE0, 0xE0, 0xE0);   // 样式表设的前景色
    const QColor kLinkBlue(0x4A, 0x9E, 0xFF);   // 打算用的链接色

    // ---- A：锚点内联 style 指定颜色 ----
    {
        QLabel label;
        label.setText(QStringLiteral(
            "<a href=\"https://example.com\" style=\"color:#4A9EFF;\">example.com</a>"));
        const QImage img = Render(label);

        const int linkPx = CountPixelsNear(img, kLinkBlue);
        std::cout << "       A 内联 style：链接色像素 " << linkPx << " 个" << std::endl;
        Check(linkPx > 0,
              "A: 锚点内联 style 指定的颜色在样式表 color 之下仍然生效");
    }

    // ---- B：锚点不指定颜色，走调色板的链接色 ----
    {
        QLabel label;
        label.setText(QStringLiteral("<a href=\"https://example.com\">example.com</a>"));
        const QImage img = Render(label);

        const QColor linkColor = label.palette().color(QPalette::Link);
        const int linkPx = CountPixelsNear(img, linkColor);
        const int textPx = CountPixelsNear(img, kDarkText);
        std::cout << "       B 调色板 Link 色 " << linkColor.name().toStdString()
                  << "：匹配像素 " << linkPx << " 个；样式表前景色像素 " << textPx << " 个" << std::endl;
        Check(linkPx > 0,
              "B: 不指定颜色时，锚点走调色板 Link 色，未被样式表前景色覆盖");
    }

    // ---- C：控件级样式表指定颜色 + 普通锚点 ----
    {
        QLabel label;
        label.setText(QStringLiteral("<a href=\"https://example.com\">example.com</a>"));
        label.setStyleSheet("QLabel { color: #4A9EFF; }");
        const QImage img = Render(label);

        const int linkPx = CountPixelsNear(img, kLinkBlue);
        const int paletteLinkPx = CountPixelsNear(img, label.palette().color(QPalette::Link));
        std::cout << "       C 控件级样式表：指定色像素 " << linkPx
                  << " 个，调色板 Link 色像素 " << paletteLinkPx << " 个" << std::endl;

        // 实测结论：控件级样式表【改不动】锚点颜色 —— 富文本里的锚点始终取调色板 Link 色。
        // 所以要换链接颜色只有一条路：在 <a> 上写内联 style（见用例 A）。
        Check(linkPx == 0,
              "C: 控件级样式表的 color 对锚点无效（指定色一个像素都没有）");
        Check(paletteLinkPx > 0,
              "C: 锚点仍按调色板 Link 色渲染 —— 故改色只能靠内联 style，不能靠控件样式表");
    }

    // ---- D：点击跳转的接线 ----
    // 决定「点击能否打开浏览器」的是 openExternalLinks：
    //   false（默认）→ 点击只发 linkActivated 信号，没人接就什么也不发生
    //   true         → 交给系统默认浏览器打开
    // 另外 QLabel 默认已带 LinksAccessibleByMouse，故"能不能点中"本就不成问题，
    // 真正要显式设的只有 openExternalLinks。
    {
        QLabel label;
        label.setText(QStringLiteral("<a href=\"https://example.com\">example.com</a>"));
        label.setTextInteractionFlags(Qt::TextBrowserInteraction);
        label.setOpenExternalLinks(true);

        Check(label.openExternalLinks(),
              "D: 已开启 openExternalLinks —— 点击由系统浏览器打开");
        Check(label.textInteractionFlags().testFlag(Qt::LinksAccessibleByMouse),
              "D: 链接可被鼠标点中（LinksAccessibleByMouse）");
        Check(label.textInteractionFlags().testFlag(Qt::LinksAccessibleByKeyboard),
              "D: 链接也可被键盘访问（LinksAccessibleByKeyboard）");

        // 对照：默认 QLabel 能点中链接，但不会打开浏览器 —— 故 openExternalLinks 必须显式开启
        QLabel plain;
        plain.setText(QStringLiteral("<a href=\"https://example.com\">example.com</a>"));
        Check(plain.textInteractionFlags().testFlag(Qt::LinksAccessibleByMouse),
              "D: 对照 —— 默认 QLabel 本就允许鼠标点链接");
        Check(!plain.openExternalLinks(),
              "D: 对照 —— 但默认不会打开浏览器（点击仅发 linkActivated），"
              "故必须显式 setOpenExternalLinks(true)");
    }

    std::cout << std::endl;
    std::cout << "=== 结果: " << g_passCount << " 通过, " << g_failCount << " 失败 ===" << std::endl;
    return g_failCount > 0 ? 1 : 0;
}
