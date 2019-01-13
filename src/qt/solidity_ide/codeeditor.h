/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef CODEEDITOR_H
#define CODEEDITOR_H

#include <QPlainTextEdit>
#include <QObject>
#include <QMap>
#include <QTextBlockUserData>
#include <QByteArray>
#include <QPointer>
#include <QSettings>
#include <QToolTip>
#include "highlighter.h"
#include "find/globalsearch.h"

QT_BEGIN_NAMESPACE
class QPaintEvent;
class QResizeEvent;
class QSize;
class QWidget;
class QTextBlock;
QT_END_NAMESPACE

class LineNumberArea;

struct ErrWarningBuildData
{
    ErrWarningBuildData(const ErrWarningBuildData & other)
    {
        iX = other.iX;
        iY = other.iY;
        type = other.type;
        message = other.message;
    }
    ErrWarningBuildData & operator=(const ErrWarningBuildData & other)
    {
        iX = other.iX;
        iY = other.iY;
        type = other.type;
        message = other.message;
        return *this;
    }
    ErrWarningBuildData()
    {}
    enum ErrBuildTypes {iEmpty, iError, iWarning};
    int iX{0};
    int iY{0};
    ErrBuildTypes type {iEmpty};
    QString message;
};
Q_DECLARE_METATYPE(ErrWarningBuildData)

class CodeEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    CodeEditor(QWidget *parent = 0);
    QSettings settings;
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    void calcLineNumberAreaWidth();
    void setErr_Warnings(const QMap<QString, QMap<int, ErrWarningBuildData> > &list);
    ErrWarningBuildData err_warnToBlockNumber(int nBlockNumber);
    void removeDocument(QString nameDocument);
    void renameDocument(QString newName, QString oldName);
    void setCurrentDocument(QString nameDocument);
    //will open all files and store them in
    //new QTextDocument in cur_projDocuments
    //+clear previous data
    void addCurrentProject(QStringList files); //files - absolute file path of current *.sol and all it imports
    bool containsFile(QString absPathFile);
    QByteArray utf8DataOfFile(QString absPathFile);
protected:
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event);
    void paintEvent(QPaintEvent *event) override;
private:
    struct DocumentFileData
    {
        DocumentFileData()
        {}
        DocumentFileData(QTextDocument * doc, Highlighter * high,
                         bool bOpenFile, bool bProjectFile,
                         QMap<int, ErrWarningBuildData> err_warnings):
                            doc(doc),
                            high(high),
                            bOpenFile(bOpenFile),
                            bProjectFile(bProjectFile),
                            err_warnings(err_warnings)
        {}
        QTextDocument * doc {nullptr};
        Highlighter * high {nullptr};
        bool bOpenFile {false};
        bool bProjectFile {false};
        QMap<int, ErrWarningBuildData> err_warnings; //<nBlockNumber (0,1,...), warningData>
    };
    QWidget *lineNumberArea;
    QPointer<Highlighter> highlighter;
    QObject documentsParent;
    QString currentName;
    int _lineNumberAreaWidth;
    int spaceLineNumber;
    QColor mix2clr(const QColor &clr1, const QColor &clr2);
    void init();
    bool loadFile {false};
    bool matchLeftBrackets(QTextBlock currentBlock,
                           int index, int numberLeftBracket);
    bool matchRightBrackets(QTextBlock currentBlock,
                            int index, int numberRightBracket);
    void createBracketsSelection(int position);
    QString strSearch;
    QVector<SearchItem> fillFindResults(QString nameFile);

    QMap<QString, DocumentFileData> allDocumentsData;
    void replaceInFile(QString fileName,
                       QString replaceStr,
                       bool bFirstFile);
public slots:
    void slotHighlightingCode(bool on);
    //find
    void slotFindAllCurrentFile();
    void slotFindAllAllFiles();
    void slotFindAllCurProject();
    void slotFindNext();
    void slotFindPrev();
    //mark
    void slotSearchMark(QString strCurrent);
    //replace
    void slotReplace(QString replaceStr);
    void slotReplaceAllCurrent(QString replaceStr);
    void slotReplaceAllAll(QString replaceStr);
    void slotReplaceAllCurProject(QString replaceStr);
    //choose
    void slotCurrentFindResultChanged(QString fileName,
                                      int blockNumber,
                                      int positionResult);

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect &, int);
    void matchBrackets();
signals:
    void newChanges();
    void sigFindResults(findResults);
    void contentsChange(int position, int charsRemoved, int charsAdded);
};

class LineNumberArea : public QWidget
{
public:
    LineNumberArea(CodeEditor *editor) : QWidget(editor) {
        codeEditor = editor;
    }

    QSize sizeHint() const override {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    bool event(QEvent *event) override {
        if (event->type() == QEvent::ToolTip) {
            QHelpEvent *helpEvent = static_cast<QHelpEvent *>(event);
            auto cursor = codeEditor->cursorForPosition(helpEvent->pos());
            auto block = cursor.block();
            bool bTooltip = false;
            if(block.isValid())
            {
                int iNumber = block.blockNumber();
                ErrWarningBuildData data = codeEditor->err_warnToBlockNumber(iNumber);
                if(data.type != ErrWarningBuildData::iEmpty)
                {
                    QToolTip::showText(helpEvent->globalPos(), data.message);
                    bTooltip = true;
                }
            }
            if(!bTooltip)
            {
                QToolTip::hideText();
                event->ignore();
            }

            return true;
        }
        return QWidget::event(event);
    }
    void paintEvent(QPaintEvent *event) override {
        codeEditor->lineNumberAreaPaintEvent(event);
    }

private:
    CodeEditor *codeEditor;
};

#endif
