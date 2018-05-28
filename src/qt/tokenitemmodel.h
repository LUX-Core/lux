#ifndef TOKENITEMMODEL_H
#define TOKENITEMMODEL_H

#include <QAbstractItemModel>
#include <QStringList>
#include <QThread>

class CWallet;
class WalletModel;
class Token;
class TokenItemPriv;
class TokenTxWorker;
class TokenItemEntry;

class TokenItemModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum ColumnIndex {
        Name = 0,
        Symbol = 1,
        Balance = 2,
    };

    enum DataRole{
        HashRole = Qt::UserRole + 1,
        AddressRole = Qt::UserRole + 2,
        NameRole = Qt::UserRole + 3,
        SymbolRole = Qt::UserRole + 4,
        DecimalsRole = Qt::UserRole + 5,
        SenderRole = Qt::UserRole + 6,
        BalanceRole = Qt::UserRole + 7,
        RawBalanceRole = Qt::UserRole + 8,
    };

    TokenItemModel(CWallet *wallet, WalletModel *parent = 0);
    ~TokenItemModel();

    /** @name Methods overridden from QAbstractItemModel
        @{*/
    QModelIndex index(int row, int column,
                              const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    /*@}*/

    Token *getTokenAbi();

public Q_SLOTS:
    void checkTokenBalanceChanged();

private Q_SLOTS:
    void updateToken(const QString &hash, int status, bool showToken);

private:
    /** Notify listeners that data changed. */
    void emitDataChanged(int index);
    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

    Token *tokenAbi;
    QStringList columns;
    WalletModel *walletModel;
    CWallet *wallet;
    TokenItemPriv* priv;
    TokenTxWorker* worker;
    QThread t;

    friend class TokenItemPriv;
};

#endif // TOKENITEMMODEL_H
