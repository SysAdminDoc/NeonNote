#pragma once
#include <QWidget>
#include <QString>
#include <QVector>

struct StatusPill {
    QString text;
    QColor bg;
    QColor fg;
};

class NeonStatusBar : public QWidget {
    Q_OBJECT
public:
    explicit NeonStatusBar(QWidget* parent = nullptr);

    void setLineCol(int line, int col);
    void setEncoding(const QString& enc);
    void setLineEnding(const QString& le);
    void setLanguage(const QString& lang);
    void setFileSize(qint64 bytes);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_lineCol = "Ln 1, Col 1";
    QString m_encoding = "UTF-8";
    QString m_lineEnding = "CRLF";
    QString m_language = "Plain Text";
    QString m_fileSize;

    static constexpr int BAR_HEIGHT = 24;
    static constexpr int PILL_HPAD = 8;
    static constexpr int PILL_RADIUS = 8;
    static constexpr int PILL_GAP = 6;
};
