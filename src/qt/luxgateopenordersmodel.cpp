#include "luxgateopenordersmodel.h"

#include "luxgategui_global.h"
#include "guiutil.h"

#include <algorithm>

#include <QColor>
#include <QBrush>
#include <QDateTime>
#include <QVector>
#include <QDebug>


static void AddRow(LuxgateOpenOrdersModel* model, std::shared_ptr<COrder> order)
{
    OrderView view(order);
    QMetaObject::invokeMethod(model, "addRow", Qt::QueuedConnection, Q_ARG(OrderView, view));
}

static void UpdateRow(LuxgateOpenOrdersModel* model, const OrderId orderId, const COrder::State state)
{
    QMetaObject::invokeMethod(model, "updateRow", Qt::QueuedConnection, Q_ARG(quint64, (quint64)orderId), Q_ARG(int, state));
}
static void DeleteRow(LuxgateOpenOrdersModel* model, const OrderId orderId)
{
    QMetaObject::invokeMethod(model, "deleteRow", Qt::QueuedConnection, Q_ARG(quint64, (quint64)orderId));
}

static void ResetOrderBook(LuxgateOpenOrdersModel* model)
{
    QMetaObject::invokeMethod(model, "reset", Qt::QueuedConnection);
}

OrderView::OrderView(const std::shared_ptr<const COrder> order)
{
    id = order->ComputeId();
    sId = QString::fromStdString(OrderIdToString(id));
    sState = stateToString(order->GetState());
    creationTime = order->OrderCreationTime();
    sCreationTime = GUIUtil::dateTimeStr(creationTime);
    sType = order->IsSell() ? QString("Sell") : QString("Buy");
    isBuy = order->IsBuy();
    sPrice = QString(Luxgate::QStrFromAmount(order->Price()));
    sAmount = QString(Luxgate::QStrFromAmount(order->Amount()));
    sTotal = QString(Luxgate::QStrFromAmount(order->Total()));
}

LuxgateOpenOrdersModel::LuxgateOpenOrdersModel(const Luxgate::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals)
{
    qRegisterMetaType<QVector<int>>();
    qRegisterMetaType<OrderView>();

    beginInsertRows(QModelIndex(), 0, 0);
    auto orders = luxgate.OrderBook().ConstRefOrders();
    for (auto it = orders.begin(); it != orders.end(); ++it) {
        OrderView view(it->second);
        LogPrintf("LuxgateOpenOrdersModel add %s\n", view.log());
        openOrders.push_front(view);
    }
    endInsertRows();

    luxgate.OrderBook().OrderChanged.connect(std::bind(&UpdateRow, this, std::placeholders::_1, std::placeholders::_2));
    luxgate.OrderBook().OrderAdded.connect(std::bind(&AddRow, this, std::placeholders::_1));
    luxgate.OrderBook().OrderDeleted.connect(std::bind(&DeleteRow, this, std::placeholders::_1));
    luxgate.OrderBook().OrderBookCleared.connect(std::bind(&ResetOrderBook, this));
}

void LuxgateOpenOrdersModel::addRow(const OrderView order)
{
    LogPrintf("LuxgateOpenOrdersModel addRow %s\n", order.log());
    beginInsertRows(QModelIndex(), 0, 0);
    openOrders.push_front(order);
    endInsertRows();
    emit rowAdded();
}

void LuxgateOpenOrdersModel::deleteRow(const quint64 id)
{
    LogPrintf("LuxgateOpenOrdersModel deleteRow %s\n", OrderIdToString(id));
    int i = 0;
    for (auto it = openOrders.cbegin(); it != openOrders.cend(); it++) {
        if ((*it).id == id) {
            beginRemoveRows(QModelIndex(), i, i);
            openOrders.erase(it);
            endRemoveRows();
            break;
        }
        ++i;
    }
}

void LuxgateOpenOrdersModel::updateRow(const quint64 id, const int state)
{
    QString ss = stateToString(static_cast<COrder::State>(state));
    LogPrintf("LuxgateOpenOrdersModel updateRow %s %s\n", OrderIdToString(id), ss.toStdString());
    int i = 0;
    for (auto it = openOrders.begin(); it != openOrders.end(); it++) {
        auto order = *it;
        if (order.id == id) {
            (*it).sState = ss;
            auto stateCell = index(i, StateColumn);
            emit dataChanged(stateCell, stateCell, { Luxgate::BidAskRole, Qt::DisplayRole });
            return;
        }
        ++i;
    }
    LogPrintf("LuxgateOpenOrdersModel::updateRow : Warning: Got update event, but entry is not in model\n");
}

void LuxgateOpenOrdersModel::reset()
{
    LogPrintf("LuxgateOpenOrdersModel reset\n");
    beginResetModel();
    openOrders.clear();
    endResetModel();
}

Qt::ItemFlags LuxgateOpenOrdersModel::flags(const QModelIndex &index) const
{
    return Qt::ItemIsSelectable |  Qt::ItemIsEnabled;
}

void LuxgateOpenOrdersModel::slotSetDecimals(const Luxgate::Decimals & decimals_)
{
    decimals = decimals_;
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), { Qt::DisplayRole });
}

QVariant LuxgateOpenOrdersModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant res;
    if (Qt::Horizontal == orientation && Qt::DisplayRole == role) {
        switch(section) {
            case IdColumn:
                res = "ID";
                break;
            case DateColumn:
                res = "DATE";
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
            case CancelColumn:
                res = "CANCEL";
                break;
            case StateColumn:
                res = "STATE";
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
        auto it = openOrders.cbegin();
        std::advance(it, index.row());
        auto order = *it;
        
        if (Luxgate::BidAskRole == role)
            res = order.isBuy;

        else if(Luxgate::CopyRowRole == role)
            res =   "Price: " + data(this->index(index.row(), PriceColumn)).toString()
                    + " ID: " + data(this->index(index.row(), IdColumn)).toString()
                    + " Base: "  + data(this->index(index.row(), BaseAmountColumn)).toString()
                    + " Quote: "  + data(this->index(index.row(), QuoteTotalColumn)).toString()
                    + " Date: " + data(this->index(index.row(), DateColumn)).toString()
                    + " State: " + data(this->index(index.row(), StateColumn)).toString();

        else if (Qt::EditRole == role || Qt::DisplayRole == role) {

            if (DateColumn == index.column()) {
                res = order.sCreationTime;
            }
            else if (TypeColumn == index.column()) {
                res = order.sType;
            }
            else if (PriceColumn == index.column()) {
                res = order.sPrice;
            }
            else if (BaseAmountColumn == index.column()) {
                res = order.sAmount;
            }
            else if (QuoteTotalColumn == index.column()) {
                res = order.sTotal;
            }
            else if (StateColumn == index.column()) {
                res = order.sState;
            }
            else if (IdColumn == index.column()) {
                res = order.sId;
            }
        }
    }
    return res;
}

QString stateToString(const COrder::State state)
{
    switch (state) {
        case COrder::State::INVALID: return QString("Invalid");
        case COrder::State::NEW: return QString("New");
        case COrder::State::MATCH_FOUND: return QString("Match found");
        case COrder::State::SWAP_REQUESTED: return QString("Swap requested");
        case COrder::State::SWAP_ACK: return QString("Swap acknowledgement sent");
        case COrder::State::CONTRACT_CREATED: return QString("Contract created");
        case COrder::State::CONTRACT_ACK: return QString("Contract aknowledgement sent");
        case COrder::State::REFUNDING: return QString("Refunding");
        case COrder::State::CONTRACT_VERIFYING: return QString("Verifying contract");
        case COrder::State::CONTRACT_ACK_VERIFYING: return QString("Verifying contract acknowledgement");
        case COrder::State::COMPLETE: return QString("Completed");
        default: return QString("Unknown");
    };
}

