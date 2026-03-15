#pragma once
#include <QColor>
#include <QString>
#include <QMap>

namespace Theme {

// Catppuccin Mocha palette
constexpr QColor Crust    {17,  17,  27};
constexpr QColor Mantle   {24,  24,  37};
constexpr QColor Base     {30,  30,  46};
constexpr QColor Surface0 {49,  50,  68};
constexpr QColor Surface1 {69,  71,  90};
constexpr QColor Surface2 {88,  91, 112};
constexpr QColor Overlay0 {108, 112, 134};
constexpr QColor Overlay1 {127, 132, 156};
constexpr QColor Subtext0 {166, 173, 200};
constexpr QColor Subtext1 {186, 194, 222};
constexpr QColor Text     {205, 214, 244};
constexpr QColor Blue     {137, 180, 250};
constexpr QColor Mauve    {203, 166, 247};
constexpr QColor Red      {243, 139, 168};
constexpr QColor Green    {166, 227, 161};
constexpr QColor Teal     {148, 226, 213};
constexpr QColor Peach    {250, 179, 135};
constexpr QColor Yellow   {249, 226, 175};
constexpr QColor Pink     {245, 194, 231};
constexpr QColor Sky      {137, 220, 235};
constexpr QColor Lavender {180, 190, 254};
constexpr QColor Flamingo {242, 205, 205};
constexpr QColor Rosewater{245, 224, 220};
constexpr QColor Sapphire {116, 199, 236};
constexpr QColor Maroon   {235, 160, 172};

// Language accent colors for tab strips
inline QColor accentForExtension(const QString& ext) {
    static const QMap<QString, QColor> map = {
        // Web
        {"html",  {227, 76, 38}},    {"htm",   {227, 76, 38}},
        {"css",   {86, 61, 124}},    {"scss",  {191, 64, 128}},
        {"js",    {247, 223, 30}},   {"jsx",   {97, 218, 251}},
        {"ts",    {49, 120, 198}},   {"tsx",   {49, 120, 198}},
        {"json",  {250, 179, 135}},  {"xml",   {148, 226, 213}},
        // Systems
        {"cpp",   {0, 89, 156}},     {"c",     {85, 85, 85}},
        {"h",     {0, 89, 156}},     {"hpp",   {0, 89, 156}},
        {"cs",    {104, 33, 122}},   {"java",  {176, 114, 25}},
        {"rs",    {222, 165, 132}},  {"go",    {0, 173, 216}},
        {"kt",   {169, 123, 255}},   {"swift", {255, 69, 0}},
        // Scripting
        {"py",    {53, 114, 165}},   {"rb",    {204, 52, 45}},
        {"lua",   {0, 0, 128}},      {"pl",    {57, 69, 126}},
        {"sh",    {166, 227, 161}},  {"bash",  {166, 227, 161}},
        {"ps1",   {1, 36, 86}},     {"bat",   {1, 36, 86}},
        // Data/Config
        {"yaml",  {203, 166, 247}},  {"yml",   {203, 166, 247}},
        {"toml",  {156, 66, 33}},    {"ini",   {108, 112, 134}},
        {"sql",   {226, 144, 0}},    {"md",    {8, 63, 161}},
        // Other
        {"php",   {119, 123, 180}},  {"vue",   {65, 184, 131}},
        {"svelte",{255, 62, 0}},     {"dart",  {0, 180, 216}},
        {"zig",   {247, 164, 29}},
    };
    return map.value(ext.toLower(), Blue);
}

// Global stylesheet for the application
inline QString globalStylesheet() {
    return QStringLiteral(R"(
        QMainWindow, QWidget {
            background-color: #1e1e2e;
            color: #cdd6f4;
        }
        QToolTip {
            background-color: #313244;
            color: #cdd6f4;
            border: 1px solid #45475a;
            padding: 4px;
        }
        QMenu {
            background-color: #1e1e2e;
            color: #cdd6f4;
            border: 1px solid #45475a;
        }
        QMenu::item:selected {
            background-color: #313244;
        }
        QMenu::separator {
            height: 1px;
            background: #45475a;
            margin: 4px 8px;
        }
        QScrollBar:vertical {
            background: #181825;
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: #45475a;
            min-height: 30px;
            border-radius: 5px;
        }
        QScrollBar::handle:vertical:hover {
            background: #585b70;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background: #181825;
            height: 10px;
            margin: 0;
        }
        QScrollBar::handle:horizontal {
            background: #45475a;
            min-width: 30px;
            border-radius: 5px;
        }
        QScrollBar::handle:horizontal:hover {
            background: #585b70;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
    )");
}

} // namespace Theme
