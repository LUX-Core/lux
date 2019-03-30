#include "luxgateopenordersmodel.h"

#include "luxgategui_global.h"
#include "luxgate/luxgate.h"
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
    update();
    orderbook.subscribeOrdersChange(std::bind(&LuxgateOpenOrdersModel::update, this));
    orderbook.subscribeOrderAdded(std::bind(&LuxgateOpenOrdersModel::addRow, this, std::placeholders::_1));
}

void LuxgateOpenOrdersModel::addRow(std::shared_ptr<COrder> order)
{
    LogPrintf("LuxgateOpenOrdersModel addRow %s\n", order->ToString());
    beginInsertRows(QModelIndex(), 0, 0);
    openOrders.insert(openOrders.begin(), order);
    endInsertRows();
}

void LuxgateOpenOrdersModel::update()
{
    LogPrintf("LuxgateOpenOrdersModel UPDATE\n");

    beginResetModel();
    openOrders.clear();
    endResetModel();

    beginInsertRows(QModelIndex(), 0, 0);
    auto orders = orderbook.ConstOrders();
    for (auto it = orders.begin(); it != orders.end(); ++it) {
        LogPrintf("LuxgateOpenOrdersModel add %s\n", it->second->ToString());
        openOrders.push_back(it->second);
    }
    endInsertRows();

    std::sort(openOrders.begin(), openOrders.end(),
              [](std::shared_ptr<const COrder> a, std::shared_ptr<const COrder> b)
              { return a->OrderCreationTime() > b->OrderCreationTime(); });
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
            res = order->IsBid();
        else if(Luxgate::CopyRowRole == role)
            res =   "Price: " + data(this->index(index.row(), PriceColumn)).toString()
                    + " Base: "  + data(this->index(index.row(), BaseAmountColumn)).toString()
                    + " Quote: "  + data(this->index(index.row(), QuoteTotalColumn)).toString()
                    + " Date: " + data(this->index(index.row(), DateColumn)).toString();
        else if(Qt::EditRole == role ||
                Qt::DisplayRole == role) {

            if (DateColumn == index.column()) {
                auto createTime = order->OrderCreationTime();
                res = GUIUtil::dateTimeStr(createTime);
            }
            else if (TypeColumn == index.column()) {
                if(order->IsAsk())
                    res = QString("Sell");
                if(order->IsBid())
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
        }
    }
    return res;
}
