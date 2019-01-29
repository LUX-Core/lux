#include "luxgatepricedelegate.h"
#include "luxgategui_global.h"

#include "math.h"

#include <QPainter>
#include <QModelIndex>
#include <QTextDocument>
#include <QRegExp>

LuxgatePriceDelegate::LuxgatePriceDelegate(QObject *parent):
    QStyledItemDelegate(parent)
{

}

//if bBid==true price of bid, else price of ask
QString LuxgatePriceDelegate::formattedText(QString text, bool bBid) const
{
    QString color;
    if(bBid)
        color =  "#267E00";
    else
        color =  "red";

    bool bDouble = true;
    text.toDouble(&bDouble);

    QRegExp rx("[1-9]");
    int  firstNumber = text.indexOf(rx) ;

    if(-1 == firstNumber||
       !text.startsWith("0") ||
       !bDouble)
    {
        return "<font color=\"" + color + "\">" + text + "</font>";
    }

    QString nullPart = "0.";
    for(int i=1; i<firstNumber; i++)
        nullPart +="0";
    QString numberPart = text.mid(firstNumber);


    return nullPart + "<font color=\"" + color + "\">" + numberPart + "</font>";
}

void LuxgatePriceDelegate::paint(QPainter* painter, const QStyleOptionViewItem & option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 options = option;
    initStyleOption(&options, index);

    painter->save();

    bool bBid = index.data(Luxgate::BidAskRole).toBool();
    QString formatText = formattedText(options.text, bBid);

    QTextDocument doc;
    doc.setHtml(formatText);

    options.text = "";
    options.widget->style()->drawControl(QStyle::CE_ItemViewItem, &options, painter);

    painter->translate(options.rect.left(), options.rect.top());
    QRect clip(0, 0, options.rect.width(), options.rect.height());
    doc.drawContents(painter, clip);

    painter->restore();
}

QSize LuxgatePriceDelegate::sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const
{
    QStyleOptionViewItemV4 options = option;
    initStyleOption(&options, index);

    QTextDocument doc;
    doc.setHtml(options.text);
    doc.setTextWidth(options.rect.width());
    return QSize(doc.idealWidth(), doc.size().height());
}
