#pragma once
#include <QWidget>
#include <QMap>
#include <Qsci/qsciscintilla.h>

class AutoScrollHandler;

class EditorWidget : public QWidget {
    Q_OBJECT
public:
    explicit EditorWidget(QWidget* parent = nullptr);

    QsciScintilla* createEditor();
    QsciScintilla* currentEditor() const;
    void setCurrentEditor(QsciScintilla* editor);

    void applyTheme(QsciScintilla* editor);
    void setLexerForFile(QsciScintilla* editor, const QString& filePath);
    void editorDeleted(QsciScintilla* editor);

private:
    QsciScintilla* m_current = nullptr;
    AutoScrollHandler* m_autoScroll = nullptr;
};
