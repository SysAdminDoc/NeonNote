#include "EditorWidget.h"
#include "AutoScrollHandler.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QFileInfo>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexercss.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexeryaml.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexerbatch.h>
#include <Qsci/qscilexersql.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexerlua.h>
#include <Qsci/qscilexerruby.h>
#include <Qsci/qscilexerperl.h>
#include <Qsci/qscilexerjava.h>
#include <Qsci/qscilexercsharp.h>
#include <Qsci/qscilexermakefile.h>
#include <Qsci/qscilexerproperties.h>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qscilexercmake.h>

EditorWidget::EditorWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_autoScroll = new AutoScrollHandler(this);
}

QsciScintilla* EditorWidget::createEditor()
{
    auto* editor = new QsciScintilla(this);
    applyTheme(editor);
    m_autoScroll->install(editor);
    return editor;
}

QsciScintilla* EditorWidget::currentEditor() const
{
    return m_current;
}

void EditorWidget::setCurrentEditor(QsciScintilla* editor)
{
    if (m_current == editor) return;

    // Hide old editor (only if it's still a valid child)
    if (m_current && m_current->parent() == this)
        m_current->hide();

    m_current = editor;

    if (m_current) {
        auto* lay = qobject_cast<QVBoxLayout*>(layout());
        if (lay->indexOf(m_current) < 0)
            lay->addWidget(m_current);
        m_current->show();
        m_current->setFocus();
    }
}

void EditorWidget::editorDeleted(QsciScintilla* editor)
{
    m_autoScroll->uninstall(editor);
    if (m_current == editor)
        m_current = nullptr;
}

void EditorWidget::applyTheme(QsciScintilla* editor)
{
    QFont font("Consolas", 11);
    editor->setFont(font);

    editor->setPaper(Theme::Base);
    editor->setColor(Theme::Text);
    editor->setCaretForegroundColor(Theme::Lavender);
    editor->setCaretLineVisible(true);
    editor->setCaretLineBackgroundColor(QColor(49, 50, 68, 80));
    editor->setSelectionBackgroundColor(Theme::Surface1);
    editor->setSelectionForegroundColor(Theme::Text);

    editor->setMarginsFont(font);
    editor->setMarginsForegroundColor(Theme::Overlay0);
    editor->setMarginsBackgroundColor(Theme::Mantle);
    editor->setMarginWidth(0, "00000");
    editor->setMarginLineNumbers(0, true);

    editor->setFolding(QsciScintilla::BoxedTreeFoldStyle, 2);
    editor->setFoldMarginColors(Theme::Mantle, Theme::Mantle);
    editor->SendScintilla(QsciScintilla::SCI_MARKERSETFORE, QsciScintilla::SC_MARKNUM_FOLDEROPEN, Theme::Overlay0.rgb() & 0xFFFFFF);
    editor->SendScintilla(QsciScintilla::SCI_MARKERSETBACK, QsciScintilla::SC_MARKNUM_FOLDEROPEN, Theme::Overlay0.rgb() & 0xFFFFFF);

    editor->setIndentationGuides(true);
    editor->setIndentationGuidesBackgroundColor(Theme::Base);
    editor->setIndentationGuidesForegroundColor(Theme::Surface1);

    editor->setBraceMatching(QsciScintilla::SloppyBraceMatch);
    editor->setMatchedBraceBackgroundColor(Theme::Surface0);
    editor->setMatchedBraceForegroundColor(Theme::Green);
    editor->setUnmatchedBraceBackgroundColor(Theme::Surface0);
    editor->setUnmatchedBraceForegroundColor(Theme::Red);

    editor->setEdgeMode(QsciScintilla::EdgeNone);

    editor->setWhitespaceSize(2);
    editor->setWhitespaceForegroundColor(Theme::Surface1);

    editor->setTabWidth(4);
    editor->setIndentationsUseTabs(false);
    editor->setAutoIndent(true);

    editor->SendScintilla(QsciScintilla::SCI_SETENDATLASTLINE, 0);
}

static void applyLexerBase(QsciLexer* lexer)
{
    QFont font("Consolas", 11);
    lexer->setFont(font);
    lexer->setDefaultPaper(Theme::Base);
    lexer->setDefaultColor(Theme::Text);
    for (int i = 0; i < 128; ++i)
        lexer->setPaper(Theme::Base, i);
}

static void styleCpp(QsciLexerCPP* lex)
{
    lex->setColor(Theme::Text,      QsciLexerCPP::Default);
    lex->setColor(Theme::Overlay1,  QsciLexerCPP::Comment);
    lex->setColor(Theme::Overlay1,  QsciLexerCPP::CommentLine);
    lex->setColor(Theme::Overlay1,  QsciLexerCPP::CommentDoc);
    lex->setColor(Theme::Overlay1,  QsciLexerCPP::CommentLineDoc);
    lex->setColor(Theme::Peach,     QsciLexerCPP::Number);
    lex->setColor(Theme::Mauve,     QsciLexerCPP::Keyword);
    lex->setColor(Theme::Green,     QsciLexerCPP::DoubleQuotedString);
    lex->setColor(Theme::Green,     QsciLexerCPP::SingleQuotedString);
    lex->setColor(Theme::Green,     QsciLexerCPP::RawString);
    lex->setColor(Theme::Green,     QsciLexerCPP::VerbatimString);
    lex->setColor(Theme::Peach,     QsciLexerCPP::UUID);
    lex->setColor(Theme::Pink,      QsciLexerCPP::PreProcessor);
    lex->setColor(Theme::Sky,       QsciLexerCPP::Operator);
    lex->setColor(Theme::Text,      QsciLexerCPP::Identifier);
    lex->setColor(Theme::Red,       QsciLexerCPP::UnclosedString);
    lex->setColor(Theme::Green,     QsciLexerCPP::Regex);
    lex->setColor(Theme::Yellow,    QsciLexerCPP::KeywordSet2);
    lex->setColor(Theme::Teal,      QsciLexerCPP::CommentDocKeyword);
    lex->setColor(Theme::Maroon,    QsciLexerCPP::CommentDocKeywordError);
    lex->setColor(Theme::Yellow,    QsciLexerCPP::GlobalClass);
    lex->setColor(Theme::Pink,      QsciLexerCPP::PreProcessorComment);
    lex->setColor(Theme::Pink,      QsciLexerCPP::PreProcessorCommentLineDoc);
    lex->setColor(Theme::Sapphire,  QsciLexerCPP::UserLiteral);
    lex->setColor(Theme::Flamingo,  QsciLexerCPP::TaskMarker);
    lex->setColor(Theme::Pink,      QsciLexerCPP::EscapeSequence);

    // Italic comments
    QFont f = lex->font(QsciLexerCPP::Comment);
    f.setItalic(true);
    lex->setFont(f, QsciLexerCPP::Comment);
    lex->setFont(f, QsciLexerCPP::CommentLine);
    lex->setFont(f, QsciLexerCPP::CommentDoc);
    lex->setFont(f, QsciLexerCPP::CommentLineDoc);

    // Bold keywords
    QFont kf = lex->font(QsciLexerCPP::Keyword);
    kf.setBold(true);
    lex->setFont(kf, QsciLexerCPP::Keyword);
}

static void stylePython(QsciLexerPython* lex)
{
    lex->setColor(Theme::Text,      QsciLexerPython::Default);
    lex->setColor(Theme::Overlay1,  QsciLexerPython::Comment);
    lex->setColor(Theme::Overlay1,  QsciLexerPython::CommentBlock);
    lex->setColor(Theme::Peach,     QsciLexerPython::Number);
    lex->setColor(Theme::Mauve,     QsciLexerPython::Keyword);
    lex->setColor(Theme::Green,     QsciLexerPython::DoubleQuotedString);
    lex->setColor(Theme::Green,     QsciLexerPython::SingleQuotedString);
    lex->setColor(Theme::Green,     QsciLexerPython::TripleSingleQuotedString);
    lex->setColor(Theme::Green,     QsciLexerPython::TripleDoubleQuotedString);
    lex->setColor(Theme::Green,     QsciLexerPython::DoubleQuotedFString);
    lex->setColor(Theme::Green,     QsciLexerPython::SingleQuotedFString);
    lex->setColor(Theme::Green,     QsciLexerPython::TripleSingleQuotedFString);
    lex->setColor(Theme::Green,     QsciLexerPython::TripleDoubleQuotedFString);
    lex->setColor(Theme::Yellow,    QsciLexerPython::ClassName);
    lex->setColor(Theme::Blue,      QsciLexerPython::FunctionMethodName);
    lex->setColor(Theme::Sky,       QsciLexerPython::Operator);
    lex->setColor(Theme::Text,      QsciLexerPython::Identifier);
    lex->setColor(Theme::Red,       QsciLexerPython::UnclosedString);
    lex->setColor(Theme::Yellow,    QsciLexerPython::HighlightedIdentifier);
    lex->setColor(Theme::Peach,     QsciLexerPython::Decorator);

    QFont f = lex->font(QsciLexerPython::Comment);
    f.setItalic(true);
    lex->setFont(f, QsciLexerPython::Comment);
    lex->setFont(f, QsciLexerPython::CommentBlock);

    QFont kf = lex->font(QsciLexerPython::Keyword);
    kf.setBold(true);
    lex->setFont(kf, QsciLexerPython::Keyword);
}

static void styleJavaScript(QsciLexerJavaScript* lex)
{
    // JS inherits CPP lexer styles
    lex->setColor(Theme::Text,      QsciLexerCPP::Default);
    lex->setColor(Theme::Overlay1,  QsciLexerCPP::Comment);
    lex->setColor(Theme::Overlay1,  QsciLexerCPP::CommentLine);
    lex->setColor(Theme::Overlay1,  QsciLexerCPP::CommentDoc);
    lex->setColor(Theme::Overlay1,  QsciLexerCPP::CommentLineDoc);
    lex->setColor(Theme::Peach,     QsciLexerCPP::Number);
    lex->setColor(Theme::Mauve,     QsciLexerCPP::Keyword);
    lex->setColor(Theme::Green,     QsciLexerCPP::DoubleQuotedString);
    lex->setColor(Theme::Green,     QsciLexerCPP::SingleQuotedString);
    lex->setColor(Theme::Green,     QsciLexerCPP::RawString);
    lex->setColor(Theme::Sky,       QsciLexerCPP::Operator);
    lex->setColor(Theme::Text,      QsciLexerCPP::Identifier);
    lex->setColor(Theme::Red,       QsciLexerCPP::UnclosedString);
    lex->setColor(Theme::Teal,      QsciLexerCPP::Regex);
    lex->setColor(Theme::Yellow,    QsciLexerCPP::KeywordSet2);
    lex->setColor(Theme::Yellow,    QsciLexerCPP::GlobalClass);
    lex->setColor(Theme::Pink,      QsciLexerCPP::EscapeSequence);

    QFont f = lex->font(QsciLexerCPP::Comment);
    f.setItalic(true);
    lex->setFont(f, QsciLexerCPP::Comment);
    lex->setFont(f, QsciLexerCPP::CommentLine);
    lex->setFont(f, QsciLexerCPP::CommentDoc);

    QFont kf = lex->font(QsciLexerCPP::Keyword);
    kf.setBold(true);
    lex->setFont(kf, QsciLexerCPP::Keyword);
}

static void styleBash(QsciLexerBash* lex)
{
    lex->setColor(Theme::Text,      QsciLexerBash::Default);
    lex->setColor(Theme::Red,       QsciLexerBash::Error);
    lex->setColor(Theme::Overlay1,  QsciLexerBash::Comment);
    lex->setColor(Theme::Peach,     QsciLexerBash::Number);
    lex->setColor(Theme::Mauve,     QsciLexerBash::Keyword);
    lex->setColor(Theme::Green,     QsciLexerBash::DoubleQuotedString);
    lex->setColor(Theme::Green,     QsciLexerBash::SingleQuotedString);
    lex->setColor(Theme::Sky,       QsciLexerBash::Operator);
    lex->setColor(Theme::Text,      QsciLexerBash::Identifier);
    lex->setColor(Theme::Flamingo,  QsciLexerBash::Scalar);
    lex->setColor(Theme::Yellow,    QsciLexerBash::ParameterExpansion);
    lex->setColor(Theme::Teal,      QsciLexerBash::Backticks);
    lex->setColor(Theme::Pink,      QsciLexerBash::HereDocumentDelimiter);
    lex->setColor(Theme::Green,     QsciLexerBash::SingleQuotedHereDocument);

    QFont f = lex->font(QsciLexerBash::Comment);
    f.setItalic(true);
    lex->setFont(f, QsciLexerBash::Comment);
}

static void styleHTML(QsciLexerHTML* lex)
{
    // HTML has many styles (0-127+), apply key ones
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Red,       1);   // Tag
    lex->setColor(Theme::Red,       2);   // Unknown tag
    lex->setColor(Theme::Yellow,    3);   // Attribute
    lex->setColor(Theme::Yellow,    4);   // Unknown attribute
    lex->setColor(Theme::Peach,     5);   // Number
    lex->setColor(Theme::Green,     6);   // Double-quoted string
    lex->setColor(Theme::Green,     7);   // Single-quoted string
    lex->setColor(Theme::Overlay1,  9);   // Comment
    lex->setColor(Theme::Mauve,     10);  // Entity
    lex->setColor(Theme::Sky,       11);  // Tag end
    lex->setColor(Theme::Teal,      12);  // XML start
    lex->setColor(Theme::Teal,      13);  // XML end
    lex->setColor(Theme::Pink,      14);  // Script
    lex->setColor(Theme::Mauve,     15);  // ASP (like <%)
    lex->setColor(Theme::Mauve,     16);  // ASP at
    lex->setColor(Theme::Sapphire,  17);  // CDATA
    lex->setColor(Theme::Pink,      18);  // PHP start
    lex->setColor(Theme::Red,       21);  // Unquoted value
    lex->setColor(Theme::Overlay1,  22);  // SGML comment

    QFont f = lex->font(9);
    f.setItalic(true);
    lex->setFont(f, 9);
}

static void styleCSS(QsciLexerCSS* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Red,       1);   // Tag
    lex->setColor(Theme::Yellow,    2);   // Class selector
    lex->setColor(Theme::Mauve,     3);   // Pseudo class
    lex->setColor(Theme::Red,       4);   // Unknown pseudo class
    lex->setColor(Theme::Sky,       5);   // Operator
    lex->setColor(Theme::Blue,      6);   // CSS1 property
    lex->setColor(Theme::Blue,      7);   // Unknown property
    lex->setColor(Theme::Green,     8);   // Value
    lex->setColor(Theme::Overlay1,  9);   // Comment
    lex->setColor(Theme::Teal,      10);  // ID selector
    lex->setColor(Theme::Green,     11);  // Important
    lex->setColor(Theme::Mauve,     12);  // At-rule
    lex->setColor(Theme::Green,     13);  // Double-quoted string
    lex->setColor(Theme::Green,     14);  // Single-quoted string
    lex->setColor(Theme::Blue,      15);  // CSS2 property
    lex->setColor(Theme::Peach,     16);  // Attribute
    lex->setColor(Theme::Blue,      17);  // CSS3 property
    lex->setColor(Theme::Mauve,     18);  // Pseudo element
    lex->setColor(Theme::Flamingo,  19);  // Extended CSS property
    lex->setColor(Theme::Sapphire,  20);  // Extended pseudo class
    lex->setColor(Theme::Sapphire,  21);  // Extended pseudo element
    lex->setColor(Theme::Mauve,     22);  // Media rule
    lex->setColor(Theme::Flamingo,  23);  // Variable

    QFont f = lex->font(9);
    f.setItalic(true);
    lex->setFont(f, 9);
}

static void styleJSON(QsciLexerJSON* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Peach,     1);   // Number
    lex->setColor(Theme::Green,     2);   // String
    lex->setColor(Theme::Red,       3);   // Unclosed string
    lex->setColor(Theme::Blue,      4);   // Property
    lex->setColor(Theme::Pink,      5);   // Escape sequence
    lex->setColor(Theme::Overlay1,  6);   // Line comment
    lex->setColor(Theme::Overlay1,  7);   // Block comment
    lex->setColor(Theme::Sky,       8);   // Operator
    lex->setColor(Theme::Red,       9);   // IRI
    lex->setColor(Theme::Red,       10);  // IRI compact
    lex->setColor(Theme::Mauve,     11);  // Keyword (true/false/null)
    lex->setColor(Theme::Mauve,     12);  // Keyword LD
    lex->setColor(Theme::Red,       13);  // Error

    QFont kf = lex->font(11);
    kf.setBold(true);
    lex->setFont(kf, 11);
}

static void styleYAML(QsciLexerYAML* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Overlay1,  1);   // Comment
    lex->setColor(Theme::Blue,      2);   // Identifier/key
    lex->setColor(Theme::Mauve,     3);   // Keyword
    lex->setColor(Theme::Peach,     4);   // Number
    lex->setColor(Theme::Yellow,    5);   // Reference
    lex->setColor(Theme::Teal,      6);   // Document delimiter
    lex->setColor(Theme::Green,     7);   // Text block
    lex->setColor(Theme::Red,       8);   // Syntax error marker
    lex->setColor(Theme::Sky,       9);   // Operator

    QFont f = lex->font(1);
    f.setItalic(true);
    lex->setFont(f, 1);
}

static void styleSQL(QsciLexerSQL* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Overlay1,  1);   // Comment
    lex->setColor(Theme::Overlay1,  2);   // Comment line
    lex->setColor(Theme::Overlay1,  3);   // Comment doc
    lex->setColor(Theme::Peach,     4);   // Number
    lex->setColor(Theme::Mauve,     5);   // Keyword
    lex->setColor(Theme::Green,     6);   // Double-quoted string
    lex->setColor(Theme::Green,     7);   // Single-quoted string
    lex->setColor(Theme::Yellow,    8);   // SQL*Plus keyword
    lex->setColor(Theme::Yellow,    9);   // SQL*Plus prompt
    lex->setColor(Theme::Sky,       10);  // Operator
    lex->setColor(Theme::Text,      11);  // Identifier
    lex->setColor(Theme::Yellow,    12);  // SQL*Plus comment
    lex->setColor(Theme::Overlay1,  13);  // # comment
    lex->setColor(Theme::Sapphire,  16);  // Keyword set 5
    lex->setColor(Theme::Teal,      17);  // Comment doc keyword
    lex->setColor(Theme::Red,       18);  // Comment doc keyword error
    lex->setColor(Theme::Blue,      19);  // Keyword set 8 (functions)

    QFont f = lex->font(1);
    f.setItalic(true);
    lex->setFont(f, 1);
    lex->setFont(f, 2);
    lex->setFont(f, 3);

    QFont kf = lex->font(5);
    kf.setBold(true);
    lex->setFont(kf, 5);
}

static void styleMarkdown(QsciLexerMarkdown* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Blue,      1);   // Special (line beginning)
    lex->setColor(Theme::Mauve,     2);   // Strong emphasis **
    lex->setColor(Theme::Pink,      3);   // Strong emphasis 2 __
    lex->setColor(Theme::Teal,      4);   // Emphasis *
    lex->setColor(Theme::Teal,      5);   // Emphasis 2 _
    lex->setColor(Theme::Blue,      6);   // H1
    lex->setColor(Theme::Blue,      7);   // H2
    lex->setColor(Theme::Sapphire,  8);   // H3
    lex->setColor(Theme::Sapphire,  9);   // H4
    lex->setColor(Theme::Lavender,  10);  // H5
    lex->setColor(Theme::Lavender,  11);  // H6
    lex->setColor(Theme::Green,     12);  // Pre char
    lex->setColor(Theme::Yellow,    13);  // Unordered list
    lex->setColor(Theme::Peach,     14);  // Ordered list
    lex->setColor(Theme::Surface1,  15);  // Blockquote
    lex->setColor(Theme::Red,       16);  // Strikeout
    lex->setColor(Theme::Surface1,  17);  // Horizontal rule
    lex->setColor(Theme::Sapphire,  18);  // Link
    lex->setColor(Theme::Green,     19);  // Code `` or ```
    lex->setColor(Theme::Green,     20);  // Code 2
    lex->setColor(Theme::Green,     21);  // Code block

    // Bold for headings
    for (int i = 6; i <= 11; ++i) {
        QFont f = lex->font(i);
        f.setBold(true);
        lex->setFont(f, i);
    }
}

static void styleLua(QsciLexerLua* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Overlay1,  1);   // Comment
    lex->setColor(Theme::Overlay1,  2);   // Line comment
    lex->setColor(Theme::Overlay1,  3);   // Comment doc
    lex->setColor(Theme::Peach,     4);   // Number
    lex->setColor(Theme::Mauve,     5);   // Keyword
    lex->setColor(Theme::Green,     6);   // String
    lex->setColor(Theme::Green,     7);   // Character
    lex->setColor(Theme::Green,     8);   // Literal string
    lex->setColor(Theme::Sky,       10);  // Operator
    lex->setColor(Theme::Text,      11);  // Identifier
    lex->setColor(Theme::Red,       12);  // Unclosed string
    lex->setColor(Theme::Yellow,    13);  // Basic functions
    lex->setColor(Theme::Teal,      14);  // String/math/table functions
    lex->setColor(Theme::Sapphire,  15);  // Coroutine/io/os functions
    lex->setColor(Theme::Pink,      16);  // Keyword set 5
    lex->setColor(Theme::Flamingo,  17);  // Keyword set 6
    lex->setColor(Theme::Blue,      18);  // Keyword set 7
    lex->setColor(Theme::Lavender,  19);  // Keyword set 8
    lex->setColor(Theme::Mauve,     20);  // Label

    QFont f = lex->font(1);
    f.setItalic(true);
    lex->setFont(f, 1);
    lex->setFont(f, 2);
}

static void styleRuby(QsciLexerRuby* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Red,       1);   // Error
    lex->setColor(Theme::Overlay1,  2);   // Comment
    lex->setColor(Theme::Overlay1,  3);   // POD
    lex->setColor(Theme::Peach,     4);   // Number
    lex->setColor(Theme::Mauve,     5);   // Keyword
    lex->setColor(Theme::Green,     6);   // String
    lex->setColor(Theme::Green,     7);   // Character
    lex->setColor(Theme::Yellow,    8);   // Class name
    lex->setColor(Theme::Blue,      9);   // Function name
    lex->setColor(Theme::Sky,       10);  // Operator
    lex->setColor(Theme::Text,      11);  // Identifier
    lex->setColor(Theme::Teal,      12);  // Regex
    lex->setColor(Theme::Flamingo,  13);  // Global
    lex->setColor(Theme::Pink,      14);  // Symbol
    lex->setColor(Theme::Blue,      15);  // Module name
    lex->setColor(Theme::Sapphire,  16);  // Instance variable
    lex->setColor(Theme::Lavender,  17);  // Class variable
    lex->setColor(Theme::Green,     18);  // Backticks
    lex->setColor(Theme::Peach,     24);  // Percent string q
    lex->setColor(Theme::Green,     25);  // Percent string Q
    lex->setColor(Theme::Green,     26);  // Percent string x
    lex->setColor(Theme::Teal,      27);  // Percent string r
    lex->setColor(Theme::Pink,      28);  // Percent string w

    QFont f = lex->font(2);
    f.setItalic(true);
    lex->setFont(f, 2);
}

static void styleDiff(QsciLexerDiff* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Overlay1,  1);   // Comment
    lex->setColor(Theme::Blue,      2);   // Command
    lex->setColor(Theme::Mauve,     3);   // Header
    lex->setColor(Theme::Teal,      4);   // Position
    lex->setColor(Theme::Red,       5);   // Removed
    lex->setColor(Theme::Green,     6);   // Added
    lex->setColor(Theme::Yellow,    7);   // Changed
}

static void styleCMake(QsciLexerCMake* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Overlay1,  1);   // Comment
    lex->setColor(Theme::Green,     2);   // String
    lex->setColor(Theme::Green,     3);   // String left-quote
    lex->setColor(Theme::Green,     4);   // String right-quote
    lex->setColor(Theme::Blue,      5);   // Function
    lex->setColor(Theme::Yellow,    6);   // Variable
    lex->setColor(Theme::Green,     7);   // String variable
    lex->setColor(Theme::Peach,     8);   // Number
    lex->setColor(Theme::Mauve,     9);   // Keyword set 3 (IF/ELSE/ENDIF)
    lex->setColor(Theme::Teal,      10);  // Keyword set WHILE/ENDWHILE
    lex->setColor(Theme::Pink,      11);  // Keyword set FOREACH
    lex->setColor(Theme::Red,       12);  // Keyword set deprecated
    lex->setColor(Theme::Sapphire,  13);  // Block macro/function
    lex->setColor(Theme::Green,     14);  // String variable

    QFont f = lex->font(1);
    f.setItalic(true);
    lex->setFont(f, 1);
}

static void styleProperties(QsciLexerProperties* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Overlay1,  1);   // Comment
    lex->setColor(Theme::Blue,      2);   // Section
    lex->setColor(Theme::Teal,      3);   // Assignment
    lex->setColor(Theme::Yellow,    4);   // Default value
    lex->setColor(Theme::Blue,      5);   // Key
}

static void styleBatch(QsciLexerBatch* lex)
{
    lex->setColor(Theme::Text,      0);   // Default
    lex->setColor(Theme::Overlay1,  1);   // Comment
    lex->setColor(Theme::Mauve,     2);   // Keyword
    lex->setColor(Theme::Green,     3);   // Label
    lex->setColor(Theme::Pink,      4);   // Hide command
    lex->setColor(Theme::Blue,      5);   // Command
    lex->setColor(Theme::Yellow,    6);   // Variable
    lex->setColor(Theme::Sky,       7);   // Operator
}

void EditorWidget::setLexerForFile(QsciScintilla* editor, const QString& filePath)
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    QString name = fi.fileName().toLower();

    QsciLexer* lexer = nullptr;

    if (ext == "cpp" || ext == "c" || ext == "h" || ext == "hpp" || ext == "cc" || ext == "cxx") {
        auto* lex = new QsciLexerCPP(editor);
        applyLexerBase(lex);
        styleCpp(lex);
        lexer = lex;
    } else if (ext == "py") {
        auto* lex = new QsciLexerPython(editor);
        applyLexerBase(lex);
        stylePython(lex);
        lexer = lex;
    } else if (ext == "js" || ext == "jsx" || ext == "mjs" || ext == "ts" || ext == "tsx") {
        auto* lex = new QsciLexerJavaScript(editor);
        applyLexerBase(lex);
        styleJavaScript(lex);
        lexer = lex;
    } else if (ext == "html" || ext == "htm" || ext == "vue" || ext == "svelte") {
        auto* lex = new QsciLexerHTML(editor);
        applyLexerBase(lex);
        styleHTML(lex);
        lexer = lex;
    } else if (ext == "css" || ext == "scss") {
        auto* lex = new QsciLexerCSS(editor);
        applyLexerBase(lex);
        styleCSS(lex);
        lexer = lex;
    } else if (ext == "json") {
        auto* lex = new QsciLexerJSON(editor);
        applyLexerBase(lex);
        styleJSON(lex);
        lexer = lex;
    } else if (ext == "yaml" || ext == "yml") {
        auto* lex = new QsciLexerYAML(editor);
        applyLexerBase(lex);
        styleYAML(lex);
        lexer = lex;
    } else if (ext == "xml" || ext == "svg" || ext == "xsl") {
        auto* lex = new QsciLexerXML(editor);
        applyLexerBase(lex);
        styleHTML(lex);  // XML shares HTML styles
        lexer = lex;
    } else if (ext == "sh" || ext == "bash" || ext == "zsh") {
        auto* lex = new QsciLexerBash(editor);
        applyLexerBase(lex);
        styleBash(lex);
        lexer = lex;
    } else if (ext == "bat" || ext == "cmd") {
        auto* lex = new QsciLexerBatch(editor);
        applyLexerBase(lex);
        styleBatch(lex);
        lexer = lex;
    } else if (ext == "sql") {
        auto* lex = new QsciLexerSQL(editor);
        applyLexerBase(lex);
        styleSQL(lex);
        lexer = lex;
    } else if (ext == "md" || ext == "markdown") {
        auto* lex = new QsciLexerMarkdown(editor);
        applyLexerBase(lex);
        styleMarkdown(lex);
        lexer = lex;
    } else if (ext == "lua") {
        auto* lex = new QsciLexerLua(editor);
        applyLexerBase(lex);
        styleLua(lex);
        lexer = lex;
    } else if (ext == "rb") {
        auto* lex = new QsciLexerRuby(editor);
        applyLexerBase(lex);
        styleRuby(lex);
        lexer = lex;
    } else if (ext == "pl" || ext == "pm") {
        auto* lex = new QsciLexerPerl(editor);
        applyLexerBase(lex);
        // Perl shares CPP-like comment/string styles
        lex->setColor(Theme::Text,      0);
        lex->setColor(Theme::Overlay1,  2);  // Comment
        lex->setColor(Theme::Peach,     4);  // Number
        lex->setColor(Theme::Mauve,     5);  // Keyword
        lex->setColor(Theme::Green,     6);  // String
        lex->setColor(Theme::Green,     7);  // String q
        lex->setColor(Theme::Green,     8);  // String qq
        lex->setColor(Theme::Sky,       10); // Operator
        lex->setColor(Theme::Text,      11); // Identifier
        lex->setColor(Theme::Flamingo,  12); // Scalar
        lex->setColor(Theme::Yellow,    13); // Array
        lex->setColor(Theme::Teal,      14); // Hash
        lex->setColor(Theme::Blue,      17); // Regex
        lex->setColor(Theme::Sapphire,  24); // Here doc delimiter
        lex->setColor(Theme::Green,     25); // Here doc single
        lex->setColor(Theme::Green,     26); // Here doc double
        lex->setColor(Theme::Green,     27); // Backtick here doc
        lex->setColor(Theme::Pink,      28); // POD
        lexer = lex;
    } else if (ext == "java") {
        auto* lex = new QsciLexerJava(editor);
        applyLexerBase(lex);
        styleCpp(lex);  // Java uses CPP lexer styles
        lexer = lex;
    } else if (ext == "cs") {
        auto* lex = new QsciLexerCSharp(editor);
        applyLexerBase(lex);
        styleCpp(lex);  // C# uses CPP lexer styles
        lexer = lex;
    } else if (name == "makefile" || name == "gnumakefile" || ext == "mk") {
        auto* lex = new QsciLexerMakefile(editor);
        applyLexerBase(lex);
        lex->setColor(Theme::Text,      0);  // Default
        lex->setColor(Theme::Overlay1,  1);  // Comment
        lex->setColor(Theme::Mauve,     2);  // Preprocessor
        lex->setColor(Theme::Yellow,    3);  // Variable
        lex->setColor(Theme::Sky,       4);  // Operator
        lex->setColor(Theme::Blue,      5);  // Target
        lex->setColor(Theme::Red,       9);  // Error
        lexer = lex;
    } else if (ext == "diff" || ext == "patch") {
        auto* lex = new QsciLexerDiff(editor);
        applyLexerBase(lex);
        styleDiff(lex);
        lexer = lex;
    } else if (name == "cmakelists.txt" || ext == "cmake") {
        auto* lex = new QsciLexerCMake(editor);
        applyLexerBase(lex);
        styleCMake(lex);
        lexer = lex;
    } else if (ext == "ini" || ext == "cfg" || ext == "conf" || ext == "properties") {
        auto* lex = new QsciLexerProperties(editor);
        applyLexerBase(lex);
        styleProperties(lex);
        lexer = lex;
    }

    if (lexer)
        editor->setLexer(lexer);
}
