#pragma once
#include <QMainWindow>
#include <QMap>
#include <QPushButton>
#include <QMenu>
#include <QStringList>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTimer>
#include "TabBar.h"
#include "BookmarkBar.h"
#include "StatusBar.h"
#include "EditorWidget.h"
#include "FindReplaceBar.h"

class QsciScintilla;
class SearchDialog;
class SearchResultsPanel;

enum class FileEncoding {
    UTF8,
    UTF8_BOM,
    UTF16_LE,
    UTF16_BE,
    Windows1252,
    ISO8859_1,
    Shift_JIS,
    GB2312,
    EUC_KR,
};

struct TabData {
    QsciScintilla* editor = nullptr;
    QString filePath;
    bool modified = false;
    bool readOnly = false;
    FileEncoding encoding = FileEncoding::UTF8;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    void newTab();
    void closeTab(int index);
    void switchTab(int index);
    void openFile();
    void saveFile();
    void saveFileAs();
    void onCursorPositionChanged(int line, int col);
    void onBookmarkClicked(int index);

private:
    void openFileByPath(const QString& filePath);
    void updateTitle();
    void updateStatusBar();
    QString tabTitle(int index) const;
    void setupCaptionButtons();
    void setupMenuButton();
    void showAppMenu();
    void showEditorContextMenu(const QPoint& pos);
    void showTabContextMenu(const QPoint& pos);
    void connectEditorSignals(QsciScintilla* editor);
    int tabIndexForEditor(QsciScintilla* editor) const;

    // Session & recent files
    void saveSession();
    void restoreSession();
    void addRecentFile(const QString& filePath);
    QStringList loadRecentFiles();

    // Tab cache (Notepad++-style backup)
    QString cacheDir() const;
    void cacheTabContent(int index);
    void cacheAllTabs();
    void clearTabCache(int index);
    void autoSaveTab(int index);
    QString defaultSaveDir() const;
    void setDefaultSaveDir(const QString& dir);
    void showSettings();

    // Text operations
    void trimTrailingWhitespace();
    void sortLines(bool ascending);
    void removeEmptyLines();
    void joinLines();
    void convertTabsToSpaces();
    void convertSpacesToTabs();

    // Line bookmarks
    void toggleBookmarkOnLine();
    void nextBookmark();
    void prevBookmark();
    void clearAllBookmarks();

    // File operations
    void reloadFromDisk();
    void toggleReadOnly();
    void reopenLastClosed();
    void closeAllTabs();
    void closeAllButActive();
    void showSummary();
    void openContainingFolder();

    // EOL helpers
    QString eolModeString(QsciScintilla* editor) const;
    void setEolMode(QsciScintilla::EolMode mode);

    // Search
    void showSearchDialog(int tab = 0);
    void onSearchResultClicked(const QString& filePath, int line, int col);
    QVector<QPair<QString, QsciScintilla*>> getOpenEditors() const;

    // Encoding
    FileEncoding detectEncoding(const QByteArray& data) const;
    QString encodingName(FileEncoding enc) const;
    QByteArray decodeToUtf8(const QByteArray& data, FileEncoding enc) const;
    QByteArray encodeFromText(const QString& text, FileEncoding enc) const;
    void convertEncoding(FileEncoding newEnc);

    // Auto-close brackets
    void onCharAdded(int ch);

    // UI components
    NeonTabBar* m_tabBar;
    BookmarkBar* m_bookmarkBar;
    NeonStatusBar* m_statusBar;
    EditorWidget* m_editorWidget;
    FindReplaceBar* m_findBar;
    SearchDialog* m_searchDialog = nullptr;
    SearchResultsPanel* m_searchResults = nullptr;
    QWidget* m_centralWidget;

    // Title bar buttons
    QPushButton* m_btnMenu = nullptr;
    QPushButton* m_btnMin = nullptr;
    QPushButton* m_btnMax = nullptr;
    QPushButton* m_btnClose = nullptr;

    // Tab data
    QVector<TabData> m_tabs;
    QStringList m_closedFiles; // for reopen last closed
    int m_untitledCount = 0;

    // View state
    bool m_showWhitespace = false;
    bool m_showEol = false;
    bool m_autoCloseBrackets = true;

    // Cache/auto-save
    QTimer* m_cacheTimer = nullptr;
    QString m_defaultSaveDir;

    // Bookmark marker number
    static constexpr int BOOKMARK_MARKER = 24;

    static constexpr int CAPTION_BTN_W = 46;
    static constexpr int CAPTION_BTN_H = 34;
    static constexpr int RESIZE_BORDER = 5;
    static constexpr int MAX_RECENT_FILES = 15;
};
