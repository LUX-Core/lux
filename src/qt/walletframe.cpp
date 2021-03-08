// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletframe.h"

#include "bitcoingui.h"
#include "walletview.h"
#include "wallet.h"
#include <cstdio>

#include <QHBoxLayout>
#include <QSettings>
#include <QLabel>

WalletFrame::WalletFrame(const PlatformStyle *platformStyle, BitcoinGUI *_gui) :
    QFrame(_gui),
    gui(_gui),
    platformStyle(platformStyle)
{
    // Leave HBox hook for adding a list view later
    QHBoxLayout* walletFrameLayout = new QHBoxLayout(this);
    setContentsMargins(0, 0, 0, 0);
    walletStack = new QStackedWidget(this);
    walletStack->setObjectName("walletStack");
	
	QSettings settings;
	if (settings.value("theme").toString() == "dark grey") {
		walletStack->setStyleSheet("#walletStack { border: 1px solid #fff5f5; margin-top: -1px !important }");
	} else if (settings.value("theme").toString() == "dark blue") {
		walletStack->setStyleSheet("#walletStack { border: 1px solid #fff5f5; margin-top: -1px !important }");
	} else { 
		walletStack->setStyleSheet("#walletStack {border: 1px solid #c4c1bd;}");
	}
    
    walletFrameLayout->setContentsMargins(0,0,0,0);
    walletFrameLayout->addWidget(walletStack);

    QLabel* noWallet = new QLabel(tr("No wallet has been loaded."));
    noWallet->setAlignment(Qt::AlignCenter);
    walletStack->addWidget(noWallet);
}

WalletFrame::~WalletFrame()
{
}

void WalletFrame::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;
}

bool WalletFrame::addWallet(const QString& name, WalletModel* walletModel)
{
    if (!gui || !clientModel || !walletModel || mapWalletViews.count(name) > 0)
        return false;

    WalletView *walletView = new WalletView(platformStyle, this);
    walletView->setBitcoinGUI(gui);
    walletView->setClientModel(clientModel);
    walletView->setWalletModel(walletModel);
    walletView->showOutOfSyncWarning(bOutOfSync);

    WalletView* CurrentWalletView = currentWalletView();
    if (CurrentWalletView) {
        walletView->setCurrentIndex(CurrentWalletView->currentIndex());
    } else {
        walletView->gotoOverviewPage();
    }

    walletStack->addWidget(walletView);
    mapWalletViews[name] = walletView;

    // Ensure a walletView is able to show the main window
    connect(walletView, SIGNAL(showNormalIfMinimized()), gui, SLOT(showNormalIfMinimized()));
    connect(walletView, SIGNAL(outOfSyncWarningClicked()), this, SLOT(outOfSyncWarningClicked()));

    return true;
}

bool WalletFrame::setCurrentWallet(const QString& name)
{
    if (mapWalletViews.count(name) == 0)
        return false;

    WalletView* walletView = mapWalletViews.value(name);
    walletStack->setCurrentWidget(walletView);
    walletView->updateEncryptionStatus();
    return true;
}

bool WalletFrame::removeWallet(const QString& name)
{
    if (mapWalletViews.count(name) == 0)
        return false;

    WalletView* walletView = mapWalletViews.take(name);
    walletStack->removeWidget(walletView);
    delete walletView;
    return true;
}

void WalletFrame::removeAllWallets()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        walletStack->removeWidget(i.value());
    mapWalletViews.clear();
}

bool WalletFrame::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    WalletView* walletView = currentWalletView();
    if (!walletView)
        return false;

    return walletView->handlePaymentRequest(recipient);
}

void WalletFrame::showOutOfSyncWarning(bool fShow)
{
    bOutOfSync = fShow;
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->showOutOfSyncWarning(fShow);
}

void WalletFrame::gotoOverviewPage()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoOverviewPage();
}

void WalletFrame::gotoHistoryPage()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoHistoryPage();
}

void WalletFrame::gotoMasternodePage() // Masternode list
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoMasternodePage();
}

void WalletFrame::gotoLSRTokenPage(bool toAddTokenPage)
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoLSRTokenPage(toAddTokenPage);
}

void WalletFrame::gotoSmartContractPage()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoSmartContractPage();
}

void WalletFrame::gotoBlockExplorerPage()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoBlockExplorerPage();
}

void WalletFrame::gotoReceiveCoinsPage()
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoReceiveCoinsPage();
}

void WalletFrame::gotoSendCoinsPage(QString addr)
{
    QMap<QString, WalletView*>::const_iterator i;
    for (i = mapWalletViews.constBegin(); i != mapWalletViews.constEnd(); ++i)
        i.value()->gotoSendCoinsPage(addr);
}

void WalletFrame::gotoSignMessageTab(QString addr)
{
    WalletView* walletView = currentWalletView();
    if (walletView)
        walletView->gotoSignMessageTab(addr);
}

void WalletFrame::gotoVerifyMessageTab(QString addr)
{
    WalletView* walletView = currentWalletView();
    if (walletView)
        walletView->gotoVerifyMessageTab(addr);
}

void WalletFrame::gotoBip38Tool()
{
    WalletView* walletView = currentWalletView();
    if (walletView)
        walletView->gotoBip38Tool();
}

void WalletFrame::gotoMultiSendDialog()
{
    WalletView* walletView = currentWalletView();

    if (walletView)
        walletView->gotoMultiSendDialog();
}


void WalletFrame::encryptWallet(bool status)
{
    WalletView* walletView = currentWalletView();
    if (walletView)
        walletView->encryptWallet(status);
}

void WalletFrame::backupWallet()
{
    WalletView* walletView = currentWalletView();
    if (walletView)
        walletView->backupWallet();
}

void WalletFrame::restoreWallet()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->restoreWallet();
}

void WalletFrame::changePassphrase()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->changePassphrase();
}

void WalletFrame::unlockWallet()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->unlockWallet();
}

void WalletFrame::lockWallet()
{
    WalletView *walletView = currentWalletView();
    if (walletView) {
        walletView->lockWallet();
        fWalletUnlockStakingOnly = false;
    }
}

void WalletFrame::usedSendingAddresses()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->usedSendingAddresses();
}

void WalletFrame::usedReceivingAddresses()
{
    WalletView *walletView = currentWalletView();
    if (walletView)
        walletView->usedReceivingAddresses();
}

WalletView *WalletFrame::currentWalletView()
{
    return qobject_cast<WalletView*>(walletStack->currentWidget());
}

void WalletFrame::outOfSyncWarningClicked()
{
    Q_EMIT requestedSyncWarningInfo();
}
