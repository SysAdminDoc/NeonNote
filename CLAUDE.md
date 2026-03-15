# NeonNote v2 - CLAUDE.md

## Project Overview
Premium code editor built from scratch with Qt 6 + QScintilla. Catppuccin Mocha theme, Chrome-style UI.
**This is NOT the Notepad++ fork** (that's in `~/repos/notepad-plus-plus/`). This is a clean rewrite.

## Build
```bash
cd ~/repos/NeonNote/build
cmake -G "Visual Studio 18 2026" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/6.8.3/msvc2022_64 ..
cmake --build . --config Release
./Release/NeonNote.exe
```
- Qt 6.8.3 MSVC 2022 x64 at `C:/Qt/6.8.3/msvc2022_64`
- QScintilla 2.14.1 static lib in `third_party/qscintilla/`
- CMake + Visual Studio 18 2026 (MSVC v145)
- C++20, CMAKE_AUTOMOC ON

## Architecture

### Source Files (`src/`)
| File | Purpose |
|------|---------|
| `main.cpp` | Entry point, dark palette setup |
| `MainWindow.h/cpp` | Main window, frameless window handling, all menus, shortcuts, features |
| `TabBar.h/cpp` | Custom QTabBar with Chrome-style rounded tabs, custom painting |
| `BookmarkBar.h/cpp` | Browser-style bookmark/pinned files bar |
| `StatusBar.h/cpp` | Pill-shaped Catppuccin status bar segments |
| `EditorWidget.h/cpp` | QStackedWidget wrapper managing QScintilla editor instances |
| `FindReplaceBar.h/cpp` | Inline find/replace bar (Ctrl+F quick bar) |
| `SearchDialog.h/cpp` | Full Notepad++-style search dialog (Find/Replace/Find in Files/Mark tabs) |
| `SearchResultsPanel.h/cpp` | Bottom panel showing search results with clickable file/line entries |
| `AutoScrollHandler.h/cpp` | Browser-style middle-click auto-scroll with overlay indicator |
| `Theme.h` | Catppuccin Mocha color constants, language accent colors |

### Layout Hierarchy
```
MainWindow (frameless, DWM extended)
  centralWidget (QWidget)
    QVBoxLayout
      tabRow (QHBoxLayout): m_btnMenu | m_tabBar(stretch=1) | m_btnMin | m_btnMax | m_btnClose
      m_bookmarkBar
      m_findBar
      m_editorWidget (stretch=1)
      m_searchResults (SearchResultsPanel, hidden by default)
      m_statusBar
```

### Tab Data
```cpp
struct TabData {
    QsciScintilla* editor;
    QString filePath;
    bool modified;
    bool readOnly;
    FileEncoding encoding; // UTF8, UTF8_BOM, UTF16_LE/BE, Windows1252, ISO8859_1, Shift_JIS, GB2312, EUC_KR
};
QVector<TabData> m_tabs;  // indexed same as tab bar
```

## Frameless Window - CRITICAL KNOWLEDGE

### What works (DO NOT CHANGE):
- `WM_NCCALCSIZE`: Always return 0 when wParam==TRUE. No rcWork clamping, no insets.
- `WM_GETMINMAXINFO`: Constrains maximized size/position to monitor work area. This alone handles maximize.
- `DwmExtendFrameIntoClientArea` with margins {-1,-1,-1,-1}

### DPI Scaling - CRITICAL:
- `WM_NCHITTEST` lParam contains **physical screen pixels**
- Qt `mapFromGlobal()` expects **logical (DPI-scaled) pixels**
- **MUST use `QCursor::pos()`** for all Qt widget hit-testing (NOT raw lParam coords)
- **MUST scale constants by `devicePixelRatioF()`** when comparing against physical coords
  - Resize border: `int border = int(RESIZE_BORDER * devicePixelRatioF());`
  - Caption height: `int captionPhysical = int(CAPTION_BTN_H * devicePixelRatioF());`

### WM_NCHITTEST order (must stay in this order):
1. Resize borders (only when !isMaximized()) - physical coords
2. Title bar buttons (menu, min, max, close) -> HTCLIENT - QCursor::pos()
3. Tab bar: individual tabs -> HTCLIENT, new tab button -> HTCLIENT, empty space -> HTCAPTION
4. Fallback: y < captionPhysical -> HTCAPTION (catches area not covered by tab bar widget)

### WM_NCLBUTTONDOWN drag-to-restore:
- When maximized, clicking HTCAPTION and dragging should restore window and start move
- Use `static bool s_restoring` guard to prevent infinite recursion
- Flow: ShowWindow(SW_RESTORE) -> SetWindowPos (proportional reposition) -> ReleaseCapture() -> SendMessage(WM_NCLBUTTONDOWN, HTCAPTION)
- The re-sent WM_NCLBUTTONDOWN is safe because s_restoring blocks re-entry

### Caption buttons:
- **In the tab row layout** (not absolutely positioned). This ensures tabs never render behind them.
- Created in setupCaptionButtons(), added to tabRow layout after m_tabBar
- Use Win32 `ShowWindow(hwnd, SW_RESTORE/SW_MAXIMIZE)` for max/restore (Qt showNormal/showMaximized unreliable with frameless)

### Past mistakes to NEVER repeat:
1. **DO NOT modify WM_NCCALCSIZE** to inset/clamp rcWork - breaks maximize
2. **DO NOT use raw lParam coords for Qt mapFromGlobal** - DPI mismatch breaks button hit areas
3. **DO NOT absolutely position caption buttons** with move()/raise() - they end up behind centralWidget, tabs render over them
4. **DO NOT call SendMessage(WM_SYSCOMMAND, SC_MOVE) from nativeEvent** without recursion guard - causes stack overflow
5. **DO NOT add layout margins when maximized** - WM_GETMINMAXINFO already handles size constraint

## Features Implemented
- Chrome-style rounded tabs with language accent strips (30+ file types)
- Tab dragging (reorder), middle-click close, Ctrl+1-9 switching
- Empty tab bar space: drag to move window, double-click to maximize/restore
- Hamburger menu (Segoe MDL2 Assets icons) with full File/Edit/View submenus
- Find/Replace bar (Ctrl+F, Ctrl+H)
- Line bookmarks (Ctrl+F2 toggle, F2/Shift+F2 navigate, margin click, marker 24)
- Auto-close brackets: (), [], {}, "", ''
- Session save/restore (QSettings)
- Recent files tracking (max 15)
- Reopen last closed (Ctrl+Shift+T)
- Text operations: trim whitespace, sort lines, remove empty lines, join lines
- Tab/space conversion
- EOL mode switching (Windows/Unix/Mac)
- Word wrap, whitespace, EOL visibility toggles
- Reload from disk, read-only toggle
- Document summary dialog
- Open containing folder (Explorer /select)
- Zoom: Ctrl++, Ctrl+-, Ctrl+0
- Drag-and-drop file opening
- Browser-style middle-click auto-scroll (origin indicator, distance-proportional speed, dead zone)
- Catppuccin Mocha theme throughout

## Constants
```cpp
CAPTION_BTN_W = 46    // Caption button width
CAPTION_BTN_H = 34    // Caption button height (also tab row height)
RESIZE_BORDER = 5     // Resize grab area (logical pixels)
MAX_RECENT_FILES = 15
BOOKMARK_MARKER = 24  // Scintilla marker number for line bookmarks
```

## Audit Fixes Applied (2026-03-15)
1. **Unsaved changes prompt** on tab close + window close (Save/Discard/Cancel)
2. **closeAllButActive index shifting** fixed — closes above active first, then below
3. **saveFile double EOL conversion** fixed — removed QIODevice::Text flag (QScintilla already stores correct EOL)
4. **Modified indicator in title bar** — shows `* filename - NeonNote`
5. **modificationChanged updates title** — title bar reflects changes in real-time
6. **Reuse empty untitled tab** when opening a file (instead of leaving orphan "new 1" tabs)
7. **Dead code removed** in onCursorPositionChanged (selection info computed but unused)
8. **Caption button backgrounds** set to Crust (#11111b) to match tab bar area

## Search System
- **Inline find bar** (Ctrl+F/Ctrl+H): Quick find/replace in current document
- **Search Dialog** (Edit > Search Dialog): Full Notepad++-style dialog with 4 tabs:
  - **Find tab**: Find Next/Prev, Count, Find All in Current, Find All in All Open. Direction, search modes (Normal/Extended/Regex)
  - **Replace tab**: Replace, Replace All, Replace All in All Open
  - **Find in Files tab** (Ctrl+Shift+F): Directory search with file filters, sub-folder toggle, hidden folders, Follow current doc. Find All, Replace in Files
  - **Mark tab**: Highlight all occurrences with indicator 8 (INDIC_ROUNDBOX), optional bookmark lines, Purge option
- **Results Panel**: Bottom panel (SearchResultsPanel) with tree view. Double-click opens file at line.
- Search history stored in combo box dropdowns (max 20 entries per field)
- Extended mode expands \n, \r, \t, \0, \\ escape sequences

## Encoding System
- **Auto-detection on file open**: BOM detection (UTF-8 BOM, UTF-16 LE/BE), then UTF-8 validation, fallback to Windows-1252
- **Supported encodings**: UTF-8, UTF-8 BOM, UTF-16 LE, UTF-16 BE, Windows-1252, ISO 8859-1, Shift-JIS, GB2312, EUC-KR
- **Encoding menu**: Shows current encoding with checkmark, click to convert
- **Save preserves encoding**: Files saved in their detected/selected encoding
- QScintilla always works in UTF-8 internally — decode on load, encode on save
- Uses Qt6 QStringDecoder/QStringEncoder for CJK encodings

## Tab Cache & Auto-Save

- **Tab caching**: All tabs (including untitled) cached to `AppDataLocation/cache/` every 30 seconds via `QTimer`
- **Cache format**: `tab_N.txt` (content) + `tab_N.meta` (INI with filePath, modified, readOnly, encoding, eolMode, tabTitle, cursorLine, cursorCol, scrollPos)
- **Session restore**: Rebuilds all tabs from cache on startup, including untitled tabs with their content, cursor positions, and scroll positions
- **Default save directory**: When set via Settings dialog, all saves happen silently — no prompts ever:
  - `closeEvent()` auto-saves all modified tabs
  - `closeTab()` auto-saves the tab being closed
  - `saveFile()` auto-names untitled tabs and saves to the default directory
  - Filenames derived from tab title, sanitized, `.txt` extension added if none, collision-safe numbering
- **Settings dialog**: Menu > Settings — Catppuccin-themed dialog to pick/clear the default save directory

## Auto-Scroll (Middle-Click)

- **Activation**: Middle-click on editor viewport → shows origin overlay, starts scroll mode
- **Overlay**: Catppuccin-themed circle with up/down arrows and center dot, arrows highlight based on direction
- **Speed**: Distance-proportional with quadratic ramp — slow near origin, accelerates further out
- **Dead zone**: 12px around origin — small mouse movement doesn't scroll
- **Deactivation**: Middle-click again, any other click, scroll wheel, key press, or focus loss
- **Cursor**: `SizeAllCursor` in dead zone, `SizeVerCursor` when actively scrolling
- **Implementation**: `AutoScrollHandler` event filter installed on each editor's viewport (not the QsciScintilla widget — mouse events go to viewport in QAbstractScrollArea)
- **Tick rate**: 16ms (60fps) with fractional line accumulation via `SCI_LINESCROLL`

## Known Issues / TODO
- Drag-to-restore from maximized uses static bool guard + SendMessage recursion (works but fragile)
- No file change watcher (external modifications not detected)
