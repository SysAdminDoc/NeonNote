#include "MainWindow.h"
#include "Theme.h"
#include "SearchDialog.h"
#include "SearchResultsPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QMimeData>
#include <QShortcut>
#include <QRegularExpression>
#include <QApplication>
#include <QScreen>
#include <QInputDialog>
#include <QMessageBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QSettings>
#include <QTextStream>
#include <QProcess>
#include <QDir>
#include <QStandardPaths>
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include <Qsci/qsciscintilla.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <windows.h>

static const QString MENU_STYLE = R"(
    QMenu {
        background: #1e1e2e;
        color: #cdd6f4;
        border: 1px solid #45475a;
        padding: 4px 0;
    }
    QMenu::item {
        padding: 6px 32px 6px 16px;
    }
    QMenu::item:selected {
        background: #313244;
    }
    QMenu::separator {
        height: 1px;
        background: #313244;
        margin: 4px 8px;
    }
    QMenu::item:disabled {
        color: #585b70;
    }
)";

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint |
                   Qt::WindowMinMaxButtonsHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAcceptDrops(true);
    setMinimumSize(800, 500);

    MARGINS margins = {-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(reinterpret_cast<HWND>(winId()), &margins);

    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);
    auto* mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Tab bar row: menu button + tabs + caption buttons
    auto* tabRow = new QHBoxLayout();
    tabRow->setContentsMargins(0, 0, 0, 0);
    tabRow->setSpacing(0);

    setupMenuButton();
    tabRow->addWidget(m_btnMenu);

    m_tabBar = new NeonTabBar(this);
    tabRow->addWidget(m_tabBar, 1);

    setupCaptionButtons();
    tabRow->addWidget(m_btnMin);
    tabRow->addWidget(m_btnMax);
    tabRow->addWidget(m_btnClose);

    mainLayout->addLayout(tabRow);

    m_bookmarkBar = new BookmarkBar(this);
    mainLayout->addWidget(m_bookmarkBar);

    m_findBar = new FindReplaceBar(this);
    mainLayout->addWidget(m_findBar);

    m_editorWidget = new EditorWidget(this);
    mainLayout->addWidget(m_editorWidget, 1);

    m_searchResults = new SearchResultsPanel(this);
    mainLayout->addWidget(m_searchResults);
    connect(m_searchResults, &SearchResultsPanel::hitDoubleClicked,
            this, &MainWindow::onSearchResultClicked);

    m_statusBar = new NeonStatusBar(this);
    mainLayout->addWidget(m_statusBar);

    // Connections
    connect(m_tabBar, &NeonTabBar::newTabRequested, this, &MainWindow::newTab);
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, &MainWindow::closeTab);
    connect(m_tabBar, &QTabBar::currentChanged, this, &MainWindow::switchTab);
    connect(m_tabBar, &NeonTabBar::middleClicked, this, &MainWindow::closeTab);
    connect(m_bookmarkBar, &BookmarkBar::bookmarkClicked, this, &MainWindow::onBookmarkClicked);
    connect(m_bookmarkBar, &BookmarkBar::heightChanged, this, [this]() { update(); });

    m_tabBar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tabBar, &QWidget::customContextMenuRequested, this, &MainWindow::showTabContextMenu);

    // ── Keyboard shortcuts ──
    auto sc = [this](const QKeySequence& key, auto slot) {
        auto* s = new QShortcut(key, this);
        connect(s, &QShortcut::activated, this, slot);
    };

    // File
    sc(QKeySequence::New, &MainWindow::newTab);
    sc(QKeySequence::Open, &MainWindow::openFile);
    sc(QKeySequence::Save, &MainWindow::saveFile);
    sc(QKeySequence("Ctrl+Shift+S"), &MainWindow::saveFileAs);
    sc(QKeySequence::Close, [this]() { closeTab(m_tabBar->currentIndex()); });
    sc(QKeySequence("Ctrl+Shift+T"), &MainWindow::reopenLastClosed);

    // Find/Replace
    sc(QKeySequence::Find, [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size()) {
            m_findBar->setEditor(m_tabs[idx].editor);
            m_findBar->showFind();
        }
    });
    sc(QKeySequence::Replace, [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size()) {
            m_findBar->setEditor(m_tabs[idx].editor);
            m_findBar->showReplace();
        }
    });
    sc(QKeySequence("Ctrl+Shift+F"), [this]() { showSearchDialog(2); }); // Find in Files

    // Navigation
    sc(QKeySequence("Ctrl+G"), [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx < 0 || idx >= m_tabs.size()) return;
        auto* e = m_tabs[idx].editor;
        int line, col;
        e->getCursorPosition(&line, &col);
        bool ok;
        int target = QInputDialog::getInt(this, "Go to Line", "Line number:",
                                           line + 1, 1, e->lines(), 1, &ok);
        if (ok) e->setCursorPosition(target - 1, 0);
    });

    // Zoom
    sc(QKeySequence::ZoomIn, [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size()) m_tabs[idx].editor->zoomIn();
    });
    sc(QKeySequence::ZoomOut, [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size()) m_tabs[idx].editor->zoomOut();
    });
    sc(QKeySequence("Ctrl+0"), [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size()) m_tabs[idx].editor->zoomTo(0);
    });

    // Text operations
    sc(QKeySequence("Ctrl+D"), [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size())
            m_tabs[idx].editor->SendScintilla(QsciScintillaBase::SCI_LINEDUPLICATE);
    });
    sc(QKeySequence("Ctrl+Shift+K"), [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size())
            m_tabs[idx].editor->SendScintilla(QsciScintillaBase::SCI_LINEDELETE);
    });
    sc(QKeySequence("Ctrl+Shift+U"), [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size())
            m_tabs[idx].editor->SendScintilla(QsciScintillaBase::SCI_UPPERCASE);
    });
    sc(QKeySequence("Ctrl+U"), [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size())
            m_tabs[idx].editor->SendScintilla(QsciScintillaBase::SCI_LOWERCASE);
    });
    sc(QKeySequence("Alt+Up"), [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size())
            m_tabs[idx].editor->SendScintilla(QsciScintillaBase::SCI_MOVESELECTEDLINESUP);
    });
    sc(QKeySequence("Alt+Down"), [this]() {
        int idx = m_tabBar->currentIndex();
        if (idx >= 0 && idx < m_tabs.size())
            m_tabs[idx].editor->SendScintilla(QsciScintillaBase::SCI_MOVESELECTEDLINESDOWN);
    });

    // Bookmarks
    sc(QKeySequence("Ctrl+F2"), &MainWindow::toggleBookmarkOnLine);
    sc(QKeySequence("F2"), &MainWindow::nextBookmark);
    sc(QKeySequence("Shift+F2"), &MainWindow::prevBookmark);

    // Tab switching: Ctrl+Tab, Ctrl+Shift+Tab, Ctrl+1-9
    sc(QKeySequence("Ctrl+Tab"), [this]() {
        int next = (m_tabBar->currentIndex() + 1) % m_tabBar->count();
        m_tabBar->setCurrentIndex(next);
    });
    sc(QKeySequence("Ctrl+Shift+Tab"), [this]() {
        int prev = (m_tabBar->currentIndex() - 1 + m_tabBar->count()) % m_tabBar->count();
        m_tabBar->setCurrentIndex(prev);
    });
    for (int i = 1; i <= 9; ++i) {
        auto* s = new QShortcut(QKeySequence(QString("Ctrl+%1").arg(i)), this);
        connect(s, &QShortcut::activated, this, [this, i]() {
            int target = (i == 9) ? m_tabBar->count() - 1 : i - 1;
            if (target >= 0 && target < m_tabBar->count())
                m_tabBar->setCurrentIndex(target);
        });
    }

    // Load settings
    {
        QSettings s("NeonNote", "NeonNote");
        m_defaultSaveDir = s.value("settings/defaultSaveDir").toString();
    }

    // Restore previous session
    restoreSession();
    if (m_tabs.isEmpty())
        newTab();

    // Auto-cache timer: save all tab contents every 30 seconds
    m_cacheTimer = new QTimer(this);
    connect(m_cacheTimer, &QTimer::timeout, this, &MainWindow::cacheAllTabs);
    m_cacheTimer->start(30000);

    resize(1200, 800);
    updateTitle();
}

// ── Menu Button ──

void MainWindow::setupMenuButton()
{
    m_btnMenu = new QPushButton(QString::fromUtf16(u"\uE700"), this);
    m_btnMenu->setFixedSize(46, CAPTION_BTN_H);
    m_btnMenu->setFlat(true);
    m_btnMenu->setCursor(Qt::ArrowCursor);
    m_btnMenu->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            color: #a6adc8;
            border: none;
            font-size: 12px;
            font-family: "Segoe MDL2 Assets";
        }
        QPushButton:hover { background: #313244; color: #cdd6f4; }
    )");
    connect(m_btnMenu, &QPushButton::clicked, this, &MainWindow::showAppMenu);
}

// ── App Menu ──

void MainWindow::showAppMenu()
{
    QMenu menu(this);
    menu.setStyleSheet(MENU_STYLE);

    int idx = m_tabBar->currentIndex();
    QsciScintilla* editor = (idx >= 0 && idx < m_tabs.size()) ? m_tabs[idx].editor : nullptr;

    // ── File ──
    auto* fileMenu = menu.addMenu("File");
    fileMenu->setStyleSheet(MENU_STYLE);
    fileMenu->addAction("New Tab                    Ctrl+N", this, &MainWindow::newTab);
    fileMenu->addAction("Open...                     Ctrl+O", this, &MainWindow::openFile);

    QStringList recentFiles = loadRecentFiles();
    if (!recentFiles.isEmpty()) {
        auto* recentMenu = fileMenu->addMenu("Recent Files");
        recentMenu->setStyleSheet(MENU_STYLE);
        for (const QString& path : recentFiles) {
            QFileInfo fi(path);
            if (!fi.exists()) continue;
            recentMenu->addAction(fi.fileName(), this, [this, path]() {
                openFileByPath(path);
            })->setToolTip(path);
        }
        recentMenu->addSeparator();
        recentMenu->addAction("Clear Recent Files", this, [this]() {
            QSettings s("NeonNote", "NeonNote");
            s.remove("recentFiles");
        });
    }

    fileMenu->addAction("Reopen Last Closed     Ctrl+Shift+T", this, &MainWindow::reopenLastClosed);
    fileMenu->addAction("Reload from Disk", this, &MainWindow::reloadFromDisk);
    fileMenu->addSeparator();
    fileMenu->addAction("Save                           Ctrl+S", this, &MainWindow::saveFile);
    fileMenu->addAction("Save As...               Ctrl+Shift+S", this, &MainWindow::saveFileAs);
    fileMenu->addSeparator();

    if (editor) {
        auto* roAction = fileMenu->addAction("Read-Only");
        roAction->setCheckable(true);
        roAction->setChecked(idx >= 0 && idx < m_tabs.size() && m_tabs[idx].readOnly);
        connect(roAction, &QAction::toggled, this, [this](bool) { toggleReadOnly(); });
    }

    fileMenu->addSeparator();
    fileMenu->addAction("Close                         Ctrl+W", this, [this]() { closeTab(m_tabBar->currentIndex()); });
    fileMenu->addAction("Close All Tabs", this, &MainWindow::closeAllTabs);
    fileMenu->addAction("Close All But Active", this, &MainWindow::closeAllButActive);
    fileMenu->addSeparator();

    if (idx >= 0 && idx < m_tabs.size() && !m_tabs[idx].filePath.isEmpty()) {
        fileMenu->addAction("Open Containing Folder", this, &MainWindow::openContainingFolder);
        fileMenu->addAction("Copy File Path", this, [this]() {
            int i = m_tabBar->currentIndex();
            if (i >= 0 && i < m_tabs.size())
                QApplication::clipboard()->setText(m_tabs[i].filePath);
        });
        fileMenu->addSeparator();
    }

    fileMenu->addAction("Exit", this, &QWidget::close);

    // ── Edit ──
    auto* editMenu = menu.addMenu("Edit");
    editMenu->setStyleSheet(MENU_STYLE);
    if (editor) {
        editMenu->addAction("Undo                       Ctrl+Z", editor, &QsciScintilla::undo)->setEnabled(editor->isUndoAvailable());
        editMenu->addAction("Redo                       Ctrl+Y", editor, &QsciScintilla::redo)->setEnabled(editor->isRedoAvailable());
        editMenu->addSeparator();
        editMenu->addAction("Cut                         Ctrl+X", editor, &QsciScintilla::cut)->setEnabled(editor->hasSelectedText());
        editMenu->addAction("Copy                       Ctrl+C", editor, &QsciScintilla::copy)->setEnabled(editor->hasSelectedText());
        editMenu->addAction("Paste                      Ctrl+V", editor, &QsciScintilla::paste);
        editMenu->addSeparator();
        editMenu->addAction("Select All                 Ctrl+A", editor, &QsciScintilla::selectAll);
        editMenu->addSeparator();

        // Text operations submenu
        auto* textOps = editMenu->addMenu("Text Operations");
        textOps->setStyleSheet(MENU_STYLE);
        textOps->addAction("UPPERCASE            Ctrl+Shift+U", this, [editor]() {
            editor->SendScintilla(QsciScintillaBase::SCI_UPPERCASE);
        })->setEnabled(editor->hasSelectedText());
        textOps->addAction("lowercase                    Ctrl+U", this, [editor]() {
            editor->SendScintilla(QsciScintillaBase::SCI_LOWERCASE);
        })->setEnabled(editor->hasSelectedText());
        textOps->addSeparator();
        textOps->addAction("Duplicate Line              Ctrl+D", this, [editor]() {
            editor->SendScintilla(QsciScintillaBase::SCI_LINEDUPLICATE);
        });
        textOps->addAction("Delete Line          Ctrl+Shift+K", this, [editor]() {
            editor->SendScintilla(QsciScintillaBase::SCI_LINEDELETE);
        });
        textOps->addAction("Move Line Up           Alt+Up", this, [editor]() {
            editor->SendScintilla(QsciScintillaBase::SCI_MOVESELECTEDLINESUP);
        });
        textOps->addAction("Move Line Down       Alt+Down", this, [editor]() {
            editor->SendScintilla(QsciScintillaBase::SCI_MOVESELECTEDLINESDOWN);
        });
        textOps->addSeparator();
        textOps->addAction("Trim Trailing Whitespace", this, &MainWindow::trimTrailingWhitespace);
        textOps->addAction("Remove Empty Lines", this, &MainWindow::removeEmptyLines);
        textOps->addAction("Join Lines", this, &MainWindow::joinLines);
        textOps->addSeparator();
        textOps->addAction("Sort Lines (Ascending)", this, [this]() { sortLines(true); });
        textOps->addAction("Sort Lines (Descending)", this, [this]() { sortLines(false); });

        // Indent submenu
        auto* indentMenu = editMenu->addMenu("Indentation");
        indentMenu->setStyleSheet(MENU_STYLE);
        indentMenu->addAction("Increase Indent        Tab", this, [editor]() {
            editor->SendScintilla(QsciScintillaBase::SCI_TAB);
        });
        indentMenu->addAction("Decrease Indent  Shift+Tab", this, [editor]() {
            editor->SendScintilla(QsciScintillaBase::SCI_BACKTAB);
        });
        indentMenu->addSeparator();
        indentMenu->addAction("Convert Tabs to Spaces", this, &MainWindow::convertTabsToSpaces);
        indentMenu->addAction("Convert Spaces to Tabs", this, &MainWindow::convertSpacesToTabs);

        // Bookmarks submenu
        auto* bmMenu = editMenu->addMenu("Bookmarks");
        bmMenu->setStyleSheet(MENU_STYLE);
        bmMenu->addAction("Toggle Bookmark     Ctrl+F2", this, &MainWindow::toggleBookmarkOnLine);
        bmMenu->addAction("Next Bookmark              F2", this, &MainWindow::nextBookmark);
        bmMenu->addAction("Previous Bookmark   Shift+F2", this, &MainWindow::prevBookmark);
        bmMenu->addAction("Clear All Bookmarks", this, &MainWindow::clearAllBookmarks);

        editMenu->addSeparator();
        editMenu->addAction("Find...                      Ctrl+F", this, [this]() {
            int i = m_tabBar->currentIndex();
            if (i >= 0 && i < m_tabs.size()) {
                m_findBar->setEditor(m_tabs[i].editor);
                m_findBar->showFind();
            }
        });
        editMenu->addAction("Replace...                 Ctrl+H", this, [this]() {
            int i = m_tabBar->currentIndex();
            if (i >= 0 && i < m_tabs.size()) {
                m_findBar->setEditor(m_tabs[i].editor);
                m_findBar->showReplace();
            }
        });
        editMenu->addAction("Find in Files...   Ctrl+Shift+F", this, [this]() { showSearchDialog(2); });
        editMenu->addAction("Search Dialog...", this, [this]() { showSearchDialog(0); });
        editMenu->addSeparator();
        editMenu->addAction("Go to Line...             Ctrl+G", this, [this, editor]() {
            int line, col;
            editor->getCursorPosition(&line, &col);
            bool ok;
            int target = QInputDialog::getInt(this, "Go to Line", "Line number:",
                                               line + 1, 1, editor->lines(), 1, &ok);
            if (ok) editor->setCursorPosition(target - 1, 0);
        });
    }

    // ── View ──
    auto* viewMenu = menu.addMenu("View");
    viewMenu->setStyleSheet(MENU_STYLE);
    if (editor) {
        auto* wrapAction = viewMenu->addAction("Word Wrap");
        wrapAction->setCheckable(true);
        wrapAction->setChecked(editor->wrapMode() != QsciScintilla::WrapNone);
        connect(wrapAction, &QAction::toggled, this, [editor](bool wrap) {
            editor->setWrapMode(wrap ? QsciScintilla::WrapWord : QsciScintilla::WrapNone);
        });

        auto* wsAction = viewMenu->addAction("Show Whitespace");
        wsAction->setCheckable(true);
        wsAction->setChecked(m_showWhitespace);
        connect(wsAction, &QAction::toggled, this, [this](bool show) {
            m_showWhitespace = show;
            for (auto& td : m_tabs)
                td.editor->setWhitespaceVisibility(show
                    ? QsciScintilla::WsVisible : QsciScintilla::WsInvisible);
        });

        auto* eolAction = viewMenu->addAction("Show Line Endings");
        eolAction->setCheckable(true);
        eolAction->setChecked(m_showEol);
        connect(eolAction, &QAction::toggled, this, [this](bool show) {
            m_showEol = show;
            for (auto& td : m_tabs)
                td.editor->setEolVisibility(show);
        });

        auto* bracketAction = viewMenu->addAction("Auto-Close Brackets");
        bracketAction->setCheckable(true);
        bracketAction->setChecked(m_autoCloseBrackets);
        connect(bracketAction, &QAction::toggled, this, [this](bool on) {
            m_autoCloseBrackets = on;
        });

        viewMenu->addSeparator();
        viewMenu->addAction("Zoom In", editor, [editor]() { editor->zoomIn(); });
        viewMenu->addAction("Zoom Out", editor, [editor]() { editor->zoomOut(); });
        viewMenu->addAction("Reset Zoom", editor, [editor]() { editor->zoomTo(0); });
    }

    // ── Line Endings ──
    if (editor) {
        auto* eolMenu = menu.addMenu("Line Endings");
        eolMenu->setStyleSheet(MENU_STYLE);
        auto curEol = editor->eolMode();
        auto addEol = [&](const QString& label, QsciScintilla::EolMode mode) {
            auto* a = eolMenu->addAction(label, this, [this, mode]() { setEolMode(mode); });
            a->setCheckable(true);
            a->setChecked(curEol == mode);
        };
        addEol("Windows (CRLF)", QsciScintilla::EolWindows);
        addEol("Unix (LF)", QsciScintilla::EolUnix);
        addEol("Mac (CR)", QsciScintilla::EolMac);
    }

    // ── Encoding ──
    if (idx >= 0 && idx < m_tabs.size()) {
        auto* encMenu = menu.addMenu("Encoding");
        encMenu->setStyleSheet(MENU_STYLE);
        FileEncoding curEnc = m_tabs[idx].encoding;
        auto addEnc = [&](const QString& label, FileEncoding enc) {
            auto* a = encMenu->addAction(label, this, [this, enc]() { convertEncoding(enc); });
            a->setCheckable(true);
            a->setChecked(curEnc == enc);
        };
        addEnc("UTF-8", FileEncoding::UTF8);
        addEnc("UTF-8 with BOM", FileEncoding::UTF8_BOM);
        addEnc("UTF-16 LE", FileEncoding::UTF16_LE);
        addEnc("UTF-16 BE", FileEncoding::UTF16_BE);
        encMenu->addSeparator();
        addEnc("Windows-1252 (ANSI)", FileEncoding::Windows1252);
        addEnc("ISO 8859-1 (Latin-1)", FileEncoding::ISO8859_1);
        encMenu->addSeparator();
        addEnc("Shift-JIS", FileEncoding::Shift_JIS);
        addEnc("GB2312", FileEncoding::GB2312);
        addEnc("EUC-KR", FileEncoding::EUC_KR);
    }

    // ── Summary ──
    menu.addSeparator();
    if (editor)
        menu.addAction("Document Summary...", this, &MainWindow::showSummary);

    menu.addSeparator();
    menu.addAction("Settings...", this, &MainWindow::showSettings);

    menu.addSeparator();
    menu.addAction("About NeonNote", this, [this]() {
        QMessageBox::about(this, "About NeonNote",
            "<h3>NeonNote</h3>"
            "<p>A premium code editor with Catppuccin Mocha theming.</p>"
            "<p>Built with Qt + QScintilla.</p>");
    });

    menu.exec(m_btnMenu->mapToGlobal(QPoint(0, m_btnMenu->height())));
}

// ── Caption Buttons ──

void MainWindow::setupCaptionButtons()
{
    auto makeBtn = [this](const QString& text, const QString& style) -> QPushButton* {
        auto* btn = new QPushButton(text, this);
        btn->setFixedSize(CAPTION_BTN_W, CAPTION_BTN_H);
        btn->setFlat(true);
        btn->setCursor(Qt::ArrowCursor);
        btn->setStyleSheet(style);
        return btn;
    };

    QString baseStyle = R"(
        QPushButton {
            background: #11111b; color: #a6adc8;
            border: none; font-size: 10px; font-family: "Segoe MDL2 Assets";
        }
        QPushButton:hover { background: #313244; color: #cdd6f4; }
    )";
    QString closeStyle = R"(
        QPushButton {
            background: #11111b; color: #a6adc8;
            border: none; font-size: 10px; font-family: "Segoe MDL2 Assets";
        }
        QPushButton:hover { background: #f38ba8; color: #11111b; }
    )";

    m_btnMin = makeBtn(QString::fromUtf16(u"\uE921"), baseStyle);
    m_btnMax = makeBtn(QString::fromUtf16(u"\uE922"), baseStyle);
    m_btnClose = makeBtn(QString::fromUtf16(u"\uE8BB"), closeStyle);

    connect(m_btnMin, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(m_btnMax, &QPushButton::clicked, this, [this]() {
        HWND hwnd = reinterpret_cast<HWND>(winId());
        ShowWindow(hwnd, isMaximized() ? SW_RESTORE : SW_MAXIMIZE);
    });
    connect(m_btnClose, &QPushButton::clicked, this, &QWidget::close);
}

// ── Native Event Handling ──

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (eventType != "windows_generic_MSG") return false;
    auto* msg = static_cast<MSG*>(message);

    switch (msg->message) {
    case WM_NCCALCSIZE:
        if (msg->wParam == TRUE) {
            *result = 0;
            return true;
        }
        break;

    case WM_NCLBUTTONDOWN: {
        // Drag-to-restore: when maximized, clicking caption restores then drags
        static bool s_restoring = false;
        if (!s_restoring && msg->wParam == HTCAPTION && isMaximized()) {
            s_restoring = true;

            POINT cur;
            GetCursorPos(&cur);
            RECT rc;
            GetWindowRect(msg->hwnd, &rc);
            double ratio = double(cur.x - rc.left) / double(rc.right - rc.left);

            ShowWindow(msg->hwnd, SW_RESTORE);

            RECT restored;
            GetWindowRect(msg->hwnd, &restored);
            int newW = restored.right - restored.left;
            int newX = cur.x - int(ratio * newW);
            SetWindowPos(msg->hwnd, nullptr, newX, cur.y - 10,
                         0, 0, SWP_NOSIZE | SWP_NOZORDER);

            ReleaseCapture();
            SendMessage(msg->hwnd, WM_NCLBUTTONDOWN, HTCAPTION,
                        MAKELPARAM(cur.x, cur.y));

            s_restoring = false;
            *result = 0;
            return true;
        }
        break;
    }

    case WM_NCHITTEST: {
        POINT pt = {GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam)};
        RECT rc;
        GetWindowRect(msg->hwnd, &rc);
        int x = pt.x - rc.left;
        int y = pt.y - rc.top;
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        if (!isMaximized()) {
            int border = int(RESIZE_BORDER * devicePixelRatioF());
            bool top = y < border;
            bool bottom = y >= h - border;
            bool left = x < border;
            bool right = x >= w - border;

            if (top && left) { *result = HTTOPLEFT; return true; }
            if (top && right) { *result = HTTOPRIGHT; return true; }
            if (bottom && left) { *result = HTBOTTOMLEFT; return true; }
            if (bottom && right) { *result = HTBOTTOMRIGHT; return true; }
            if (top) { *result = HTTOP; return true; }
            if (bottom) { *result = HTBOTTOM; return true; }
            if (left) { *result = HTLEFT; return true; }
            if (right) { *result = HTRIGHT; return true; }
        }

        // Use QCursor::pos() for Qt widget checks (DPI-aware logical coords)
        QPoint globalPt = QCursor::pos();

        // Title bar buttons
        for (auto* btn : {m_btnMenu, m_btnMin, m_btnMax, m_btnClose}) {
            if (btn && btn->isVisible() && btn->rect().contains(btn->mapFromGlobal(globalPt))) {
                *result = HTCLIENT;
                return true;
            }
        }

        // Tab bar area
        if (m_tabBar) {
            QPoint tabPt = m_tabBar->mapFromGlobal(globalPt);
            if (m_tabBar->rect().contains(tabPt)) {
                for (int i = 0; i < m_tabBar->count(); ++i) {
                    if (m_tabBar->tabRect(i).contains(tabPt)) {
                        *result = HTCLIENT;
                        return true;
                    }
                }
                if (m_tabBar->newTabButtonRect().contains(tabPt)) {
                    *result = HTCLIENT;
                    return true;
                }
                // Empty tab bar space = draggable caption
                *result = HTCAPTION;
                return true;
            }
        }

        // Anything in the top tab-row height that isn't a widget = caption
        int captionPhysical = int(CAPTION_BTN_H * devicePixelRatioF());
        if (y < captionPhysical) {
            *result = HTCAPTION;
            return true;
        }

        break;
    }

    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(msg->lParam);
        HMONITOR mon = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfo(mon, &mi)) {
            mmi->ptMaxPosition.x = mi.rcWork.left - mi.rcMonitor.left;
            mmi->ptMaxPosition.y = mi.rcWork.top - mi.rcMonitor.top;
            mmi->ptMaxSize.x = mi.rcWork.right - mi.rcWork.left;
            mmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top;
        }
        *result = 0;
        return true;
    }

    case WM_NCPAINT:
        *result = 0;
        return true;
    }

    return QMainWindow::nativeEvent(eventType, message, result);
}

// ── Window Events ──

void MainWindow::paintEvent(QPaintEvent*) {}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        m_btnMax->setText(isMaximized()
            ? QString::fromUtf16(u"\uE923")
            : QString::fromUtf16(u"\uE922"));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // If default save dir is set, auto-save everything silently
    if (!m_defaultSaveDir.isEmpty()) {
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i].modified)
                autoSaveTab(i);
        }
    } else {
        // Check for any unsaved tabs
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs[i].modified) {
                m_tabBar->setCurrentIndex(i);
                QString name = m_tabs[i].filePath.isEmpty()
                    ? tabTitle(i) : QFileInfo(m_tabs[i].filePath).fileName();
                auto result = QMessageBox::question(this, "Unsaved Changes",
                    QString("Save changes to \"%1\"?").arg(name),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
                if (result == QMessageBox::Save) {
                    saveFile();
                    if (m_tabs[i].modified) { event->ignore(); return; }
                } else if (result == QMessageBox::Cancel) {
                    event->ignore();
                    return;
                }
            }
        }
    }
    saveSession();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            openFileByPath(url.toLocalFile());
    }
}

// ── Session & Recent Files ──

QString MainWindow::cacheDir() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cache";
    QDir().mkpath(dir);
    return dir;
}

void MainWindow::cacheTabContent(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    const auto& td = m_tabs[index];

    QString cache = cacheDir();
    QString cacheFile = QString("%1/tab_%2.txt").arg(cache).arg(index);
    QString metaFile = QString("%1/tab_%2.meta").arg(cache).arg(index);

    // Save content
    QFile f(cacheFile);
    if (f.open(QIODevice::WriteOnly)) {
        f.write(td.editor->text().toUtf8());
        f.close();
    }

    // Save metadata
    QSettings meta(metaFile, QSettings::IniFormat);
    meta.setValue("filePath", td.filePath);
    meta.setValue("modified", td.modified);
    meta.setValue("readOnly", td.readOnly);
    meta.setValue("encoding", static_cast<int>(td.encoding));
    meta.setValue("eolMode", static_cast<int>(td.editor->eolMode()));
    meta.setValue("tabTitle", m_tabBar->tabText(index).remove(QRegularExpression("^\\* ")));

    int line, col;
    td.editor->getCursorPosition(&line, &col);
    meta.setValue("cursorLine", line);
    meta.setValue("cursorCol", col);
    meta.setValue("scrollPos", static_cast<int>(td.editor->SendScintilla(
        QsciScintillaBase::SCI_GETFIRSTVISIBLELINE)));
}

void MainWindow::cacheAllTabs()
{
    // Clear old cache files first
    QDir cache(cacheDir());
    for (const auto& f : cache.entryList({"tab_*.txt", "tab_*.meta"}, QDir::Files))
        cache.remove(f);

    for (int i = 0; i < m_tabs.size(); ++i)
        cacheTabContent(i);
}

void MainWindow::clearTabCache(int index)
{
    QString cache = cacheDir();
    QFile::remove(QString("%1/tab_%2.txt").arg(cache).arg(index));
    QFile::remove(QString("%1/tab_%2.meta").arg(cache).arg(index));
}

void MainWindow::autoSaveTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    auto& td = m_tabs[index];

    // If file already has a path, save to it
    if (!td.filePath.isEmpty()) {
        QFile file(td.filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(encodeFromText(td.editor->text(), td.encoding));
            td.editor->setModified(false);
        }
        return;
    }

    // If default save dir is set, auto-assign a path and save
    if (!m_defaultSaveDir.isEmpty()) {
        QDir dir(m_defaultSaveDir);
        if (!dir.exists()) dir.mkpath(".");

        // Generate a filename from tab title
        QString title = m_tabBar->tabText(index).remove(QRegularExpression("^\\* "));
        if (title.isEmpty()) title = "untitled";

        // Sanitize filename
        title.replace(QRegularExpression("[<>:\"/\\\\|?*]"), "_");

        // Add .txt if no extension
        if (!title.contains('.'))
            title += ".txt";

        QString path = dir.absoluteFilePath(title);

        // Avoid overwriting existing files from other tabs
        if (QFile::exists(path)) {
            QFileInfo fi(path);
            QString base = fi.completeBaseName();
            QString ext = fi.suffix();
            int n = 1;
            while (QFile::exists(path)) {
                path = dir.absoluteFilePath(QString("%1_%2.%3").arg(base).arg(n).arg(ext));
                n++;
            }
        }

        QFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(encodeFromText(td.editor->text(), td.encoding));
            td.filePath = path;
            td.editor->setModified(false);
            m_tabBar->setTabText(index, QFileInfo(path).fileName());
            m_editorWidget->setLexerForFile(td.editor, path);
            addRecentFile(path);
        }
        return;
    }

    // No default dir set and no path — just cache the content (already handled by timer)
}

QString MainWindow::defaultSaveDir() const
{
    return m_defaultSaveDir;
}

void MainWindow::setDefaultSaveDir(const QString& dir)
{
    m_defaultSaveDir = dir;
    QSettings s("NeonNote", "NeonNote");
    s.setValue("settings/defaultSaveDir", dir);
}

void MainWindow::showSettings()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Settings");
    dlg.setFixedSize(500, 180);
    dlg.setStyleSheet(
        "QDialog { background: #1e1e2e; color: #cdd6f4; }"
        "QLabel { color: #cdd6f4; font-size: 13px; }"
        "QLineEdit { background: #313244; color: #cdd6f4; border: 1px solid #45475a;"
        "  border-radius: 4px; padding: 6px; font-size: 13px; }"
        "QPushButton { background: #45475a; color: #cdd6f4; border: none;"
        "  border-radius: 4px; padding: 6px 16px; font-size: 13px; }"
        "QPushButton:hover { background: #585b70; }"
        "QPushButton#clearBtn { background: #f38ba8; color: #1e1e2e; }"
        "QPushButton#clearBtn:hover { background: #eba0ac; }"
    );

    auto* layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 20, 20, 20);

    // Default save directory
    auto* dirLabel = new QLabel("Default Save Directory:");
    layout->addWidget(dirLabel);

    auto* dirRow = new QHBoxLayout();
    auto* dirEdit = new QLineEdit(m_defaultSaveDir);
    dirEdit->setPlaceholderText("Not set — will prompt on save");
    dirEdit->setReadOnly(true);
    dirRow->addWidget(dirEdit, 1);

    auto* browseBtn = new QPushButton("Browse...");
    dirRow->addWidget(browseBtn);

    auto* clearBtn = new QPushButton("Clear");
    clearBtn->setObjectName("clearBtn");
    dirRow->addWidget(clearBtn);
    layout->addLayout(dirRow);

    auto* hint = new QLabel("When set, unsaved tabs are auto-saved here without prompts.");
    hint->setStyleSheet("color: #a6adc8; font-size: 11px;");
    layout->addWidget(hint);

    layout->addStretch();

    // OK / Cancel
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    auto* okBtn = new QPushButton("OK");
    auto* cancelBtn = new QPushButton("Cancel");
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    layout->addLayout(btnRow);

    connect(browseBtn, &QPushButton::clicked, &dlg, [&]() {
        QString dir = QFileDialog::getExistingDirectory(&dlg, "Select Default Save Directory", m_defaultSaveDir);
        if (!dir.isEmpty())
            dirEdit->setText(dir);
    });

    connect(clearBtn, &QPushButton::clicked, &dlg, [&]() {
        dirEdit->clear();
    });

    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        setDefaultSaveDir(dirEdit->text());
    }
}

void MainWindow::saveSession()
{
    QSettings s("NeonNote", "NeonNote");
    s.setValue("session/tabCount", m_tabs.size());
    s.setValue("session/activeIndex", m_tabBar->currentIndex());
    s.setValue("session/windowGeometry", saveGeometry());

    // Cache all tab contents (including untitled)
    cacheAllTabs();

    // Also save file paths for quick reference
    QStringList paths;
    for (const auto& td : m_tabs)
        paths << td.filePath; // empty string for untitled tabs
    s.setValue("session/files", paths);
}

void MainWindow::restoreSession()
{
    QSettings s("NeonNote", "NeonNote");
    int tabCount = s.value("session/tabCount", 0).toInt();
    int activeIdx = s.value("session/activeIndex", 0).toInt();

    if (tabCount <= 0) {
        // Legacy restore: try old file paths list
        QStringList paths = s.value("session/files").toStringList();
        for (const QString& path : paths) {
            if (!path.isEmpty() && QFileInfo::exists(path))
                openFileByPath(path);
        }
        if (activeIdx >= 0 && activeIdx < m_tabBar->count())
            m_tabBar->setCurrentIndex(activeIdx);
        QByteArray geo = s.value("session/windowGeometry").toByteArray();
        if (!geo.isEmpty()) restoreGeometry(geo);
        return;
    }

    QString cache = cacheDir();

    for (int i = 0; i < tabCount; ++i) {
        QString cacheFile = QString("%1/tab_%2.txt").arg(cache).arg(i);
        QString metaFile = QString("%1/tab_%2.meta").arg(cache).arg(i);

        if (!QFile::exists(cacheFile)) continue;

        QSettings meta(metaFile, QSettings::IniFormat);
        QString filePath = meta.value("filePath").toString();
        bool wasModified = meta.value("modified", false).toBool();
        bool readOnly = meta.value("readOnly", false).toBool();
        FileEncoding enc = static_cast<FileEncoding>(meta.value("encoding", 0).toInt());
        int eolMode = meta.value("eolMode", 0).toInt();
        QString tabTitle = meta.value("tabTitle", QString("new %1").arg(i + 1)).toString();
        int curLine = meta.value("cursorLine", 0).toInt();
        int curCol = meta.value("cursorCol", 0).toInt();
        int scrollPos = meta.value("scrollPos", 0).toInt();

        // If file exists on disk and wasn't modified, reload from disk (fresher)
        if (!filePath.isEmpty() && QFileInfo::exists(filePath) && !wasModified) {
            openFileByPath(filePath);
            continue;
        }

        // Otherwise restore from cache
        QFile f(cacheFile);
        if (!f.open(QIODevice::ReadOnly)) continue;
        QByteArray data = f.readAll();
        f.close();

        auto* editor = m_editorWidget->createEditor();
        editor->setWhitespaceVisibility(m_showWhitespace
            ? QsciScintilla::WsVisible : QsciScintilla::WsInvisible);
        editor->setEolVisibility(m_showEol);
        connectEditorSignals(editor);

        editor->setEolMode(static_cast<QsciScintilla::EolMode>(eolMode));
        editor->setText(QString::fromUtf8(data));

        if (!filePath.isEmpty())
            m_editorWidget->setLexerForFile(editor, filePath);

        // Mark as modified if it was modified when cached
        editor->setModified(wasModified);
        editor->setReadOnly(readOnly);

        TabData td;
        td.editor = editor;
        td.filePath = filePath;
        td.modified = wasModified;
        td.readOnly = readOnly;
        td.encoding = enc;
        m_tabs.append(td);

        QString displayTitle = tabTitle;
        if (wasModified) displayTitle.prepend("* ");
        m_tabBar->addTab(displayTitle);

        // Restore cursor and scroll
        editor->setCursorPosition(curLine, curCol);
        editor->SendScintilla(QsciScintillaBase::SCI_SETFIRSTVISIBLELINE, scrollPos);
    }

    if (activeIdx >= 0 && activeIdx < m_tabBar->count())
        m_tabBar->setCurrentIndex(activeIdx);

    QByteArray geo = s.value("session/windowGeometry").toByteArray();
    if (!geo.isEmpty()) restoreGeometry(geo);
}

void MainWindow::addRecentFile(const QString& filePath)
{
    QSettings s("NeonNote", "NeonNote");
    QStringList recent = s.value("recentFiles").toStringList();
    recent.removeAll(filePath);
    recent.prepend(filePath);
    while (recent.size() > MAX_RECENT_FILES)
        recent.removeLast();
    s.setValue("recentFiles", recent);
}

QStringList MainWindow::loadRecentFiles()
{
    QSettings s("NeonNote", "NeonNote");
    return s.value("recentFiles").toStringList();
}

// ── Text Operations ──

void MainWindow::trimTrailingWhitespace()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;

    e->SendScintilla(QsciScintillaBase::SCI_BEGINUNDOACTION);
    for (int i = e->lines() - 1; i >= 0; --i) {
        int lineStart = e->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, i);
        int lineEnd = e->SendScintilla(QsciScintillaBase::SCI_GETLINEENDPOSITION, i);
        int pos = lineEnd;
        while (pos > lineStart) {
            char ch = static_cast<char>(e->SendScintilla(QsciScintillaBase::SCI_GETCHARAT, pos - 1));
            if (ch != ' ' && ch != '\t') break;
            --pos;
        }
        if (pos < lineEnd) {
            e->SendScintilla(QsciScintillaBase::SCI_SETTARGETSTART, pos);
            e->SendScintilla(QsciScintillaBase::SCI_SETTARGETEND, lineEnd);
            e->SendScintilla(QsciScintillaBase::SCI_REPLACETARGET, (uintptr_t)0, "");
        }
    }
    e->SendScintilla(QsciScintillaBase::SCI_ENDUNDOACTION);
}

void MainWindow::removeEmptyLines()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;

    e->SendScintilla(QsciScintillaBase::SCI_BEGINUNDOACTION);
    for (int i = e->lines() - 1; i >= 0; --i) {
        QString line = e->text(i).trimmed();
        if (line.isEmpty() && i < e->lines() - 1) {
            int start = e->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, i);
            int end = e->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, i + 1);
            e->SendScintilla(QsciScintillaBase::SCI_SETTARGETSTART, start);
            e->SendScintilla(QsciScintillaBase::SCI_SETTARGETEND, end);
            e->SendScintilla(QsciScintillaBase::SCI_REPLACETARGET, (uintptr_t)0, "");
        }
    }
    e->SendScintilla(QsciScintillaBase::SCI_ENDUNDOACTION);
}

void MainWindow::joinLines()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;

    int lineFrom, lineTo, colFrom, colTo;
    e->getSelection(&lineFrom, &colFrom, &lineTo, &colTo);
    if (lineFrom == lineTo) return;

    e->SendScintilla(QsciScintillaBase::SCI_BEGINUNDOACTION);
    for (int i = lineTo - 1; i >= lineFrom; --i) {
        int endPos = e->SendScintilla(QsciScintillaBase::SCI_GETLINEENDPOSITION, i);
        int nextStart = e->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, i + 1);
        // Find first non-whitespace on next line
        int nextEnd = e->SendScintilla(QsciScintillaBase::SCI_GETLINEENDPOSITION, i + 1);
        int ws = nextStart;
        while (ws < nextEnd) {
            char ch = static_cast<char>(e->SendScintilla(QsciScintillaBase::SCI_GETCHARAT, ws));
            if (ch != ' ' && ch != '\t') break;
            ++ws;
        }
        e->SendScintilla(QsciScintillaBase::SCI_SETTARGETSTART, endPos);
        e->SendScintilla(QsciScintillaBase::SCI_SETTARGETEND, ws);
        e->SendScintilla(QsciScintillaBase::SCI_REPLACETARGET, (uintptr_t)1, " ");
    }
    e->SendScintilla(QsciScintillaBase::SCI_ENDUNDOACTION);
}

void MainWindow::sortLines(bool ascending)
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;

    int lineFrom, lineTo, cf, ct;
    e->getSelection(&lineFrom, &cf, &lineTo, &ct);
    if (lineFrom == lineTo && cf == ct) {
        lineFrom = 0;
        lineTo = e->lines() - 1;
    }

    QStringList lines;
    for (int i = lineFrom; i <= lineTo; ++i)
        lines << e->text(i).trimmed();

    while (!lines.isEmpty() && lines.last().isEmpty())
        lines.removeLast();

    std::sort(lines.begin(), lines.end(), [ascending](const QString& a, const QString& b) {
        return ascending ? a.compare(b, Qt::CaseInsensitive) < 0
                         : a.compare(b, Qt::CaseInsensitive) > 0;
    });

    QString eol = "\r\n";
    if (e->eolMode() == QsciScintilla::EolUnix) eol = "\n";
    else if (e->eolMode() == QsciScintilla::EolMac) eol = "\r";

    QString result = lines.join(eol) + eol;

    e->SendScintilla(QsciScintillaBase::SCI_BEGINUNDOACTION);
    int startPos = e->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, lineFrom);
    int endPos = (lineTo + 1 < e->lines())
        ? e->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, lineTo + 1)
        : e->SendScintilla(QsciScintillaBase::SCI_GETLENGTH);

    e->SendScintilla(QsciScintillaBase::SCI_SETTARGETSTART, startPos);
    e->SendScintilla(QsciScintillaBase::SCI_SETTARGETEND, endPos);
    QByteArray utf8 = result.toUtf8();
    e->SendScintilla(QsciScintillaBase::SCI_REPLACETARGET, (uintptr_t)utf8.length(), utf8.constData());
    e->SendScintilla(QsciScintillaBase::SCI_ENDUNDOACTION);
}

void MainWindow::convertTabsToSpaces()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;
    int tabW = e->tabWidth();
    QString spaces(tabW, ' ');

    e->SendScintilla(QsciScintillaBase::SCI_BEGINUNDOACTION);
    QString text = e->text();
    text.replace('\t', spaces);
    e->setText(text);
    e->SendScintilla(QsciScintillaBase::SCI_ENDUNDOACTION);
    e->setIndentationsUseTabs(false);
}

void MainWindow::convertSpacesToTabs()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;
    int tabW = e->tabWidth();
    QString spaces(tabW, ' ');

    e->SendScintilla(QsciScintillaBase::SCI_BEGINUNDOACTION);
    // Only convert leading spaces
    for (int i = 0; i < e->lines(); ++i) {
        QString line = e->text(i);
        int spaceCount = 0;
        for (QChar ch : line) {
            if (ch == ' ') spaceCount++;
            else break;
        }
        if (spaceCount >= tabW) {
            int tabs = spaceCount / tabW;
            int remainder = spaceCount % tabW;
            QString newLeading = QString(tabs, '\t') + QString(remainder, ' ');
            int lineStart = e->SendScintilla(QsciScintillaBase::SCI_POSITIONFROMLINE, i);
            e->SendScintilla(QsciScintillaBase::SCI_SETTARGETSTART, lineStart);
            e->SendScintilla(QsciScintillaBase::SCI_SETTARGETEND, lineStart + spaceCount);
            QByteArray utf8 = newLeading.toUtf8();
            e->SendScintilla(QsciScintillaBase::SCI_REPLACETARGET, (uintptr_t)utf8.length(), utf8.constData());
        }
    }
    e->SendScintilla(QsciScintillaBase::SCI_ENDUNDOACTION);
    e->setIndentationsUseTabs(true);
}

// ── Line Bookmarks ──

void MainWindow::toggleBookmarkOnLine()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;

    int line, col;
    e->getCursorPosition(&line, &col);

    unsigned markers = e->SendScintilla(QsciScintillaBase::SCI_MARKERGET, line);
    if (markers & (1 << BOOKMARK_MARKER))
        e->markerDelete(line, BOOKMARK_MARKER);
    else
        e->markerAdd(line, BOOKMARK_MARKER);
}

void MainWindow::nextBookmark()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;

    int line, col;
    e->getCursorPosition(&line, &col);
    int next = e->SendScintilla(QsciScintillaBase::SCI_MARKERNEXT, line + 1, 1 << BOOKMARK_MARKER);
    if (next < 0)
        next = e->SendScintilla(QsciScintillaBase::SCI_MARKERNEXT, 0, 1 << BOOKMARK_MARKER);
    if (next >= 0)
        e->setCursorPosition(next, 0);
}

void MainWindow::prevBookmark()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;

    int line, col;
    e->getCursorPosition(&line, &col);
    int prev = e->SendScintilla(QsciScintillaBase::SCI_MARKERPREVIOUS, line - 1, 1 << BOOKMARK_MARKER);
    if (prev < 0)
        prev = e->SendScintilla(QsciScintillaBase::SCI_MARKERPREVIOUS, e->lines() - 1, 1 << BOOKMARK_MARKER);
    if (prev >= 0)
        e->setCursorPosition(prev, 0);
}

void MainWindow::clearAllBookmarks()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    m_tabs[idx].editor->markerDeleteAll(BOOKMARK_MARKER);
}

// ── File Operations ──

void MainWindow::reloadFromDisk()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    const QString& path = m_tabs[idx].filePath;
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return;

    auto* e = m_tabs[idx].editor;
    int line, col;
    e->getCursorPosition(&line, &col);

    e->setText(QString::fromUtf8(file.readAll()));
    e->setModified(false);

    // Restore cursor position
    if (line < e->lines())
        e->setCursorPosition(line, col);

    m_statusBar->setFileSize(QFileInfo(path).size());
}

void MainWindow::toggleReadOnly()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;

    m_tabs[idx].readOnly = !m_tabs[idx].readOnly;
    m_tabs[idx].editor->setReadOnly(m_tabs[idx].readOnly);
    updateStatusBar();
}

void MainWindow::reopenLastClosed()
{
    if (m_closedFiles.isEmpty()) return;
    QString path = m_closedFiles.takeLast();
    if (QFileInfo::exists(path))
        openFileByPath(path);
}

void MainWindow::closeAllTabs()
{
    while (m_tabs.size() > 1)
        closeTab(m_tabs.size() - 1);
    closeTab(0);
}

void MainWindow::closeAllButActive()
{
    // Close tabs above active first (indices don't shift), then below
    int active = m_tabBar->currentIndex();
    for (int i = m_tabs.size() - 1; i > active; --i)
        closeTab(i);
    for (int i = active - 1; i >= 0; --i)
        closeTab(i);
}

void MainWindow::showSummary()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;

    QString text = e->text();
    int lines = e->lines();
    int chars = text.length();
    int charsNoSpaces = 0;
    int words = 0;
    bool inWord = false;

    for (const QChar& ch : text) {
        if (!ch.isSpace()) {
            charsNoSpaces++;
            if (!inWord) { words++; inWord = true; }
        } else {
            inWord = false;
        }
    }

    QString fileName = m_tabs[idx].filePath.isEmpty() ? "Untitled" : QFileInfo(m_tabs[idx].filePath).fileName();
    QString eol = eolModeString(e);

    QMessageBox::information(this, "Document Summary",
        QString("<h3>%1</h3>"
                "<table>"
                "<tr><td><b>Lines:</b></td><td>%2</td></tr>"
                "<tr><td><b>Words:</b></td><td>%3</td></tr>"
                "<tr><td><b>Characters:</b></td><td>%4</td></tr>"
                "<tr><td><b>Characters (no spaces):</b></td><td>%5</td></tr>"
                "<tr><td><b>Line Ending:</b></td><td>%6</td></tr>"
                "<tr><td><b>Encoding:</b></td><td>%7</td></tr>"
                "</table>")
            .arg(fileName)
            .arg(lines).arg(words).arg(chars).arg(charsNoSpaces).arg(eol)
            .arg(encodingName(m_tabs[idx].encoding)));
}

void MainWindow::openContainingFolder()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size() || m_tabs[idx].filePath.isEmpty()) return;

    QString folder = QFileInfo(m_tabs[idx].filePath).absolutePath();
    // Use Explorer with /select to highlight the file
    QProcess::startDetached("explorer.exe", {"/select,", QDir::toNativeSeparators(m_tabs[idx].filePath)});
}

// ── EOL Helpers ──

QString MainWindow::eolModeString(QsciScintilla* editor) const
{
    switch (editor->eolMode()) {
    case QsciScintilla::EolWindows: return "CRLF";
    case QsciScintilla::EolUnix:    return "LF";
    case QsciScintilla::EolMac:     return "CR";
    }
    return "CRLF";
}

void MainWindow::setEolMode(QsciScintilla::EolMode mode)
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;
    e->setEolMode(mode);
    e->convertEols(mode);
    m_statusBar->setLineEnding(eolModeString(e));
}

// ── Auto-Close Brackets ──

void MainWindow::onCharAdded(int ch)
{
    if (!m_autoCloseBrackets) return;

    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;
    if (m_tabs[idx].readOnly) return;

    // Get char after cursor
    int pos = e->SendScintilla(QsciScintillaBase::SCI_GETCURRENTPOS);
    int nextCh = e->SendScintilla(QsciScintillaBase::SCI_GETCHARAT, pos);

    QString insert;
    switch (ch) {
    case '(':  insert = ")"; break;
    case '[':  insert = "]"; break;
    case '{':  insert = "}"; break;
    case '"':
        if (nextCh == '"') {
            // Skip over existing quote
            e->SendScintilla(QsciScintillaBase::SCI_DELETERANGE, pos - 1, 1);
            e->SendScintilla(QsciScintillaBase::SCI_GOTOPOS, pos);
            return;
        }
        insert = "\"";
        break;
    case '\'':
        if (nextCh == '\'') {
            e->SendScintilla(QsciScintillaBase::SCI_DELETERANGE, pos - 1, 1);
            e->SendScintilla(QsciScintillaBase::SCI_GOTOPOS, pos);
            return;
        }
        insert = "'";
        break;
    default: return;
    }

    // Don't auto-close if next char is alphanumeric
    if (nextCh && QChar(nextCh).isLetterOrNumber()) return;

    QByteArray utf8 = insert.toUtf8();
    e->SendScintilla(QsciScintillaBase::SCI_INSERTTEXT, pos, utf8.constData());
}

// ── Tab Management ──

int MainWindow::tabIndexForEditor(QsciScintilla* editor) const
{
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].editor == editor) return i;
    }
    return -1;
}

void MainWindow::connectEditorSignals(QsciScintilla* editor)
{
    editor->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(editor, &QWidget::customContextMenuRequested, this, &MainWindow::showEditorContextMenu);
    connect(editor, &QsciScintilla::cursorPositionChanged, this, &MainWindow::onCursorPositionChanged);
    connect(editor, &QsciScintilla::modificationChanged, this, [this, editor](bool m) {
        int idx = tabIndexForEditor(editor);
        if (idx >= 0) {
            m_tabs[idx].modified = m;
            QString title = tabTitle(idx);
            if (m) title.prepend("* ");
            m_tabBar->setTabText(idx, title);
            updateTitle();
        }
    });

    // Auto-close brackets
    connect(editor, &QsciScintillaBase::SCN_CHARADDED, this, &MainWindow::onCharAdded);

    // Setup bookmark marker appearance
    editor->markerDefine(QsciScintilla::Circle, BOOKMARK_MARKER);
    editor->setMarkerBackgroundColor(Theme::Blue, BOOKMARK_MARKER);
    editor->setMarkerForegroundColor(Theme::Crust, BOOKMARK_MARKER);
    editor->SendScintilla(QsciScintillaBase::SCI_SETMARGINWIDTHN, 1, 16);
    editor->SendScintilla(QsciScintillaBase::SCI_SETMARGINSENSITIVEN, 1, true);

    // Click margin to toggle bookmark
    connect(editor, &QsciScintilla::marginClicked, this, [this, editor](int margin, int line, Qt::KeyboardModifiers) {
        if (margin == 1) {
            unsigned markers = editor->SendScintilla(QsciScintillaBase::SCI_MARKERGET, line);
            if (markers & (1 << BOOKMARK_MARKER))
                editor->markerDelete(line, BOOKMARK_MARKER);
            else
                editor->markerAdd(line, BOOKMARK_MARKER);
        }
    });
}

void MainWindow::newTab()
{
    auto* editor = m_editorWidget->createEditor();
    editor->setWhitespaceVisibility(m_showWhitespace
        ? QsciScintilla::WsVisible : QsciScintilla::WsInvisible);
    editor->setEolVisibility(m_showEol);
    connectEditorSignals(editor);

    TabData td;
    td.editor = editor;
    m_tabs.append(td);

    m_untitledCount++;
    int idx = m_tabBar->addTab(QString("new %1").arg(m_untitledCount));
    m_tabBar->setCurrentIndex(idx);
}

void MainWindow::closeTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;

    // Prompt if modified (auto-save silently when default dir is set)
    if (m_tabs[index].modified) {
        if (!m_defaultSaveDir.isEmpty()) {
            autoSaveTab(index);
        } else {
            QString name = m_tabs[index].filePath.isEmpty()
                ? m_tabBar->tabText(index).remove(QRegularExpression("^\\* "))
                : QFileInfo(m_tabs[index].filePath).fileName();
            auto result = QMessageBox::question(this, "Unsaved Changes",
                QString("Save changes to \"%1\"?").arg(name),
                QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
            if (result == QMessageBox::Save) {
                m_tabBar->setCurrentIndex(index);
                saveFile();
                if (m_tabs[index].modified) return; // save was cancelled
            } else if (result == QMessageBox::Cancel) {
                return;
            }
        }
    }

    // Track closed file for reopen
    if (!m_tabs[index].filePath.isEmpty())
        m_closedFiles.append(m_tabs[index].filePath);

    auto* editor = m_tabs[index].editor;
    m_editorWidget->editorDeleted(editor);
    m_tabs.removeAt(index);
    m_tabBar->removeTab(index);
    delete editor;

    if (m_tabs.isEmpty())
        newTab();
}

void MainWindow::switchTab(int index)
{
    if (index < 0 || index >= m_tabs.size()) return;
    m_editorWidget->setCurrentEditor(m_tabs[index].editor);
    m_findBar->setEditor(m_tabs[index].editor);
    updateTitle();
    updateStatusBar();
}

void MainWindow::openFile()
{
    QStringList files = QFileDialog::getOpenFileNames(this, "Open File", QString(),
        "All Files (*);;Text Files (*.txt);;Source Files (*.cpp *.h *.py *.js *.ts *.rs *.go *.java *.cs)");
    for (const auto& f : files)
        openFileByPath(f);
}

void MainWindow::openFileByPath(const QString& filePath)
{
    // Switch to existing tab if file is already open
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath.compare(filePath, Qt::CaseInsensitive) == 0) {
            m_tabBar->setCurrentIndex(i);
            return;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return;

    QByteArray data = file.readAll();
    FileEncoding enc = detectEncoding(data);

    // Reuse current tab if it's an empty untitled tab
    int reuseIdx = -1;
    int cur = m_tabBar->currentIndex();
    if (cur >= 0 && cur < m_tabs.size()
        && m_tabs[cur].filePath.isEmpty()
        && !m_tabs[cur].modified
        && m_tabs[cur].editor->text().isEmpty()) {
        reuseIdx = cur;
    }

    QsciScintilla* editor;
    if (reuseIdx >= 0) {
        editor = m_tabs[reuseIdx].editor;
    } else {
        editor = m_editorWidget->createEditor();
        editor->setWhitespaceVisibility(m_showWhitespace
            ? QsciScintilla::WsVisible : QsciScintilla::WsInvisible);
        editor->setEolVisibility(m_showEol);
        connectEditorSignals(editor);
    }

    // Decode to UTF-8 for QScintilla (it always works in UTF-8 internally)
    QByteArray utf8Data = decodeToUtf8(data, enc);

    // Detect EOL from decoded data
    if (utf8Data.contains("\r\n"))
        editor->setEolMode(QsciScintilla::EolWindows);
    else if (utf8Data.contains('\n'))
        editor->setEolMode(QsciScintilla::EolUnix);
    else if (utf8Data.contains('\r'))
        editor->setEolMode(QsciScintilla::EolMac);

    editor->setText(QString::fromUtf8(utf8Data));
    editor->setModified(false);
    m_editorWidget->setLexerForFile(editor, filePath);

    QFileInfo fi(filePath);
    int idx;
    if (reuseIdx >= 0) {
        m_tabs[reuseIdx].filePath = filePath;
        m_tabs[reuseIdx].encoding = enc;
        m_tabBar->setTabText(reuseIdx, fi.fileName());
        idx = reuseIdx;
    } else {
        TabData td;
        td.editor = editor;
        td.filePath = filePath;
        td.encoding = enc;
        m_tabs.append(td);
        idx = m_tabBar->addTab(fi.fileName());
    }

    m_tabBar->setCurrentIndex(idx);
    updateTitle();
    updateStatusBar();
    m_statusBar->setFileSize(fi.size());
    addRecentFile(filePath);
}

void MainWindow::saveFile()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    if (m_tabs[idx].readOnly) return;

    if (m_tabs[idx].filePath.isEmpty()) {
        if (!m_defaultSaveDir.isEmpty()) {
            autoSaveTab(idx);
            return;
        }
        saveFileAs();
        return;
    }

    QFile file(m_tabs[idx].filePath);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(encodeFromText(m_tabs[idx].editor->text(), m_tabs[idx].encoding));
    m_tabs[idx].editor->setModified(false);
    m_statusBar->setFileSize(QFileInfo(m_tabs[idx].filePath).size());
}

void MainWindow::saveFileAs()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;

    QString path = QFileDialog::getSaveFileName(this, "Save As");
    if (path.isEmpty()) return;

    m_tabs[idx].filePath = path;
    m_tabs[idx].readOnly = false;
    m_tabs[idx].editor->setReadOnly(false);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;
    file.write(encodeFromText(m_tabs[idx].editor->text(), m_tabs[idx].encoding));
    m_tabs[idx].editor->setModified(false);

    QFileInfo fi(path);
    m_tabBar->setTabText(idx, fi.fileName());
    m_editorWidget->setLexerForFile(m_tabs[idx].editor, path);
    updateTitle();
    updateStatusBar();
    addRecentFile(path);
}

void MainWindow::onCursorPositionChanged(int line, int col)
{
    m_statusBar->setLineCol(line + 1, col + 1);
}

void MainWindow::onBookmarkClicked(int index)
{
    openFileByPath(m_bookmarkBar->bookmarkPath(index));
}

void MainWindow::updateTitle()
{
    int idx = m_tabBar->currentIndex();
    QString title = "NeonNote";
    if (idx >= 0 && idx < m_tabs.size()) {
        QString name = m_tabs[idx].filePath.isEmpty()
            ? tabTitle(idx) : QFileInfo(m_tabs[idx].filePath).fileName();
        if (m_tabs[idx].modified)
            name.prepend("* ");
        title = name + " - NeonNote";
    }
    setWindowTitle(title);
}

void MainWindow::updateStatusBar()
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;

    const auto& td = m_tabs[idx];

    m_statusBar->setLineEnding(eolModeString(td.editor));
    m_statusBar->setEncoding(encodingName(td.encoding));

    if (!td.filePath.isEmpty()) {
        QFileInfo fi(td.filePath);
        QString ext = fi.suffix().toLower();
        static const QMap<QString, QString> langMap = {
            {"cpp", "C++"}, {"c", "C"}, {"h", "C/C++ Header"}, {"hpp", "C++ Header"},
            {"py", "Python"}, {"js", "JavaScript"}, {"ts", "TypeScript"},
            {"html", "HTML"}, {"css", "CSS"}, {"json", "JSON"}, {"xml", "XML"},
            {"yaml", "YAML"}, {"yml", "YAML"}, {"md", "Markdown"},
            {"rs", "Rust"}, {"go", "Go"}, {"java", "Java"}, {"cs", "C#"},
            {"rb", "Ruby"}, {"lua", "Lua"}, {"sh", "Shell"}, {"bash", "Bash"},
            {"sql", "SQL"}, {"ps1", "PowerShell"}, {"bat", "Batch"},
            {"kt", "Kotlin"}, {"swift", "Swift"}, {"dart", "Dart"},
            {"php", "PHP"}, {"vue", "Vue"}, {"toml", "TOML"}, {"ini", "INI"},
        };
        m_statusBar->setLanguage(langMap.value(ext, "Plain Text"));
        m_statusBar->setFileSize(fi.size());
    } else {
        m_statusBar->setLanguage("Plain Text");
    }
}

QString MainWindow::tabTitle(int index) const
{
    if (index < 0 || index >= m_tabs.size()) return {};
    if (m_tabs[index].filePath.isEmpty())
        return m_tabBar->tabText(index).remove(QRegularExpression("^\\* "));
    return QFileInfo(m_tabs[index].filePath).fileName();
}

// ── Search Dialog ──

void MainWindow::showSearchDialog(int tab)
{
    if (!m_searchDialog) {
        m_searchDialog = new SearchDialog(this);
        m_searchDialog->setResultsPanel(m_searchResults);
        connect(m_searchDialog, &SearchDialog::openFileAtLine,
                this, &MainWindow::onSearchResultClicked);
    }

    int idx = m_tabBar->currentIndex();
    if (idx >= 0 && idx < m_tabs.size())
        m_searchDialog->setEditor(m_tabs[idx].editor);

    m_searchDialog->setOpenEditors(getOpenEditors());

    // Set directory from current file if available
    if (tab == 2 && idx >= 0 && idx < m_tabs.size() && !m_tabs[idx].filePath.isEmpty()) {
        // The dialog's Follow current doc checkbox handles this
    }

    m_searchDialog->showTab(tab);
}

void MainWindow::onSearchResultClicked(const QString& filePath, int line, int col)
{
    // Check if file is already open
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (m_tabs[i].filePath.compare(filePath, Qt::CaseInsensitive) == 0) {
            m_tabBar->setCurrentIndex(i);
            m_tabs[i].editor->setCursorPosition(line, col);
            m_tabs[i].editor->setFocus();
            return;
        }
    }
    // Open the file and go to line
    openFileByPath(filePath);
    int idx = m_tabBar->currentIndex();
    if (idx >= 0 && idx < m_tabs.size()) {
        m_tabs[idx].editor->setCursorPosition(line, col);
        m_tabs[idx].editor->setFocus();
    }
}

QVector<QPair<QString, QsciScintilla*>> MainWindow::getOpenEditors() const
{
    QVector<QPair<QString, QsciScintilla*>> result;
    for (const auto& td : m_tabs)
        result.append({td.filePath, td.editor});
    return result;
}

// ── Encoding ──

FileEncoding MainWindow::detectEncoding(const QByteArray& data) const
{
    // Check BOM first
    if (data.startsWith("\xEF\xBB\xBF"))
        return FileEncoding::UTF8_BOM;
    if (data.startsWith("\xFF\xFE"))
        return FileEncoding::UTF16_LE;
    if (data.startsWith("\xFE\xFF"))
        return FileEncoding::UTF16_BE;

    // Check if valid UTF-8
    bool validUtf8 = true;
    bool hasHighBytes = false;
    int i = 0;
    while (i < data.size() && i < 65536) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c < 0x80) {
            ++i;
            continue;
        }
        hasHighBytes = true;
        int expected = 0;
        if ((c & 0xE0) == 0xC0) expected = 1;
        else if ((c & 0xF0) == 0xE0) expected = 2;
        else if ((c & 0xF8) == 0xF0) expected = 3;
        else { validUtf8 = false; break; }

        for (int j = 0; j < expected; ++j) {
            ++i;
            if (i >= data.size() || (static_cast<unsigned char>(data[i]) & 0xC0) != 0x80) {
                validUtf8 = false;
                break;
            }
        }
        if (!validUtf8) break;
        ++i;
    }

    if (validUtf8)
        return FileEncoding::UTF8;

    // Default to Windows-1252 for non-UTF-8 files with high bytes
    if (hasHighBytes)
        return FileEncoding::Windows1252;

    return FileEncoding::UTF8;
}

QString MainWindow::encodingName(FileEncoding enc) const
{
    switch (enc) {
    case FileEncoding::UTF8:         return "UTF-8";
    case FileEncoding::UTF8_BOM:     return "UTF-8 BOM";
    case FileEncoding::UTF16_LE:     return "UTF-16 LE";
    case FileEncoding::UTF16_BE:     return "UTF-16 BE";
    case FileEncoding::Windows1252:  return "Windows-1252";
    case FileEncoding::ISO8859_1:    return "ISO 8859-1";
    case FileEncoding::Shift_JIS:    return "Shift-JIS";
    case FileEncoding::GB2312:       return "GB2312";
    case FileEncoding::EUC_KR:       return "EUC-KR";
    }
    return "UTF-8";
}

QByteArray MainWindow::decodeToUtf8(const QByteArray& data, FileEncoding enc) const
{
    switch (enc) {
    case FileEncoding::UTF8:
        return data;
    case FileEncoding::UTF8_BOM:
        return data.mid(3);
    case FileEncoding::UTF16_LE: {
        QString text = QString::fromUtf16(
            reinterpret_cast<const char16_t*>(data.constData() + 2),
            (data.size() - 2) / 2);
        return text.toUtf8();
    }
    case FileEncoding::UTF16_BE: {
        // Swap bytes first
        QByteArray swapped = data.mid(2);
        for (int i = 0; i + 1 < swapped.size(); i += 2)
            std::swap(swapped[i], swapped[i + 1]);
        QString text = QString::fromUtf16(
            reinterpret_cast<const char16_t*>(swapped.constData()),
            swapped.size() / 2);
        return text.toUtf8();
    }
    case FileEncoding::Windows1252:
    case FileEncoding::ISO8859_1: {
        // Latin-1 is a subset — decode each byte as Unicode code point
        QString text;
        text.reserve(data.size());
        for (char c : data)
            text += QChar(static_cast<unsigned char>(c));
        return text.toUtf8();
    }
    case FileEncoding::Shift_JIS:
    case FileEncoding::GB2312:
    case FileEncoding::EUC_KR: {
        // Use QStringDecoder if available, fallback to Latin-1
        const char* codecName = nullptr;
        if (enc == FileEncoding::Shift_JIS) codecName = "Shift-JIS";
        else if (enc == FileEncoding::GB2312) codecName = "GB2312";
        else if (enc == FileEncoding::EUC_KR) codecName = "EUC-KR";
        auto decoder = QStringDecoder(codecName);
        if (decoder.isValid()) {
            QString text = decoder(data);
            return text.toUtf8();
        }
        return data; // fallback
    }
    }
    return data;
}

QByteArray MainWindow::encodeFromText(const QString& text, FileEncoding enc) const
{
    switch (enc) {
    case FileEncoding::UTF8:
        return text.toUtf8();
    case FileEncoding::UTF8_BOM: {
        QByteArray bom("\xEF\xBB\xBF", 3);
        return bom + text.toUtf8();
    }
    case FileEncoding::UTF16_LE: {
        QByteArray bom("\xFF\xFE", 2);
        auto data = QByteArray(reinterpret_cast<const char*>(text.utf16()), text.size() * 2);
        return bom + data;
    }
    case FileEncoding::UTF16_BE: {
        QByteArray bom("\xFE\xFF", 2);
        QByteArray data(reinterpret_cast<const char*>(text.utf16()), text.size() * 2);
        // Swap bytes for big-endian
        for (int i = 0; i + 1 < data.size(); i += 2)
            std::swap(data[i], data[i + 1]);
        return bom + data;
    }
    case FileEncoding::Windows1252:
    case FileEncoding::ISO8859_1: {
        QByteArray result;
        result.reserve(text.size());
        for (const QChar& ch : text) {
            if (ch.unicode() < 256)
                result += static_cast<char>(ch.unicode());
            else
                result += '?';
        }
        return result;
    }
    case FileEncoding::Shift_JIS:
    case FileEncoding::GB2312:
    case FileEncoding::EUC_KR: {
        const char* codecName = nullptr;
        if (enc == FileEncoding::Shift_JIS) codecName = "Shift-JIS";
        else if (enc == FileEncoding::GB2312) codecName = "GB2312";
        else if (enc == FileEncoding::EUC_KR) codecName = "EUC-KR";
        auto encoder = QStringEncoder(codecName);
        if (encoder.isValid())
            return encoder(text);
        return text.toUtf8();
    }
    }
    return text.toUtf8();
}

void MainWindow::convertEncoding(FileEncoding newEnc)
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;

    m_tabs[idx].encoding = newEnc;
    m_statusBar->setEncoding(encodingName(newEnc));
    m_tabs[idx].editor->setModified(true);
}

// ── Context Menus ──

void MainWindow::showEditorContextMenu(const QPoint& pos)
{
    int idx = m_tabBar->currentIndex();
    if (idx < 0 || idx >= m_tabs.size()) return;
    auto* e = m_tabs[idx].editor;

    QMenu menu(this);
    menu.setStyleSheet(MENU_STYLE);

    menu.addAction("Undo", e, &QsciScintilla::undo)->setEnabled(e->isUndoAvailable());
    menu.addAction("Redo", e, &QsciScintilla::redo)->setEnabled(e->isRedoAvailable());
    menu.addSeparator();
    menu.addAction("Cut", e, &QsciScintilla::cut)->setEnabled(e->hasSelectedText());
    menu.addAction("Copy", e, &QsciScintilla::copy)->setEnabled(e->hasSelectedText());
    menu.addAction("Paste", e, &QsciScintilla::paste);
    menu.addAction("Delete", e, &QsciScintilla::removeSelectedText)->setEnabled(e->hasSelectedText());
    menu.addSeparator();
    menu.addAction("Select All", e, &QsciScintilla::selectAll);
    menu.addSeparator();

    if (e->hasSelectedText()) {
        menu.addAction("UPPERCASE", this, [e]() {
            e->SendScintilla(QsciScintillaBase::SCI_UPPERCASE);
        });
        menu.addAction("lowercase", this, [e]() {
            e->SendScintilla(QsciScintillaBase::SCI_LOWERCASE);
        });
        menu.addSeparator();
    }

    menu.addAction("Duplicate Line", this, [e]() {
        e->SendScintilla(QsciScintillaBase::SCI_LINEDUPLICATE);
    });
    menu.addAction("Delete Line", this, [e]() {
        e->SendScintilla(QsciScintillaBase::SCI_LINEDELETE);
    });
    menu.addAction("Toggle Bookmark", this, &MainWindow::toggleBookmarkOnLine);

    menu.addSeparator();
    auto* wrapAction = menu.addAction("Word Wrap");
    wrapAction->setCheckable(true);
    wrapAction->setChecked(e->wrapMode() != QsciScintilla::WrapNone);
    connect(wrapAction, &QAction::toggled, this, [e](bool wrap) {
        e->setWrapMode(wrap ? QsciScintilla::WrapWord : QsciScintilla::WrapNone);
    });

    menu.addAction("Go to Line...", this, [this, e]() {
        int line, col;
        e->getCursorPosition(&line, &col);
        bool ok;
        int target = QInputDialog::getInt(this, "Go to Line", "Line number:",
                                           line + 1, 1, e->lines(), 1, &ok);
        if (ok) e->setCursorPosition(target - 1, 0);
    });

    menu.addSeparator();
    menu.addAction("Zoom In", this, [e]() { e->zoomIn(); });
    menu.addAction("Zoom Out", this, [e]() { e->zoomOut(); });
    menu.addAction("Reset Zoom", this, [e]() { e->zoomTo(0); });

    menu.exec(e->mapToGlobal(pos));
}

void MainWindow::showTabContextMenu(const QPoint& pos)
{
    int tabIdx = m_tabBar->tabAt(pos);

    QMenu menu(this);
    menu.setStyleSheet(MENU_STYLE);

    if (tabIdx >= 0) {
        menu.addAction("Close", this, [this, tabIdx]() { closeTab(tabIdx); });
        menu.addAction("Close Other Tabs", this, [this, tabIdx]() {
            for (int i = m_tabs.size() - 1; i >= 0; --i) {
                if (i != tabIdx) closeTab(i);
            }
        });
        menu.addAction("Close Tabs to the Right", this, [this, tabIdx]() {
            for (int i = m_tabs.size() - 1; i > tabIdx; --i)
                closeTab(i);
        });
        menu.addSeparator();

        if (tabIdx < m_tabs.size() && !m_tabs[tabIdx].filePath.isEmpty()) {
            menu.addAction("Copy File Path", this, [this, tabIdx]() {
                QApplication::clipboard()->setText(m_tabs[tabIdx].filePath);
            });
            menu.addAction("Open Containing Folder", this, [this, tabIdx]() {
                QProcess::startDetached("explorer.exe",
                    {"/select,", QDir::toNativeSeparators(m_tabs[tabIdx].filePath)});
            });
            menu.addAction("Reload from Disk", this, [this, tabIdx]() {
                m_tabBar->setCurrentIndex(tabIdx);
                reloadFromDisk();
            });
            menu.addSeparator();
        }

        // Read-only toggle
        if (tabIdx < m_tabs.size()) {
            auto* roAction = menu.addAction("Read-Only");
            roAction->setCheckable(true);
            roAction->setChecked(m_tabs[tabIdx].readOnly);
            connect(roAction, &QAction::toggled, this, [this, tabIdx](bool ro) {
                m_tabs[tabIdx].readOnly = ro;
                m_tabs[tabIdx].editor->setReadOnly(ro);
            });
            menu.addSeparator();
        }
    }

    menu.addAction("New Tab", this, &MainWindow::newTab);
    if (!m_closedFiles.isEmpty())
        menu.addAction("Reopen Last Closed", this, &MainWindow::reopenLastClosed);

    menu.exec(m_tabBar->mapToGlobal(pos));
}
