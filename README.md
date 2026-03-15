# NeonNote

A premium code editor built from scratch with **Qt 6** and **QScintilla**, featuring a **Catppuccin Mocha** theme and Chrome-style UI.

## Features

### Editor
- **30+ language lexers** with syntax highlighting and themed colors (C/C++, Python, JavaScript, TypeScript, HTML, CSS, JSON, YAML, SQL, Markdown, Lua, Ruby, Perl, Java, C#, Bash, Batch, CMake, Diff, and more)
- **Auto-close brackets** — `()`, `[]`, `{}`, `""`, `''`
- **Line bookmarks** — toggle, navigate, clear (Ctrl+F2, F2, Shift+F2)
- **Code folding** with boxed tree fold markers
- **Brace matching** with colored highlights
- **Zoom** — Ctrl++, Ctrl+-, Ctrl+0
- **Word wrap**, whitespace, and EOL visibility toggles
- **Browser-style middle-click auto-scroll** — distance-proportional speed with origin indicator overlay

### Search
- **Inline find/replace bar** (Ctrl+F, Ctrl+H)
- **Full search dialog** with 4 tabs — Find, Replace, Find in Files, Mark
- **Find in Files** (Ctrl+Shift+F) — recursive directory search with file filters, sub-folder and hidden folder toggles
- **Mark** — highlight all occurrences with optional line bookmarking
- **Search modes** — Normal, Extended (`\n`, `\r`, `\t` escapes), Regex
- **Results panel** — bottom tree view with clickable file/line navigation

### Tabs & Sessions
- **Chrome-style rounded tabs** with language-specific accent color strips
- **Tab dragging** to reorder, middle-click to close, Ctrl+1-9 switching
- **Session persistence** — all tabs (including untitled) cached and restored on restart
- **Tab caching** — content saved every 30 seconds, survives crashes
- **Reopen last closed** (Ctrl+Shift+T)
- **Recent files** tracking (max 15)

### Auto-Save
- **Default save directory** — set once in Settings, never see a save dialog again
- Untitled tabs auto-named and saved silently on close
- All modified tabs auto-saved on window close when default directory is set

### Encoding
- **Auto-detection** on file open — BOM detection, UTF-8 validation, Windows-1252 fallback
- **9 encodings** — UTF-8, UTF-8 BOM, UTF-16 LE/BE, Windows-1252, ISO 8859-1, Shift-JIS, GB2312, EUC-KR
- **Live conversion** via encoding menu with current encoding indicator
- Save preserves detected encoding

### UI
- **Frameless window** with DWM-extended client area and custom caption buttons
- **Catppuccin Mocha** color palette throughout
- **Hamburger menu** with File, Edit, View submenus (Segoe MDL2 Assets icons)
- **Bookmark bar** — pin frequently used files for one-click access
- **Pill-shaped status bar** — line/col, encoding, EOL mode, file size
- **Drag-and-drop** file opening
- **DPI-aware** — correct hit-testing and scaling at any display scale

### Text Operations
- Trim trailing whitespace
- Sort lines (ascending/descending)
- Remove empty lines
- Join lines
- Convert tabs to spaces / spaces to tabs
- EOL mode switching (Windows/Unix/Mac)

## Build

### Requirements
- **Qt 6.8+** (Widgets, PrintSupport)
- **CMake 3.20+**
- **MSVC** (Visual Studio 2022+)
- **C++20**

QScintilla 2.14.1 is included as a static library in `third_party/`.

### Compile
```bash
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=<path-to-qt> ..
cmake --build . --config Release
./Release/NeonNote.exe
```

## Architecture

```
src/
  main.cpp                  Entry point, dark palette setup
  MainWindow.h/cpp          Frameless window, menus, shortcuts, all features
  TabBar.h/cpp              Chrome-style rounded tabs with custom painting
  BookmarkBar.h/cpp         Browser-style pinned files bar
  StatusBar.h/cpp           Pill-shaped Catppuccin status bar
  EditorWidget.h/cpp        QScintilla editor instance management
  FindReplaceBar.h/cpp      Inline find/replace bar
  SearchDialog.h/cpp        Full search dialog (Find/Replace/Find in Files/Mark)
  SearchResultsPanel.h/cpp  Bottom panel with clickable search results
  AutoScrollHandler.h/cpp   Browser-style middle-click auto-scroll
  Theme.h                   Catppuccin Mocha color constants
```

## License

This project uses [QScintilla](https://www.riverbankcomputing.com/software/qscintilla/) (GPL v3) and [Scintilla](https://www.scintilla.org/) (License included in `third_party/`).
