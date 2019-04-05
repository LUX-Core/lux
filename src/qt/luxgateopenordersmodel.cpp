#include "luxgateopenordersmodel.h"

#include "luxgategui_global.h"
#include "guiutil.h"

#include <algorithm>

#include <QColor>
#include <QBrush>
#include <QDateTime>
#include <QVector>

LuxgateOpenOrdersModel::LuxgateOpenOrdersModel(const Luxgate::Decimals & decimals, QObject *parent)
    : QAbstractTableModel(parent),
      decimals(decimals)
{
    qRegisterMetaType<QVector<int>>();

    beginInsertRows(QModelIndex(), 0, 0);
    auto orders = orderbook.ConstRefOrders();
    for (auto it = orders.begin(); it != orders.end(); ++it) {
        LogPrintf("LuxgateOpenOrdersModel add %s\n", it->second->ToString());
        openOrders.push_back(it->second);
    }
    endInsertRows();

    std::sort(openOrders.begin(), openOrders.end(),
              [](std::shared_ptr<const COrder> a, std::shared_ptr<const COrder> b)
              { return a->OrderCreationTime() > b->OrderCreationTime(); });

    orderbook.subscribeOrdersChange(std::bind(&LuxgateOpenOrdersModel::updateRow, this, std::placeholders::_1, std::placeholders::_2));
    orderbook.subscribeOrderAdded(std::bind(&LuxgateOpenOrdersModel::addRow, this, std::placeholders::_1));
    orderbook.subscribeOrderDeleted(std::bind(&LuxgateOpenOrdersModel::deleteRow, this, std::placeholders::_1));
    orderbook.subscribeOrderbookCleared(std::bind(&LuxgateOpenOrdersModel::reset, this));
}

void LuxgateOpenOrdersModel::addRow(std::shared_ptr<COrder> order)
{
    LogPrintf("LuxgateOpenOrdersModel addRow %s\n", order->ToString());
    beginInsertRows(QModelIndex(), 0, 0);
    openOrders.insert(openOrders.begin(), order);
    endInsertRows();
}

void LuxgateOpenOrdersModel::deleteRow(const OrderId& id)
{
    LogPrintf("LuxgateOpenOrdersModel deleteRow %s\n", OrderIdToString(id));
    int i = 0;
    for (auto it = openOrders.cbegin(); it != openOrders.cend(); it++) {
        if ((*it)->ComputeId() == id) {
            beginRemoveRows(QModelIndex(), i, i);
            openOrders.erase(it);
            endRemoveRows();
            break;
        }
        ++i;
    }
}

void LuxgateOpenOrdersModel::updateRow(const OrderId& id, COrder::State state)
{
    LogPrintf("LuxgateOpenOrdersModel updateRow %s %u\n", OrderIdToString(id), (uint64_t) state);
    int i = 0;
    for (auto it = openOrders.begin(); it != openOrders.end(); it++) {
        if ((*it)->ComputeId() == id) {
            auto porder = *it;
            *it = std::make_shared<const COrder>(porder->Base(),
                                                 porder->Rel(),
                                                 porder->BaseAmount(),
                                                 porder->Price(),
                                                 porder->OrderCreationTime(),
                                                 state);
            auto stateCell = index(i, StateColumn);
            emit QAbstractItemModel::dataChanged(stateCell, stateCell, { Luxgate::BidAskRole, Qt::DisplayRole });
            break;
        }
        ++i;
    }
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
        auto order = openOrders[index.row()];

        if (Luxgate::BidAskRole == role)
            res = order->IsBuy();
        else if(Luxgate::CopyRowRole == role)
            res =   "Price: " + data(this->index(index.row(), PriceColumn)).toString()
                    + " Base: "  + data(this->index(index.row(), BaseAmountColumn)).toString()
                    + " Quote: "  + data(this->index(index.row(), QuoteTotalColumn)).toString()
                    + " Date: " + data(this->index(index.row(), DateColumn)).toString()
                    + " State: " + data(this->index(index.row(), StateColumn)).toString();
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role) {

            if (DateColumn == index.column()) {
                auto createTime = order->OrderCreationTime();
                res = GUIUtil::dateTimeStr(createTime);
            }
            else if (TypeColumn == index.column()) {
                if(order->IsSell())
                    res = QString("Sell");
                if(order->IsBuy())
                    res = QString("Buy");
            }
            else if (PriceColumn == index.column()) {
                res = QString(Luxgate::QStrFromAmount(order->Price()));
            }
            else if (BaseAmountColumn == index.column()) {
                res = QString(Luxgate::QStrFromAmount(order->Amount()));
            }
            else if (QuoteTotalColumn == index.column()) {
                res = QString(Luxgate::QStrFromAmount(order->Total()));
            }
            else if (StateColumn == index.column()) {
                res = QString(stateToString(order->GetState()));
            }
        }
    }
    return res;
}

QString LuxgateOpenOrdersModel::stateToString(const COrder::State state) const
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

