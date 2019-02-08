#include "luxgateopenordersmodel.h"

#include "luxgategui_global.h"
#include "luxgate/luxgate.h"
#include "guiutil.h"
#include "bitmexnetwork.h"

#include <algorithm>

#include <QColor>
#include <QBrush>
#include <QDateTime>

extern BitMexNetwork * bitMexNetw;

LuxgateOpenOrdersModel::LuxgateOpenOrdersModel(const Luxgate::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals)
{
    //update();
    //orderbook.subscribeOrdersChange(std::bind(&LuxgateOpenOrdersModel::update, this));

    connect(bitMexNetw, &BitMexNetwork::sigOpenOrdersData,
            this, &LuxgateOpenOrdersModel::slotUpdateData);
}

/*void LuxgateOpenOrdersModel::update()
{
    openOrders.clear();
    for( auto it = orderbook.Orders().begin(); it != orderbook.Orders().end(); ++it ) {
        openOrders.push_back( it->second );
    }
    std::sort(openOrders.begin(), openOrders.end(),

              [](std::shared_ptr<COrder> a, std::shared_ptr<COrder> b)
              { return a->OrderCreationTime() > b->OrderCreationTime();});
    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Luxgate::BidAskRole, Qt::DisplayRole});
}*/

void LuxgateOpenOrdersModel::slotUpdateData(const LuxgateOpenOrdersData & luxgateData_)
{
    int oldRowCount = rowCount();

    removeRows(0, oldRowCount);
    luxgateData = luxgateData_;
    insertRows(0, rowCount());

    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Qt::DisplayRole});
    emit sigUpdateButtons();
}


bool LuxgateOpenOrdersModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || (row > rowCount() && rowCount()!= 0))
        return false;
    beginInsertRows(parent,row, row+count-1);
    endInsertRows();
    return true;
}

bool LuxgateOpenOrdersModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || row+count > rowCount())
        return false;
    beginRemoveRows(parent,row, row+count-1);
    endRemoveRows();
    return true;
}

Qt::ItemFlags LuxgateOpenOrdersModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable |  Qt::ItemIsEnabled;
}

void LuxgateOpenOrdersModel::slotSetDecimals(const Luxgate::Decimals & decimals_)
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
                res = tr("DATE");
                break;
            case TypeColumn:
                res = tr("TYPE");
                break;
            case PriceColumn:
                res = tr("PRICE");
                break;
            case QtyColumn:
                res = tr("QTY");
                break;
            case RemainQtyColumn:
                res = tr("REMAINING");
                break;
            case ValueColumn:
                res = tr("VALUE");
                break;
            case StatusColumn:
                res = tr("STATUS");
                break;
            case CancelColumn:
                res = tr("CANCEL");
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
        return luxgateData.size();
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
        if (Luxgate::BidAskRole == role)
            res = luxgateData[index.row()].bBuy;
        else if(Luxgate::CopyRowRole == role) {
            QString side;
            if(luxgateData[index.row()].bBuy)
                side = QString("Buy");
            else
                side = QString("Sell");
            res = tr("Price: ") + data(this->index(index.row(), PriceColumn)).toString()
                  + tr(" Type: ") + data(this->index(index.row(), TypeColumn)).toString()
                  + tr(" Side: ") + side
                  + tr(" Qty: ") + data(this->index(index.row(), QtyColumn)).toString()
                  + tr(" Remaining: ") + data(this->index(index.row(), RemainQtyColumn)).toString()
                  + tr(" Value: ") + data(this->index(index.row(), ValueColumn)).toString()
                  + tr(" Status: ") + data(this->index(index.row(), StatusColumn)).toString();
        }
        else if(OrderIdRole == role)
            res = luxgateData[index.row()].orderId;
        else if (Qt::ToolTipRole == role) {
            if (DateColumn == index.column())
                res = luxgateData[index.row()].dateTimeOpen.toString("yyyy-MM-dd HH:mm:ss.zzz");
        }
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role) {
            if (DateColumn == index.column())
                res = luxgateData[index.row()].dateTimeOpen.toString("hh:mm:ss");
            else if (TypeColumn == index.column())
                res = luxgateData[index.row()].type;
            else if (PriceColumn == index.column())
                res = QString::number(luxgateData[index.row()].dbPrice, 'f',decimals.price);
            else if (QtyColumn == index.column())
                res = QString::number(luxgateData[index.row()].dbQty, 'f',decimals.quote);
            else if (RemainQtyColumn == index.column())
                res = QString::number(luxgateData[index.row()].dbLeavesQty, 'f',decimals.quote);
            else if (ValueColumn == index.column())
                res = QString::number(luxgateData[index.row()].dbValue, 'f',decimals.base);
            else if (StatusColumn == index.column())
                res = luxgateData[index.row()].status;
        }
    }
    return res;
}
