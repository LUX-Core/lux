// Copyright (c) 2019 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "luxgatehistorymodel.h"

#include "luxgategui_global.h"
#include "luxgate/luxgate.h"

#include <QColor>
#include <QBrush>

static void UpdateHistory(LuxgateHistoryModel* model, const OrderId id)
{
    Q_UNUSED(id);
    QMetaObject::invokeMethod(model, "updateTable", Qt::QueuedConnection);
}

LuxgateHistoryModel::LuxgateHistoryModel(const Luxgate::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals)
{
    qRegisterMetaType<OrderView>();

    std::map<OrderId, OrderPtr> history;
    if (!luxgate.OrderBook().LoadHistory(history)) {
        LogPrintf("Failed to update LuxgateHistoryModel");
        return;
    }
    beginInsertRows(QModelIndex(), 0, 0);
    for (const auto& it : history) {
        OrderView view(it.second);
        LogPrintf("LuxgateOpenOrdersModel add %s\n", view.log());
        historyOrders.push_front(view);
    }
    endInsertRows();

    luxgate.OrderBook().OrderDeleted.connect(std::bind(&UpdateHistory, this, std::placeholders::_1));
}

void LuxgateHistoryModel::updateTable()
{
    LogPrintf("LuxgateHistoryModel: updateTable");
    beginResetModel();
    historyOrders.clear();
    endResetModel();
    std::map<OrderId, OrderPtr> history;
    if (!luxgate.OrderBook().LoadHistory(history)) {
        LogPrintf("Failed to update LuxgateHistoryModel");
        return;
    }
    beginInsertRows(QModelIndex(), 0, 0);
    for (const auto& it : history) {
        OrderView view(it.second);
        LogPrintf("LuxgateOpenOrdersModel add %s\n", view.log());
        historyOrders.push_front(view);
    }
    endInsertRows();
}

QVariant LuxgateHistoryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant res;
    if (Qt::Horizontal == orientation && Qt::DisplayRole == role) {
        switch(section) {
            case IdColumn:
                res = "ID";
                break;
            case CompleteDateColumn:
                res = "COMPLETION DATE";
                break;
            case TypeColumn:
                res = "TYPE";
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
            case StateColumn:
                res = "STATE";
                break;
        }
    }
    return res;
}

void LuxgateHistoryModel::slotSetDecimals(const Luxgate::Decimals & decimals_)
{
    decimals = decimals_;
    emit dataChanged(index(0,0), index(rowCount() - 1, columnCount() - 1), { Qt::DisplayRole });
}

int LuxgateHistoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    else
        return historyOrders.size();
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

    if (!index.isValid())
        return res;

    auto it = historyOrders.cbegin();
    std::advance(it, index.row());
    auto order = *it;

    if (Luxgate::BidAskRole == role)
        res = order.isBuy;
    else if(Luxgate::CopyRowRole == role)
        res = "Price: " + data(this->index(index.row(), PriceColumn)).toString() +
              " ID: " + data(this->index(index.row(), IdColumn)).toString() +
              " Base: "  + data(this->index(index.row(), BaseAmountColumn)).toString() +
              " Quote: "  + data(this->index(index.row(), QuoteTotalColumn)).toString() +
              " Date: " + data(this->index(index.row(), CompleteDateColumn)).toString() +
              " State: " + data(this->index(index.row(), StateColumn)).toString();
    else if (Qt::EditRole == role || Qt::DisplayRole == role) {
        if (CompleteDateColumn == index.column())
            res = order.sCreationTime;
        else if (TypeColumn == index.column())
            res = order.sType;
        else if (PriceColumn == index.column())
            res = order.sPrice;
        else if (BaseAmountColumn == index.column())
            res = order.sAmount;
        else if (QuoteTotalColumn == index.column())
            res = order.sTotal;
        else if (StateColumn == index.column())
            res = order.sState;
        else if (IdColumn == index.column())
            res = order.sId;
    }
    return res;
}

