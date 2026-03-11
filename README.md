# NeonNote v1.0.0

A premium, visually enhanced fork of [Notepad++](https://github.com/notepad-plus-plus/notepad-plus-plus) with a Catppuccin Mocha dark theme, Chrome-style UI, and animated visual elements. Built for developers who want their text editor to look as good as their code.

![Windows](https://img.shields.io/badge/platform-Windows-blue)
![License](https://img.shields.io/badge/license-GPL--3.0-green)
![Version](https://img.shields.io/badge/version-1.0.0-purple)

## Features

### Chrome-Style Rounded Tabs

- Fully custom-drawn tabs with smooth rounded corners
- Active tab underglow effect with subtle gradient highlighting
- Language-colored accent strips at the top of each tab (30+ file type mappings)
- Per-tab color coding by file extension: `.py` = Blue, `.js` = Yellow, `.cpp` = Purple, `.html` = Orange, and more

### Bookmark Bar

- Browser-style bookmark bar for pinning frequently used files
- Folder support with dropdown menus ("Bookmark All Tabs" to capture your workspace)
- Drag-and-drop reorder with visual insertion indicator
- X close buttons with hover feedback for quick removal
- File-type colored icons matching the tab accent system
- Animated shimmer gradient background (Mauve > Blue > Teal cycling)
- Right-click context menu: rename, remove, hide/show bar
- XML persistence across sessions

### Modernized Status Bar

- Pill-shaped segments with Catppuccin accent colors:
  - **Doc Type** - Mauve pill
  - **EOF Format** - Green pill
  - **Encoding** - Blue pill
  - **Typing Mode** - Peach pill
- DPI-aware rounded badge rendering
- 1px accent line at top matching the tab system

### Dark Theme

- Full Catppuccin Mocha palette throughout
- Deep dark backgrounds (Crust, Mantle, Base, Surface0)
- Accent colors: Mauve, Blue, Teal, Green, Peach, Yellow
- Dark title bar via DWM integration
- All custom-drawn UI elements respect the dark theme

## Building

### Prerequisites

- Visual Studio 2022+ with C++ Desktop Development workload
- Windows 10/11 SDK

### Build Steps

```bash
# Clone the repository
git clone https://github.com/SysAdminDoc/NeonNote.git
cd NeonNote

# Build with MSBuild (Release x64)
MSBuild.exe PowerEditor/visual.net/notepadPlus.vcxproj -p:Configuration=Release -p:Platform=x64
```

The output binary `NeonNote.exe` will be in `PowerEditor\bin64\`.

## Screenshots

Screenshots coming soon.

## Credits

- Based on [Notepad++](https://notepad-plus-plus.org/) by Don HO
- Color palette: [Catppuccin Mocha](https://github.com/catppuccin/catppuccin)
- Tab and UI design inspired by Chromium browser

## License

NeonNote is released under the [GNU General Public License v3.0](LICENSE), the same license as Notepad++.
