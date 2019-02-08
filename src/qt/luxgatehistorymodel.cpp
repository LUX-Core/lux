#include "luxgatehistorymodel.h"

#include "luxgategui_global.h"
#include "bitmexnetwork.h"

#include <QColor>
#include <QBrush>
#include <QDateTime>

#include "math.h"

extern BitMexNetwork * bitMexNetw;
LuxgateHistoryModel::LuxgateHistoryModel(const Luxgate::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals)
{
    connect(bitMexNetw, &BitMexNetwork::sigGlobalHistoryData,
            this, &LuxgateHistoryModel::slotUpdateData);
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
                res = tr("DATE");
                break;
            case PriceColumn:
                res = tr("PRICE (")+ curCurrencyPair.baseCurrency + "/" + curCurrencyPair.quoteCurrency + ")";
                break;
            case SizeColumn:
                res = tr("SIZE (") + curCurrencyPair.quoteCurrency + ")";
                break;
            case SideColumn:
                res = tr("SIDE");
                break;
            case TickColumn:
                res = "";
                break;
        }
    }
    return res;
}

void LuxgateHistoryModel::setRowsDisplayed (int RowsDisplayed_)
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
        return qMin(luxgateData.size(), nRowsDisplayed);
}

int LuxgateHistoryModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    else
        return NColumns;
}

bool LuxgateHistoryModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || (row > luxgateData.size() && luxgateData.size()!= 0))
        return false;
    beginInsertRows(parent,row, row+count-1);
    endInsertRows();
    return true;
}

bool LuxgateHistoryModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || row+count > luxgateData.size())
        return false;
    beginRemoveRows(parent,row, row+count-1);
    endRemoveRows();
    return true;
}

void LuxgateHistoryModel::slotUpdateData(const LuxgateHistoryData & luxgateData_)
{
    int oldRowCount = rowCount();
    removeRows(0, oldRowCount);
    luxgateData = luxgateData_;
    insertRows(0, rowCount());

    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Qt::DisplayRole});
}

QVariant LuxgateHistoryModel::data(const QModelIndex &index, int role) const
{
    QVariant res;

    if (index.isValid()) {
        if (Luxgate::BidAskRole == role)
            return luxgateData[index.row()].bBuy;
        else if (Luxgate::CopyRowRole == role)
            res = "Price: " + data(this->index(index.row(), PriceColumn)).toString()
                  + " Size: " + data(this->index(index.row(), SizeColumn)).toString()
                  + " Date: " + data(this->index(index.row(), DateColumn)).toString();
        else if (Qt::ToolTipRole == role) {
            if (DateColumn == index.column())
                res = luxgateData[index.row()].dateTime.toString("yyyy-MM-dd HH:mm:ss.zzz");
        }
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role)
        {
            if (DateColumn == index.column())
                res = luxgateData[index.row()].dateTime.toString("hh:mm:ss");
            else if (SideColumn == index.column()) {
                if(luxgateData[index.row()].bBuy) {
                    QString color =  "#267E00";
                    res = "<font color=\"" + color + "\">" + "<b>Buy</b>" + "</font>";
                }
                else {
                    QString color =  "red";
                    res = "<font color=\"" + color + "\">" + "<b>Sell</b>" + "</font>";
                }
            }
            else if (TickColumn == index.column()) {
                QString color;
                if(luxgateData[index.row()].bBuy)
                    color =  "#267E00";
                else
                    color =  "red";
                if(1 == luxgateData[index.row()].arrayDirection)
                    res = "<font color=\"" + color + "\">" + "<b>&#8593;</b>" + "</font>";
                else if(-1 == luxgateData[index.row()].arrayDirection)
                    res = "<font color=\"" + color + "\">" + "<b>&#8595;</b>" + "</font>";
            }
            else if (PriceColumn == index.column()) {
                res = QString::number(luxgateData[index.row()].dbPrice, 'f',decimals.price);
            }
            else if (SizeColumn == index.column()) {
                res = QString::number(luxgateData[index.row()].dbSize, 'f',decimals.quote);
            }
        }
    }
    return res;
}
