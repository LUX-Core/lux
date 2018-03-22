#ifndef STAKINGTABLEMODEL_H
#define STAKINGTABLEMODEL_H


#include <QAbstractTableModel>
#include <QStringList>

class CWallet;
class StakingTablePriv;
class WalletModel;

class StakingTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    explicit StakingTableModel(CWallet * wallet, WalletModel *parent = 0);
    ~StakingTableModel();

    enum ColumnIndex {
        TxHash = 0,
        Address = 1,
        Balance = 2,
        Age = 3,
        CoinDay = 4,
        MintProbability = 5
    };

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const;

    void setStakingInterval(int interval);

private:
    CWallet* wallet;
    WalletModel *walletModel;
    QStringList columns;
    int stakingInterval;
    StakingTablePriv *priv;

    QString lookupAddress(const std::string &address, bool tooltip) const;

private slots:
    void update();

    friend class StakingTablePriv;
};

#endif // STAKINGTABLEMODEL_H
