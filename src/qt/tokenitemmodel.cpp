#include "tokenitemmodel.h"
#include "token.h"
#include "walletmodel.h"
#include "wallet.h"
#include "bitcoinunits.h"
#include "main.h"

#include <algorithm>

#include <QDateTime>
#include <QFont>
#include <QDebug>
#include <QThread>

class TokenItemEntry
{
public:
    TokenItemEntry()
    {}

    TokenItemEntry(uint256 tokenHash, CTokenInfo tokenInfo)
    {
        hash = tokenHash;
        createTime.setTime_t(tokenInfo.nCreateTime);
        contractAddress = QString::fromStdString(tokenInfo.strContractAddress);
        tokenName = QString::fromStdString(tokenInfo.strTokenName);
        tokenSymbol = QString::fromStdString(tokenInfo.strTokenSymbol);
        decimals = tokenInfo.nDecimals;
        senderAddress = QString::fromStdString(tokenInfo.strSenderAddress);
    }

    TokenItemEntry( const TokenItemEntry &obj)
    {
        hash = obj.hash;
        createTime = obj.createTime;
        contractAddress = obj.contractAddress;
        tokenName = obj.tokenName;
        tokenSymbol = obj.tokenSymbol;
        decimals = obj.decimals;
        senderAddress = obj.senderAddress;
        balance = obj.balance;
    }

    bool update(Token* tokenAbi)
    {
        bool modified;
        return update(tokenAbi, modified);
    }

    bool update(Token* tokenAbi, bool& modified)
    {
        modified = false;

        if(!tokenAbi)
            return false;

        bool ret = true;
        tokenAbi->setAddress(contractAddress.toStdString());
        tokenAbi->setSender(senderAddress.toStdString());
        std::string strBalance;
        ret &= tokenAbi->balanceOf(strBalance);
        if(ret)
        {
            int256_t val(strBalance);
            if(val != balance)
            {
                modified = true;
            }
            balance = val;
        }

        return ret;
    }

    ~TokenItemEntry()
    {}

    uint256 hash;
    QDateTime createTime;
    QString contractAddress;
    QString tokenName;
    QString tokenSymbol;
    quint8 decimals;
    QString senderAddress;
    int256_t balance;
};

class TokenTxWorker : public QObject
{
    Q_OBJECT
public:
    CWallet *wallet;
    bool first;
    Token tokenTxAbi;
    TokenTxWorker(CWallet *_wallet):
        wallet(_wallet), first(true) {}

private Q_SLOTS:
    void updateTokenTx(const QString &hash)
    {

        uint256 tokenHash = uint256S(hash.toStdString());
        int64_t fromBlock = 0;
        int64_t toBlock = -1;
        CTokenInfo tokenInfo;
        uint256 blockHash;
        bool found = false;

        LOCK2(cs_main, wallet->cs_wallet);

        int64_t backInPast = first ? Params().COINBASE_MATURITY() : 10;
        first = false;

        CBlockIndex* tip = chainActive.Tip();
        if(tip)
        {
            blockHash = tip->GetBlockHash();
            toBlock = chainActive.Height();

            std::map<uint256, CTokenInfo>::iterator mi = wallet->mapToken.find(tokenHash);
            found = mi != wallet->mapToken.end();
            if(found)
            {
                tokenInfo = mi->second;
                CBlockIndex* index = chainActive[tokenInfo.blockNumber];
                if(tokenInfo.blockNumber < toBlock)
                {
                    if(index && index->GetBlockHash() == tokenInfo.blockHash)
                    {
                        fromBlock = tokenInfo.blockNumber;
                    }
                    else
                    {
                        fromBlock = tokenInfo.blockNumber - backInPast;
                    }
                }
                else
                {
                    fromBlock = toBlock - backInPast;
                }
                if(fromBlock < 0)
                    fromBlock = 0;

                tokenInfo.blockHash = blockHash;
                tokenInfo.blockNumber = toBlock;
            }
        }

        if(found)
        {
            std::vector<TokenEvent> tokenEvents;
            tokenTxAbi.setAddress(tokenInfo.strContractAddress);
            tokenTxAbi.setSender(tokenInfo.strSenderAddress);
            tokenTxAbi.transferEvents(tokenEvents, fromBlock, toBlock);
            for(size_t i = 0; i < tokenEvents.size(); i++)
            {
                TokenEvent event = tokenEvents[i];
                CTokenTx tokenTx;
                tokenTx.strContractAddress = event.address;
                tokenTx.strSenderAddress = event.sender;
                tokenTx.strReceiverAddress = event.receiver;
                tokenTx.nValue = event.value;
                tokenTx.transactionHash = event.transactionHash;
                tokenTx.blockHash = event.blockHash;
                tokenTx.blockNumber = event.blockNumber;
                wallet->AddTokenTxEntry(tokenTx, false);
            }

            wallet->AddTokenEntry(tokenInfo);
        }
    }
};

#include "tokenitemmodel.moc"

struct TokenItemEntryLessThan
{
    bool operator()(const TokenItemEntry &a, const TokenItemEntry &b) const
    {
        return a.hash < b.hash;
    }
    bool operator()(const TokenItemEntry &a, const uint256 &b) const
    {
        return a.hash < b;
    }
    bool operator()(const uint256 &a, const TokenItemEntry &b) const
    {
        return a < b.hash;
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
            LOCK2(cs_main, wallet->cs_wallet);
            for(std::map<uint256, CTokenInfo>::iterator it = wallet->mapToken.begin(); it != wallet->mapToken.end(); ++it)
            {
                TokenItemEntry tokenItem(it->first, it->second);
                if(parent)
                {
                    tokenItem.update(parent->getTokenAbi());
                }
                cachedTokenItem.append(tokenItem);
            }
        }
        std::sort(cachedTokenItem.begin(), cachedTokenItem.end(), TokenItemEntryLessThan());
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
            cachedTokenItem[lowerIndex] = item;
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

TokenItemModel::TokenItemModel(CWallet *_wallet, WalletModel *parent):
    QAbstractItemModel(parent),
    tokenAbi(0),
    walletModel(parent),
    wallet(_wallet),
    priv(0),
    worker(0)
{
    columns << tr("Token Name") << tr("Token Symbol") << tr("Balance");
    tokenAbi = new Token();

    priv = new TokenItemPriv(wallet, this);
    priv->refreshTokenItem();

    worker = new TokenTxWorker(wallet);
    worker->moveToThread(&(t));

    t.start();

    subscribeToCoreSignals();
}

TokenItemModel::~TokenItemModel()
{
    unsubscribeFromCoreSignals();

    t.quit();
    t.wait();

    if(tokenAbi)
    {
        delete tokenAbi;
        tokenAbi = 0;
    }

    if(priv)
    {
        delete priv;
        priv = 0;
    }
}

QModelIndex TokenItemModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    TokenItemEntry *data = priv->index(row);
    if(data)
    {
        return createIndex(row, column, priv->index(row));
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
    return priv->size();
}

int TokenItemModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant TokenItemModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    TokenItemEntry *rec = static_cast<TokenItemEntry*>(index.internalPointer());

    switch (role) {
    case Qt::DisplayRole:
    switch(index.column())
    {
    case Name:
        return rec->tokenName;
    case Symbol:
        return rec->tokenSymbol;
    case Balance:
        return BitcoinUnits::formatToken(rec->decimals, rec->balance, false, BitcoinUnits::separatorAlways);
    default:
        break;
    }
        break;
    case TokenItemModel::HashRole:
        return QString::fromStdString(rec->hash.ToString());
        break;
    case TokenItemModel::AddressRole:
        return rec->contractAddress;
        break;
    case TokenItemModel::NameRole:
        return rec->tokenName;
        break;
    case TokenItemModel::SymbolRole:
        return rec->tokenSymbol;
        break;
    case TokenItemModel::DecimalsRole:
        return rec->decimals;
        break;
    case TokenItemModel::SenderRole:
        return rec->senderAddress;
        break;
    case TokenItemModel::BalanceRole:
        return BitcoinUnits::formatToken(rec->decimals, rec->balance, false, BitcoinUnits::separatorAlways);
        break;
    case TokenItemModel::RawBalanceRole:
        return QString::fromStdString(rec->balance.str());
        break;
    default:
        break;
    }

    return QVariant();
}

Token *TokenItemModel::getTokenAbi()
{
    return tokenAbi;
}

void TokenItemModel::updateToken(const QString &hash, int status, bool showToken)
{
    // Find token in wallet
    LOCK2(cs_main, wallet->cs_wallet);

    uint256 updated;
    updated.SetHex(hash.toStdString());
    std::map<uint256, CTokenInfo>::iterator mi = wallet->mapToken.find(updated);
    showToken &= mi != wallet->mapToken.end();

    TokenItemEntry tokenEntry;
    if(showToken)
    {
        tokenEntry = TokenItemEntry(mi->first, mi->second);
        tokenEntry.update(getTokenAbi());
    }
    else
    {
        tokenEntry.hash = updated;
    }
    priv->updateEntry(tokenEntry, status);
}

void TokenItemModel::checkTokenBalanceChanged()
{
    if(!priv && !tokenAbi)
        return;

    for(int i = 0; i < priv->cachedTokenItem.size(); i++)
    {
        TokenItemEntry tokenEntry = priv->cachedTokenItem[i];
        bool modified = false;
        tokenEntry.update(tokenAbi, modified);
        if(modified)
        {
            priv->cachedTokenItem[i] = tokenEntry;
            emitDataChanged(i);
        }

        // Search for token transactions
        if(fLogEvents)
        {
            QString hash = QString::fromStdString(tokenEntry.hash.ToString());
            QMetaObject::invokeMethod(worker, "updateTokenTx", Qt::QueuedConnection,
                                      Q_ARG(QString, hash));
        }
    }
}

void TokenItemModel::emitDataChanged(int idx)
{
    Q_EMIT dataChanged(index(idx, 0, QModelIndex()), index(idx, columns.length()-1, QModelIndex()));
}

struct TokenNotification
{
public:
    TokenNotification() {}
    TokenNotification(uint256 _hash, ChangeType _status, bool _showToken):
        hash(_hash), status(_status), showToken(_showToken) {}

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
    ChangeType status;
    bool showToken;
};

static void NotifyTokenChanged(TokenItemModel *tim, CWallet *wallet, const uint256 &hash, ChangeType status)
{
    // Find token in wallet
    LOCK2(cs_main, wallet->cs_wallet);
    std::map<uint256, CTokenInfo>::iterator mi = wallet->mapToken.find(hash);
    bool showToken = mi != wallet->mapToken.end();

    TokenNotification notification(hash, status, showToken);
    notification.invoke(tim);
}

void TokenItemModel::subscribeToCoreSignals()
{
    // Connect signals to wallet
    wallet->NotifyTokenChanged.connect(boost::bind(NotifyTokenChanged, this, _1, _2, _3));
}

void TokenItemModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from wallet
    wallet->NotifyTokenChanged.disconnect(boost::bind(NotifyTokenChanged, this, _1, _2, _3));
}
