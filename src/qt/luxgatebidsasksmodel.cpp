#include "luxgatebidsasksmodel.h"
#include "luxgategui_global.h"
#include <QColor>
#include <QBrush>

LuxgateBidsAsksModel::LuxgateBidsAsksModel(bool bBids, const Luxgate::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals),
      bLeft(bBids),
      bBids(bBids)
{

}

//convert value from Columns to columnNum
int LuxgateBidsAsksModel::columnNum(Columns col) const
{
    return bLeft? NColumns - 1 -  col: col;
}

void LuxgateBidsAsksModel::setOrientation(bool bLeft_)
{
    bLeft = bLeft_;
    emit headerDataChanged(Qt::Horizontal, 0, NColumns-1);
    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Qt::DisplayRole, Qt::ForegroundRole});
}

Qt::ItemFlags LuxgateBidsAsksModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable |  Qt::ItemIsEnabled;
}

void LuxgateBidsAsksModel::slotSetDecimals(const Luxgate::Decimals & decimals_)
{
    decimals = decimals_;
    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Qt::DisplayRole});
}

QVariant LuxgateBidsAsksModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant res;
    if (Qt::Horizontal == orientation
            &&
        Qt::DisplayRole == role)
    {
        switch(columnNum(static_cast<Columns>(section)))
        {
            case PriceColumn:
                res = "PRICE ("+ curCurrencyPair.quoteCurrency + ")";
                break;
            case BaseColumn:
                res = curCurrencyPair.baseCurrency;
                break;
            case QuoteColumn:
                res = curCurrencyPair.quoteCurrency;
                break;
        }
    }
    return res;
}

int LuxgateBidsAsksModel::rowCount(const QModelIndex &parent) const
{
    // FIXME: Implement me!
    Q_UNUSED(parent);
    return 2;
}

int LuxgateBidsAsksModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return NColumns;
}

QVariant LuxgateBidsAsksModel::data(const QModelIndex &index, int role) const
{
    QVariant res;

    if (index.isValid()) {
        if (Qt::ForegroundRole == role) {
            if (columnNum(PriceColumn) == index.column()) {
                QColor col;
                if(bBids)
                    col.setNamedColor("#267E00");
                else
                    col = Qt::red;
                res = QBrush(col);
            }
        }
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role)
        {
            if (columnNum(PriceColumn) == index.column()) {
                res = QString::number(0.00001234 + 0.000001 * index.row(), 'f',decimals.price);
            }
            else if (columnNum(BaseColumn) == index.column()) {
                res = QString::number(104.2354 + 3 * index.row(), 'f',decimals.base);
            }
            else if (columnNum(QuoteColumn) == index.column()) {
                res = QString::number(0.00024512 + 3 * index.row(), 'f',decimals.quote);
            }
        }
    }
    // FIXME: Implement me!
    return res;
}

