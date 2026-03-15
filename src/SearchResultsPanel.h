#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QVector>

struct SearchHit {
    QString filePath;
    int line;
    int col;
    QString lineText;
};

class SearchResultsPanel : public QWidget {
    Q_OBJECT
public:
    explicit SearchResultsPanel(QWidget* parent = nullptr);

    void clear();
    void setSearchTerm(const QString& term);
    void addFileResults(const QString& filePath, const QVector<SearchHit>& hits);
    void addHit(const SearchHit& hit);
    void finalize();

signals:
    void hitDoubleClicked(const QString& filePath, int line, int col);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QTreeWidget* m_tree;
    QLabel* m_summaryLabel;
    QPushButton* m_btnClose;
    QPushButton* m_btnClear;
    QString m_searchTerm;
    int m_totalHits = 0;
    int m_fileCount = 0;
};
