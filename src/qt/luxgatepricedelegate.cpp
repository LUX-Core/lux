#include "luxgatepricedelegate.h"
#include "luxgategui_global.h"

#include "math.h"

#include <QPainter>
#include <QModelIndex>
#include <QTextDocument>

LuxgatePriceDelegate::LuxgatePriceDelegate(QObject *parent):
    QStyledItemDelegate(parent)
{

}

void LuxgatePriceDelegate::paint(QPainter* painter, const QStyleOptionViewItem & option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 options = option;
    initStyleOption(&options, index);

    painter->save();

    bool bBid = index.data(Luxgate::BidAskRole).toBool();
    QString formatText = Luxgate::priceFormattedText(options.text, bBid);

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
    bool bBid = index.data(Luxgate::BidAskRole).toBool();
    doc.setHtml(Luxgate::priceFormattedText(options.text, bBid));
    doc.setTextWidth(options.rect.width());
    return QSize(doc.idealWidth(), doc.size().height());
}
