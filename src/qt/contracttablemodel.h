#ifndef CONTRACTTABLEMODEL_H
#define CONTRACTTABLEMODEL_H

#include <QAbstractTableModel>
#include <QStringList>

class ContractTablePriv;
class WalletModel;

class CWallet;

/**
   Qt model of the contract book in the core. This allows views to access and modify the contract book.
 */
class ContractTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit ContractTableModel(CWallet *wallet, WalletModel *parent = 0);
    ~ContractTableModel();

    enum ColumnIndex {
        Label = 0,   /**< User specified label */
        Address = 1, /**< Contract address */
        ABI = 2      /**< Contract abi */
    };

    /** Return status of edit/insert operation */
    enum EditStatus {
        OK = 0,                     /**< Everything ok */
        NO_CHANGES,             /**< No changes were made during edit operation */
        DUPLICATE_ADDRESS,      /**< Address already in contract book */
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role);
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex());
    /*@}*/

    /* Add an address to the model.
       Returns the added address on success, and an empty string otherwise.
     */
    QString addRow(const QString &label, const QString &address, const QString &abi);

    /* Label for address in contract book, if not found return empty string.
     */
    QString labelForAddress(const QString &address) const;

    /* ABI for address in contract book, if not found return empty string.
     */
    QString abiForAddress(const QString &address) const;

    /* Look up row index of an address in the model.
       Return -1 if not found.
     */
    int lookupAddress(const QString &address) const;

    EditStatus getEditStatus() const { return editStatus; }

    void resetEditStatus();

private:
    WalletModel *walletModel;
    CWallet *wallet;
    ContractTablePriv *priv;
    QStringList columns;
    EditStatus editStatus;

    /** Notify listeners that data changed. */
    void emitDataChanged(int index);

    void updateEditStatus(EditStatus status);

public Q_SLOTS:
    /* Update address list from core.
     */
    void updateEntry(const QString &address, const QString &label, const QString &abi, int status);

    friend class ContractTablePriv;
};

#endif // CONTRACTTABLEMODEL_H
