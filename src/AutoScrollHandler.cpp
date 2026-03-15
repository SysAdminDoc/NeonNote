#include "AutoScrollHandler.h"
#include "Theme.h"
#include <QEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QPainter>
#include <QApplication>
#include <QCursor>
#include <Qsci/qsciscintilla.h>
#include <cmath>

// ── Overlay Widget ──

AutoScrollOverlay::AutoScrollOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(28, 28);
}

void AutoScrollOverlay::setDirection(int dir)
{
    if (m_dir != dir) {
        m_dir = dir;
        update();
    }
}

void AutoScrollOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    QRectF r = rect().adjusted(1, 1, -1, -1);
    QPointF center = r.center();

    // Circle background
    p.setPen(QPen(Theme::Overlay0, 1.5));
    p.setBrush(QColor(30, 30, 46, 200)); // Catppuccin Base with alpha
    p.drawEllipse(r);

    // Arrows
    p.setPen(Qt::NoPen);
    double arrowH = 5.0;
    double arrowW = 4.0;

    // Up arrow (dimmed if scrolling down)
    QColor upColor = (m_dir >= 0) ? Theme::Overlay0 : Theme::Lavender;
    p.setBrush(upColor);
    QPolygonF upArrow;
    upArrow << QPointF(center.x(), center.y() - 9)
            << QPointF(center.x() - arrowW, center.y() - 9 + arrowH)
            << QPointF(center.x() + arrowW, center.y() - 9 + arrowH);
    p.drawPolygon(upArrow);

    // Down arrow (dimmed if scrolling up)
    QColor downColor = (m_dir <= 0) ? Theme::Overlay0 : Theme::Lavender;
    p.setBrush(downColor);
    QPolygonF downArrow;
    downArrow << QPointF(center.x(), center.y() + 9)
              << QPointF(center.x() - arrowW, center.y() + 9 - arrowH)
              << QPointF(center.x() + arrowW, center.y() + 9 - arrowH);
    p.drawPolygon(downArrow);

    // Center dot
    p.setBrush(Theme::Overlay1);
    p.drawEllipse(center, 2.0, 2.0);
}

// ── Handler ──

AutoScrollHandler::AutoScrollHandler(QObject* parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(TICK_MS);
    connect(m_timer, &QTimer::timeout, this, &AutoScrollHandler::tick);
}

void AutoScrollHandler::install(QsciScintilla* editor)
{
    auto* vp = editor->viewport();
    m_viewportMap[vp] = editor;
    vp->installEventFilter(this);
}

void AutoScrollHandler::uninstall(QsciScintilla* editor)
{
    if (m_activeEditor == editor)
        deactivate();
    auto* vp = editor->viewport();
    vp->removeEventFilter(this);
    m_viewportMap.remove(vp);
}

QsciScintilla* AutoScrollHandler::editorForViewport(QObject* viewport) const
{
    auto* w = qobject_cast<QWidget*>(viewport);
    return w ? m_viewportMap.value(w, nullptr) : nullptr;
}

bool AutoScrollHandler::eventFilter(QObject* obj, QEvent* event)
{
    auto* editor = editorForViewport(obj);
    if (!editor) return false;

    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::MiddleButton) {
            if (m_activeEditor == editor) {
                deactivate();
            } else {
                activate(editor, me->pos());
            }
            return true;
        }
        // Any other button click while scrolling → stop
        if (m_activeEditor == editor) {
            deactivate();
            return true;
        }
        break;
    }

    case QEvent::MouseMove: {
        if (m_activeEditor == editor) {
            auto* me = static_cast<QMouseEvent*>(event);
            int dy = me->pos().y() - m_origin.y();
            int dir = 0;
            if (dy < -DEAD_ZONE) dir = -1;
            else if (dy > DEAD_ZONE) dir = 1;

            if (m_overlay)
                m_overlay->setDirection(dir);

            // Update cursor based on direction
            if (dir != 0)
                editor->viewport()->setCursor(Qt::SizeVerCursor);
            else
                editor->viewport()->setCursor(Qt::SizeAllCursor);

            return true;
        }
        break;
    }

    case QEvent::MouseButtonRelease: {
        if (m_activeEditor == editor) {
            auto* me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::MiddleButton)
                return true; // Eat the release from our activation click
        }
        break;
    }

    case QEvent::Wheel: {
        // Scroll wheel should stop auto-scroll
        if (m_activeEditor == editor) {
            deactivate();
        }
        break;
    }

    case QEvent::KeyPress: {
        if (m_activeEditor == editor) {
            deactivate();
            return true;
        }
        break;
    }

    case QEvent::FocusOut: {
        if (m_activeEditor == editor)
            deactivate();
        break;
    }

    default:
        break;
    }

    return false;
}

void AutoScrollHandler::activate(QsciScintilla* editor, const QPoint& pos)
{
    if (m_activeEditor)
        deactivate();

    m_activeEditor = editor;
    m_origin = pos;
    m_accumulator = 0.0;

    // Create overlay centered on click point
    m_overlay = new AutoScrollOverlay(editor->viewport());
    m_overlay->move(pos.x() - OVERLAY_SIZE / 2, pos.y() - OVERLAY_SIZE / 2);
    m_overlay->show();

    editor->viewport()->setCursor(Qt::SizeAllCursor);
    m_timer->start();
}

void AutoScrollHandler::deactivate()
{
    m_timer->stop();

    if (m_overlay) {
        m_overlay->hide();
        m_overlay->deleteLater();
        m_overlay = nullptr;
    }

    if (m_activeEditor) {
        m_activeEditor->viewport()->unsetCursor();
        m_activeEditor = nullptr;
    }

    m_accumulator = 0.0;
}

void AutoScrollHandler::tick()
{
    if (!m_activeEditor) return;

    QPoint mousePos = m_activeEditor->viewport()->mapFromGlobal(QCursor::pos());
    int dy = mousePos.y() - m_origin.y();

    // Dead zone
    if (std::abs(dy) <= DEAD_ZONE)
        return;

    // Remove dead zone from distance
    double effectiveDy = (dy > 0) ? (dy - DEAD_ZONE) : (dy + DEAD_ZONE);

    // Non-linear speed curve: quadratic ramp for natural feel
    double sign = (effectiveDy > 0) ? 1.0 : -1.0;
    double absDy = std::abs(effectiveDy);
    double speed = sign * SPEED_FACTOR * absDy * (1.0 + absDy * 0.008);

    // Accumulate fractional lines
    m_accumulator += speed;

    int lines = static_cast<int>(m_accumulator);
    if (lines != 0) {
        m_accumulator -= lines;
        m_activeEditor->SendScintilla(QsciScintilla::SCI_LINESCROLL, 0, lines);
    }
}
