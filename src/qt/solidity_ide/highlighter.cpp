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

#include "highlighter.h"
#include <QLocale>
#include <QDateTime>
#include <QTextDocument>
#include "global.h"


Highlighter::Highlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRegExpRule rule;

    quotationFormat.setForeground(Qt::darkGreen);
    rule.pattern = QRegularExpression("\".*\"");
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    functionFormat.setFontItalic(true);
    functionFormat.setFontWeight(QFont::Bold);
    functionFormat.setForeground(QBrush(QColor(0,38,255)));
    rule.pattern = QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()");
    rule.format = functionFormat;
    highlightingRules.append(rule);

    allLanguageWordsFormat.setForeground(QBrush(QColor(147,170,28)));
    rule.pattern = QRegularExpression("\\b(var|import|function|enum|constant|pure|view|payable|nonpayable|if|else|for|while|do|break|continue|throw|returns?|private|public|external|inherited|storage|delete|memory|this|suicide|let|new|is|ether|wei|finney|szabo|seconds|minutes|hours|days|weeks|years\\_)\\b");
    rule.format = allLanguageWordsFormat;
    highlightingRules.append(rule);

    eventsFormat.setFontItalic(true);
    functionFormat.setFontWeight(QFont::Bold);
    eventsFormat.setForeground(QBrush(QColor(71,184,255)));
    rule.pattern = QRegularExpression("\\b(event)\\b");
    rule.format = eventsFormat;
    highlightingRules.append(rule);

    mainKeywordsFormat.setForeground(QBrush(QColor(178,0,255)));
    rule.pattern = QRegularExpression("\\b(pragma|contract|interface|struct|library|function|modifier|enum|assembly|is|indexed)\\b");
    rule.format = mainKeywordsFormat;
    highlightingRules.append(rule);

    //builtInTypes
    {
        builtInTypesFormat.setForeground(QBrush(QColor(147,170,28)));
        rule.pattern = QRegularExpression("\\b(address|string|byte|bytes|int|uint|bool)\\b");
        rule.format = builtInTypesFormat;
        highlightingRules.append(rule);
        for(int i=1; i<=64; i++)
        {
            rule.pattern = QRegularExpression("\\bint" + QString::number(i*8) + "\\b");
            rule.format = builtInTypesFormat;
            highlightingRules.append(rule);
            rule.pattern = QRegularExpression("\\buint" + QString::number(i*8) + "\\b");
            rule.format = builtInTypesFormat;
            highlightingRules.append(rule);
        }
        for(int i=1; i<=32; i++)
        {
            rule.pattern = QRegularExpression("\\bbytes" + QString::number(i) + "\\b");
            rule.format = builtInTypesFormat;
            highlightingRules.append(rule);
        }
    }

    mappingFormat.setFontItalic(true);
    functionFormat.setFontWeight(QFont::Bold);
    mappingFormat.setForeground(QBrush(QColor(122,255,217)));
    rule.pattern = QRegularExpression("\\bmapping\\b");
    rule.format = mappingFormat;
    highlightingRules.append(rule);

    msg_blocksFormat.setForeground(QBrush(QColor(147,39,28)));
    rule.pattern = QRegularExpression("\\b(msg|block|tx)\\b");
    rule.format = msg_blocksFormat;
    highlightingRules.append(rule);

    numbersFormat.setForeground(QBrush(QColor(0,38,255)));
    rule.pattern = QRegularExpression("\\b(\\d+)\\b");
    rule.format = numbersFormat;
    highlightingRules.append(rule);

    hexFormat.setForeground(QBrush(QColor(0,38,255)));
    rule.pattern = QRegularExpression("\\b(0[xX][a-fA-F0-9]+)\\b");
    rule.format = hexFormat;
    highlightingRules.append(rule);

    singleLineCommentFormat.setForeground(Qt::red);
    rule.pattern = QRegularExpression("//[^\n]*");
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    commentStartExpression = QRegularExpression("/\\*");
    commentEndExpression = QRegularExpression("\\*/");

    multiLineCommentFormat.setForeground(Qt::red);

    findFormat.setForeground(Qt::red);
    findFormat.setBackground(Qt::yellow);
}

//return position first finding (<0 not finding)
int Highlighter::markSearch(const QString &strMark)
{
    int firstFinding = -1;
    bool bRegExp = settings.value(defRegularExpressions).toBool();
    bool bMatchWholeWord = settings.value(defWholeWord).toBool();
    bool bMatchCase = settings.value(defCase).toBool();
    //fill findTextRule and findRegExpRule
    {
        findTextRule.findText = "";
        findRegExpRule = QRegularExpression();
        if(bRegExp)
        {
            findRegExpRule = QRegularExpression(strMark);
            if(!bMatchCase)
                findRegExpRule.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        }
        else
        {
            findTextRule.findText = strMark;
            findTextRule.bMatchCase = bMatchCase;
            findTextRule.bMatchWholeWord = bMatchWholeWord;
        }
    }
    //mark
    {
        QTextBlock block = document()->firstBlock();
        while(block.isValid())
        {
            TextBlockUserData *data = static_cast<TextBlockUserData *>(block.userData());
            if(data == nullptr)
            {
                data = new TextBlockUserData();
            }
            bool oldFind = false;
            //fill oldFind
            {
                if(!data->finds().isEmpty())
                    oldFind = true;
            }
            matchSearch(block, data);
            if(!data->finds().isEmpty()
                    ||
                    oldFind)
            {
                if(firstFinding == -1
                        &&
                   !data->finds().isEmpty())
                {
                    firstFinding = data->finds().first()->posStart
                            + block.position();
                }
                manualRehighlight = true;
                rehighlightBlock(block);
            }
            block = block.next();
        }
    }
    return firstFinding;
}

void Highlighter::setHighlightSearch(bool bOn)
{
    highlightSearch = bOn;
}

void Highlighter::matchSearch(const QTextBlock& block,
                              TextBlockUserData * data)
{
    QVector <USearchInfo *> finds;
    if(!findRegExpRule.pattern().isEmpty())
    {
        QRegularExpressionMatchIterator matchIterator
                = findRegExpRule.globalMatch(block.text());
        while (matchIterator.hasNext())
        {
            QRegularExpressionMatch match = matchIterator.next();
            USearchInfo *info = new USearchInfo();
            info->posStart = match.capturedStart();
            info->length = match.capturedLength();
            info->color = findFormat.background().color();
            finds.append(info);
        }
    }
    if(!findTextRule.findText.isEmpty())
    {
        QTextDocument::FindFlags findFlags;
        findFlags.setFlag(QTextDocument::FindCaseSensitively,
                          findTextRule.bMatchCase);
        findFlags.setFlag(QTextDocument::FindWholeWords,
                          findTextRule.bMatchWholeWord);
        QTextCursor cursor = QTextCursor(block);
        while (1)
        {
            cursor = document()->find(findTextRule.findText,
                                      cursor,
                                      findFlags);
            if(cursor.isNull() ||
                cursor.block().blockNumber() > block.blockNumber())
                break;

            USearchInfo *info = new USearchInfo();
            info->posStart = cursor.anchor() - block.position();
            info->length = cursor.position() - cursor.anchor();
            info->color = findFormat.background().color();
            finds.append(info);
        }
    }
    data->addFindInfo(finds);
}

void Highlighter::highlightBlock(const QString &text)
{
    TextBlockUserData *data = static_cast<TextBlockUserData *>(currentBlockUserData());
    if(data == nullptr)
    {
        data = new TextBlockUserData();
    }
    else
        data->insert(QVector <UBracketInfo *>());
    bool highlighting_On = true;
    if(settings.value("Highlighting_Text").type() != QVariant::Invalid)
    {
        highlighting_On = settings.value("Highlighting_Text").toBool();
    }
    if(highlighting_On)
    {
        foreach (const HighlightingRegExpRule &rule, highlightingRules)
        {
            QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
            while (matchIterator.hasNext()) {
                QRegularExpressionMatch match = matchIterator.next();
                setFormat(match.capturedStart(), match.capturedLength(), rule.format);
            }
        }
        //highlight search
        if(highlightSearch)
        {
            if(!manualRehighlight)
                matchSearch(currentBlock(), data);
            manualRehighlight = false;
            QVector <USearchInfo *> finds = data->finds();
            foreach(auto find, finds)
            {
                setFormat(find->posStart,
                          find->length,
                          findFormat);
            }
        }
        insertBrackets(RoundBrackets, data, text);
        insertBrackets(CurlyBraces, data, text);
        insertBrackets(SquareBrackets, data, text);
        setCurrentBlockUserData(data);
        setCurrentBlockState(0);

        int startIndex = 0;
        if (previousBlockState() != 1)
            startIndex = text.indexOf(commentStartExpression);

        while (startIndex >= 0) {
            QRegularExpressionMatch match = commentEndExpression.match(text, startIndex);
            int endIndex = match.capturedStart();
            int commentLength = 0;
            if (endIndex == -1) {
                setCurrentBlockState(1);
                commentLength = text.length() - startIndex;
            } else {
                commentLength = endIndex - startIndex
                                + match.capturedLength();
            }
            setFormat(startIndex, commentLength, multiLineCommentFormat);
            startIndex = text.indexOf(commentStartExpression, startIndex + commentLength);
        }
    }
}

void Highlighter::insertBrackets(QChar leftChar, QChar rightChar,
                                        TextBlockUserData *data, QString text)
{
    int leftPosition = text.indexOf(leftChar);

    while(leftPosition != -1)
    {
        UBracketInfo *info = new UBracketInfo();
        info->character = leftChar;
        info->position = leftPosition;

        data->insert(info);
        leftPosition = text.indexOf(leftChar, leftPosition + 1);
    }


    int rightPosition = text.indexOf(rightChar);

    while(rightPosition != -1)
    {
        UBracketInfo *info = new UBracketInfo();
        info->character = rightChar;
        info->position = rightPosition;

        data->insert(info);
        rightPosition = text.indexOf(rightChar, rightPosition + 1);
    }
}

void Highlighter::insertBrackets(UBrackets brackets,
                                 TextBlockUserData *data, QString text)
{
    QChar leftChar = '0';
    QChar rightChar = '0';

    switch(brackets)
    {
    case RoundBrackets:
        leftChar = '('; rightChar = ')';
        break;

    case CurlyBraces:
        leftChar = '{'; rightChar = '}';
        break;

    case SquareBrackets:
        leftChar = '['; rightChar = ']';
        break;
    }

    insertBrackets(leftChar, rightChar, data, text);
}
