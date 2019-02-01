#include "luxgatehistorymodel.h"

#include "luxgategui_global.h"

#include <QColor>
#include <QBrush>

LuxgateHistoryModel::LuxgateHistoryModel(const Luxgate::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals)
{
}

QVariant LuxgateHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant res;
    if (Qt::Horizontal == orientation
        &&
        Qt::DisplayRole == role)
    {
        switch(section)
        {
            case DateColumn:
                res = "DATE";
                break;
            case PriceColumn:
                res = "PRICE";
                break;
            case BaseAmountColumn:
                res = "AMOUNT ( " + curCurrencyPair.baseCurrency + " )";
                break;
            case QuoteTotalColumn:
                res = "TOTAL ( " + curCurrencyPair.quoteCurrency + " )";
                break;
        }
    }
    return res;
}

void LuxgateHistoryModel::slotSetDecimals(const Luxgate::Decimals & decimals_)
{
    decimals = decimals_;
    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Qt::DisplayRole});
}

int LuxgateHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    else
        return 3;
    // FIXME: Implement me!
}

int LuxgateHistoryModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    else
        return NColumns;
}

QVariant LuxgateHistoryModel::data(const QModelIndex &index, int role) const
{
    QVariant res;

    if (index.isValid()) {
        if (Luxgate::BidAskRole == role) {
            if(index.row()%2)
                return true;
            else
                return false;
        }
        else if(Luxgate::CopyRowRole == role)
            res =   "Price: " + data(this->index(index.row(), PriceColumn)).toString()
                    + " Base: "  + data(this->index(index.row(), BaseAmountColumn)).toString()
                    + " Quote: "  + data(this->index(index.row(), QuoteTotalColumn)).toString()
                    + " Date: " + data(this->index(index.row(), DateColumn)).toString();
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role)
        {
            if (DateColumn == index.column()) {
                res = QString("12-21 19:59:10");
            }

            else if (PriceColumn == index.column()) {
                res = QString::number(0.00005234 + 0.000001 * index.row(), 'f',decimals.price);
            }
            else if (BaseAmountColumn == index.column()) {
                res = QString::number(574.2354 + 3 * index.row(), 'f',decimals.base);
            }
            else if (QuoteTotalColumn == index.column()) {
                res = QString::number(0.00034512 + 3 * index.row(), 'f',decimals.quote);
            }
        }
    }
    // FIXME: Implement me!
    return res;
}
