#ifndef CsTypesMODEL_H
#define CsTypesMODEL_H
#include "luxgate/lgconfig.h"
#include <QList>
#include <QAbstractTableModel>

Q_DECLARE_METATYPE(BlockchainConfig)
class LuxgateConfigModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    LuxgateConfigModel(QObject *parent = nullptr);

    enum UserRoles{AllDataRole};

    enum Columns{
        TickerColumn = 0, HostColumn,
        PortColumn, RpcuserColumn, RpcpasswordColumn,
        Zmq_pub_raw_tx_endpointColumn, NColumns
    };

    //override functions
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

    bool setData(int iRow,int iColumn,  const QVariant &value, int role = Qt::EditRole);
private:
    QList<BlockchainConfig> items;
};

#endif // CsTypesMODEL_H
