#include "luxgatebidsasksmodel.h"

#include "luxgategui_global.h"
#include "bitmexnetwork.h"

#include <QColor>
#include <QBrush>

#include "math.h"

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

void LuxgateBidsAsksModel::slotGroup(bool bGroup_, double dbStepGroup_)
{
    bGroup = bGroup_;
    dbStepGroup = dbStepGroup_;
    groupData();
}

void LuxgateBidsAsksModel::groupData()
{
    int oldRowCount = rowCount();
    if(luxgateData.size() == 0)
        return;
    if(!bGroup)
        luxgateDataGroup = luxgateData;
    else {
        luxgateDataGroup.clear();
        qint64 groupFactor = 0;
        if(bBids)
            groupFactor = floor(luxgateData[0].dbPrice / dbStepGroup);
        else
            groupFactor = ceil(luxgateData[0].dbPrice / dbStepGroup);
        LuxgateOrderBookRow rowGroup;
        rowGroup.dbPrice = groupFactor*dbStepGroup;
        int iLastAdded = 0;
        for(int i=0; i<luxgateData.size(); i++) {
            qint64 newFactor = 0;
            if(bBids)
                newFactor = floor(luxgateData[i].dbPrice / dbStepGroup);
            else
                newFactor = ceil(luxgateData[i].dbPrice / dbStepGroup);
            if(groupFactor != newFactor) {
                luxgateDataGroup.append(rowGroup);
                groupFactor = newFactor;
                rowGroup.dbPrice = groupFactor*dbStepGroup;
                rowGroup.dbSize = 0.f;
                iLastAdded = i;
            }
            rowGroup.dbSize += luxgateData[i].dbSize;
        }
        if(luxgateData.size()-1 != iLastAdded)
            luxgateDataGroup.append(rowGroup);
    }
    if(rowCount() != oldRowCount)
    {
        removeRows(0, oldRowCount);
        insertRows(0, rowCount());
    }
    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Qt::DisplayRole});
}

void LuxgateBidsAsksModel::setRowsDisplayed (int RowsDisplayed_)
{
    int oldRowCount = rowCount();
    nRowsDisplayed = RowsDisplayed_;
    if(rowCount() != oldRowCount)
    {
        removeRows(0, oldRowCount);
        insertRows(0, rowCount());
    }
    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Qt::DisplayRole});
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
    luxgateData = luxgateData_;
    groupData();
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
    bGroup = false;
    dbStepGroup = 0.f;
    groupData();
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
                res = tr("PRICE (")+ curCurrencyPair.baseCurrency + "/" + curCurrencyPair.quoteCurrency + ")";
                break;
            case SizeColumn:
                res = tr("SIZE (") + curCurrencyPair.quoteCurrency + ")";
                break;
            case TotalColumn:
                res = tr("TOTAL (") + curCurrencyPair.quoteCurrency + ")";
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
        return qMin(luxgateDataGroup.size(), nRowsDisplayed);
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
                    + " Size: "  + data(this->index(index.row(), columnNum(SizeColumn))).toString()
                    + " Total: "  + data(this->index(index.row(), columnNum(TotalColumn))).toString();
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role)
        {
            if (columnNum(PriceColumn) == index.column()) {
                res = QString::number(luxgateDataGroup[index.row()].dbPrice, 'f',decimals.price);
            }
            else if (columnNum(SizeColumn) == index.column()) {
                res = QString::number(luxgateDataGroup[index.row()].dbSize, 'f',decimals.quote);
            }
            else if (columnNum(TotalColumn) == index.column()) {
                double dbTotal = 0;
                for(int i=0; i<=index.row(); i++) {
                    dbTotal +=  luxgateDataGroup[i].dbSize;
                }
                res = QString::number(dbTotal, 'f',decimals.quote);
            }
        }
    }
    return res;
}

