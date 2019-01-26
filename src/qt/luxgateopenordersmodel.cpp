#include "luxgateopenordersmodel.h"
#include "luxgategui_global.h"

#include <QColor>
#include <QBrush>

LuxgateOpenOrdersModel::LuxgateOpenOrdersModel(const OptionsModel::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals)
{
}

Qt::ItemFlags LuxgateOpenOrdersModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable |  Qt::ItemIsEnabled;
}

void LuxgateOpenOrdersModel::slotSetDecimals(const OptionsModel::Decimals & decimals_)
{
    decimals = decimals_;
    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Qt::DisplayRole});
}

QVariant LuxgateOpenOrdersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant res;
    if (Qt::Horizontal == orientation
        &&
        Qt::DisplayRole == role)
    {
        switch(section)
        {
            case DateColumn:
                res = "Date";
                break;
            case TypeColumn:
                res = "Type";
                break;
            case SideColumn:
                res = "Side";
                break;
            case PriceColumn:
                res = "Price";
                break;
            case BaseAmountColumn:
                res = "Amount ( " + curCurrencyPair.baseCurrency + " )";
                break;
            case QuoteTotalColumn:
                res = "Total ( " + curCurrencyPair.quoteCurrency + " )";
                break;
            case CancelColumn:
                res = "Cancel";
                break;
        }
    }
    return res;
}

int LuxgateOpenOrdersModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    else
        return 2;
    // FIXME: Implement me!
}

int LuxgateOpenOrdersModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    else
        return NColumns;
}

QVariant LuxgateOpenOrdersModel::data(const QModelIndex &index, int role) const
{
    QVariant res;

    if (index.isValid()) {
        if (Qt::ForegroundRole == role) {
            if (SideColumn == index.column()) {
                QColor col;
                if("Sell" == data(index).toString())
                    col = Qt::red;
                else
                    col.setNamedColor("#267E00");
                res = QBrush(col);
            }
        }
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role)
        {
            if (DateColumn == index.column()) {
                res = QString("12-21 19:59:10");
            }
            else if (TypeColumn == index.column()) {
                res = QString("limit");
            }
            else if (SideColumn == index.column()) {
                if(0 == index.row())
                    res = QString("Sell");
                if(1 == index.row())
                    res = QString("Buy");
            }
            else if (PriceColumn == index.column()) {
                res = QString::number(0.00001234 + 0.000001 * index.row(), 'f',decimals.price);
            }
            else if (BaseAmountColumn == index.column()) {
                res = QString::number(104.2354 + 3 * index.row(), 'f',decimals.base);
            }
            else if (QuoteTotalColumn == index.column()) {
                res = QString::number(0.00024512 + 3 * index.row(), 'f',decimals.quote);
            }
        }
    }
    // FIXME: Implement me!
    return res;
}
