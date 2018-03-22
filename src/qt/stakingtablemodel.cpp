#include "stakingtablemodel.h"
#include "stakingfilterproxy.h"
#include "transactiontablemodel.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "transactiondesc.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "util.h"
#include "stakingdialog.h"

#include "wallet.h"

#include <QLocale>
#include <QList>
#include <QColor>
#include <QTimer>
#include <QIcon>
#include <QDateTime>
#include <QtAlgorithms>

extern double GetDifficulty(const CBlockIndex* blockindex);

static int column_alignments[] = {
    Qt::AlignLeft|Qt::AlignVCenter,
    Qt::AlignLeft|Qt::AlignVCenter,
    Qt::AlignLeft|Qt::AlignVCenter,
    Qt::AlignLeft|Qt::AlignVCenter,
    Qt::AlignLeft|Qt::AlignVCenter,
    Qt::AlignLeft|Qt::AlignVCenter,
    Qt::AlignLeft|Qt::AlignVCenter
};

struct TxLessThan
{
    
};

class StakingTablePriv
{
public:
    StakingTablePriv(CWallet *wallet, StakingTableModel *parent):
        wallet(wallet),
        parent(parent)
    {
    }
    CWallet *wallet;
    StakingTableModel *parent;


    void refreshWallet()
    {

    }

     /* Update our model of the wallet incrementally, to synchronize our model of the wallet
       with that of the core.
       Call with list of hashes of transactions that were added, removed or changed.
     */
    void updateWallet(const QList<uint256> &updated)
    {
    }
};


StakingTableModel::StakingTableModel(CWallet *wallet, WalletModel *parent):
        QAbstractTableModel(parent),
        wallet(wallet),
        walletModel(parent),
        stakingInterval(10),
        priv(new StakingTablePriv(wallet, this))
{
    columns << tr("Transaction") <<  tr("Address") << tr("Balance") << tr("Age") << tr("Coin Days") << tr("Stake Probability"); //Removed Stake Reward temporarily
    priv->refreshWallet();

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(update()));
    timer->start(MODEL_UPDATE_DELAY);
}

StakingTableModel::~StakingTableModel()
{
    delete priv;
}

void StakingTableModel::update()
{
    QList<uint256> updated;

    // Check if there are changes to wallet map
    {
        TRY_LOCK(wallet->cs_wallet, lockWallet);
        if (lockWallet && !wallet->vStakingWalletUpdated.empty())
        {
            BOOST_FOREACH(uint256 hash, wallet->vStakingWalletUpdated)
            {
                updated.append(hash);
            }
            wallet->vStakingWalletUpdated.clear();
        }
    }

    if(!updated.empty())
    {
        priv->updateWallet(updated);
    }
}


int StakingTableModel::rowCount(const QModelIndex &parent) const
{
    return 0;
}

int StakingTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return columns.length();
}

QVariant StakingTableModel::data(const QModelIndex &index, int role) const
{
    if(!index.isValid())
        return QVariant();

    switch(role)
    {
      case Qt::DisplayRole:
        switch(index.column())
        {
        
        }
        break;
      case Qt::TextAlignmentRole:
        return column_alignments[index.column()];
        break;
      case Qt::ToolTipRole:
        switch(index.column())
        {
        case MintProbability:
            int interval = this->stakingInterval;
            QString unit = tr("minutes");

            int hours = interval / 60;
            int days = hours  / 24;

            if(hours > 1) {
                interval = hours;
                unit = tr("hours");
            }
            if(days > 1) {
                interval = days;
                unit = tr("days");
            }

            QString str = QString(tr("You have %1 chance to find a POS block if you stake %2 %3 at current difficulty."));
            return str.arg(index.data().toString().toUtf8().constData()).arg(interval).arg(unit);
        }
        break;
      case Qt::EditRole:
        switch(index.column())
        {
        }
        break;
      case Qt::BackgroundColorRole:
        {
            return COLOR_MINT_OLD;
        }
        break;

    }
    return QVariant();
}

void StakingTableModel::setStakingInterval(int interval)
{
    stakingInterval = interval;
}

QString StakingTableModel::lookupAddress(const std::string &address, bool tooltip) const
{
    QString label = walletModel->getAddressTableModel()->labelForAddress(QString::fromStdString(address));
    QString description;
    if(!label.isEmpty())
    {
        description += label + QString(" ");
    }
    return description;
}

QVariant StakingTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal)
    {
        if(role == Qt::DisplayRole)
        {
            return columns[section];
        }
        else if (role == Qt::TextAlignmentRole)
        {
            return column_alignments[section];
        } else if (role == Qt::ToolTipRole)
        {
            switch(section)
            {
            case Address:
                return tr("Destination address of the output.");
            case TxHash:
                return tr("Original transaction ID.");
            case Age:
                return tr("Age of the transaction in days.");
            case Balance:
                return tr("Balance of the output.");
            case CoinDay:
                return tr("Coin age in the output.");
            case MintProbability:
                return tr("Chance to stake a block within given time interval.");
            }
        }
    }
    return QVariant();
}

QModelIndex StakingTableModel::index(int row, int column, const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    {
        return QModelIndex();
    }
}
