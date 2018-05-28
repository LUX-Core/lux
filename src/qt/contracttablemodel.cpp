#include "contracttablemodel.h"

#include "guiutil.h"
#include "walletmodel.h"

#include "wallet.h"

#include <boost/foreach.hpp>

#include <QFont>
#include <QDebug>

struct ContractTableEntry
{
    QString label;
    QString address;
    QString abi;

    ContractTableEntry() {}
    ContractTableEntry(const QString &_label, const QString &_address, const QString &_abi):
        label(_label), address(_address), abi(_abi) {}
};

struct ContractTableEntryLessThan
{
    bool operator()(const ContractTableEntry &a, const ContractTableEntry &b) const
    {
        return a.address < b.address;
    }
    bool operator()(const ContractTableEntry &a, const QString &b) const
    {
        return a.address < b;
    }
    bool operator()(const QString &a, const ContractTableEntry &b) const
    {
        return a < b.address;
    }
};

// Private implementation
class ContractTablePriv
{
public:
    CWallet *wallet;
    QList<ContractTableEntry> cachedContractTable;
    ContractTableModel *parent;

    ContractTablePriv(CWallet *_wallet, ContractTableModel *_parent):
        wallet(_wallet), parent(_parent) {}

    void refreshContractTable()
    {
        cachedContractTable.clear();
        {
            LOCK(wallet->cs_wallet);
            BOOST_FOREACH(const PAIRTYPE(std::string, CContractBookData)& item, wallet->mapContractBook)
            {
                const std::string& address = item.first;
                const std::string& strName = item.second.name;
                const std::string& strAbi = item.second.abi;
                cachedContractTable.append(ContractTableEntry(
                                  QString::fromStdString(strName),
                                  QString::fromStdString(address),
                                  QString::fromStdString(strAbi)));
            }
        }
        // qLowerBound() and qUpperBound() require our cachedContractTable list to be sorted in asc order
        qSort(cachedContractTable.begin(), cachedContractTable.end(), ContractTableEntryLessThan());
    }

    void updateEntry(const QString &address, const QString &label, const QString &abi, int status)
    {
        // Find address / label in model
        QList<ContractTableEntry>::iterator lower = qLowerBound(
            cachedContractTable.begin(), cachedContractTable.end(), address, ContractTableEntryLessThan());
        QList<ContractTableEntry>::iterator upper = qUpperBound(
            cachedContractTable.begin(), cachedContractTable.end(), address, ContractTableEntryLessThan());
        int lowerIndex = (lower - cachedContractTable.begin());
        int upperIndex = (upper - cachedContractTable.begin());
        bool inModel = (lower != upper);

        switch(status)
        {
        case CT_NEW:
            if(inModel)
            {
                qWarning() << "ContractTablePriv::updateEntry: Warning: Got CT_NEW, but entry is already in model";
                break;
            }
            parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
            cachedContractTable.insert(lowerIndex, ContractTableEntry(label, address, abi));
            parent->endInsertRows();
            break;
        case CT_UPDATED:
            if(!inModel)
            {
                qWarning() << "ContractTablePriv::updateEntry: Warning: Got CT_UPDATED, but entry is not in model";
                break;
            }
            lower->label = label;
            lower->abi = abi;
            parent->emitDataChanged(lowerIndex);
            break;
        case CT_DELETED:
            if(!inModel)
            {
                qWarning() << "ContractTablePriv::updateEntry: Warning: Got CT_DELETED, but entry is not in model";
                break;
            }
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
            cachedContractTable.erase(lower, upper);
            parent->endRemoveRows();
            break;
        }
    }

    int size()
    {
        return cachedContractTable.size();
    }

    ContractTableEntry *index(int idx)
    {
        if(idx >= 0 && idx < cachedContractTable.size())
        {
            return &cachedContractTable[idx];
        }
        else
        {
            return 0;
        }
    }
};

ContractTableModel::ContractTableModel(CWallet *_wallet, WalletModel *parent) :
    QAbstractTableModel(parent),walletModel(parent),wallet(_wallet),priv(0)
{
    columns << tr("Label") << tr("Contract Address") << tr("Interface (ABI)");
    priv = new ContractTablePriv(wallet, this);
    priv->refreshContractTable();
}

ContractTableModel::~ContractTableModel()
{
    delete priv;
}

int ContractTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int ContractTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant ContractTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    ContractTableEntry *rec = static_cast<ContractTableEntry*>(index.internalPointer());

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(index.column())
        {
        case Label:
            if(rec->label.isEmpty() && role == Qt::DisplayRole)
            {
                return tr("(no label)");
            }
            else
            {
                return rec->label;
            }
        case Address:
            return rec->address;
        case ABI:
            return rec->abi;
        }
    }
    else if (role == Qt::FontRole)
    {
        QFont font;
        if(index.column() == Address)
        {
            font = GUIUtil::bitcoinAddressFont();
        }
        return font;
    }

    return QVariant();
}

bool ContractTableModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if(!index.isValid())
        return false;
    ContractTableEntry *rec = static_cast<ContractTableEntry*>(index.internalPointer());

    if(role == Qt::EditRole)
    {
        LOCK(wallet->cs_wallet); /* For SetContractBook / DelContractBook */
        std::string curAddress = rec->address.toStdString();
        std::string curLabel = rec->label.toStdString();
        std::string curAbi = rec->abi.toStdString();
        if(index.column() == Label)
        {
            // Do nothing, if old label == new label
            if(rec->label == value.toString())
            {
                updateEditStatus(NO_CHANGES);
                return false;
            }
            wallet->SetContractBook(curAddress, value.toString().toStdString(), curAbi);
        } else if(index.column() == Address) {
            std::string newAddress = value.toString().toStdString();

            // Do nothing, if old address == new address
            if(newAddress == curAddress)
            {
                updateEditStatus(NO_CHANGES);
                return false;
            }
            // Check for duplicate addresses to prevent accidental deletion of addresses, if you try
            // to paste an existing address over another address (with a different label)
            else if(wallet->mapContractBook.count(newAddress))
            {
                updateEditStatus(DUPLICATE_ADDRESS);
                return false;
            }
            else
            {
                // Remove old entry
                wallet->DelContractBook(curAddress);
                // Add new entry with new address
                wallet->SetContractBook(newAddress, curLabel, curAbi);
            }
        }
        else if(index.column() == ABI) {
            // Do nothing, if old abi == new abi
            if(rec->abi == value.toString())
            {
                updateEditStatus(NO_CHANGES);
                return false;
            }
            wallet->SetContractBook(curAddress, curLabel, value.toString().toStdString());
        }
        return true;
    }
    return false;
}

QVariant ContractTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole && section < columns.size())
        {
            return columns[section];
        }
    }
    return QVariant();
}

QModelIndex ContractTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    ContractTableEntry *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
    }
    else
    {
        return QModelIndex();
    }
}

void ContractTableModel::updateEntry(const QString &address,
        const QString &label, const QString &abi, int status)
{
    // Update contract book model from Lux core
    priv->updateEntry(address, label, abi, status);
}

QString ContractTableModel::addRow(const QString &label, const QString &address, const QString &abi)
{
    // Check for duplicate entry
    if(lookupAddress(address) != -1)
    {
        editStatus = DUPLICATE_ADDRESS;
        return "";
    }

    // Add new entry
    std::string strLabel = label.toStdString();
    std::string strAddress = address.toStdString();
    std::string strAbi = abi.toStdString();
    editStatus = OK;
    {
        LOCK(wallet->cs_wallet);
        wallet->SetContractBook(strAddress, strLabel, strAbi);
    }
    return address;
}

bool ContractTableModel::removeRows(int row, int count, const QModelIndex &parent)
{
    Q_UNUSED(parent);
    ContractTableEntry *rec = priv->index(row);
    if(count != 1 || !rec )
    {
        // Can only remove one row at a time, and cannot remove rows not in model.
        return false;
    }
    {
        LOCK(wallet->cs_wallet);
        wallet->DelContractBook(rec->address.toStdString());
    }
    return true;
}

/* Label for address in contract book, if not found return empty string.
 */
QString ContractTableModel::labelForAddress(const QString &address) const
{
    {
        LOCK(wallet->cs_wallet);
        std::string address_parsed(address.toStdString());
        std::map<std::string, CContractBookData>::iterator mi = wallet->mapContractBook.find(address_parsed);
        if (mi != wallet->mapContractBook.end())
        {
            return QString::fromStdString(mi->second.name);
        }
    }
    return QString();
}

/* ABI for address in contract book, if not found return empty string.
 */
QString ContractTableModel::abiForAddress(const QString &address) const
{
    {
        LOCK(wallet->cs_wallet);
        std::string address_parsed(address.toStdString());
        std::map<std::string, CContractBookData>::iterator mi = wallet->mapContractBook.find(address_parsed);
        if (mi != wallet->mapContractBook.end())
        {
            return QString::fromStdString(mi->second.abi);
        }
    }
    return QString();
}

int ContractTableModel::lookupAddress(const QString &address) const
{
    QModelIndexList lst = match(index(0, Address, QModelIndex()),
                                Qt::EditRole, address, 1, Qt::MatchExactly);
    if(lst.isEmpty())
    {
        return -1;
    }
    else
    {
        return lst.at(0).row();
    }
}

void ContractTableModel::resetEditStatus()
{
    editStatus = OK;
}

void ContractTableModel::emitDataChanged(int idx)
{
    Q_EMIT dataChanged(index(idx, 0, QModelIndex()), index(idx, columns.length()-1, QModelIndex()));
}

void ContractTableModel::updateEditStatus(ContractTableModel::EditStatus status)
{
    if(status > editStatus)
    {
        editStatus = status;
    }
}
