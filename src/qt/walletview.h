// Copyright (c) 2018 The Luxcore Developer
// Copyright (c) 2018 The Luxcore Developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_WALLETVIEW_H
#define BITCOIN_QT_WALLETVIEW_H

#include "amount.h"
#include "masternodemanager.h"
#include "smartcontract.h"

#include <QStackedWidget>

class BitcoinGUI;
class ClientModel;
class OverviewPage;
class PlatformStyle;
class ReceiveCoinsDialog;
class SendCoinsDialog;
class SendCoinsRecipient;
class TransactionView;
class WalletModel;
class BlockExplorer;
class CreateContract;
class SendToContract;
class CallContractPage;
class LSRToken;


QT_BEGIN_NAMESPACE
class QLabel;
class QModelIndex;
class QProgressDialog;
QT_END_NAMESPACE

/*
  WalletView class. This class represents the view to a single wallet.
  It was added to support multiple wallet functionality. Each wallet gets its own WalletView instance.
  It communicates with both the client and the wallet models to give the user an up-to-date view of the
  current core state.
*/
class WalletView : public QStackedWidget
{
    Q_OBJECT

public:
    explicit WalletView(const PlatformStyle *platformStyle, QWidget *parent);
    ~WalletView();

    void setBitcoinGUI(BitcoinGUI* gui);
    /** Set the client model.
        The client model represents the part of the core that communicates with the P2P network, and is wallet-agnostic.
    */
    void setClientModel(ClientModel* clientModel);
    /** Set the wallet model.
        The wallet model represents a bitcoin wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    void setWalletModel(WalletModel* walletModel);
    WalletModel *getWalletModel() { return walletModel; }
 
    bool handlePaymentRequest(const SendCoinsRecipient& recipient);

    void showOutOfSyncWarning(bool fShow);

private:
    ClientModel* clientModel;
    WalletModel* walletModel;

    OverviewPage* overviewPage;
    SmartContract* smartContractPage;
    LSRToken* LSRTokenPage;
    QWidget* transactionsPage;
    ReceiveCoinsDialog* receiveCoinsPage;
    SendCoinsDialog* sendCoinsPage;
    BlockExplorer* explorerWindow;
    MasternodeManager* masternodeManagerPage;

    TransactionView* transactionView;

    QProgressDialog* progressDialog;
    QLabel* transactionSum;
    const PlatformStyle* platformStyle;

public Q_SLOTS:
    /** Switch to overview (home) page */
    void gotoOverviewPage();
    /** Switch to history (transactions) page */
    void gotoHistoryPage();
    /** Switch to masternode page */
    void gotoMasternodePage();
    /** Switch to smart contract page */
    void gotoSmartContractPage();
    /** Switch to LSRToken page */
    void gotoLSRTokenPage(bool toAddTokenPage);
    /** Switch to explorer page */
    void gotoBlockExplorerPage();
    /** Switch to receive coins page */
    void gotoReceiveCoinsPage();
    /** Switch to send coins page */
    void gotoSendCoinsPage(QString addr = "");

    /** Show Sign/Verify Message dialog and switch to sign message tab */
    void gotoSignMessageTab(QString addr = "");
    /** Show Sign/Verify Message dialog and switch to verify message tab */
    void gotoVerifyMessageTab(QString addr = "");
    /** Show MultiSend Dialog */
    void gotoMultiSendDialog();

    /** Show BIP 38 tool - default to Encryption tab */
    void gotoBip38Tool();

    /** Show incoming transaction notification for new transactions.

        The new items are those between start and end inclusive, under the given parent item.
    */
    void processNewTransaction(const QModelIndex& parent, int start, int /*end*/);

    /** Show incoming token transaction notification for new token transactions.

        The new items are those between start and end inclusive, under the given parent item.
    */
    void processNewTokenTransaction(const QModelIndex& parent, int start, int /*end*/);

    /** Encrypt the wallet */
    void encryptWallet(bool status);
    /** Backup the wallet */
    void backupWallet();
    /** Restore the wallet */
    void restoreWallet();
    /** Change encrypted wallet passphrase */
    void changePassphrase();
    /** Ask for passphrase to unlock wallet temporarily */
    void unlockWallet();
    /** Lock wallet */
    void lockWallet();

    /** Show used sending addresses */
    void usedSendingAddresses();
    /** Show used receiving addresses */
    void usedReceivingAddresses();

    /** Re-emit encryption status signal */
    void updateEncryptionStatus();

    /** Show progress dialog e.g. for rescan */
    void showProgress(const QString& title, int nProgress);

    /** User has requested more information about the out of sync state */
    void requestedSyncWarningInfo();

    /** Update selected LUX amount from transactionview */
    void trxAmount(QString amount);

signals:
    /** Signal that we want to show the main window */
    void showNormalIfMinimized();
    /**  Fired when a message should be reported to the user */
    void message(const QString& title, const QString& message, unsigned int style);
    /** Encryption status of wallet changed */
    void encryptionStatusChanged(int status);
    /** HD-Enabled status of wallet changed (only possible during startup) */
    void hdEnabledStatusChanged(int hdEnabled);
    /** Notify that a new transaction appeared */
    void incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label);
    /** Notify that a new token transaction appeared */
    void incomingTokenTransaction(const QString& date, const QString& amount, const QString& type, const QString& address, const QString& label, const QString& title);
    /** Notify that the out of sync warning icon has been pressed */
    void outOfSyncWarningClicked();
};

#endif // BITCOIN_QT_WALLETVIEW_H
