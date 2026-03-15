#pragma once
#include <QWidget>
#include <QTimer>
#include <QPropertyAnimation>
#include <QString>
#include <QVector>

struct BookmarkItem {
    QString filePath;
    QString displayName;
};

class BookmarkBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int expandedHeight READ expandedHeight)

public:
    explicit BookmarkBar(QWidget* parent = nullptr);

    void addBookmark(const QString& filePath);
    void removeBookmark(int index);
    bool hasBookmark(const QString& filePath) const;
    int bookmarkCount() const { return m_bookmarks.size(); }
    const QString& bookmarkPath(int index) const { return m_bookmarks[index].filePath; }

    int expandedHeight() const { return 28; }
    bool isExpanded() const { return m_expanded; }

signals:
    void bookmarkClicked(int index);
    void heightChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void enterEvent(QEnterEvent* event) override;

private:
    int hitTest(const QPoint& pos) const;
    QRect closeRect(int index) const;

    QVector<BookmarkItem> m_bookmarks;
    QVector<QRect> m_itemRects;
    int m_hoverIndex = -1;
    bool m_closeHover = false;
    bool m_expanded = false;
    QTimer m_collapseTimer;

    void recalcLayout();

    static constexpr int ITEM_HPAD = 10;
    static constexpr int ITEM_GAP = 4;
    static constexpr int BAR_LPAD = 8;
    static constexpr int REVEAL_HEIGHT = 4;
};
