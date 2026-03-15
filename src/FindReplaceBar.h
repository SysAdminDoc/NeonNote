#pragma once
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <Qsci/qsciscintilla.h>

class FindReplaceBar : public QWidget {
    Q_OBJECT
public:
    explicit FindReplaceBar(QWidget* parent = nullptr);

    void setEditor(QsciScintilla* editor);
    void showFind();
    void showReplace();

protected:
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void findNext();
    void findPrev();
    void replaceCurrent();
    void replaceAll();
    void onSearchTextChanged();

private:
    void updateMatchCount();
    bool doFind(bool forward);

    QsciScintilla* m_editor = nullptr;

    // Find row
    QLineEdit* m_findEdit;
    QPushButton* m_btnPrev;
    QPushButton* m_btnNext;
    QPushButton* m_btnClose;
    QLabel* m_matchLabel;
    QCheckBox* m_caseSensitive;
    QCheckBox* m_wholeWord;
    QCheckBox* m_regex;

    // Replace row
    QWidget* m_replaceRow;
    QLineEdit* m_replaceEdit;
    QPushButton* m_btnReplace;
    QPushButton* m_btnReplaceAll;

    bool m_replaceVisible = false;
};
