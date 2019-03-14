#ifndef CsTypesMODEL_H
#define CsTypesMODEL_H
#include "luxgate/lgconfig.h"
#include "luxgate/blockchainclient.h"
#include "luxgate/luxgate.h"
#include "luxgategui_global.h"
#include <QList>
#include <QAbstractTableModel>
#include <QString>
#include <QBrush>

struct BlockchainConfigQt
{
    BlockchainConfigQt()
    {}
    BlockchainConfigQt(const BlockchainConfig & conf):
            ticker(QString::fromStdString(conf.ticker)),
            host(QString::fromStdString(conf.host)),
            port(conf.port),
            rpcuser(QString::fromStdString(conf.rpcuser)),
            rpcpassword(QString::fromStdString(conf.rpcpassword)),
            bSwapConnect(false)
    {}
    BlockchainConfig toBlockchainConfig()
    {
        BlockchainConfig res;
        res.ticker = ticker.toStdString();
        res.host = host.toStdString();
        res.port = port;
        res.rpcuser =  rpcuser.toStdString();
        res.rpcpassword =  rpcpassword.toStdString();
        return res;
    }
    QString ticker;
    QString host;
    int port;
    QString rpcuser;
    QString rpcpassword;
    bool bSwapConnect;
};
Q_DECLARE_METATYPE(BlockchainConfigQt)


class LuxgateConfigModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    LuxgateConfigModel(QObject *parent = nullptr);

    enum UserRoles{AllDataRole = Luxgate::IndividualRole, ValidRole};

    enum Columns{
        TickerColumn = 0, HostColumn,
        PortColumn, RpcuserColumn, RpcpasswordColumn,
        SwapSupportColumn, NColumns
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
    QMap<int, QMap<int, QBrush> > backgroundBrushs; //QMap<row,QMap<column, brush>>
    QMap<int, QMap<int, bool> > validItems; //QMap<row,QMap<column, bValid>>
};

#endif // CsTypesMODEL_H
