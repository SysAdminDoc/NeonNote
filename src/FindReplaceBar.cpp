#include "FindReplaceBar.h"
#include "Theme.h"
#include <QPainter>
#include <QKeyEvent>

FindReplaceBar::FindReplaceBar(QWidget* parent) : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 4, 8, 4);
    mainLayout->setSpacing(4);

    QString editStyle = QStringLiteral(
        "QLineEdit {"
        "  background: #181825; color: #cdd6f4; border: 1px solid #45475a;"
        "  border-radius: 4px; padding: 3px 6px; font-size: 12px;"
        "  selection-background-color: #45475a;"
        "}"
        "QLineEdit:focus { border: 1px solid #89b4fa; }"
    );

    QString btnStyle = QStringLiteral(
        "QPushButton {"
        "  background: transparent; color: #a6adc8; border: none;"
        "  font-size: 10px; font-family: 'Segoe MDL2 Assets';"
        "  padding: 4px 6px; border-radius: 3px;"
        "}"
        "QPushButton:hover { background: #313244; color: #cdd6f4; }"
    );

    QString checkStyle = QStringLiteral(
        "QCheckBox { color: #a6adc8; font-size: 11px; spacing: 3px; }"
        "QCheckBox:hover { color: #cdd6f4; }"
        "QCheckBox::indicator {"
        "  width: 14px; height: 14px; border-radius: 3px;"
        "  border: 1px solid #45475a; background: #181825;"
        "}"
        "QCheckBox::indicator:checked { background: #89b4fa; border-color: #89b4fa; }"
    );

    // Find row
    auto* findRow = new QHBoxLayout();
    findRow->setSpacing(4);

    m_findEdit = new QLineEdit(this);
    m_findEdit->setPlaceholderText("Find");
    m_findEdit->setMinimumWidth(200);
    m_findEdit->setStyleSheet(editStyle);

    m_matchLabel = new QLabel("No results", this);
    m_matchLabel->setStyleSheet("QLabel { color: #6c7086; font-size: 11px; }");
    m_matchLabel->setFixedWidth(80);

    m_caseSensitive = new QCheckBox("Aa", this);
    m_caseSensitive->setToolTip("Case Sensitive");
    m_caseSensitive->setStyleSheet(checkStyle);

    m_wholeWord = new QCheckBox("W", this);
    m_wholeWord->setToolTip("Whole Word");
    m_wholeWord->setStyleSheet(checkStyle);

    m_regex = new QCheckBox(".*", this);
    m_regex->setToolTip("Regular Expression");
    m_regex->setStyleSheet(checkStyle);

    m_btnPrev = new QPushButton(QString::fromUtf16(u"\uE70E"), this);  // Up arrow
    m_btnPrev->setToolTip("Previous Match");
    m_btnPrev->setFixedSize(26, 26);
    m_btnPrev->setStyleSheet(btnStyle);

    m_btnNext = new QPushButton(QString::fromUtf16(u"\uE70D"), this);  // Down arrow
    m_btnNext->setToolTip("Next Match");
    m_btnNext->setFixedSize(26, 26);
    m_btnNext->setStyleSheet(btnStyle);

    m_btnClose = new QPushButton(QString::fromUtf16(u"\uE8BB"), this);
    m_btnClose->setToolTip("Close");
    m_btnClose->setFixedSize(26, 26);
    m_btnClose->setStyleSheet(btnStyle);

    findRow->addWidget(m_findEdit);
    findRow->addWidget(m_matchLabel);
    findRow->addWidget(m_caseSensitive);
    findRow->addWidget(m_wholeWord);
    findRow->addWidget(m_regex);
    findRow->addWidget(m_btnPrev);
    findRow->addWidget(m_btnNext);
    findRow->addStretch();
    findRow->addWidget(m_btnClose);

    mainLayout->addLayout(findRow);

    // Replace row
    m_replaceRow = new QWidget(this);
    auto* replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(0, 0, 0, 0);
    replaceLayout->setSpacing(4);

    m_replaceEdit = new QLineEdit(m_replaceRow);
    m_replaceEdit->setPlaceholderText("Replace");
    m_replaceEdit->setMinimumWidth(200);
    m_replaceEdit->setStyleSheet(editStyle);

    m_btnReplace = new QPushButton("Replace", m_replaceRow);
    m_btnReplace->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: none;"
        "  border-radius: 3px; padding: 4px 10px; font-size: 11px; }"
        "QPushButton:hover { background: #45475a; }"
    );

    m_btnReplaceAll = new QPushButton("Replace All", m_replaceRow);
    m_btnReplaceAll->setStyleSheet(
        "QPushButton { background: #313244; color: #cdd6f4; border: none;"
        "  border-radius: 3px; padding: 4px 10px; font-size: 11px; }"
        "QPushButton:hover { background: #45475a; }"
    );

    replaceLayout->addWidget(m_replaceEdit);
    replaceLayout->addWidget(m_btnReplace);
    replaceLayout->addWidget(m_btnReplaceAll);
    replaceLayout->addStretch();

    mainLayout->addWidget(m_replaceRow);
    m_replaceRow->hide();

    hide();

    // Connections
    connect(m_findEdit, &QLineEdit::textChanged, this, &FindReplaceBar::onSearchTextChanged);
    connect(m_findEdit, &QLineEdit::returnPressed, this, &FindReplaceBar::findNext);
    connect(m_btnNext, &QPushButton::clicked, this, &FindReplaceBar::findNext);
    connect(m_btnPrev, &QPushButton::clicked, this, &FindReplaceBar::findPrev);
    connect(m_btnClose, &QPushButton::clicked, this, [this]() { hide(); });
    connect(m_btnReplace, &QPushButton::clicked, this, &FindReplaceBar::replaceCurrent);
    connect(m_btnReplaceAll, &QPushButton::clicked, this, &FindReplaceBar::replaceAll);
    connect(m_caseSensitive, &QCheckBox::toggled, this, &FindReplaceBar::onSearchTextChanged);
    connect(m_wholeWord, &QCheckBox::toggled, this, &FindReplaceBar::onSearchTextChanged);
    connect(m_regex, &QCheckBox::toggled, this, &FindReplaceBar::onSearchTextChanged);
}

void FindReplaceBar::setEditor(QsciScintilla* editor)
{
    m_editor = editor;
}

void FindReplaceBar::showFind()
{
    m_replaceVisible = false;
    m_replaceRow->hide();
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();

    if (m_editor && m_editor->hasSelectedText())
        m_findEdit->setText(m_editor->selectedText());

    updateMatchCount();
}

void FindReplaceBar::showReplace()
{
    m_replaceVisible = true;
    m_replaceRow->show();
    show();
    m_findEdit->setFocus();
    m_findEdit->selectAll();

    if (m_editor && m_editor->hasSelectedText())
        m_findEdit->setText(m_editor->selectedText());

    updateMatchCount();
}

void FindReplaceBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Theme::Surface0);
    p.setPen(Theme::Surface1);
    p.drawLine(0, height() - 1, width(), height() - 1);
}

void FindReplaceBar::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        if (m_editor) m_editor->setFocus();
        return;
    }
    QWidget::keyPressEvent(event);
}

void FindReplaceBar::findNext()
{
    doFind(true);
}

void FindReplaceBar::findPrev()
{
    doFind(false);
}

bool FindReplaceBar::doFind(bool forward)
{
    if (!m_editor) return false;
    QString text = m_findEdit->text();
    if (text.isEmpty()) return false;

    bool cs = m_caseSensitive->isChecked();
    bool ww = m_wholeWord->isChecked();
    bool re = m_regex->isChecked();

    bool found = m_editor->findFirst(text, re, cs, ww, true, forward);

    m_matchLabel->setStyleSheet(found
        ? "QLabel { color: #a6adc8; font-size: 11px; }"
        : "QLabel { color: #f38ba8; font-size: 11px; }");

    updateMatchCount();
    return found;
}

void FindReplaceBar::replaceCurrent()
{
    if (!m_editor) return;
    if (m_editor->hasSelectedText()) {
        m_editor->replace(m_replaceEdit->text());
        findNext();
    } else {
        findNext();
    }
    updateMatchCount();
}

void FindReplaceBar::replaceAll()
{
    if (!m_editor) return;
    QString find = m_findEdit->text();
    QString replace = m_replaceEdit->text();
    if (find.isEmpty()) return;

    bool cs = m_caseSensitive->isChecked();
    bool ww = m_wholeWord->isChecked();
    bool re = m_regex->isChecked();

    int count = 0;
    m_editor->SendScintilla(QsciScintilla::SCI_BEGINUNDOACTION);

    // Start from beginning
    m_editor->setCursorPosition(0, 0);
    while (m_editor->findFirst(find, re, cs, ww, false, true)) {
        m_editor->replace(replace);
        count++;
        if (count > 100000) break;  // safety limit
    }

    m_editor->SendScintilla(QsciScintilla::SCI_ENDUNDOACTION);

    m_matchLabel->setText(QString("%1 replaced").arg(count));
    m_matchLabel->setStyleSheet("QLabel { color: #a6e3a1; font-size: 11px; }");
}

void FindReplaceBar::onSearchTextChanged()
{
    if (!m_editor) return;
    QString text = m_findEdit->text();
    if (text.isEmpty()) {
        m_matchLabel->setText("No results");
        m_matchLabel->setStyleSheet("QLabel { color: #6c7086; font-size: 11px; }");
        return;
    }

    // Live search: find from current position
    bool cs = m_caseSensitive->isChecked();
    bool ww = m_wholeWord->isChecked();
    bool re = m_regex->isChecked();

    m_editor->findFirst(text, re, cs, ww, true, true, 0, 0);
    updateMatchCount();
}

void FindReplaceBar::updateMatchCount()
{
    if (!m_editor) return;
    QString text = m_findEdit->text();
    if (text.isEmpty()) {
        m_matchLabel->setText("No results");
        m_matchLabel->setStyleSheet("QLabel { color: #6c7086; font-size: 11px; }");
        return;
    }

    // Count total matches using Scintilla search
    bool cs = m_caseSensitive->isChecked();
    int flags = 0;
    if (cs) flags |= QsciScintilla::SCFIND_MATCHCASE;
    if (m_wholeWord->isChecked()) flags |= QsciScintilla::SCFIND_WHOLEWORD;
    if (m_regex->isChecked()) flags |= QsciScintilla::SCFIND_REGEXP;

    QByteArray textBytes = text.toUtf8();
    long docLen = m_editor->SendScintilla(QsciScintilla::SCI_GETLENGTH);
    int count = 0;
    long pos = 0;

    while (pos < docLen) {
        m_editor->SendScintilla(QsciScintilla::SCI_SETTARGETSTART, pos);
        m_editor->SendScintilla(QsciScintilla::SCI_SETTARGETEND, docLen);
        m_editor->SendScintilla(QsciScintilla::SCI_SETSEARCHFLAGS, flags);
        long found = m_editor->SendScintilla(QsciScintilla::SCI_SEARCHINTARGET,
                                              textBytes.length(), textBytes.constData());
        if (found < 0) break;
        count++;
        pos = m_editor->SendScintilla(QsciScintilla::SCI_GETTARGETEND);
        if (pos <= found) break;  // prevent infinite loop on zero-length match
    }

    if (count > 0) {
        m_matchLabel->setText(QString("%1 found").arg(count));
        m_matchLabel->setStyleSheet("QLabel { color: #a6adc8; font-size: 11px; }");
    } else {
        m_matchLabel->setText("No results");
        m_matchLabel->setStyleSheet("QLabel { color: #f38ba8; font-size: 11px; }");
    }
}
