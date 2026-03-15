#include "SearchResultsPanel.h"
#include "Theme.h"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>

SearchResultsPanel::SearchResultsPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header bar
    auto* header = new QWidget(this);
    header->setFixedHeight(26);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 0, 4, 0);
    headerLayout->setSpacing(4);

    m_summaryLabel = new QLabel("Search Results", this);
    m_summaryLabel->setStyleSheet("QLabel { color: #cdd6f4; font-size: 11px; font-weight: bold; }");
    headerLayout->addWidget(m_summaryLabel);
    headerLayout->addStretch();

    QString btnStyle = "QPushButton { background: transparent; color: #a6adc8; border: none;"
                       "  font-size: 10px; font-family: 'Segoe MDL2 Assets'; padding: 2px 6px; border-radius: 3px; }"
                       "QPushButton:hover { background: #313244; color: #cdd6f4; }";

    m_btnClear = new QPushButton(QString::fromUtf16(u"\uE894"), this); // Delete
    m_btnClear->setToolTip("Clear Results");
    m_btnClear->setFixedSize(22, 22);
    m_btnClear->setStyleSheet(btnStyle);
    headerLayout->addWidget(m_btnClear);

    m_btnClose = new QPushButton(QString::fromUtf16(u"\uE8BB"), this);
    m_btnClose->setToolTip("Close Panel");
    m_btnClose->setFixedSize(22, 22);
    m_btnClose->setStyleSheet(btnStyle);
    headerLayout->addWidget(m_btnClose);

    layout->addWidget(header);

    // Tree widget
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setRootIsDecorated(true);
    m_tree->setIndentation(16);
    m_tree->setAnimated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setStyleSheet(
        "QTreeWidget {"
        "  background: #181825; color: #cdd6f4; border: none;"
        "  font-family: Consolas; font-size: 11px;"
        "  selection-background-color: #313244;"
        "}"
        "QTreeWidget::item { padding: 1px 0; }"
        "QTreeWidget::item:hover { background: #1e1e2e; }"
        "QTreeWidget::item:selected { background: #313244; }"
        "QTreeWidget::branch { background: #181825; }"
        "QTreeWidget::branch:has-children:!has-siblings:closed,"
        "QTreeWidget::branch:closed:has-children:has-siblings {"
        "  border-image: none; image: none;"
        "}"
        "QTreeWidget::branch:open:has-children:!has-siblings,"
        "QTreeWidget::branch:open:has-children:has-siblings {"
        "  border-image: none; image: none;"
        "}"
    );
    layout->addWidget(m_tree);

    setMinimumHeight(100);
    setMaximumHeight(300);
    hide();

    connect(m_btnClose, &QPushButton::clicked, this, &QWidget::hide);
    connect(m_btnClear, &QPushButton::clicked, this, &SearchResultsPanel::clear);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item->parent()) return; // file-level item
        QString filePath = item->data(0, Qt::UserRole).toString();
        int line = item->data(0, Qt::UserRole + 1).toInt();
        int col = item->data(0, Qt::UserRole + 2).toInt();
        emit hitDoubleClicked(filePath, line, col);
    });
}

void SearchResultsPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(24, 24, 37)); // Mantle
    p.setPen(Theme::Surface0);
    p.drawLine(0, 0, width(), 0);
}

void SearchResultsPanel::clear()
{
    m_tree->clear();
    m_totalHits = 0;
    m_fileCount = 0;
    m_summaryLabel->setText("Search Results");
}

void SearchResultsPanel::setSearchTerm(const QString& term)
{
    m_searchTerm = term;
    m_totalHits = 0;
    m_fileCount = 0;
    m_tree->clear();
}

void SearchResultsPanel::addFileResults(const QString& filePath, const QVector<SearchHit>& hits)
{
    if (hits.isEmpty()) return;
    m_fileCount++;

    auto* fileItem = new QTreeWidgetItem(m_tree);
    fileItem->setText(0, QString("%1 (%2 hits)").arg(filePath).arg(hits.size()));
    fileItem->setForeground(0, Theme::Blue);
    QFont f = fileItem->font(0);
    f.setBold(true);
    fileItem->setFont(0, f);

    for (const auto& hit : hits) {
        auto* child = new QTreeWidgetItem(fileItem);
        QString display = QString("  Line %1: %2").arg(hit.line + 1).arg(hit.lineText.trimmed());
        child->setText(0, display);
        child->setData(0, Qt::UserRole, hit.filePath);
        child->setData(0, Qt::UserRole + 1, hit.line);
        child->setData(0, Qt::UserRole + 2, hit.col);
        child->setForeground(0, Theme::Subtext0);
        m_totalHits++;
    }

    fileItem->setExpanded(true);
}

void SearchResultsPanel::addHit(const SearchHit& hit)
{
    // Find or create parent for this file
    QTreeWidgetItem* fileItem = nullptr;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        if (m_tree->topLevelItem(i)->data(0, Qt::UserRole + 10).toString() == hit.filePath) {
            fileItem = m_tree->topLevelItem(i);
            break;
        }
    }
    if (!fileItem) {
        fileItem = new QTreeWidgetItem(m_tree);
        fileItem->setText(0, hit.filePath);
        fileItem->setForeground(0, Theme::Blue);
        fileItem->setData(0, Qt::UserRole + 10, hit.filePath);
        QFont f = fileItem->font(0);
        f.setBold(true);
        fileItem->setFont(0, f);
        fileItem->setExpanded(true);
        m_fileCount++;
    }

    auto* child = new QTreeWidgetItem(fileItem);
    QString display = QString("  Line %1: %2").arg(hit.line + 1).arg(hit.lineText.trimmed());
    child->setText(0, display);
    child->setData(0, Qt::UserRole, hit.filePath);
    child->setData(0, Qt::UserRole + 1, hit.line);
    child->setData(0, Qt::UserRole + 2, hit.col);
    child->setForeground(0, Theme::Subtext0);
    m_totalHits++;

    // Update parent text with count
    int count = fileItem->childCount();
    fileItem->setText(0, QString("%1 (%2 hits)").arg(hit.filePath).arg(count));
}

void SearchResultsPanel::finalize()
{
    m_summaryLabel->setText(QString("Search \"%1\" - %2 hits in %3 files")
        .arg(m_searchTerm).arg(m_totalHits).arg(m_fileCount));
    show();
}
