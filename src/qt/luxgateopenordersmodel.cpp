#include "luxgateopenordersmodel.h"

#include "luxgategui_global.h"
#include "luxgate/luxgate.h"
#include "guiutil.h"

#include <algorithm>

#include <QColor>
#include <QBrush>
#include <QDateTime>

LuxgateOpenOrdersModel::LuxgateOpenOrdersModel(const Luxgate::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals)
{
    update();
}

void LuxgateOpenOrdersModel::update()
{
    openOrders.clear();
    for( auto it = orderbook.Orders().begin(); it != orderbook.Orders().end(); ++it ) {
        openOrders.push_back( it->second );
    }
    std::sort(openOrders.begin(), openOrders.end(),

              [](std::shared_ptr<COrder> a, std::shared_ptr<COrder> b)
              { return a->OrderCreationTime() > b->OrderCreationTime();});
    emit dataChanged(index(0,0), index(rowCount()-1,columnCount()-1), {Luxgate::BidAskRole, Qt::DisplayRole});
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
                res = "DATE";
                break;
            case TypeColumn:
                res = "TYPE";
                break;
            case SideColumn:
                res = "SIDE";
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
            case CancelColumn:
                res = "CANCEL";
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
        return openOrders.size();
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
        auto order = openOrders[index.row()];

        if (Luxgate::BidAskRole == role)
            return order->IsBid();
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role) {

            if (DateColumn == index.column()) {
                auto createTime = order->OrderCreationTime();
                res = GUIUtil::dateTimeStr(createTime);
            }
            else if (TypeColumn == index.column()) {
                res = QString("limit");
            }
            else if (SideColumn == index.column()) {
                if(order->IsAsk())
                    res = QString("Sell");
                if(order->IsBid())
                    res = QString("Buy");
            }
            else if (PriceColumn == index.column()) {
                res = QString::number(order->Price(), 'f',decimals.price);
            }
            else if (BaseAmountColumn == index.column()) {
                res = QString::number(order->Amount(), 'f',decimals.base);
            }
            else if (QuoteTotalColumn == index.column()) {
                res = QString::number(order->Total(), 'f',decimals.quote);
            }
        }
    }
    return res;
}
