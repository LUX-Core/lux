/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "searchresulttreeitemdelegate.h"

#include <QPainter>
#include <QApplication>

#include <QModelIndex>
#include <QDebug>

SearchResultTreeItemDelegate::SearchResultTreeItemDelegate(int tabWidth, QObject *parent)
    : QItemDelegate(parent)
{
    setTabWidth(tabWidth);
}

void SearchResultTreeItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->save();

    QStyleOptionViewItem opt = setOptions(index, option);
    painter->setFont(opt.font);

    QItemDelegate::drawBackground(painter, opt, index);

    // ---- do the layout
    QRect checkRect;
    QRect pixmapRect;
    QRect textRect;

    // text
    textRect = opt.rect.adjusted(0, 0, checkRect.width() + pixmapRect.width(), 0);

    // do layout
    doLayout(opt, &checkRect, &pixmapRect, &textRect, false);
    // ---- draw the items

    // line numbers
    if(index.parent().isValid())
    {
        int lineNumberAreaWidth = drawLineNumber(painter, opt, textRect, index);
        textRect.adjust(lineNumberAreaWidth, 0, 0, 0);
    }
    // text and focus/selection
    drawText(painter, opt, textRect, index);
    QItemDelegate::drawFocus(painter, opt, opt.rect);

    painter->restore();
}

void SearchResultTreeItemDelegate::setTabWidth(int width)
{
    m_tabString = QString(width, QLatin1Char(' '));
}

// returns the width of the line number area
int SearchResultTreeItemDelegate::drawLineNumber(QPainter *painter, const QStyleOptionViewItem &option,
                                                 const QRect &rect,
                                                 const QModelIndex &index) const
{
    static const int lineNumberAreaHorizontalPadding = 4;
    int lineNumber = index.model()->data(index, defNumberLineRole).toInt();
    if (lineNumber < 1)
        return 0;
    const bool isSelected = option.state & QStyle::State_Selected;
    QString lineText = QString::number(lineNumber);
    int minimumLineNumberDigits = qMax((int)m_minimumLineNumberDigits, lineText.count());
    int fontWidth = painter->fontMetrics().width(QString(minimumLineNumberDigits, QLatin1Char('0')));
    int lineNumberAreaWidth = lineNumberAreaHorizontalPadding + fontWidth + lineNumberAreaHorizontalPadding;
    QRect lineNumberAreaRect(rect);
    lineNumberAreaRect.setWidth(lineNumberAreaWidth);

    QPalette::ColorGroup cg = QPalette::Normal;
    if (!(option.state & QStyle::State_Active))
        cg = QPalette::Inactive;
    else if (!(option.state & QStyle::State_Enabled))
        cg = QPalette::Disabled;

    painter->fillRect(lineNumberAreaRect, QBrush(isSelected ?
        option.palette.brush(cg, QPalette::Highlight) :
        option.palette.color(cg, QPalette::Base).darker(111)));

    QStyleOptionViewItem opt = option;
    opt.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
    opt.palette.setColor(cg, QPalette::Text, Qt::darkGray);

    const QStyle *style = QApplication::style();
    const int textMargin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, 0) + 1;

    const QRect rowRect = lineNumberAreaRect.adjusted(-textMargin, 0, textMargin-lineNumberAreaHorizontalPadding, 0);
    QItemDelegate::drawDisplay(painter, opt, rowRect, lineText);

    return lineNumberAreaWidth;
}

void SearchResultTreeItemDelegate::drawText(QPainter *painter,
                                            const QStyleOptionViewItem &option,
                                            const QRect &rect,
                                            const QModelIndex &index) const
{
    QString text = index.model()->data(index, Qt::DisplayRole).toString();
    //QString text = "Test Example";
    // show number of subresults in displayString
    if (index.model()->hasChildren(index)) {
        text += QLatin1String(" (")
                + QString::number(index.model()->rowCount(index))
                + QLatin1Char(')');
    }

//    const int searchTermStart = index.model()->data(index, ItemDataRoles::ResultBeginColumnNumberRole).toInt();
//    int searchTermLength = index.model()->data(index, ItemDataRoles::SearchTermLengthRole).toInt();
    if(index.parent().isValid())
    {
        const int searchTermStart = index.model()->data(index, defStartFindRole).toInt();
        int searchTermLength = index.model()->data(index, defLengthFindRole).toInt();
        if (searchTermStart < 0 || searchTermStart >= text.length() || searchTermLength < 1) {
            QItemDelegate::drawDisplay(painter, option, rect, text.replace(QLatin1Char('\t'), m_tabString));
            return;
        }

        // clip searchTermLength to end of line
        searchTermLength = qMin(searchTermLength, text.length() - searchTermStart);
        const int textMargin = QApplication::style()->pixelMetric(QStyle::PM_FocusFrameHMargin) + 1;
        const QString textBefore = text.left(searchTermStart).replace(QLatin1Char('\t'), m_tabString);
        const QString textHighlight = text.mid(searchTermStart, searchTermLength).replace(QLatin1Char('\t'), m_tabString);
        const QString textAfter = text.mid(searchTermStart + searchTermLength).replace(QLatin1Char('\t'), m_tabString);
        int searchTermStartPixels = painter->fontMetrics().width(textBefore);
        int searchTermLengthPixels = painter->fontMetrics().width(textHighlight);

        // rects
        QRect beforeHighlightRect(rect);
        beforeHighlightRect.setRight(beforeHighlightRect.left() + searchTermStartPixels);

        QRect resultHighlightRect(rect);
        resultHighlightRect.setLeft(beforeHighlightRect.right());
        resultHighlightRect.setRight(resultHighlightRect.left() + searchTermLengthPixels);

        QRect afterHighlightRect(rect);
        afterHighlightRect.setLeft(resultHighlightRect.right());

        // paint all highlight backgrounds
        // qitemdelegate has problems with painting background when highlighted
        // (highlighted background at wrong position because text is offset with textMargin)
        // so we duplicate a lot here, see qitemdelegate for reference
        bool isSelected = option.state & QStyle::State_Selected;
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                                  ? QPalette::Normal : QPalette::Disabled;
        if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
            cg = QPalette::Inactive;
        QStyleOptionViewItem baseOption = option;
        baseOption.state &= ~QStyle::State_Selected;
        if (isSelected) {
            painter->fillRect(beforeHighlightRect.adjusted(textMargin, 0, textMargin, 0),
                              option.palette.brush(cg, QPalette::Highlight));
            painter->fillRect(afterHighlightRect.adjusted(textMargin, 0, textMargin, 0),
                              option.palette.brush(cg, QPalette::Highlight));
        }
    //    const QColor highlightBackground =
    //            index.model()->data(index, ItemDataRoles::ResultHighlightBackgroundColor).value<QColor>();
        const QColor highlightBackground = Qt::yellow;
        painter->fillRect(resultHighlightRect.adjusted(textMargin, 0, textMargin - 1, 0), QBrush(highlightBackground));

        // Text before the highlighting
        QStyleOptionViewItem noHighlightOpt = baseOption;
        noHighlightOpt.rect = beforeHighlightRect;
        noHighlightOpt.textElideMode = Qt::ElideNone;
        if (isSelected)
            noHighlightOpt.palette.setColor(QPalette::Text, noHighlightOpt.palette.color(cg, QPalette::HighlightedText));
        QItemDelegate::drawDisplay(painter, noHighlightOpt, beforeHighlightRect, textBefore);

        // Highlight text
        QStyleOptionViewItem highlightOpt = noHighlightOpt;
    //    const QColor highlightForeground =
    //            index.model()->data(index, ItemDataRoles::ResultHighlightForegroundColor).value<QColor>();
        const QColor highlightForeground = Qt::red;
        highlightOpt.palette.setColor(QPalette::Text, highlightForeground);
        QItemDelegate::drawDisplay(painter, highlightOpt, resultHighlightRect, textHighlight);

        // Text after the Highlight
        noHighlightOpt.rect = afterHighlightRect;
        QItemDelegate::drawDisplay(painter, noHighlightOpt, afterHighlightRect, textAfter);
    }
    else
    {
        QItemDelegate::drawDisplay(painter, option, rect, text.replace(QLatin1Char('\t'), m_tabString));
        return;
    }
}
