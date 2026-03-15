#include "BookmarkBar.h"
#include "Theme.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFileInfo>

BookmarkBar::BookmarkBar(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFixedHeight(REVEAL_HEIGHT);

    m_collapseTimer.setSingleShot(true);
    m_collapseTimer.setInterval(300);
    connect(&m_collapseTimer, &QTimer::timeout, this, [this]() {
        if (m_expanded) {
            m_expanded = false;
            setFixedHeight(m_bookmarks.isEmpty() ? 0 : REVEAL_HEIGHT);
            emit heightChanged();
        }
    });
}

void BookmarkBar::addBookmark(const QString& filePath)
{
    if (hasBookmark(filePath)) return;
    BookmarkItem item;
    item.filePath = filePath;
    item.displayName = QFileInfo(filePath).fileName();
    m_bookmarks.append(item);
    recalcLayout();
    if (!m_expanded)
        setFixedHeight(REVEAL_HEIGHT);
    update();
}

void BookmarkBar::removeBookmark(int index)
{
    if (index < 0 || index >= m_bookmarks.size()) return;
    m_bookmarks.removeAt(index);
    m_hoverIndex = -1;
    recalcLayout();
    if (m_bookmarks.isEmpty()) {
        m_expanded = false;
        setFixedHeight(0);
    }
    update();
    emit heightChanged();
}

bool BookmarkBar::hasBookmark(const QString& filePath) const
{
    for (const auto& bm : m_bookmarks) {
        if (bm.filePath.compare(filePath, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

void BookmarkBar::recalcLayout()
{
    m_itemRects.clear();
    QFontMetrics fm(font());
    int x = BAR_LPAD;
    int h = expandedHeight();
    for (int i = 0; i < m_bookmarks.size(); ++i) {
        int w = fm.horizontalAdvance(m_bookmarks[i].displayName) + ITEM_HPAD * 2 + 18; // 18 for close btn
        QRect r(x, 2, w, h - 4);
        m_itemRects.append(r);
        x += w + ITEM_GAP;
    }
}

int BookmarkBar::hitTest(const QPoint& pos) const
{
    for (int i = 0; i < m_itemRects.size(); ++i) {
        if (m_itemRects[i].contains(pos))
            return i;
    }
    return -1;
}

QRect BookmarkBar::closeRect(int index) const
{
    if (index < 0 || index >= m_itemRects.size()) return {};
    QRect r = m_itemRects[index];
    return {r.right() - 16, r.top() + (r.height() - 12) / 2, 12, 12};
}

void BookmarkBar::enterEvent(QEnterEvent*)
{
    if (m_bookmarks.isEmpty()) return;
    m_collapseTimer.stop();
    if (!m_expanded) {
        m_expanded = true;
        setFixedHeight(expandedHeight());
        recalcLayout();
        emit heightChanged();
    }
}

void BookmarkBar::leaveEvent(QEvent*)
{
    m_hoverIndex = -1;
    m_closeHover = false;
    update();
    if (m_expanded)
        m_collapseTimer.start();
}

void BookmarkBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background: Mantle
    p.fillRect(rect(), Theme::Mantle);

    if (!m_expanded) {
        // Bottom border
        p.setPen(Theme::Surface0);
        p.drawLine(0, height() - 1, width(), height() - 1);
        return;
    }

    // Bottom border
    p.setPen(Theme::Surface0);
    p.drawLine(0, height() - 1, width(), height() - 1);

    // Draw bookmark items
    QFont f = font();
    f.setPointSize(8);
    p.setFont(f);

    for (int i = 0; i < m_bookmarks.size(); ++i) {
        bool hover = (i == m_hoverIndex);
        QRect r = m_itemRects[i];

        if (hover) {
            QPainterPath path;
            path.addRoundedRect(QRectF(r), 4, 4);
            p.fillPath(path, Theme::Surface0);
        }

        // Text
        p.setPen(hover ? Theme::Text : Theme::Subtext0);
        QRect textR = r.adjusted(ITEM_HPAD, 0, -20, 0);
        QString elided = p.fontMetrics().elidedText(m_bookmarks[i].displayName, Qt::ElideRight, textR.width());
        p.drawText(textR, Qt::AlignVCenter | Qt::AlignLeft, elided);

        // Close X (only on hover)
        if (hover) {
            QRect cr = closeRect(i);
            p.setPen(QPen(m_closeHover ? Theme::Red : Theme::Overlay0, 1.2));
            int m = 3;
            p.drawLine(cr.left() + m, cr.top() + m, cr.right() - m, cr.bottom() - m);
            p.drawLine(cr.right() - m, cr.top() + m, cr.left() + m, cr.bottom() - m);
        }
    }
}

void BookmarkBar::mouseMoveEvent(QMouseEvent* event)
{
    int oldHover = m_hoverIndex;
    bool oldClose = m_closeHover;
    m_hoverIndex = hitTest(event->pos());
    m_closeHover = (m_hoverIndex >= 0) && closeRect(m_hoverIndex).contains(event->pos());
    if (m_hoverIndex != oldHover || m_closeHover != oldClose)
        update();
}

void BookmarkBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;
    int idx = hitTest(event->pos());
    if (idx < 0) return;

    if (closeRect(idx).contains(event->pos())) {
        removeBookmark(idx);
    } else {
        emit bookmarkClicked(idx);
    }
}
