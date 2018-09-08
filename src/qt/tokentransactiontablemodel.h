#ifndef BITCOIN_QT_TOKENTOKENTRANSACTIONTABLEMODEL_H
#define BITCOIN_QT_TOKENTOKENTRANSACTIONTABLEMODEL_H

#include "bitcoinunits.h"

#include <QAbstractTableModel>
#include <QStringList>

class PlatformStyle;
class TokenTransactionRecord;
class TokenTransactionTablePriv;
class WalletModel;

class CWallet;

/** UI model for the transaction table of a wallet.
 */
class TokenTransactionTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit TokenTransactionTableModel(const PlatformStyle *platformStyle, CWallet* wallet, WalletModel *parent = 0);
    ~TokenTransactionTableModel();

    enum ColumnIndex {
        Status = 0,
        Date = 1,
        Type = 2,
        ToAddress = 3,
        Name = 4,
        Amount = 5
    };

    /** Roles to get specific information from a transaction row.
        These are independent of column.
    */
    enum RoleIndex {
        /** Type of transaction */
        TypeRole = Qt::UserRole,
        /** Date and time this transaction was created */
        DateRole,
        /** Long description (HTML format) */
        LongDescriptionRole,
        /** Token name */
        NameRole,
        /** Address of transaction */
        AddressRole,
        /** Label of address related to transaction */
        LabelRole,
        /** Net amount of transaction */
        AmountRole,
        /** Transaction hash */
        TxHashRole,
        /** Transaction data, hex-encoded */
        TxHexRole,
        /** Whole transaction as plain text */
        TxPlainTextRole,
        /** Is transaction confirmed? */
        ConfirmedRole,
        /** Formatted amount, without brackets when unconfirmed */
        FormattedAmountRole,
        /** Formatted amount, with unit */
        FormattedAmountWithUnitRole,
        /** Transaction status (TokenTransactionRecord::Status) */
        StatusRole,
        /** Unprocessed icon */
        RawDecorationRole,
    };

    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const;
    bool processingQueuedTransactions() { return fProcessingQueuedTransactions; }

private:
    CWallet* wallet;
    WalletModel *walletModel;
    QStringList columns;
    TokenTransactionTablePriv *priv;
    bool fProcessingQueuedTransactions;
    const PlatformStyle *platformStyle;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

    QString lookupAddress(const std::string &address, const std::string &label, bool tooltip) const;
    QVariant addressColor(const TokenTransactionRecord *wtx) const;
    QString formatTxStatus(const TokenTransactionRecord *wtx) const;
    QString formatTxDate(const TokenTransactionRecord *wtx) const;
    QString formatTxType(const TokenTransactionRecord *wtx) const;
    QString formatTxToAddress(const TokenTransactionRecord *wtx, bool tooltip) const;
    QString formatTxTokenSymbol(const TokenTransactionRecord *wtx) const;
    QString formatTxAmount(const TokenTransactionRecord *wtx, bool showUnconfirmed=true, BitcoinUnits::SeparatorStyle separators=BitcoinUnits::separatorStandard) const;
    QString formatTxAmountWithUnit(const TokenTransactionRecord *wtx, bool showUnconfirmed=true, BitcoinUnits::SeparatorStyle separators=BitcoinUnits::separatorStandard) const;
    QString formatTooltip(const TokenTransactionRecord *rec) const;
    QVariant txStatusDecoration(const TokenTransactionRecord *wtx) const;
    QVariant txAddressDecoration(const TokenTransactionRecord *wtx) const;

public Q_SLOTS:
    void emitDataChanged(int index);
    /* New transaction, or transaction changed status */
    void updateTransaction(const QString &hash, int status, bool showTransaction);
    void updateConfirmations();
    /* Needed to update fProcessingQueuedTransactions through a QueuedConnection */
    void setProcessingQueuedTransactions(bool value) { fProcessingQueuedTransactions = value; }

    friend class TokenTransactionTablePriv;
};

#endif // BITCOIN_QT_TOKENTOKENTRANSACTIONTABLEMODEL_H
