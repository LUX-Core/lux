#ifndef CsTypesMODEL_H
#define CsTypesMODEL_H
#include "luxgate/lgconfig.h"
#include <QList>
#include <QAbstractTableModel>
#include <QString>

struct BlockchainConfigQt
{
    BlockchainConfigQt()
    {}
    BlockchainConfigQt(BlockchainConfig conf):
            ticker(QString::fromStdString(conf.ticker)),
            host(QString::fromStdString(conf.host)),
            port(conf.port),
            rpcuser(QString::fromStdString(conf.rpcuser)),
            rpcpassword(QString::fromStdString(conf.rpcpassword)),
            zmq_pub_raw_tx_endpoint(QString::fromStdString(conf.zmq_pub_raw_tx_endpoint))
    {}
    BlockchainConfig toBlockchainConfig()
    {
        BlockchainConfig res;
        res.ticker = ticker.toStdString();
        res.host = host.toStdString();
        res.port = port;
        res.rpcuser =  rpcuser.toStdString();
        res.rpcpassword =  rpcpassword.toStdString();
        res.zmq_pub_raw_tx_endpoint =  zmq_pub_raw_tx_endpoint.toStdString();
        return res;
    }
    QString ticker;
    QString host;
    int port;
    QString rpcuser;
    QString rpcpassword;
    QString zmq_pub_raw_tx_endpoint;
};
Q_DECLARE_METATYPE(BlockchainConfigQt)


class LuxgateConfigModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    LuxgateConfigModel(QObject *parent = nullptr);

    enum UserRoles{AllDataRole = Qt::UserRole + 1};

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
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    bool setData(int iRow,int iColumn,  const QVariant &value, int role = Qt::EditRole);
    QVariant data(int iRow,int iColumn,  int role = Qt::DisplayRole);
private:
    QList<BlockchainConfigQt> items;
};

#endif // CsTypesMODEL_H
