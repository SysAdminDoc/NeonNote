#include "SearchDialog.h"
#include "SearchResultsPanel.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QKeyEvent>
#include <QFileDialog>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QApplication>

SearchDialog::SearchDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Search");
    setMinimumSize(520, 340);
    resize(540, 380);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setStyleSheet(
        "QDialog { background: #1e1e2e; color: #cdd6f4; }"
        "QTabWidget::pane { border: 1px solid #45475a; background: #1e1e2e; }"
        "QTabBar::tab { background: #181825; color: #a6adc8; padding: 6px 16px;"
        "  border: 1px solid #45475a; border-bottom: none; border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px; margin-right: 2px; }"
        "QTabBar::tab:selected { background: #1e1e2e; color: #cdd6f4; }"
        "QTabBar::tab:hover { background: #313244; }"
    );

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createFindPage(), "Find");
    m_tabs->addTab(createReplacePage(), "Replace");
    m_tabs->addTab(createFindInFilesPage(), "Find in Files");
    m_tabs->addTab(createMarkPage(), "Mark");
    layout->addWidget(m_tabs);

    // Sync search text across tabs when switching
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int idx) {
        QString text;
        switch (idx) {
        case 0: text = m_findCombo->currentText(); break;
        case 1: text = m_replaceSearchCombo->currentText(); break;
        case 2: text = m_fifSearchCombo->currentText(); break;
        case 3: text = m_markCombo->currentText(); break;
        }
        // Don't sync — each tab keeps its own state like Notepad++
        Q_UNUSED(text)
    });
}

void SearchDialog::setEditor(QsciScintilla* editor)
{
    m_editor = editor;
    // Pre-fill with selected text
    if (editor && editor->hasSelectedText()) {
        QString sel = editor->selectedText();
        if (!sel.contains('\n')) {
            m_findCombo->setCurrentText(sel);
            m_replaceSearchCombo->setCurrentText(sel);
            m_fifSearchCombo->setCurrentText(sel);
            m_markCombo->setCurrentText(sel);
        }
    }
}

void SearchDialog::setResultsPanel(SearchResultsPanel* panel)
{
    m_resultsPanel = panel;
}

void SearchDialog::setOpenEditors(const QVector<QPair<QString, QsciScintilla*>>& editors)
{
    m_openEditors = editors;
}

void SearchDialog::showTab(int tabIndex)
{
    m_tabs->setCurrentIndex(tabIndex);
    show();
    raise();
    activateWindow();

    switch (tabIndex) {
    case 0: m_findCombo->setFocus(); break;
    case 1: m_replaceSearchCombo->setFocus(); break;
    case 2: m_fifSearchCombo->setFocus(); break;
    case 3: m_markCombo->setFocus(); break;
    }
}

void SearchDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        int tab = m_tabs->currentIndex();
        if (tab == 0) findNext();
        else if (tab == 1) replaceNext();
        else if (tab == 2) findInFiles();
        else if (tab == 3) markAll();
        return;
    }
    QDialog::keyPressEvent(event);
}

// ── Style Helpers ──

QString SearchDialog::editStyle() const
{
    return "QLineEdit, QComboBox {"
           "  background: #181825; color: #cdd6f4; border: 1px solid #45475a;"
           "  border-radius: 4px; padding: 4px 6px; font-size: 12px;"
           "  selection-background-color: #45475a;"
           "}"
           "QLineEdit:focus, QComboBox:focus { border: 1px solid #89b4fa; }"
           "QComboBox::drop-down {"
           "  border: none; background: transparent; width: 20px;"
           "}"
           "QComboBox::down-arrow { image: none; }"
           "QComboBox QAbstractItemView {"
           "  background: #181825; color: #cdd6f4; border: 1px solid #45475a;"
           "  selection-background-color: #313244;"
           "}";
}

QString SearchDialog::btnStyle() const
{
    return "QPushButton { background: #313244; color: #cdd6f4; border: none;"
           "  border-radius: 4px; padding: 5px 14px; font-size: 11px; }"
           "QPushButton:hover { background: #45475a; }"
           "QPushButton:pressed { background: #585b70; }";
}

QString SearchDialog::btnPrimaryStyle() const
{
    return "QPushButton { background: #89b4fa; color: #11111b; border: none;"
           "  border-radius: 4px; padding: 5px 14px; font-size: 11px; font-weight: bold; }"
           "QPushButton:hover { background: #b4d0fb; }"
           "QPushButton:pressed { background: #7ba3e8; }";
}

QString SearchDialog::checkStyle() const
{
    return "QCheckBox { color: #cdd6f4; font-size: 11px; spacing: 4px; }"
           "QCheckBox::indicator {"
           "  width: 14px; height: 14px; border-radius: 3px;"
           "  border: 1px solid #45475a; background: #181825;"
           "}"
           "QCheckBox::indicator:checked { background: #89b4fa; border-color: #89b4fa; }";
}

QString SearchDialog::radioStyle() const
{
    return "QRadioButton { color: #cdd6f4; font-size: 11px; spacing: 4px; }"
           "QRadioButton::indicator {"
           "  width: 14px; height: 14px; border-radius: 7px;"
           "  border: 1px solid #45475a; background: #181825;"
           "}"
           "QRadioButton::indicator:checked { background: #89b4fa; border-color: #89b4fa; }";
}

QString SearchDialog::groupStyle() const
{
    return "QGroupBox { color: #a6adc8; font-size: 11px; border: 1px solid #45475a;"
           "  border-radius: 4px; margin-top: 8px; padding-top: 12px; }"
           "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }";
}

QString SearchDialog::comboStyle() const { return editStyle(); }

// ── Page Creation ──

QWidget* SearchDialog::createFindPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    // Search term
    auto* searchRow = new QHBoxLayout();
    auto* searchLabel = new QLabel("Find what:", page);
    searchLabel->setStyleSheet("QLabel { color: #cdd6f4; font-size: 11px; }");
    searchLabel->setFixedWidth(80);
    m_findCombo = new QComboBox(page);
    m_findCombo->setEditable(true);
    m_findCombo->setMaxCount(20);
    m_findCombo->setStyleSheet(comboStyle());
    searchRow->addWidget(searchLabel);
    searchRow->addWidget(m_findCombo, 1);
    layout->addLayout(searchRow);

    // Options row
    auto* optRow = new QHBoxLayout();
    m_findMatchCase = new QCheckBox("Match case", page);
    m_findWholeWord = new QCheckBox("Whole word", page);
    m_findWrapAround = new QCheckBox("Wrap around", page);
    m_findWrapAround->setChecked(true);
    m_findMatchCase->setStyleSheet(checkStyle());
    m_findWholeWord->setStyleSheet(checkStyle());
    m_findWrapAround->setStyleSheet(checkStyle());
    optRow->addWidget(m_findMatchCase);
    optRow->addWidget(m_findWholeWord);
    optRow->addWidget(m_findWrapAround);
    optRow->addStretch();
    layout->addLayout(optRow);

    // Search mode + Direction in a row
    auto* modeRow = new QHBoxLayout();

    auto* modeGroup = new QGroupBox("Search Mode", page);
    modeGroup->setStyleSheet(groupStyle());
    auto* modeLayout = new QVBoxLayout(modeGroup);
    modeLayout->setSpacing(2);
    m_findModeNormal = new QRadioButton("Normal", modeGroup);
    m_findModeExtended = new QRadioButton("Extended (\\n, \\t, ...)", modeGroup);
    m_findModeRegex = new QRadioButton("Regular expression", modeGroup);
    m_findModeNormal->setChecked(true);
    m_findModeNormal->setStyleSheet(radioStyle());
    m_findModeExtended->setStyleSheet(radioStyle());
    m_findModeRegex->setStyleSheet(radioStyle());
    modeLayout->addWidget(m_findModeNormal);
    modeLayout->addWidget(m_findModeExtended);
    modeLayout->addWidget(m_findModeRegex);
    modeRow->addWidget(modeGroup);

    auto* dirGroup = new QGroupBox("Direction", page);
    dirGroup->setStyleSheet(groupStyle());
    auto* dirLayout = new QVBoxLayout(dirGroup);
    dirLayout->setSpacing(2);
    m_findDirUp = new QRadioButton("Up", dirGroup);
    m_findDirDown = new QRadioButton("Down", dirGroup);
    m_findDirDown->setChecked(true);
    m_findDirUp->setStyleSheet(radioStyle());
    m_findDirDown->setStyleSheet(radioStyle());
    dirLayout->addWidget(m_findDirUp);
    dirLayout->addWidget(m_findDirDown);
    dirLayout->addStretch();
    modeRow->addWidget(dirGroup);

    layout->addLayout(modeRow);

    // Buttons
    auto* btnRow = new QHBoxLayout();
    m_findStatusLabel = new QLabel(page);
    m_findStatusLabel->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; }");
    btnRow->addWidget(m_findStatusLabel);
    btnRow->addStretch();

    auto* btnFindNext = new QPushButton("Find Next", page);
    auto* btnFindPrev = new QPushButton("Find Previous", page);
    auto* btnCount = new QPushButton("Count", page);
    auto* btnFindAllCurrent = new QPushButton("Find All in Current", page);
    auto* btnFindAllOpen = new QPushButton("Find All in All Open", page);

    btnFindNext->setStyleSheet(btnPrimaryStyle());
    btnFindPrev->setStyleSheet(btnStyle());
    btnCount->setStyleSheet(btnStyle());
    btnFindAllCurrent->setStyleSheet(btnStyle());
    btnFindAllOpen->setStyleSheet(btnStyle());

    auto* btnGrid = new QGridLayout();
    btnGrid->addWidget(btnFindNext, 0, 0);
    btnGrid->addWidget(btnFindPrev, 0, 1);
    btnGrid->addWidget(btnCount, 0, 2);
    btnGrid->addWidget(btnFindAllCurrent, 1, 0, 1, 2);
    btnGrid->addWidget(btnFindAllOpen, 1, 2);
    btnGrid->setSpacing(4);

    layout->addLayout(btnGrid);
    layout->addLayout(btnRow);
    layout->addStretch();

    connect(btnFindNext, &QPushButton::clicked, this, &SearchDialog::findNext);
    connect(btnFindPrev, &QPushButton::clicked, this, &SearchDialog::findPrev);
    connect(btnCount, &QPushButton::clicked, this, &SearchDialog::findCount);
    connect(btnFindAllCurrent, &QPushButton::clicked, this, &SearchDialog::findAllInCurrent);
    connect(btnFindAllOpen, &QPushButton::clicked, this, &SearchDialog::findAllInAllOpen);

    return page;
}

QWidget* SearchDialog::createReplacePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto addRow = [&](const QString& label, QComboBox*& combo) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label, page);
        lbl->setStyleSheet("QLabel { color: #cdd6f4; font-size: 11px; }");
        lbl->setFixedWidth(80);
        combo = new QComboBox(page);
        combo->setEditable(true);
        combo->setMaxCount(20);
        combo->setStyleSheet(comboStyle());
        row->addWidget(lbl);
        row->addWidget(combo, 1);
        layout->addLayout(row);
    };

    addRow("Find what:", m_replaceSearchCombo);
    addRow("Replace with:", m_replaceWithCombo);

    auto* optRow = new QHBoxLayout();
    m_replaceMatchCase = new QCheckBox("Match case", page);
    m_replaceWholeWord = new QCheckBox("Whole word", page);
    m_replaceWrapAround = new QCheckBox("Wrap around", page);
    m_replaceWrapAround->setChecked(true);
    m_replaceMatchCase->setStyleSheet(checkStyle());
    m_replaceWholeWord->setStyleSheet(checkStyle());
    m_replaceWrapAround->setStyleSheet(checkStyle());
    optRow->addWidget(m_replaceMatchCase);
    optRow->addWidget(m_replaceWholeWord);
    optRow->addWidget(m_replaceWrapAround);
    optRow->addStretch();
    layout->addLayout(optRow);

    auto* modeGroup = new QGroupBox("Search Mode", page);
    modeGroup->setStyleSheet(groupStyle());
    auto* modeLayout = new QHBoxLayout(modeGroup);
    m_replaceModeNormal = new QRadioButton("Normal", modeGroup);
    m_replaceModeExtended = new QRadioButton("Extended", modeGroup);
    m_replaceModeRegex = new QRadioButton("Regex", modeGroup);
    m_replaceModeNormal->setChecked(true);
    m_replaceModeNormal->setStyleSheet(radioStyle());
    m_replaceModeExtended->setStyleSheet(radioStyle());
    m_replaceModeRegex->setStyleSheet(radioStyle());
    modeLayout->addWidget(m_replaceModeNormal);
    modeLayout->addWidget(m_replaceModeExtended);
    modeLayout->addWidget(m_replaceModeRegex);
    layout->addWidget(modeGroup);

    auto* btnRow = new QHBoxLayout();
    m_replaceStatusLabel = new QLabel(page);
    m_replaceStatusLabel->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; }");
    btnRow->addWidget(m_replaceStatusLabel);
    btnRow->addStretch();

    auto* btnFindNext = new QPushButton("Find Next", page);
    auto* btnReplace = new QPushButton("Replace", page);
    auto* btnReplaceAll = new QPushButton("Replace All", page);
    auto* btnReplaceAllOpen = new QPushButton("Replace All in All Open", page);

    btnFindNext->setStyleSheet(btnPrimaryStyle());
    btnReplace->setStyleSheet(btnStyle());
    btnReplaceAll->setStyleSheet(btnStyle());
    btnReplaceAllOpen->setStyleSheet(btnStyle());

    auto* btnGrid = new QGridLayout();
    btnGrid->addWidget(btnFindNext, 0, 0);
    btnGrid->addWidget(btnReplace, 0, 1);
    btnGrid->addWidget(btnReplaceAll, 1, 0);
    btnGrid->addWidget(btnReplaceAllOpen, 1, 1);
    btnGrid->setSpacing(4);

    layout->addLayout(btnGrid);
    layout->addLayout(btnRow);
    layout->addStretch();

    connect(btnFindNext, &QPushButton::clicked, this, [this]() {
        SearchOptions opts = gatherReplaceOptions();
        if (m_editor) doFind(m_editor, opts, true);
    });
    connect(btnReplace, &QPushButton::clicked, this, &SearchDialog::replaceNext);
    connect(btnReplaceAll, &QPushButton::clicked, this, &SearchDialog::replaceAll);
    connect(btnReplaceAllOpen, &QPushButton::clicked, this, &SearchDialog::replaceAllInAllOpen);

    return page;
}

QWidget* SearchDialog::createFindInFilesPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto addRow = [&](const QString& label, QComboBox*& combo, QPushButton** browseBtn = nullptr) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label, page);
        lbl->setStyleSheet("QLabel { color: #cdd6f4; font-size: 11px; }");
        lbl->setFixedWidth(80);
        combo = new QComboBox(page);
        combo->setEditable(true);
        combo->setMaxCount(20);
        combo->setStyleSheet(comboStyle());
        row->addWidget(lbl);
        row->addWidget(combo, 1);
        if (browseBtn) {
            *browseBtn = new QPushButton("...", page);
            (*browseBtn)->setFixedWidth(32);
            (*browseBtn)->setStyleSheet(btnStyle());
            row->addWidget(*browseBtn);
        }
        layout->addLayout(row);
    };

    addRow("Find what:", m_fifSearchCombo);
    addRow("Replace with:", m_fifReplaceCombo);

    QPushButton* browseBtn = nullptr;
    addRow("Directory:", m_fifDirCombo, &browseBtn);

    auto* filterRow = new QHBoxLayout();
    auto* filterLabel = new QLabel("Filters:", page);
    filterLabel->setStyleSheet("QLabel { color: #cdd6f4; font-size: 11px; }");
    filterLabel->setFixedWidth(80);
    m_fifFilters = new QLineEdit("*.*", page);
    m_fifFilters->setStyleSheet(editStyle());
    filterRow->addWidget(filterLabel);
    filterRow->addWidget(m_fifFilters, 1);
    layout->addLayout(filterRow);

    auto* optRow = new QHBoxLayout();
    m_fifMatchCase = new QCheckBox("Match case", page);
    m_fifWholeWord = new QCheckBox("Whole word", page);
    m_fifSubFolders = new QCheckBox("In sub-folders", page);
    m_fifHiddenFolders = new QCheckBox("In hidden folders", page);
    m_fifFollowDoc = new QCheckBox("Follow current doc", page);
    m_fifSubFolders->setChecked(true);
    m_fifMatchCase->setStyleSheet(checkStyle());
    m_fifWholeWord->setStyleSheet(checkStyle());
    m_fifSubFolders->setStyleSheet(checkStyle());
    m_fifHiddenFolders->setStyleSheet(checkStyle());
    m_fifFollowDoc->setStyleSheet(checkStyle());
    optRow->addWidget(m_fifMatchCase);
    optRow->addWidget(m_fifWholeWord);
    optRow->addWidget(m_fifSubFolders);
    layout->addLayout(optRow);

    auto* optRow2 = new QHBoxLayout();
    optRow2->addWidget(m_fifHiddenFolders);
    optRow2->addWidget(m_fifFollowDoc);
    optRow2->addStretch();
    layout->addLayout(optRow2);

    auto* modeGroup = new QGroupBox("Search Mode", page);
    modeGroup->setStyleSheet(groupStyle());
    auto* modeLayout = new QHBoxLayout(modeGroup);
    m_fifModeNormal = new QRadioButton("Normal", modeGroup);
    m_fifModeExtended = new QRadioButton("Extended", modeGroup);
    m_fifModeRegex = new QRadioButton("Regex", modeGroup);
    m_fifModeNormal->setChecked(true);
    m_fifModeNormal->setStyleSheet(radioStyle());
    m_fifModeExtended->setStyleSheet(radioStyle());
    m_fifModeRegex->setStyleSheet(radioStyle());
    modeLayout->addWidget(m_fifModeNormal);
    modeLayout->addWidget(m_fifModeExtended);
    modeLayout->addWidget(m_fifModeRegex);
    layout->addWidget(modeGroup);

    auto* btnRow = new QHBoxLayout();
    m_fifStatusLabel = new QLabel(page);
    m_fifStatusLabel->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; }");
    btnRow->addWidget(m_fifStatusLabel);
    btnRow->addStretch();

    auto* btnFindAll = new QPushButton("Find All", page);
    auto* btnReplaceInFiles = new QPushButton("Replace in Files", page);
    btnFindAll->setStyleSheet(btnPrimaryStyle());
    btnReplaceInFiles->setStyleSheet(btnStyle());
    btnRow->addWidget(btnFindAll);
    btnRow->addWidget(btnReplaceInFiles);
    layout->addLayout(btnRow);
    layout->addStretch();

    connect(browseBtn, &QPushButton::clicked, this, &SearchDialog::browseFindInFilesDir);
    connect(btnFindAll, &QPushButton::clicked, this, &SearchDialog::findInFiles);
    connect(btnReplaceInFiles, &QPushButton::clicked, this, &SearchDialog::replaceInFiles);
    connect(m_fifFollowDoc, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked && m_editor) {
            // Find the file path of current editor
            for (const auto& pair : m_openEditors) {
                if (pair.second == m_editor && !pair.first.isEmpty()) {
                    m_fifDirCombo->setCurrentText(QFileInfo(pair.first).absolutePath());
                    break;
                }
            }
        }
    });

    return page;
}

QWidget* SearchDialog::createMarkPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(8);

    auto* searchRow = new QHBoxLayout();
    auto* searchLabel = new QLabel("Find what:", page);
    searchLabel->setStyleSheet("QLabel { color: #cdd6f4; font-size: 11px; }");
    searchLabel->setFixedWidth(80);
    m_markCombo = new QComboBox(page);
    m_markCombo->setEditable(true);
    m_markCombo->setMaxCount(20);
    m_markCombo->setStyleSheet(comboStyle());
    searchRow->addWidget(searchLabel);
    searchRow->addWidget(m_markCombo, 1);
    layout->addLayout(searchRow);

    auto* optRow = new QHBoxLayout();
    m_markMatchCase = new QCheckBox("Match case", page);
    m_markWholeWord = new QCheckBox("Whole word", page);
    m_markBookmarkLine = new QCheckBox("Bookmark line", page);
    m_markPurge = new QCheckBox("Purge for each search", page);
    m_markPurge->setChecked(true);
    m_markMatchCase->setStyleSheet(checkStyle());
    m_markWholeWord->setStyleSheet(checkStyle());
    m_markBookmarkLine->setStyleSheet(checkStyle());
    m_markPurge->setStyleSheet(checkStyle());
    optRow->addWidget(m_markMatchCase);
    optRow->addWidget(m_markWholeWord);
    layout->addLayout(optRow);

    auto* optRow2 = new QHBoxLayout();
    optRow2->addWidget(m_markBookmarkLine);
    optRow2->addWidget(m_markPurge);
    optRow2->addStretch();
    layout->addLayout(optRow2);

    auto* modeGroup = new QGroupBox("Search Mode", page);
    modeGroup->setStyleSheet(groupStyle());
    auto* modeLayout = new QHBoxLayout(modeGroup);
    m_markModeNormal = new QRadioButton("Normal", modeGroup);
    m_markModeExtended = new QRadioButton("Extended", modeGroup);
    m_markModeRegex = new QRadioButton("Regex", modeGroup);
    m_markModeNormal->setChecked(true);
    m_markModeNormal->setStyleSheet(radioStyle());
    m_markModeExtended->setStyleSheet(radioStyle());
    m_markModeRegex->setStyleSheet(radioStyle());
    modeLayout->addWidget(m_markModeNormal);
    modeLayout->addWidget(m_markModeExtended);
    modeLayout->addWidget(m_markModeRegex);
    layout->addWidget(modeGroup);

    auto* btnRow = new QHBoxLayout();
    m_markStatusLabel = new QLabel(page);
    m_markStatusLabel->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; }");
    btnRow->addWidget(m_markStatusLabel);
    btnRow->addStretch();

    auto* btnMarkAll = new QPushButton("Mark All", page);
    auto* btnClear = new QPushButton("Clear All Marks", page);
    btnMarkAll->setStyleSheet(btnPrimaryStyle());
    btnClear->setStyleSheet(btnStyle());
    btnRow->addWidget(btnMarkAll);
    btnRow->addWidget(btnClear);
    layout->addLayout(btnRow);
    layout->addStretch();

    connect(btnMarkAll, &QPushButton::clicked, this, &SearchDialog::markAll);
    connect(btnClear, &QPushButton::clicked, this, &SearchDialog::clearMarks);

    return page;
}

// ── Option Gathering ──

SearchOptions SearchDialog::gatherFindOptions() const
{
    SearchOptions opts;
    opts.searchTerm = m_findCombo->currentText();
    opts.matchCase = m_findMatchCase->isChecked();
    opts.wholeWord = m_findWholeWord->isChecked();
    opts.wrapAround = m_findWrapAround->isChecked();
    opts.directionUp = m_findDirUp->isChecked();
    if (m_findModeExtended->isChecked()) opts.searchMode = 1;
    else if (m_findModeRegex->isChecked()) opts.searchMode = 2;
    return opts;
}

SearchOptions SearchDialog::gatherReplaceOptions() const
{
    SearchOptions opts;
    opts.searchTerm = m_replaceSearchCombo->currentText();
    opts.replaceTerm = m_replaceWithCombo->currentText();
    opts.matchCase = m_replaceMatchCase->isChecked();
    opts.wholeWord = m_replaceWholeWord->isChecked();
    opts.wrapAround = m_replaceWrapAround->isChecked();
    if (m_replaceModeExtended->isChecked()) opts.searchMode = 1;
    else if (m_replaceModeRegex->isChecked()) opts.searchMode = 2;
    return opts;
}

FindInFilesOptions SearchDialog::gatherFifOptions() const
{
    FindInFilesOptions opts;
    opts.searchTerm = m_fifSearchCombo->currentText();
    opts.replaceTerm = m_fifReplaceCombo->currentText();
    opts.matchCase = m_fifMatchCase->isChecked();
    opts.wholeWord = m_fifWholeWord->isChecked();
    opts.directory = m_fifDirCombo->currentText();
    opts.filters = m_fifFilters->text();
    opts.inSubFolders = m_fifSubFolders->isChecked();
    opts.inHiddenFolders = m_fifHiddenFolders->isChecked();
    if (m_fifModeExtended->isChecked()) opts.searchMode = 1;
    else if (m_fifModeRegex->isChecked()) opts.searchMode = 2;
    return opts;
}

SearchOptions SearchDialog::gatherMarkOptions() const
{
    SearchOptions opts;
    opts.searchTerm = m_markCombo->currentText();
    opts.matchCase = m_markMatchCase->isChecked();
    opts.wholeWord = m_markWholeWord->isChecked();
    if (m_markModeExtended->isChecked()) opts.searchMode = 1;
    else if (m_markModeRegex->isChecked()) opts.searchMode = 2;
    return opts;
}

// ── Search Engine ──

int SearchDialog::searchFlags(const SearchOptions& opts) const
{
    int flags = 0;
    if (opts.matchCase) flags |= QsciScintilla::SCFIND_MATCHCASE;
    if (opts.wholeWord) flags |= QsciScintilla::SCFIND_WHOLEWORD;
    if (opts.searchMode == 2) flags |= QsciScintilla::SCFIND_REGEXP | QsciScintilla::SCFIND_POSIX;
    return flags;
}

QString SearchDialog::expandExtended(const QString& text) const
{
    QString result;
    for (int i = 0; i < text.size(); ++i) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            QChar next = text[i + 1];
            if (next == 'n') { result += '\n'; ++i; }
            else if (next == 'r') { result += '\r'; ++i; }
            else if (next == 't') { result += '\t'; ++i; }
            else if (next == '0') { result += '\0'; ++i; }
            else if (next == '\\') { result += '\\'; ++i; }
            else result += text[i];
        } else {
            result += text[i];
        }
    }
    return result;
}

bool SearchDialog::doFind(QsciScintilla* editor, const SearchOptions& opts, bool forward)
{
    if (!editor || opts.searchTerm.isEmpty()) return false;

    QString term = (opts.searchMode == 1) ? expandExtended(opts.searchTerm) : opts.searchTerm;
    bool isRegex = (opts.searchMode == 2);
    bool found = editor->findFirst(term, isRegex, opts.matchCase, opts.wholeWord,
                                    opts.wrapAround, forward);
    return found;
}

int SearchDialog::countMatches(QsciScintilla* editor, const SearchOptions& opts)
{
    if (!editor || opts.searchTerm.isEmpty()) return 0;

    QString term = (opts.searchMode == 1) ? expandExtended(opts.searchTerm) : opts.searchTerm;
    QByteArray textBytes = term.toUtf8();
    int flags = searchFlags(opts);
    long docLen = editor->SendScintilla(QsciScintilla::SCI_GETLENGTH);
    int count = 0;
    long pos = 0;

    while (pos < docLen) {
        editor->SendScintilla(QsciScintilla::SCI_SETTARGETSTART, pos);
        editor->SendScintilla(QsciScintilla::SCI_SETTARGETEND, docLen);
        editor->SendScintilla(QsciScintilla::SCI_SETSEARCHFLAGS, flags);
        long found = editor->SendScintilla(QsciScintilla::SCI_SEARCHINTARGET,
                                            textBytes.length(), textBytes.constData());
        if (found < 0) break;
        count++;
        pos = editor->SendScintilla(QsciScintilla::SCI_GETTARGETEND);
        if (pos <= found) break;
    }
    return count;
}

QVector<QPair<int, int>> SearchDialog::findAllPositions(QsciScintilla* editor, const SearchOptions& opts)
{
    QVector<QPair<int, int>> results;
    if (!editor || opts.searchTerm.isEmpty()) return results;

    QString term = (opts.searchMode == 1) ? expandExtended(opts.searchTerm) : opts.searchTerm;
    QByteArray textBytes = term.toUtf8();
    int flags = searchFlags(opts);
    long docLen = editor->SendScintilla(QsciScintilla::SCI_GETLENGTH);
    long pos = 0;

    while (pos < docLen) {
        editor->SendScintilla(QsciScintilla::SCI_SETTARGETSTART, pos);
        editor->SendScintilla(QsciScintilla::SCI_SETTARGETEND, docLen);
        editor->SendScintilla(QsciScintilla::SCI_SETSEARCHFLAGS, flags);
        long found = editor->SendScintilla(QsciScintilla::SCI_SEARCHINTARGET,
                                            textBytes.length(), textBytes.constData());
        if (found < 0) break;
        long end = editor->SendScintilla(QsciScintilla::SCI_GETTARGETEND);
        results.append({static_cast<int>(found), static_cast<int>(end)});
        pos = end;
        if (pos <= found) break;
        if (results.size() > 500000) break;
    }
    return results;
}

// ── Find Operations ──

void SearchDialog::findNext()
{
    SearchOptions opts = gatherFindOptions();
    if (opts.searchTerm.isEmpty()) return;

    // Add to history
    if (m_findCombo->findText(opts.searchTerm) < 0)
        m_findCombo->insertItem(0, opts.searchTerm);

    bool found = doFind(m_editor, opts, !opts.directionUp);
    m_findStatusLabel->setText(found ? "" : "Not found");
    m_findStatusLabel->setStyleSheet(found
        ? "QLabel { color: #a6adc8; font-size: 11px; }"
        : "QLabel { color: #f38ba8; font-size: 11px; }");
}

void SearchDialog::findPrev()
{
    SearchOptions opts = gatherFindOptions();
    if (opts.searchTerm.isEmpty()) return;

    bool found = doFind(m_editor, opts, false);
    m_findStatusLabel->setText(found ? "" : "Not found");
    m_findStatusLabel->setStyleSheet(found
        ? "QLabel { color: #a6adc8; font-size: 11px; }"
        : "QLabel { color: #f38ba8; font-size: 11px; }");
}

void SearchDialog::findCount()
{
    SearchOptions opts = gatherFindOptions();
    if (opts.searchTerm.isEmpty()) return;
    int count = countMatches(m_editor, opts);
    m_findStatusLabel->setText(QString("%1 matches found").arg(count));
    m_findStatusLabel->setStyleSheet(count > 0
        ? "QLabel { color: #a6e3a1; font-size: 11px; }"
        : "QLabel { color: #f38ba8; font-size: 11px; }");
}

void SearchDialog::findAllInCurrent()
{
    if (!m_resultsPanel || !m_editor) return;
    SearchOptions opts = gatherFindOptions();
    if (opts.searchTerm.isEmpty()) return;

    if (m_findCombo->findText(opts.searchTerm) < 0)
        m_findCombo->insertItem(0, opts.searchTerm);

    m_resultsPanel->clear();
    m_resultsPanel->setSearchTerm(opts.searchTerm);

    auto positions = findAllPositions(m_editor, opts);
    QString filePath = "Current Document";
    for (const auto& pair : m_openEditors) {
        if (pair.second == m_editor && !pair.first.isEmpty()) {
            filePath = pair.first;
            break;
        }
    }

    QVector<SearchHit> hits;
    for (const auto& pos : positions) {
        int line = static_cast<int>(m_editor->SendScintilla(QsciScintilla::SCI_LINEFROMPOSITION, pos.first));
        int lineStart = static_cast<int>(m_editor->SendScintilla(QsciScintilla::SCI_POSITIONFROMLINE, line));
        int col = pos.first - lineStart;
        SearchHit hit;
        hit.filePath = filePath;
        hit.line = line;
        hit.col = col;
        hit.lineText = m_editor->text(line);
        hits.append(hit);
    }

    m_resultsPanel->addFileResults(filePath, hits);
    m_resultsPanel->finalize();

    m_findStatusLabel->setText(QString("%1 matches").arg(positions.size()));
    m_findStatusLabel->setStyleSheet("QLabel { color: #a6e3a1; font-size: 11px; }");
}

void SearchDialog::findAllInAllOpen()
{
    if (!m_resultsPanel) return;
    SearchOptions opts = gatherFindOptions();
    if (opts.searchTerm.isEmpty()) return;

    if (m_findCombo->findText(opts.searchTerm) < 0)
        m_findCombo->insertItem(0, opts.searchTerm);

    m_resultsPanel->clear();
    m_resultsPanel->setSearchTerm(opts.searchTerm);

    int totalHits = 0;
    for (const auto& editorPair : m_openEditors) {
        auto* editor = editorPair.second;
        auto positions = findAllPositions(editor, opts);
        if (positions.isEmpty()) continue;

        QString filePath = editorPair.first.isEmpty() ? "Untitled" : editorPair.first;
        QVector<SearchHit> hits;
        for (const auto& pos : positions) {
            int line = static_cast<int>(editor->SendScintilla(QsciScintilla::SCI_LINEFROMPOSITION, pos.first));
            int lineStart = static_cast<int>(editor->SendScintilla(QsciScintilla::SCI_POSITIONFROMLINE, line));
            SearchHit hit;
            hit.filePath = filePath;
            hit.line = line;
            hit.col = pos.first - lineStart;
            hit.lineText = editor->text(line);
            hits.append(hit);
        }
        m_resultsPanel->addFileResults(filePath, hits);
        totalHits += hits.size();
    }

    m_resultsPanel->finalize();

    m_findStatusLabel->setText(QString("%1 total matches").arg(totalHits));
    m_findStatusLabel->setStyleSheet("QLabel { color: #a6e3a1; font-size: 11px; }");
}

// ── Replace Operations ──

void SearchDialog::replaceNext()
{
    if (!m_editor) return;
    SearchOptions opts = gatherReplaceOptions();
    if (opts.searchTerm.isEmpty()) return;

    if (m_editor->hasSelectedText()) {
        QString replace = (opts.searchMode == 1) ? expandExtended(opts.replaceTerm) : opts.replaceTerm;
        m_editor->replace(replace);
    }
    bool found = doFind(m_editor, opts, true);
    m_replaceStatusLabel->setText(found ? "" : "No more matches");
    m_replaceStatusLabel->setStyleSheet(found
        ? "QLabel { color: #a6adc8; font-size: 11px; }"
        : "QLabel { color: #f38ba8; font-size: 11px; }");
}

void SearchDialog::replaceAll()
{
    if (!m_editor) return;
    SearchOptions opts = gatherReplaceOptions();
    if (opts.searchTerm.isEmpty()) return;

    QString term = (opts.searchMode == 1) ? expandExtended(opts.searchTerm) : opts.searchTerm;
    QString replace = (opts.searchMode == 1) ? expandExtended(opts.replaceTerm) : opts.replaceTerm;
    bool isRegex = (opts.searchMode == 2);

    int count = 0;
    m_editor->SendScintilla(QsciScintilla::SCI_BEGINUNDOACTION);
    m_editor->setCursorPosition(0, 0);
    while (m_editor->findFirst(term, isRegex, opts.matchCase, opts.wholeWord, false, true)) {
        m_editor->replace(replace);
        count++;
        if (count > 500000) break;
    }
    m_editor->SendScintilla(QsciScintilla::SCI_ENDUNDOACTION);

    m_replaceStatusLabel->setText(QString("%1 replacements made").arg(count));
    m_replaceStatusLabel->setStyleSheet("QLabel { color: #a6e3a1; font-size: 11px; }");
}

void SearchDialog::replaceAllInAllOpen()
{
    SearchOptions opts = gatherReplaceOptions();
    if (opts.searchTerm.isEmpty()) return;

    QString term = (opts.searchMode == 1) ? expandExtended(opts.searchTerm) : opts.searchTerm;
    QString replace = (opts.searchMode == 1) ? expandExtended(opts.replaceTerm) : opts.replaceTerm;
    bool isRegex = (opts.searchMode == 2);

    int totalCount = 0;
    for (const auto& editorPair : m_openEditors) {
        auto* editor = editorPair.second;
        int count = 0;
        editor->SendScintilla(QsciScintilla::SCI_BEGINUNDOACTION);
        editor->setCursorPosition(0, 0);
        while (editor->findFirst(term, isRegex, opts.matchCase, opts.wholeWord, false, true)) {
            editor->replace(replace);
            count++;
            if (count > 500000) break;
        }
        editor->SendScintilla(QsciScintilla::SCI_ENDUNDOACTION);
        totalCount += count;
    }

    m_replaceStatusLabel->setText(QString("%1 replacements in all documents").arg(totalCount));
    m_replaceStatusLabel->setStyleSheet("QLabel { color: #a6e3a1; font-size: 11px; }");
}

// ── Find in Files ──

void SearchDialog::browseFindInFilesDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Directory",
                                                     m_fifDirCombo->currentText());
    if (!dir.isEmpty())
        m_fifDirCombo->setCurrentText(dir);
}

void SearchDialog::findInFiles()
{
    FindInFilesOptions opts = gatherFifOptions();
    if (opts.searchTerm.isEmpty() || opts.directory.isEmpty()) return;

    if (m_fifSearchCombo->findText(opts.searchTerm) < 0)
        m_fifSearchCombo->insertItem(0, opts.searchTerm);
    if (m_fifDirCombo->findText(opts.directory) < 0)
        m_fifDirCombo->insertItem(0, opts.directory);

    if (!m_resultsPanel) return;
    m_resultsPanel->clear();
    m_resultsPanel->setSearchTerm(opts.searchTerm);

    QApplication::setOverrideCursor(Qt::WaitCursor);

    QString searchText = (opts.searchMode == 1) ? expandExtended(opts.searchTerm) : opts.searchTerm;

    // Build file filter list
    QStringList filterList = opts.filters.split(';', Qt::SkipEmptyParts);
    for (auto& f : filterList)
        f = f.trimmed();

    QDir::Filters dirFilters = QDir::Files | QDir::Readable;
    if (!opts.inHiddenFolders) dirFilters |= QDir::NoDotAndDotDot;

    QDirIterator::IteratorFlags iterFlags = opts.inSubFolders
        ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;

    QDirIterator it(opts.directory, filterList, dirFilters, iterFlags);

    QRegularExpression regex;
    if (opts.searchMode == 2) {
        QRegularExpression::PatternOptions regexOpts = QRegularExpression::NoPatternOption;
        if (!opts.matchCase) regexOpts |= QRegularExpression::CaseInsensitiveOption;
        regex = QRegularExpression(opts.searchTerm, regexOpts);
    }

    int filesSearched = 0;
    int maxFiles = 50000;

    while (it.hasNext() && filesSearched < maxFiles) {
        QString filePath = it.next();
        filesSearched++;

        // Skip binary files (check first 8KB)
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) continue;
        QByteArray header = file.peek(8192);
        if (header.contains('\0')) { file.close(); continue; }

        QTextStream stream(&file);
        QVector<SearchHit> hits;
        int lineNum = 0;

        while (!stream.atEnd()) {
            QString line = stream.readLine();
            bool matched = false;

            if (opts.searchMode == 2) {
                matched = regex.match(line).hasMatch();
            } else {
                Qt::CaseSensitivity cs = opts.matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
                if (opts.wholeWord) {
                    QRegularExpression wordRe(QString("\\b%1\\b").arg(QRegularExpression::escape(searchText)),
                                              opts.matchCase ? QRegularExpression::NoPatternOption
                                                             : QRegularExpression::CaseInsensitiveOption);
                    matched = wordRe.match(line).hasMatch();
                } else {
                    matched = line.contains(searchText, cs);
                }
            }

            if (matched) {
                SearchHit hit;
                hit.filePath = filePath;
                hit.line = lineNum;
                hit.col = 0;
                hit.lineText = line;
                hits.append(hit);
            }
            lineNum++;
        }
        file.close();

        if (!hits.isEmpty())
            m_resultsPanel->addFileResults(filePath, hits);
    }

    m_resultsPanel->finalize();
    QApplication::restoreOverrideCursor();

    m_fifStatusLabel->setText(QString("Searched %1 files").arg(filesSearched));
    m_fifStatusLabel->setStyleSheet("QLabel { color: #a6e3a1; font-size: 11px; }");
}

void SearchDialog::replaceInFiles()
{
    FindInFilesOptions opts = gatherFifOptions();
    if (opts.searchTerm.isEmpty() || opts.directory.isEmpty() || opts.replaceTerm.isEmpty()) return;

    QApplication::setOverrideCursor(Qt::WaitCursor);

    QString searchText = (opts.searchMode == 1) ? expandExtended(opts.searchTerm) : opts.searchTerm;
    QString replaceText = (opts.searchMode == 1) ? expandExtended(opts.replaceTerm) : opts.replaceTerm;

    QStringList filterList = opts.filters.split(';', Qt::SkipEmptyParts);
    for (auto& f : filterList) f = f.trimmed();

    QDir::Filters dirFilters = QDir::Files | QDir::Readable;
    QDirIterator::IteratorFlags iterFlags = opts.inSubFolders
        ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;

    QDirIterator it(opts.directory, filterList, dirFilters, iterFlags);

    int totalReplacements = 0;
    int filesModified = 0;

    while (it.hasNext()) {
        QString filePath = it.next();
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) continue;
        QByteArray data = file.readAll();
        file.close();

        // Skip binary
        if (data.contains('\0')) continue;

        QString content = QString::fromUtf8(data);
        int countBefore = 0;

        if (opts.searchMode == 2) {
            QRegularExpression regex(opts.searchTerm,
                opts.matchCase ? QRegularExpression::NoPatternOption
                               : QRegularExpression::CaseInsensitiveOption);
            countBefore = content.count(regex);
            if (countBefore > 0)
                content.replace(regex, replaceText);
        } else {
            if (opts.wholeWord) {
                QRegularExpression wordRe(QString("\\b%1\\b").arg(QRegularExpression::escape(searchText)),
                    opts.matchCase ? QRegularExpression::NoPatternOption
                                   : QRegularExpression::CaseInsensitiveOption);
                countBefore = content.count(wordRe);
                if (countBefore > 0)
                    content.replace(wordRe, replaceText);
            } else {
                Qt::CaseSensitivity cs = opts.matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
                countBefore = content.count(searchText, cs);
                if (countBefore > 0)
                    content.replace(searchText, replaceText, cs);
            }
        }

        if (countBefore > 0) {
            QFile outFile(filePath);
            if (outFile.open(QIODevice::WriteOnly)) {
                outFile.write(content.toUtf8());
                outFile.close();
                totalReplacements += countBefore;
                filesModified++;
            }
        }
    }

    QApplication::restoreOverrideCursor();

    m_fifStatusLabel->setText(QString("%1 replacements in %2 files").arg(totalReplacements).arg(filesModified));
    m_fifStatusLabel->setStyleSheet("QLabel { color: #a6e3a1; font-size: 11px; }");
}

// ── Mark Operations ──

void SearchDialog::markAll()
{
    if (!m_editor) return;
    SearchOptions opts = gatherMarkOptions();
    if (opts.searchTerm.isEmpty()) return;

    if (m_markCombo->findText(opts.searchTerm) < 0)
        m_markCombo->insertItem(0, opts.searchTerm);

    if (m_markPurge->isChecked())
        clearMarks();

    auto positions = findAllPositions(m_editor, opts);

    // Setup indicator for highlighting
    m_editor->SendScintilla(QsciScintilla::SCI_INDICSETSTYLE, MARK_INDICATOR, QsciScintilla::INDIC_ROUNDBOX);
    m_editor->SendScintilla(QsciScintilla::SCI_INDICSETFORE, MARK_INDICATOR, 0xFA89B4); // Blue (BGR)
    m_editor->SendScintilla(QsciScintilla::SCI_INDICSETALPHA, MARK_INDICATOR, 60);
    m_editor->SendScintilla(QsciScintilla::SCI_INDICSETOUTLINEALPHA, MARK_INDICATOR, 120);
    m_editor->SendScintilla(QsciScintilla::SCI_SETINDICATORCURRENT, MARK_INDICATOR);

    for (const auto& pos : positions) {
        m_editor->SendScintilla(QsciScintilla::SCI_INDICATORFILLRANGE, pos.first, pos.second - pos.first);

        if (m_markBookmarkLine->isChecked()) {
            int line = static_cast<int>(m_editor->SendScintilla(QsciScintilla::SCI_LINEFROMPOSITION, pos.first));
            m_editor->markerAdd(line, 24); // BOOKMARK_MARKER
        }
    }

    m_markStatusLabel->setText(QString("%1 marks placed").arg(positions.size()));
    m_markStatusLabel->setStyleSheet("QLabel { color: #a6e3a1; font-size: 11px; }");
}

void SearchDialog::clearMarks()
{
    if (!m_editor) return;
    long docLen = m_editor->SendScintilla(QsciScintilla::SCI_GETLENGTH);
    m_editor->SendScintilla(QsciScintilla::SCI_SETINDICATORCURRENT, MARK_INDICATOR);
    m_editor->SendScintilla(QsciScintilla::SCI_INDICATORCLEARRANGE, 0, docLen);

    m_markStatusLabel->setText("Marks cleared");
    m_markStatusLabel->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; }");
}
