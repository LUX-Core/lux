#include "luxgateconfigmodel.h"

LuxgateConfigModel::LuxgateConfigModel(QObject *parent):
        QAbstractTableModel(parent)
{

}

int LuxgateConfigModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return items.size();
}

QVariant LuxgateConfigModel::data(int iRow, int iColumn, int role)
{
    return data(index(iRow, iColumn), role);
}

QVariant LuxgateConfigModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QVariant res;
    if (Qt::Horizontal == orientation
            &&
        Qt::DisplayRole == role)
    {
        switch(section)
        {
            case TickerColumn:
                res = tr("TICKER");
                break;
            case HostColumn:
                res = tr("HOST");
                break;
            case PortColumn:
                res = tr("PORT");
                break;
            case RpcuserColumn:
                res = tr("RPC USER");
                break;
            case RpcpasswordColumn:
                res = tr("RPC PASSW");
                break;
            case Zmq_pub_raw_tx_endpointColumn:
                res = tr("ZMQ");
                break;
            case SwapSupportColumn:
                res = tr("SWAP");
                break;
        }
    }
    return res;
}

QVariant LuxgateConfigModel::data(const QModelIndex &index, int role) const
{
    QVariant res;
    if (index.isValid()) {
        if (Qt::EditRole == role ||
            Qt::DisplayRole == role) {
            switch (index.column()) {
                case TickerColumn:
                    res = items[index.row()].ticker;
                    break;
                case HostColumn:
                    res = items[index.row()].host;
                    break;
                case PortColumn:
                    res = items[index.row()].port;
                    break;
                case RpcuserColumn:
                    res = items[index.row()].rpcuser;
                    break;
                case RpcpasswordColumn:
                    res = items[index.row()].rpcpassword;
                    break;
                case Zmq_pub_raw_tx_endpointColumn:
                    res = items[index.row()].zmq_pub_raw_tx_endpoint;
                    break;
                case SwapSupportColumn:
                    res = QVariant();
                    break;
            }
        } else if (Qt::BackgroundRole == role)
            res = backgroundBrushs[index.row()][index.column()];
        else if (AllDataRole == role)
            res = QVariant::fromValue(items[index.row()]);
        else if (ValidRole == role)
            res = validItems[index.row()][index.column()];
        else
            res = QVariant();
    }
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
    for(int iR=row; iR<row+count; iR++)
    {
        items.insert(iR, BlockchainConfig());
        for(int iC=0; iC<columnCount(); iC++)
        {
            QColor col;
            col.setNamedColor("#fcfcfc");
            backgroundBrushs[iR][iC] = QBrush(col);
            validItems[iR][iC] =   true;
        }
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
        backgroundBrushs.remove(row+i);
        validItems.remove(row+i);
    }
    endRemoveRows();
    return true;
}

int LuxgateConfigModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return NColumns;
}

Qt::ItemFlags LuxgateConfigModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags res = Qt::ItemIsSelectable |  Qt::ItemIsEnabled;
    if(SwapSupportColumn != index.column())
        res |= Qt::ItemIsEditable;
    return  res;
}

bool LuxgateConfigModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(index.row() >= items.size() || index.row() < 0)
        return QAbstractTableModel::setData(index,value,role);


    if(Qt::EditRole == role
        || Qt::DisplayRole == role)
    {
        switch (index.column())
        {
            case TickerColumn:
                items[index.row()].ticker = value.toString();
            break;
            case HostColumn:
                items[index.row()].host = value.toString();
            break;
            case PortColumn:
                items[index.row()].port = value.toInt();
            break;
            case RpcuserColumn:
                items[index.row()].rpcuser = value.toString();
            break;
            case RpcpasswordColumn:
                items[index.row()].rpcpassword = value.toString();
            break;
            case Zmq_pub_raw_tx_endpointColumn:
                items[index.row()].zmq_pub_raw_tx_endpoint = value.toString();
            break;
            case SwapSupportColumn:
                items[index.row()].bSwapConnect = value.toBool();
            break;
        }
    }
    else if (Qt::BackgroundRole == role)
    {
        backgroundBrushs[index.row()][index.column()] =   qvariant_cast<QBrush>(value);
    }
    else if (ValidRole == role)
    {
        validItems[index.row()][index.column()] =   value.toBool();
        if(value.toBool())
        {
            QColor col;
            col.setNamedColor("#fcfcfc");
            backgroundBrushs[index.row()][index.column()] = QBrush(col);
        }
        else
            backgroundBrushs[index.row()][index.column()] = QBrush(QColor(Qt::red));
    }
    else if(AllDataRole == role)
    {
        items[index.row()] = qvariant_cast<BlockchainConfigQt>(value);
    }
    else
        return QAbstractTableModel::setData(index,value,role);

    //emit dataChanged
    if(AllDataRole == role)
        emit dataChanged(  this->index(index.row(), 0),
                           this->index(index.row(), NColumns-1),
                           {Qt::EditRole, Qt::DisplayRole, AllDataRole});
    else if (ValidRole == role)
        emit dataChanged(index, index, {ValidRole, Qt::BackgroundRole});
    else
        emit dataChanged(index, index, {role});

    return true;
}
