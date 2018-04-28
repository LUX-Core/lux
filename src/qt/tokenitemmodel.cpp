#include "tokenitemmodel.h"
#include "token.h"
#include "walletmodel.h"
#include "wallet.h"
#include <boost/foreach.hpp>

#include <QDateTime>
#include <QFont>
#include <QDebug>

class TokenItemEntry
{
public:
    TokenItemEntry(const uint256& tokenHash, const CTokenInfo& tokenInfo)
    {
        hash = QString::fromStdString(tokenHash.ToString());
        createTime.setTime_t(tokenInfo.nCreateTime);
        contractAddress = QString::fromStdString(tokenInfo.strContractAddress);
        tokenName = QString::fromStdString(tokenInfo.strTokenName);
        tokenSymbol = QString::fromStdString(tokenInfo.strTokenSymbol);
        decimals = tokenInfo.nDecimals;
        senderAddress = QString::fromStdString(tokenInfo.strSenderAddress);
    }

    QString hash;
    QDateTime createTime;
    QString contractAddress;
    QString tokenName;
    QString tokenSymbol;
    quint8 decimals;
    QString senderAddress;
    QString totalSupply;
    QString balance;
};

struct TokenItemEntryLessThan
{
    bool operator()(const TokenItemEntry &a, const TokenItemEntry &b) const
    {
        if(a.tokenSymbol == b.tokenSymbol)
            return a.contractAddress < b.contractAddress;

        return a.tokenSymbol < b.tokenSymbol;
    }
};

class TokenItemPriv
{
public:
    CWallet *wallet;
    QList<TokenItemEntry> cachedTokenItem;
    TokenItemModel *parent;

    TokenItemPriv(CWallet *_wallet, TokenItemModel *_parent):
        wallet(_wallet), parent(_parent) {}

    void refreshTokenItem()
    {
        cachedTokenItem.clear();
        {
            LOCK(wallet->cs_wallet);

            BOOST_FOREACH(const PAIRTYPE(uint256, CTokenInfo)& item, wallet->mapToken)
            {
                const uint256& tokenHash = item.first;
                const CTokenInfo& tokenInfo = item.second;
                cachedTokenItem.append(TokenItemEntry(tokenHash, tokenInfo));
            }
        }
        qSort(cachedTokenItem.begin(), cachedTokenItem.end(), TokenItemEntryLessThan());
    }

    void updateEntry(const TokenItemEntry &item, int status)
    {
        // Find address / label in model
        QList<TokenItemEntry>::iterator lower = qLowerBound(
            cachedTokenItem.begin(), cachedTokenItem.end(), item, TokenItemEntryLessThan());
        QList<TokenItemEntry>::iterator upper = qUpperBound(
            cachedTokenItem.begin(), cachedTokenItem.end(), item, TokenItemEntryLessThan());
        int lowerIndex = (lower - cachedTokenItem.begin());
        int upperIndex = (upper - cachedTokenItem.begin());
        bool inModel = (lower != upper);

        switch(status)
        {
        case CT_NEW:
            if(inModel)
            {
                qWarning() << "TokenItemPriv::updateEntry: Warning: Got CT_NEW, but entry is already in model";
                break;
            }
            parent->beginInsertRows(QModelIndex(), lowerIndex, lowerIndex);
            cachedTokenItem.insert(lowerIndex, item);
            parent->endInsertRows();
            break;
        case CT_UPDATED:
            if(!inModel)
            {
                qWarning() << "TokenItemPriv::updateEntry: Warning: Got CT_UPDATED, but entry is not in model";
                break;
            }
            parent->emitDataChanged(lowerIndex);
            break;
        case CT_DELETED:
            if(!inModel)
            {
                qWarning() << "TokenItemPriv::updateEntry: Warning: Got CT_DELETED, but entry is not in model";
                break;
            }
            parent->beginRemoveRows(QModelIndex(), lowerIndex, upperIndex-1);
            cachedTokenItem.erase(lower, upper);
            parent->endRemoveRows();
            break;
        }
    }

    int size()
    {
        return cachedTokenItem.size();
    }

    TokenItemEntry *index(int idx)
    {
        if(idx >= 0 && idx < cachedTokenItem.size())
        {
            return &cachedTokenItem[idx];
        }
        else
        {
            return 0;
        }
    }
};

struct TokenModelData
{
    Token *tokenAbi;
    QStringList columns;
    WalletModel *walletModel;
    CWallet *wallet;
    TokenItemPriv* priv;
};

TokenItemModel::TokenItemModel(CWallet *wallet, WalletModel *parent):
    QAbstractItemModel(parent),
    d(0)
{
    d = new TokenModelData();
    d->columns << tr("Hash") << tr("Create Time") << tr("Contract Address") << tr("Token Name") << tr("Token Symbol");
    d->columns << tr("Decimals") << tr("Sender Address") << tr("Total Supply") << tr("Balance");
    d->tokenAbi = new Token();
    d->wallet = wallet;
    d->walletModel = parent;

    d->priv = new TokenItemPriv(wallet, this);
    d->priv->refreshTokenItem();

    subscribeToCoreSignals();
}

TokenItemModel::~TokenItemModel()
{
    unsubscribeFromCoreSignals();

    if(d)
    {
        if(d->tokenAbi)
        {
            delete d->tokenAbi;
            d->tokenAbi = 0;
        }

        if(d->priv)
        {
            delete d->priv;
            d->priv = 0;
        }

        delete d;
        d = 0;
    }
}

QModelIndex TokenItemModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    TokenItemEntry *data = d->priv->index(row);
    if(data)
    {
        return createIndex(row, column, d->priv->index(row));
    }
    return QModelIndex();
}

QModelIndex TokenItemModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child);
    return QModelIndex();
}

int TokenItemModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return d->priv->size();
}

int TokenItemModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return d->columns.length();
}

QVariant TokenItemModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    TokenItemEntry *rec = static_cast<TokenItemEntry*>(index.internalPointer());

    if(role == Qt::DisplayRole || role == Qt::EditRole)
    {
        switch(index.column())
        {
            return QVariant();
        }
    }
    else if (role == Qt::FontRole)
    {
        return QFont();
    }

    return QVariant();
}

void TokenItemModel::updateToken(const QString &hash, int status, bool showToken)
{

}

void TokenItemModel::emitDataChanged(int idx)
{
    Q_EMIT dataChanged(index(idx, 0, QModelIndex()), index(idx, d->columns.length()-1, QModelIndex()));
}

struct TokenNotification
{
public:
    TokenNotification() {}
    TokenNotification(uint256 _hash, CTokenInfo _tokenInfo, ChangeType _status, bool _showToken):
        hash(_hash), tokenInfo(_tokenInfo), status(_status), showToken(_showToken) {}

    void invoke(QObject *tim)
    {
        QString strHash = QString::fromStdString(hash.GetHex());
        qDebug() << "NotifyTokenChanged: " + strHash + " status= " + QString::number(status);
        QMetaObject::invokeMethod(tim, "updateToken", Qt::QueuedConnection,
                                  Q_ARG(QString, strHash),
                                  Q_ARG(int, status),
                                  Q_ARG(bool, showToken));
    }
private:
    uint256 hash;
    CTokenInfo tokenInfo;
    ChangeType status;
    bool showToken;
};

static void NotifyTokenChanged(TokenItemModel *tim, CWallet *wallet, const uint256 &hash, ChangeType status)
{
    // Find token in wallet
    std::map<uint256, CTokenInfo>::iterator mi = wallet->mapToken.find(hash);
    bool showToken = mi != wallet->mapToken.end();
    CTokenInfo tokenInfo;
    if(showToken)
    {
        tokenInfo = mi->second;
    }

    TokenNotification notification(hash, tokenInfo, status, showToken);
    notification.invoke(tim);
}

void TokenItemModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    d->wallet->NotifyTokenChanged.connect(boost::bind(NotifyTokenChanged, this, _1, _2, _3));
}

void TokenItemModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    d->wallet->NotifyTokenChanged.disconnect(boost::bind(NotifyTokenChanged, this, _1, _2, _3));
}
