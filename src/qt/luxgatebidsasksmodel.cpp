#include "luxgatebidsasksmodel.h"

#include "luxgategui_global.h"
#include "bitmexnetwork.h"

#include <QColor>
#include <QBrush>

extern BitMexNetwork * bitMexNetw;

LuxgateBidsAsksModel::LuxgateBidsAsksModel(bool bBids, const Luxgate::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals),
      bLeft(bBids),
      bBids(bBids)
{
    if(bBids)
        connect(bitMexNetw, &BitMexNetwork::sigOrderBookBidsData,
                this, &LuxgateBidsAsksModel::slotUpdateData);
    else
        connect(bitMexNetw, &BitMexNetwork::sigOrderBookAsksData,
                this, &LuxgateBidsAsksModel::slotUpdateData);
}

bool LuxgateBidsAsksModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || (row > luxgateData.size() && luxgateData.size()!= 0))
        return false;
    beginInsertRows(parent,row, row+count-1);
    endInsertRows();
    return true;
}

bool LuxgateBidsAsksModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || row+count > luxgateData.size())
        return false;
    beginRemoveRows(parent,row, row+count-1);
    endRemoveRows();
    return true;
}

void LuxgateBidsAsksModel::slotUpdateData(const LuxgateOrderBookData & luxgateData_)
{
    if(luxgateData_.size() != rowCount())
    {
        removeRows(0, rowCount());
        insertRows(0, luxgateData_.size());
    }
    luxgateData = luxgateData_;
    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Qt::DisplayRole});
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
    if (parent.isValid())
        return 0;
    else
        return luxgateData.size();
}

int LuxgateBidsAsksModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    else
        return NColumns;
}

QVariant LuxgateBidsAsksModel::data(const QModelIndex &index, int role) const
{
    QVariant res;

    if (index.isValid()) {
        if(Luxgate::BidAskRole == role)
            res =  bBids;
        else if(Luxgate::CopyRowRole == role)
            res =   "Price: " + data(this->index(index.row(), columnNum(PriceColumn))).toString()
                    + " Base: "  + data(this->index(index.row(), columnNum(BaseColumn))).toString()
                    + " Quote: "  + data(this->index(index.row(), columnNum(QuoteColumn))).toString();
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role)
        {
            if (columnNum(PriceColumn) == index.column()) {
                res = QString::number(luxgateData[index.row()].dbPrice, 'f',decimals.price);
            }
            else if (columnNum(BaseColumn) == index.column()) {
                res = QString::number(luxgateData[index.row()].dbSize/luxgateData[index.row()].dbPrice, 'f',decimals.base);
            }
            else if (columnNum(QuoteColumn) == index.column()) {
                res = QString::number(luxgateData[index.row()].dbSize, 'f',decimals.quote);
            }
        }
    }
    return res;
}

