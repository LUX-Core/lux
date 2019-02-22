// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "addresstablemodel.h"

#include "guiutil.h"
#include "walletmodel.h"

#include "base58.h"
#include "wallet.h"
#include "script/standard.h"

#include <QDebug>
#include <QFont>

const QString AddressTableModel::Send = "S";
const QString AddressTableModel::Receive = "R";

struct AddressTableEntry {
    enum Type {
        Sending,
        Receiving,
        Hidden /* QSortFilterProxyModel will filter these out */
    };

    Type type;
    QString label;
    QString address;

    AddressTableEntry() {}
    AddressTableEntry(Type type, const QString& label, const QString& address) : type(type), label(label), address(address) {}
};

struct AddressTableEntryLessThan {
    bool operator()(const AddressTableEntry& a, const AddressTableEntry& b) const
    {
        return a.address < b.address;
    }
    bool operator()(const AddressTableEntry& a, const QString& b) const
    {
        return a.address < b;
    }
    bool operator()(const QString& a, const AddressTableEntry& b) const
    {
        return a < b.address;
    }
};

/* Determine address type from address purpose */
static AddressTableEntry::Type translateTransactionType(const QString& strPurpose, bool isMine)
{
    AddressTableEntry::Type addressType = AddressTableEntry::Hidden;
    // "refund" addresses aren't shown, and change addresses aren't in mapAddressBook at all.
    if (strPurpose == "send")
        addressType = AddressTableEntry::Sending;
    else if (strPurpose == "receive")
        addressType = AddressTableEntry::Receiving;
    else if (strPurpose == "unknown" || strPurpose == "") // if purpose not set, guess
        addressType = (isMine ? AddressTableEntry::Receiving : AddressTableEntry::Sending);
    return addressType;
}

// Private implementation
class AddressTablePriv
{
public:
    CWallet* wallet;
    QList<AddressTableEntry> cachedAddressTable;
    AddressTableModel* parent;

    AddressTablePriv(CWallet* wallet, AddressTableModel* parent) : wallet(wallet), parent(parent) {}

    void refreshAddressTable()
    {
        cachedAddressTable.clear();
        {
            LOCK(wallet->cs_wallet);
            for (const PAIRTYPE(CTxDestination, CAddressBookData) & item : wallet->mapAddressBook) {
                const CTxDestination& address = item.first;
                bool fMine = IsMine(*wallet, address);
                AddressTableEntry::Type addressType = translateTransactionType(
                    QString::fromStdString(item.second.purpose), fMine);
                const std::string& strName = item.second.name;
                cachedAddressTable.append(AddressTableEntry(addressType,
                    QString::fromStdString(strName),
                    QString::fromStdString(EncodeDestination(address))));
            }
        }
        // qLowerBound() and qUpperBound() require our cachedAddressTable list to be sorted in asc order
        // Even though the map is already sorted this re-sorting step is needed because the originating map
        // is sorted by binary address, not by base58() address.
        qSort(cachedAddressTable.begin(), cachedAddressTable.end(), AddressTableEntryLessThan());
    }

    void updateEntry(const QString& address, const QString& label, bool isMine, const QString& purpose, int status)
    {
        // Find address / label in model
        QList<AddressTableEntry>::iterator lower = qLowerBound(
            cachedAddressTable.begin(), cachedAddressTable.end(), address, AddressTableEntryLessThan());
        QList<AddressTableEntry>::iterator upper = qUpperBound(
            cachedAddressTable.begin(), cachedAddressTable.end(), address, AddressTableEntryLessThan());
        int lowerIndex = (lower - cachedAddressTable.begin());
        int upperIndex = (upper - cachedAddressTable.begin());
        bool inModel = (lower != upper);
        AddressTableEntry::Type newEntryType = translateTransactionType(purpose, isMine);

        switch (status) {
        case CT_NEW:
            if (inModel) {
                qWarning() << "AddressTablePriv::updateEntry : Warning: Got CT_NEW, but entry is already in model";
                break;
            }
            parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
            cachedAddressTable.insert(lowerIndex, AddressTableEntry(newEntryType, label, address));
            parent->endInsertRows();
            break;
        case CT_UPDATED:
            if (!inModel) {
                qWarning() << "AddressTablePriv::updateEntry : Warning: Got CT_UPDATED, but entry is not in model";
                break;
            }
            lower->type = newEntryType;
            lower->label = label;
            parent->emitDataChanged(lowerIndex);
            break;
        case CT_DELETED:
            if (!inModel) {
                qWarning() << "AddressTablePriv::updateEntry : Warning: Got CT_DELETED, but entry is not in model";
                break;
            }
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex - 1);
            cachedAddressTable.erase(lower, upper);
            parent->endRemoveRows();
            break;
        }
    }

    int size()
    {
        return cachedAddressTable.size();
    }

    AddressTableEntry* index(int idx)
    {
        if (idx >= 0 && idx < cachedAddressTable.size()) {
            return &cachedAddressTable[idx];
        } else {
            return 0;
        }
    }
};

AddressTableModel::AddressTableModel(CWallet* wallet, WalletModel* parent) : QAbstractTableModel(parent), walletModel(parent), wallet(wallet), priv(0)
{
    columns << tr("Label") << tr("Address");
    priv = new AddressTablePriv(wallet, this);
    priv->refreshAddressTable();
}

AddressTableModel::~AddressTableModel()
{
    delete priv;
}

int AddressTableModel::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return priv->size();
}

int AddressTableModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant AddressTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    AddressTableEntry* rec = static_cast<AddressTableEntry*>(index.internalPointer());

    if (role == Qt::DisplayRole || role == Qt::EditRole) {
        switch (index.column()) {
        case Label:
            if (rec->label.isEmpty() && role == Qt::DisplayRole) {
                return tr("(no label)");
            } else {
                return rec->label;
            }
        case Address:
            return rec->address;
        }
    } else if (role == Qt::FontRole) {
        QFont font;
        if (index.column() == Address) {
            font = GUIUtil::bitcoinAddressFont();
        }
        return font;
    } else if (role == TypeRole) {
        switch (rec->type) {
        case AddressTableEntry::Sending:
            return Send;
        case AddressTableEntry::Receiving:
            return Receive;
        default:
            break;
        }
    }
    return QVariant();
}

bool AddressTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid())
        return false;
    AddressTableEntry* rec = static_cast<AddressTableEntry*>(index.internalPointer());
    std::string strPurpose = (rec->type == AddressTableEntry::Sending ? "send" : "receive");
    editStatus = OK;

    if (role == Qt::EditRole) {
        LOCK(wallet->cs_wallet); /* For SetAddressBook / DelAddressBook */
        CTxDestination curAddress = DecodeDestination(rec->address.toStdString());
        if (index.column() == Label) {
            // Do nothing, if old label == new label
            if (rec->label == value.toString()) {
                editStatus = NO_CHANGES;
                return false;
            }
            wallet->SetAddressBook(curAddress, value.toString().toStdString(), strPurpose);
        } else if (index.column() == Address) {
            CTxDestination newAddress = DecodeDestination(value.toString().toStdString());
            // Refuse to set invalid address, set error status and return false
            if (boost::get<CNoDestination>(&newAddress)) {
                editStatus = INVALID_ADDRESS;
                return false;
            }
            // Do nothing, if old address == new address
            else if (newAddress == curAddress) {
                editStatus = NO_CHANGES;
                return false;
            }
            // Check for duplicate addresses to prevent accidental deletion of addresses, if you try
            // to paste an existing address over another address (with a different label)
            else if (wallet->mapAddressBook.count(newAddress)) {
                editStatus = DUPLICATE_ADDRESS;
                return false;
            }
            // Double-check that we're not overwriting a receiving address
            else if (rec->type == AddressTableEntry::Sending) {
                // Remove old entry
                wallet->DelAddressBook(curAddress);
                // Add new entry with new address
                wallet->SetAddressBook(newAddress, rec->label.toStdString(), strPurpose);
            }
        }
        return true;
    }
    return false;
}

QVariant AddressTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole && section < columns.size()) {
            return columns[section];
        }
    }
    return QVariant();
}

Qt::ItemFlags AddressTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return 0;
    AddressTableEntry* rec = static_cast<AddressTableEntry*>(index.internalPointer());

    Qt::ItemFlags retval = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
    // Can edit address and label for sending addresses,
    // and only label for receiving addresses.
    if (rec->type == AddressTableEntry::Sending ||
        (rec->type == AddressTableEntry::Receiving && index.column() == Label)) {
        retval |= Qt::ItemIsEditable;
    }
    return retval;
}

QModelIndex AddressTableModel::index(int row, int column, const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    AddressTableEntry* data = priv->index(row);
    if (data) {
        return createIndex(row, column, priv->index(row));
    } else {
        return QModelIndex();
    }
}

void AddressTableModel::updateEntry(const QString& address,
    const QString& label,
    bool isMine,
    const QString& purpose,
    int status)
{
    // Update address book model from Luxcore
    priv->updateEntry(address, label, isMine, purpose, status);
}

QString AddressTableModel::addRow(const QString& type, const QString& label, const QString& address, const OutputType address_type)
{
    std::string strLabel = label.toStdString();
    std::string strAddress = address.toStdString();

    editStatus = OK;

    if (type == Send) {
        if (!walletModel->validateAddress(address)) {
            editStatus = INVALID_ADDRESS;
            return QString();
        }
        // Check for duplicate addresses
        {
            LOCK(wallet->cs_wallet);
            if (wallet->mapAddressBook.count(DecodeDestination(strAddress))) {
                editStatus = DUPLICATE_ADDRESS;
                return QString();
            }
        }
    } else if (type == Receive) {
        // Generate a new address to associate with given label
        CPubKey newKey;
        if (!wallet->GetKeyFromPool(newKey, false)) {
            WalletModel::UnlockContext ctx(walletModel->requestUnlock(true));
            if (!ctx.isValid()) {
                // Unlock wallet failed or was cancelled
                editStatus = WALLET_UNLOCK_FAILURE;
                return QString();
            }
            if (!wallet->GetKeyFromPool(newKey, false)) {
                editStatus = KEY_GENERATION_FAILURE;
                return QString();
            }
        }
        wallet->LearnRelatedScripts(newKey, address_type);
        strAddress = EncodeDestination(GetDestinationForKey(newKey, address_type));
    } else {
        return QString();
    }

    // Add entry
    {
        LOCK(wallet->cs_wallet);
        wallet->SetAddressBook(DecodeDestination(strAddress), strLabel,
            (type == Send ? "send" : "receive"));
    }
    return QString::fromStdString(strAddress);
}

bool AddressTableModel::removeRows(int row, int count, const QModelIndex& parent)
{
    Q_UNUSED(parent);
    AddressTableEntry* rec = priv->index(row);
    if (count != 1 || !rec || rec->type == AddressTableEntry::Receiving) {
        // Can only remove one row at a time, and cannot remove rows not in model.
        // Also refuse to remove receiving addresses.
        return false;
    }
    {
        LOCK(wallet->cs_wallet);
        wallet->DelAddressBook(DecodeDestination(rec->address.toStdString()));
    }
    return true;
}

/* Look up label for address in address book, if not found return empty string.
 */
QString AddressTableModel::labelForAddress(const QString& address) const
{
    {
        LOCK(wallet->cs_wallet);
        CTxDestination address_parsed = DecodeDestination(address.toStdString());
        std::map<CTxDestination, CAddressBookData>::iterator mi = wallet->mapAddressBook.find(address_parsed);
        if (mi != wallet->mapAddressBook.end()) {
            return QString::fromStdString(mi->second.name);
        }
    }
    return QString();
}

int AddressTableModel::lookupAddress(const QString& address) const
{
    QModelIndexList lst = match(index(0, Address, QModelIndex()),
        Qt::EditRole, address, 1, Qt::MatchExactly);
    if (lst.isEmpty()) {
        return -1;
    } else {
        return lst.at(0).row();
    }
}

void AddressTableModel::emitDataChanged(int idx)
{
    Q_EMIT dataChanged(index(idx, 0, QModelIndex()), index(idx, columns.length() - 1, QModelIndex()));
}
