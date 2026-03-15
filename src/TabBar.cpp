#include "TabBar.h"
#include "Theme.h"
#include <QPainterPath>
#include <QStyleOption>
#include <QFileInfo>

NeonTabBar::NeonTabBar(QWidget* parent) : QTabBar(parent)
{
    setMovable(true);
    setTabsClosable(false);
    setExpanding(false);
    setMouseTracking(true);
    setElideMode(Qt::ElideRight);
    setDocumentMode(true);
    setDrawBase(false);

    setStyleSheet("QTabBar { background: transparent; border: none; }"
                  "QTabBar::tab { background: transparent; border: none; padding: 0; margin: 0; }"
                  "QTabBar::close-button { image: none; width: 0; height: 0; }");
}

QSize NeonTabBar::tabSizeHint(int index) const
{
    Q_UNUSED(index)
    int w = qBound(TAB_MIN_WIDTH, width() / qMax(count(), 1) - 4, TAB_MAX_WIDTH);
    return {w, TAB_HEIGHT};
}

QSize NeonTabBar::minimumTabSizeHint(int index) const
{
    Q_UNUSED(index)
    return {TAB_MIN_WIDTH, TAB_HEIGHT};
}

QRect NeonTabBar::newTabButtonRect() const
{
    int x = 8;
    if (count() > 0) {
        QRect last = tabRect(count() - 1);
        x = last.right() + 8;
    }
    int y = (TAB_HEIGHT - NEWTAB_BTN_SIZE) / 2;
    return {x, y, NEWTAB_BTN_SIZE, NEWTAB_BTN_SIZE};
}

QRect NeonTabBar::closeButtonRect(int index) const
{
    QRect tr = tabRect(index);
    int x = tr.right() - CLOSE_BTN_SIZE - 8;
    int y = tr.top() + (tr.height() - CLOSE_BTN_SIZE) / 2;
    return {x, y, CLOSE_BTN_SIZE, CLOSE_BTN_SIZE};
}

void NeonTabBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    p.fillRect(rect(), Theme::Crust);

    for (int i = 0; i < count(); ++i) {
        QRect tr = tabRect(i);
        bool active = (i == currentIndex());
        bool hover = (i == m_hoverIndex && !active);

        QPainterPath path;
        QRect body(tr.left() + 2, tr.top(), tr.width() - 4, tr.height());
        path.addRoundedRect(body, TAB_RADIUS, TAB_RADIUS);

        if (active) {
            p.fillPath(path, Theme::Surface0);
            QLinearGradient glow(body.left(), body.bottom() - 4, body.left(), body.bottom());
            glow.setColorAt(0.0, QColor(137, 180, 250, 0));
            glow.setColorAt(1.0, QColor(137, 180, 250, 40));
            p.fillPath(path, glow);
        } else if (hover) {
            p.fillPath(path, QColor(49, 50, 68, 120));
        }

        // Language accent strip
        QString tabText = this->tabText(i);
        QFileInfo fi(tabText);
        QString ext = fi.suffix();
        if (!ext.isEmpty()) {
            QColor accent = Theme::accentForExtension(ext);
            QRect strip(body.left() + TAB_RADIUS, body.top(), body.width() - TAB_RADIUS * 2, ACCENT_HEIGHT);
            p.fillRect(strip, accent);
        }

        // Tab text
        p.setPen(active ? Theme::Text : (hover ? Theme::Subtext0 : Theme::Overlay1));
        QFont f = font();
        f.setPointSize(9);
        p.setFont(f);
        QRect textRect = body.adjusted(12, 0, -(CLOSE_BTN_SIZE + 12), 0);
        QString elidedText = p.fontMetrics().elidedText(tabText, Qt::ElideRight, textRect.width());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, elidedText);

        // Close button (X)
        if (active || hover) {
            QRect cbr = closeButtonRect(i);
            bool closeHover = (i == m_closeHoverIndex);

            if (closeHover) {
                QPainterPath closePath;
                closePath.addRoundedRect(QRectF(cbr), 4, 4);
                p.fillPath(closePath, QColor(243, 139, 168, 60));
            }

            p.setPen(QPen(closeHover ? Theme::Red : Theme::Overlay0, 1.5));
            int m = 4;
            p.drawLine(cbr.left() + m, cbr.top() + m, cbr.right() - m, cbr.bottom() - m);
            p.drawLine(cbr.right() - m, cbr.top() + m, cbr.left() + m, cbr.bottom() - m);
        }

        // Separator lines between inactive tabs
        if (!active && i > 0 && (i - 1) != currentIndex()) {
            p.setPen(QPen(Theme::Surface1, 1));
            p.drawLine(tr.left(), tr.top() + 10, tr.left(), tr.bottom() - 10);
        }
    }

    // New tab (+) button
    QRect nbr = newTabButtonRect();
    QPainterPath btnPath;
    btnPath.addRoundedRect(QRectF(nbr), 6, 6);
    p.fillPath(btnPath, m_newTabHover ? Theme::Surface0 : Qt::transparent);
    p.setPen(QPen(m_newTabHover ? Theme::Text : Theme::Overlay0, 1.5));
    int cx = nbr.center().x(), cy = nbr.center().y();
    int half = 6;
    p.drawLine(cx - half, cy, cx + half, cy);
    p.drawLine(cx, cy - half, cx, cy + half);
}

void NeonTabBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        for (int i = 0; i < count(); ++i) {
            if (closeButtonRect(i).contains(event->pos())) {
                emit tabCloseRequested(i);
                return;
            }
        }
        if (newTabButtonRect().contains(event->pos())) {
            emit newTabRequested();
            return;
        }
    }

    // Middle-click on tab to close
    if (event->button() == Qt::MiddleButton) {
        for (int i = 0; i < count(); ++i) {
            if (tabRect(i).contains(event->pos())) {
                emit middleClicked(i);
                return;
            }
        }
        // Middle-click on empty space opens new tab
        emit newTabRequested();
        return;
    }

    QTabBar::mousePressEvent(event);
}

void NeonTabBar::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        // Check if double-click is on a tab — don't maximize
        for (int i = 0; i < count(); ++i) {
            if (tabRect(i).contains(event->pos())) {
                QTabBar::mouseDoubleClickEvent(event);
                return;
            }
        }
        // Double-click on empty tab bar space — let WM_NCHITTEST handle maximize
        // by not consuming the event
    }
    QTabBar::mouseDoubleClickEvent(event);
}

void NeonTabBar::mouseMoveEvent(QMouseEvent* event)
{
    int oldHover = m_hoverIndex;
    int oldClose = m_closeHoverIndex;
    bool oldNewTab = m_newTabHover;

    m_hoverIndex = -1;
    m_closeHoverIndex = -1;
    m_newTabHover = newTabButtonRect().contains(event->pos());

    for (int i = 0; i < count(); ++i) {
        if (tabRect(i).contains(event->pos())) {
            m_hoverIndex = i;
            if (closeButtonRect(i).contains(event->pos()))
                m_closeHoverIndex = i;
            break;
        }
    }

    if (m_hoverIndex != oldHover || m_closeHoverIndex != oldClose || m_newTabHover != oldNewTab)
        update();

    QTabBar::mouseMoveEvent(event);
}

void NeonTabBar::leaveEvent(QEvent* event)
{
    m_hoverIndex = -1;
    m_closeHoverIndex = -1;
    m_newTabHover = false;
    update();
    QTabBar::leaveEvent(event);
}
