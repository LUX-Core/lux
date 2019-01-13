#include "luxgateconfigmodel.h"

LuxgateConfigModel::LuxgateConfigModel(QObject *parent):
        QAbstractTableModel(parent)
{

}

int LuxgateConfigModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return items.size();
}

QVariant LuxgateConfigModel::data(int iRow, int iColumn, int role)
{
    return data(index(iRow, iColumn), role);
}

QVariant LuxgateConfigModel::data(const QModelIndex &index, int role) const
{
    QVariant res;
    if(role == Qt::EditRole || role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case TickerColumn:
                res = QString::fromStdString(items[index.row()].ticker);
            break;
            case HostColumn:
                res = QString::fromStdString(items[index.row()].host);
            break;
            case PortColumn:
                res = items[index.row()].port;
            break;
            case RpcuserColumn:
                res = QString::fromStdString(items[index.row()].rpcuser);
            break;
            case RpcpasswordColumn:
                res = QString::fromStdString(items[index.row()].rpcpassword);
            break;
            case Zmq_pub_raw_tx_endpointColumn:
                res = QString::fromStdString(items[index.row()].zmq_pub_raw_tx_endpoint);
            break;
        }
    }
    else if(role == AllDataRole)
        res = QVariant::fromValue(items[index.row()]);
    else
        return QVariant();
    return res;
}

bool LuxgateConfigModel::setData(int iRow,int iColumn, const QVariant &value, int role)
{
    return setData(index(iRow, iColumn), value, role);
}

bool LuxgateConfigModel::insertRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || (row > items.size() && items.size()!= 0))
        return false;
    beginInsertRows(parent,row, row+count-1);
    for(int i=row; i<row+count; i++)
    {
        items.insert(i, BlockchainConfig());
    }
    endInsertRows();
    return true;
}

bool LuxgateConfigModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if(row < 0 || row+count > items.size())
        return false;
    beginRemoveRows(parent,row, row+count-1);
    for(int i=0; i<count; i++)
    {
        items.removeAt(row);
    }
    endRemoveRows();
    return true;
}

int LuxgateConfigModel::columnCount(const QModelIndex &parent) const
{
    return NColumns;
}

bool LuxgateConfigModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(index.row() >= items.size() || index.row() < 0)
        return QAbstractTableModel::setData(index,value,role);

    if(role == Qt::EditRole || role == Qt::DisplayRole)
    {
        switch (index.column())
        {
            case TickerColumn:
                items[index.row()].ticker = value.toString().toStdString();
            case HostColumn:
                items[index.row()].host = value.toString().toStdString();
            case PortColumn:
                items[index.row()].port = value.toInt();
            case RpcuserColumn:
                items[index.row()].rpcuser = value.toString().toStdString();
            case RpcpasswordColumn:
                items[index.row()].rpcpassword = value.toString().toStdString();
            case Zmq_pub_raw_tx_endpointColumn:
                items[index.row()].zmq_pub_raw_tx_endpoint = value.toString().toStdString();
        }
    }
    else if(role == AllDataRole)
        items[index.row()] = qvariant_cast<BlockchainConfig>(value);
    else
        return QAbstractTableModel::setData(index,value,role);

    emit dataChanged(index, index, {role});
    return true;
}
