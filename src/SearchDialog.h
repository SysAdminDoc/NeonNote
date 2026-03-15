#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QRadioButton>
#include <Qsci/qsciscintilla.h>

class SearchResultsPanel;

struct SearchOptions {
    QString searchTerm;
    QString replaceTerm;
    bool matchCase = false;
    bool wholeWord = false;
    bool wrapAround = true;
    int searchMode = 0; // 0=Normal, 1=Extended, 2=Regex
    bool directionUp = false;
};

struct FindInFilesOptions : public SearchOptions {
    QString directory;
    QString filters = "*.*";
    bool inSubFolders = true;
    bool inHiddenFolders = false;
};

class SearchDialog : public QDialog {
    Q_OBJECT
public:
    explicit SearchDialog(QWidget* parent = nullptr);

    void setEditor(QsciScintilla* editor);
    void setResultsPanel(SearchResultsPanel* panel);
    void setOpenEditors(const QVector<QPair<QString, QsciScintilla*>>& editors);
    void showTab(int tabIndex);

signals:
    void openFileAtLine(const QString& filePath, int line, int col);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    // Tab pages
    QWidget* createFindPage();
    QWidget* createReplacePage();
    QWidget* createFindInFilesPage();
    QWidget* createMarkPage();

    // Shared option widgets factory
    QGroupBox* createSearchModeGroup(QWidget* parent);
    QWidget* createOptionsRow(QWidget* parent, QCheckBox*& matchCase, QCheckBox*& wholeWord, QCheckBox*& wrapAround);

    // Common style
    QString editStyle() const;
    QString btnStyle() const;
    QString btnPrimaryStyle() const;
    QString checkStyle() const;
    QString radioStyle() const;
    QString groupStyle() const;
    QString comboStyle() const;

    // Find operations
    void findNext();
    void findPrev();
    void findCount();
    void findAllInCurrent();
    void findAllInAllOpen();

    // Replace operations
    void replaceNext();
    void replaceAll();
    void replaceAllInAllOpen();

    // Find in Files operations
    void findInFiles();
    void replaceInFiles();
    void browseFindInFilesDir();

    // Mark operations
    void markAll();
    void clearMarks();

    // Helpers
    SearchOptions gatherFindOptions() const;
    SearchOptions gatherReplaceOptions() const;
    FindInFilesOptions gatherFifOptions() const;
    SearchOptions gatherMarkOptions() const;
    bool doFind(QsciScintilla* editor, const SearchOptions& opts, bool forward);
    int countMatches(QsciScintilla* editor, const SearchOptions& opts);
    QVector<QPair<int, int>> findAllPositions(QsciScintilla* editor, const SearchOptions& opts);
    QString expandExtended(const QString& text) const;
    int searchFlags(const SearchOptions& opts) const;

    QsciScintilla* m_editor = nullptr;
    SearchResultsPanel* m_resultsPanel = nullptr;
    QVector<QPair<QString, QsciScintilla*>> m_openEditors;

    QTabWidget* m_tabs;

    // Find tab widgets
    QComboBox* m_findCombo;
    QCheckBox* m_findMatchCase;
    QCheckBox* m_findWholeWord;
    QCheckBox* m_findWrapAround;
    QRadioButton* m_findModeNormal;
    QRadioButton* m_findModeExtended;
    QRadioButton* m_findModeRegex;
    QRadioButton* m_findDirDown;
    QRadioButton* m_findDirUp;
    QLabel* m_findStatusLabel;

    // Replace tab widgets
    QComboBox* m_replaceSearchCombo;
    QComboBox* m_replaceWithCombo;
    QCheckBox* m_replaceMatchCase;
    QCheckBox* m_replaceWholeWord;
    QCheckBox* m_replaceWrapAround;
    QRadioButton* m_replaceModeNormal;
    QRadioButton* m_replaceModeExtended;
    QRadioButton* m_replaceModeRegex;
    QLabel* m_replaceStatusLabel;

    // Find in Files tab widgets
    QComboBox* m_fifSearchCombo;
    QComboBox* m_fifReplaceCombo;
    QComboBox* m_fifDirCombo;
    QLineEdit* m_fifFilters;
    QCheckBox* m_fifMatchCase;
    QCheckBox* m_fifWholeWord;
    QCheckBox* m_fifSubFolders;
    QCheckBox* m_fifHiddenFolders;
    QRadioButton* m_fifModeNormal;
    QRadioButton* m_fifModeExtended;
    QRadioButton* m_fifModeRegex;
    QCheckBox* m_fifFollowDoc;
    QLabel* m_fifStatusLabel;

    // Mark tab widgets
    QComboBox* m_markCombo;
    QCheckBox* m_markMatchCase;
    QCheckBox* m_markWholeWord;
    QCheckBox* m_markBookmarkLine;
    QCheckBox* m_markPurge;
    QRadioButton* m_markModeNormal;
    QRadioButton* m_markModeExtended;
    QRadioButton* m_markModeRegex;
    QLabel* m_markStatusLabel;

    static constexpr int MARK_INDICATOR = 8;
};
