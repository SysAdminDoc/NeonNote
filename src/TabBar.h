#pragma once
#include <QTabBar>
#include <QPainter>
#include <QMouseEvent>

class NeonTabBar : public QTabBar {
    Q_OBJECT
public:
    explicit NeonTabBar(QWidget* parent = nullptr);

    QRect newTabButtonRect() const;

signals:
    void newTabRequested();
    void middleClicked(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    QSize tabSizeHint(int index) const override;
    QSize minimumTabSizeHint(int index) const override;

private:
    QRect closeButtonRect(int index) const;
    int m_hoverIndex = -1;
    int m_closeHoverIndex = -1;
    bool m_newTabHover = false;

    static constexpr int TAB_HEIGHT = 34;
    static constexpr int TAB_MIN_WIDTH = 60;
    static constexpr int TAB_MAX_WIDTH = 240;
    static constexpr int TAB_RADIUS = 8;
    static constexpr int ACCENT_HEIGHT = 2;
    static constexpr int NEWTAB_BTN_SIZE = 28;
    static constexpr int CLOSE_BTN_SIZE = 16;
};
