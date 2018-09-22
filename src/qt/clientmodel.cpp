// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "clientmodel.h"

#include "guiconstants.h"
#include "peertablemodel.h"
#include "bantablemodel.h"

#include "alert.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "clientversion.h"
#include "main.h"
#include "net.h"
#include "ui_interface.h"
#include "util.h"

#include "i2pwrapper.h"            // Include for i2p interface

#include <stdint.h>

#include <QDateTime>
#include <QDebug>
#include <QTimer>

static const int64_t nClientStartupTime = GetTime();

ClientModel::ClientModel(OptionsModel* optionsModel, QObject* parent) : QObject(parent),
                                                                        optionsModel(optionsModel),
                                                                        peerTableModel(0),
                                                                        banTableModel(0),
                                                                        cachedNumBlocks(0),
                                                                        cachedMasternodeCountString(""),
                                                                        cachedReindexing(0), cachedImporting(0),
                                                                        numBlocksAtStartup(-1), pollTimer(0)
{
    peerTableModel = new PeerTableModel(this);
    banTableModel = new BanTableModel(this);
    pollTimer = new QTimer(this);
    connect(pollTimer, SIGNAL(timeout()), this, SLOT(updateTimer()));
    pollTimer->start(MODEL_UPDATE_DELAY);

    pollMnTimer = new QTimer(this);
    connect(pollMnTimer, SIGNAL(timeout()), this, SLOT(updateMnTimer()));
    // no need to update as frequent as data for balances/txes/blocks
    pollMnTimer->start(MODEL_UPDATE_DELAY * 4);

    subscribeToCoreSignals();
}

ClientModel::~ClientModel()
{
    unsubscribeFromCoreSignals();
}

int ClientModel::getNumConnections(unsigned int flags) const
{
    // Use local variable to avoid the lock here. This will save
    // unneccessary time to wait for the lock 
    vector<CNode*> vNodesCopy = vNodes;

    if (flags == CONNECTIONS_ALL) // Shortcut if we want total
        return vNodesCopy.size();

    int nNum = 0;
    bool fI2pSum = flags & CONNECTIONS_I2P_ALL;
    // Set this flags outside the loop, its faster than recomputing them every iteration
    bool fMatchI2pInbound = flags & CONNECTIONS_I2P_IN;
    bool fMatchI2pOutbound = flags & CONNECTIONS_I2P_OUT;
    for (CNode* pnode : vNodesCopy) {
        if( fI2pSum && pnode->addr.IsI2P() ) {
            if( pnode->fInbound ) {
                if( fMatchI2pInbound ) nNum++;
            } else if( fMatchI2pOutbound ) nNum++;
        } else if(flags & (pnode->fInbound ? CONNECTIONS_IN : CONNECTIONS_OUT))
            nNum++;
    }
    return nNum;
}

int ClientModel::getNumBlocks() const
{
    LOCK(cs_main);
    return chainActive.Height();
}

int ClientModel::getNumBlocksAtStartup()
{
    if (numBlocksAtStartup == -1) numBlocksAtStartup = getNumBlocks();
    return numBlocksAtStartup;
}

quint64 ClientModel::getTotalBytesRecv() const
{
    return CNode::GetTotalBytesRecv();
}

quint64 ClientModel::getTotalBytesSent() const
{
    return CNode::GetTotalBytesSent();
}

QDateTime ClientModel::getLastBlockDate() const
{
    LOCK(cs_main);
    if (chainActive.Tip())
        return QDateTime::fromTime_t(chainActive.Tip()->GetBlockTime());
    else
        return QDateTime::fromTime_t(Params().GenesisBlock().GetBlockTime()); // Genesis block's time of current network
}

double ClientModel::getVerificationProgress() const
{
    LOCK(cs_main);
    return Checkpoints::GuessVerificationProgress(Params().Checkpoints(), chainActive.Tip());
}

void ClientModel::updateTimer()
{
    // Get required lock upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if (!lockMain)
        return;

    // Some quantities (such as number of blocks) change so fast that we don't want to be notified for each change.
    // Periodically check and update with a timer.
    int newNumBlocks = getNumBlocks();

    // check for changed number of blocks we have, number of blocks peers claim to have, reindexing state and importing state
    if (cachedNumBlocks != newNumBlocks || cachedReindexing != fReindex || cachedImporting != fImporting) {
        cachedNumBlocks = newNumBlocks;
        cachedReindexing = fReindex;
        cachedImporting = fImporting;

        emit numBlocksChanged(newNumBlocks);
    }

    emit bytesChanged(getTotalBytesRecv(), getTotalBytesSent());
}

void ClientModel::updateMnTimer()
{
#if 0
    // Get required lock upfront. This avoids the GUI from getting stuck on
    // periodical polls if the core is holding the locks for a longer time -
    // for example, during a wallet rescan.
    TRY_LOCK(cs_main, lockMain);
    if (!lockMain)
        return;

    QString newMasternodeCountString = getMasternodeCountString();

    if (cachedMasternodeCountString != newMasternodeCountString) {
        cachedMasternodeCountString = newMasternodeCountString;

        emit strMasternodesChanged(cachedMasternodeCountString);
    }
#endif
}

void ClientModel::updateNumConnections(int numConnections)
{
    emit numConnectionsChanged(numConnections);
}

void ClientModel::updateNetworkActive(bool networkActive)
{
    emit networkActiveChanged(networkActive);
}

void ClientModel::updateAlert(const QString &hash, int status)
{
    // Show error message notification for new alert
    if (status == CT_NEW) {
        uint256 hash_256;
        hash_256.SetHex(hash.toStdString());
        CAlert alert = CAlert::getAlertByHash(hash_256);
        if (!alert.IsNull()) {
            emit message(tr("Network Alert"), QString::fromStdString(alert.strStatusBar), CClientUIInterface::ICON_ERROR);
        }
    }

    emit alertsChanged(getStatusBarWarnings());
}

void ClientModel::getGasInfo(uint64_t& blockGasLimit, uint64_t& minGasPrice, uint64_t& nGasPrice) const
{
    LOCK(cs_main);

    LuxDGP luxDGP(globalState.get(), fGettingValuesDGP);
    blockGasLimit = luxDGP.getBlockGasLimit(chainActive.Height());
    minGasPrice = CAmount(luxDGP.getMinGasPrice(chainActive.Height()));
    nGasPrice = (minGasPrice>DEFAULT_GAS_PRICE) ? minGasPrice : DEFAULT_GAS_PRICE;
}

bool ClientModel::inInitialBlockDownload() const
{
    return IsInitialBlockDownload();
}

enum BlockSource ClientModel::getBlockSource() const
{
    if (fReindex)
        return BLOCK_SOURCE_REINDEX;
    else if (fImporting)
        return BLOCK_SOURCE_DISK;
    else if (getNumConnections() > 0)
        return BLOCK_SOURCE_NETWORK;

    return BLOCK_SOURCE_NONE;
}

void ClientModel::setNetworkActive(bool active)
{
    SetNetworkActive(active);
}

bool ClientModel::getNetworkActive() const
{
    return fNetworkActive;
}

QString ClientModel::getStatusBarWarnings() const
{
    return QString::fromStdString(GetWarnings("statusbar"));
}

OptionsModel* ClientModel::getOptionsModel()
{
    return optionsModel;
}

PeerTableModel* ClientModel::getPeerTableModel()
{
    return peerTableModel;
}

BanTableModel *ClientModel::getBanTableModel()
{
    return banTableModel;
}

QString ClientModel::formatFullVersion() const
{
    return QString::fromStdString(FormatFullVersion());
}

QString ClientModel::formatBuildDate() const
{
    return QString::fromStdString(CLIENT_DATE);
}

bool ClientModel::isReleaseVersion() const
{
    return CLIENT_VERSION_IS_RELEASE;
}

QString ClientModel::clientName() const
{
    return QString::fromStdString(CLIENT_NAME);
}

QString ClientModel::formatClientStartupTime() const
{
    return QDateTime::fromTime_t(nClientStartupTime).toString();
}

void ClientModel::updateBanlist()
{
    banTableModel->refresh();
    emit banListChanged();
}

/**********************************************************************
 *          These I2P functions handle values for the view
 */
QString ClientModel::formatI2PNativeFullVersion() const
{
    return QString::fromStdString(FormatI2PNativeFullVersion());
}

QString ClientModel::getPublicI2PKey() const
{
    return IsI2PEnabled() ? QString::fromStdString(I2PSession::Instance().getMyDestination().pub) : QString( "Not Available" );
}

QString ClientModel::getPrivateI2PKey() const
{
    return IsI2PEnabled() ? QString::fromStdString(I2PSession::Instance().getMyDestination().priv) : QString( "Not Available" );
}

bool ClientModel::isI2PAddressGenerated() const
{
    return IsI2PEnabled() ? I2PSession::Instance().getMyDestination().isGenerated : false;
}

bool ClientModel::isI2POnly() const
{
    return IsI2POnly();
}

bool ClientModel::isTorOnly() const
{
    return IsTorOnly();
}

bool ClientModel::isDarknetOnly() const
{
    return IsDarknetOnly();
}

bool ClientModel::isBehindDarknet() const
{
    return IsBehindDarknet();
}

QString ClientModel::getB32Address(const QString& destination) const
{
    return  IsI2PEnabled() ? QString::fromStdString(I2PSession::GenerateB32AddressFromDestination(destination.toStdString())) : QString( "Not Available" );
}

void ClientModel::generateI2PDestination(QString& pub, QString& priv) const
{
    SAM::FullDestination generatedDest( "Not Available", "Not Available", false );
    if( IsI2PEnabled() )
        generatedDest = I2PSession::Instance().destGenerate();
    pub = QString::fromStdString(generatedDest.pub);
    priv = QString::fromStdString(generatedDest.priv);
}

// Handlers for core signals
static void ShowProgress(ClientModel* clientmodel, const std::string& title, int nProgress)
{
    // emits signal "showProgress"
    QMetaObject::invokeMethod(clientmodel, "showProgress", Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(title)),
        Q_ARG(int, nProgress));
}

static void NotifyNumConnectionsChanged(ClientModel* clientmodel, int newNumConnections)
{
    // Too noisy: qDebug() << "NotifyNumConnectionsChanged : " + QString::number(newNumConnections);
    QMetaObject::invokeMethod(clientmodel, "updateNumConnections", Qt::QueuedConnection,
        Q_ARG(int, newNumConnections));
}

static void NotifyNetworkActiveChanged(ClientModel *clientmodel, bool networkActive)
{
    QMetaObject::invokeMethod(clientmodel, "updateNetworkActive", Qt::QueuedConnection,
                              Q_ARG(bool, networkActive));
}

static void NotifyAlertChanged(ClientModel *clientmodel, const uint256 &hash, ChangeType status)
{
    qDebug() << "NotifyAlertChanged : " + QString::fromStdString(hash.GetHex()) + " status=" + QString::number(status);
    QMetaObject::invokeMethod(clientmodel, "updateAlert", Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(hash.GetHex())),
        Q_ARG(int, status));
}

static void BannedListChanged(ClientModel *clientmodel)
{
    qDebug() << "BannedListChanged";
    QMetaObject::invokeMethod(clientmodel, "updateBanlist", Qt::QueuedConnection);
}

void ClientModel::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ShowProgress.connect(boost::bind(ShowProgress, this, _1, _2));
    uiInterface.NotifyNumConnectionsChanged.connect(boost::bind(NotifyNumConnectionsChanged, this, _1));
    uiInterface.NotifyNetworkActiveChanged.connect(boost::bind(NotifyNetworkActiveChanged, this, _1));
    uiInterface.BannedListChanged.connect(boost::bind(BannedListChanged, this));
    uiInterface.NotifyAlertChanged.connect(boost::bind(NotifyAlertChanged, this, _1, _2));

}

void ClientModel::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ShowProgress.disconnect(boost::bind(ShowProgress, this, _1, _2));
    uiInterface.NotifyNumConnectionsChanged.disconnect(boost::bind(NotifyNumConnectionsChanged, this, _1));
    uiInterface.NotifyNetworkActiveChanged.disconnect(boost::bind(NotifyNetworkActiveChanged, this, _1));
    uiInterface.BannedListChanged.disconnect(boost::bind(BannedListChanged, this));
    uiInterface.NotifyAlertChanged.disconnect(boost::bind(NotifyAlertChanged, this, _1, _2));
}
