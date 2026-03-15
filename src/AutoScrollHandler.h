#pragma once
#include <QObject>
#include <QPoint>
#include <QWidget>
#include <QMap>

class QsciScintilla;
class QTimer;

class AutoScrollOverlay : public QWidget {
    Q_OBJECT
public:
    explicit AutoScrollOverlay(QWidget* parent);
    void setDirection(int dir); // -1=up, 0=neutral, 1=down

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int m_dir = 0;
};

class AutoScrollHandler : public QObject {
    Q_OBJECT
public:
    explicit AutoScrollHandler(QObject* parent = nullptr);

    void install(QsciScintilla* editor);
    void uninstall(QsciScintilla* editor);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void activate(QsciScintilla* editor, const QPoint& pos);
    void deactivate();
    void tick();
    QsciScintilla* editorForViewport(QObject* viewport) const;

    QsciScintilla* m_activeEditor = nullptr;
    QMap<QWidget*, QsciScintilla*> m_viewportMap;
    AutoScrollOverlay* m_overlay = nullptr;
    QTimer* m_timer = nullptr;
    QPoint m_origin;
    double m_accumulator = 0.0;

    static constexpr int DEAD_ZONE = 12;
    static constexpr int OVERLAY_SIZE = 28;
    static constexpr double SPEED_FACTOR = 0.04;
    static constexpr int TICK_MS = 16;
};
