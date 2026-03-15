#include "StatusBar.h"
#include "Theme.h"
#include <QPainter>
#include <QPainterPath>

NeonStatusBar::NeonStatusBar(QWidget* parent) : QWidget(parent)
{
    setFixedHeight(BAR_HEIGHT);
}

void NeonStatusBar::setLineCol(int line, int col)
{
    m_lineCol = QString("Ln %1, Col %2").arg(line).arg(col);
    update();
}

void NeonStatusBar::setEncoding(const QString& enc)
{
    m_encoding = enc;
    update();
}

void NeonStatusBar::setLineEnding(const QString& le)
{
    m_lineEnding = le;
    update();
}

void NeonStatusBar::setLanguage(const QString& lang)
{
    m_language = lang;
    update();
}

void NeonStatusBar::setFileSize(qint64 bytes)
{
    if (bytes < 1024)
        m_fileSize = QString("%1 B").arg(bytes);
    else if (bytes < 1024 * 1024)
        m_fileSize = QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    else
        m_fileSize = QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    update();
}

void NeonStatusBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Background: Mantle
    p.fillRect(rect(), Theme::Mantle);

    // Top border
    p.setPen(Theme::Surface0);
    p.drawLine(0, 0, width(), 0);

    QFont f = font();
    f.setPointSize(8);
    p.setFont(f);
    QFontMetrics fm(f);

    // Build pill list
    QVector<StatusPill> pills;
    pills.append({m_lineCol, Theme::Blue, Theme::Crust});
    if (!m_fileSize.isEmpty())
        pills.append({m_fileSize, Theme::Surface0, Theme::Subtext0});
    pills.append({m_encoding, Theme::Surface0, Theme::Subtext0});
    pills.append({m_lineEnding, Theme::Teal, Theme::Crust});
    pills.append({m_language, Theme::Mauve, Theme::Crust});

    // Draw pills from right to left
    int x = width() - PILL_GAP;
    for (int i = pills.size() - 1; i >= 0; --i) {
        const auto& pill = pills[i];
        int textW = fm.horizontalAdvance(pill.text);
        int pillW = textW + PILL_HPAD * 2;
        x -= pillW;

        QRectF pillRect(x, 4, pillW, BAR_HEIGHT - 8);
        QPainterPath path;
        path.addRoundedRect(pillRect, PILL_RADIUS, PILL_RADIUS);
        p.fillPath(path, pill.bg);

        p.setPen(pill.fg);
        p.drawText(pillRect, Qt::AlignCenter, pill.text);

        x -= PILL_GAP;
    }
}
