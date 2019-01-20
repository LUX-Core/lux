#include "luxgatebidsmodel.h"
#include "luxgatedialog.h"

LuxgateBidsModel::LuxgateBidsModel(const OptionsModel::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals)
{

}


void LuxgateBidsModel::slotSetDecimals(const OptionsModel::Decimals & decimals_)
{
    decimals = decimals_;
    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Qt::DisplayRole});
}

QVariant LuxgateBidsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant res;
    if (Qt::Horizontal == orientation
            &&
        Qt::DisplayRole == role)
    {
        switch(section)
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

int LuxgateBidsModel::rowCount(const QModelIndex &parent) const
{
    // FIXME: Implement me!
    Q_UNUSED(parent);
    return 2;
}

int LuxgateBidsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return NColumns;
}

QVariant LuxgateBidsModel::data(const QModelIndex &index, int role) const
{
    QVariant res;

    if (index.isValid()) {
        if (Qt::ForegroundRole == role) {
            if (PriceColumn == index.column()) {
                QColor col;
                col.setNamedColor("#267E00");
                res = QBrush(col);
            }
        }
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role)
        {
            if (PriceColumn == index.column()) {
                res = QString::number(0.00001234 + 0.000001 * index.row(), 'f',decimals.price);
            }
            else if (BaseColumn == index.column()) {
                res = QString::number(104.2354 + 3 * index.row(), 'f',decimals.base);
            }
            else if (QuoteColumn == index.column()) {
                res = QString::number(0.00024512 + 3 * index.row(), 'f',decimals.quote);
            }
        }
    }
    // FIXME: Implement me!
    return res;
}

bool LuxgateBidsModel::insertRows(int row, int count, const QModelIndex &parent)
{
    beginInsertRows(parent, row, row + count - 1);
    // FIXME: Implement me!
    endInsertRows();
}

bool LuxgateBidsModel::removeRows(int row, int count, const QModelIndex &parent)
{
    beginRemoveRows(parent, row, row + count - 1);
    // FIXME: Implement me!
    endRemoveRows();
}

