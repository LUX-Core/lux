// Copyright (c) 2011-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BITCOINGUI_H
#define BITCOIN_QT_BITCOINGUI_H

#if defined(HAVE_CONFIG_H)
#include "config/lux-config.h"
#endif

#include "amount.h"

#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QPoint>
#include <QPushButton>
#include <QSystemTrayIcon>

#ifdef ENABLE_UPDATER
#include "lux-installer/src/updatecontroller.h"
#endif

class ClientModel;
class NetworkStyle;
class Notificator;
class OptionsModel;
class BlockExplorer;
class PlatformStyle;
class RPCConsole;
class SendCoinsRecipient;
class UnitDisplayStatusBarControl;
class WalletFrame;
class WalletModel;
class HelpMessageDialog;
class ModalOverlay;
class MasternodeList;
class HexAddressConverter;
class CBlockIndex;
class UpdateDialog;
class HelpMessageDialog;


class CWallet;

QT_BEGIN_NAMESPACE
class QAction;
class QProgressBar;
class QProgressDialog;
class QDockWidget;
QT_END_NAMESPACE

/**
  Bitcoin GUI main class. This class represents the main window of the Bitcoin UI. It communicates with both the client and
  wallet models to give the user an up-to-date view of the current core state.
*/
class BitcoinGUI : public QMainWindow
{
    Q_OBJECT

public:
    static const QString DEFAULT_WALLET;
    static const std::string DEFAULT_UIPLATFORM;

    explicit BitcoinGUI(const PlatformStyle *platformStyle, const NetworkStyle* networkStyle, QWidget* parent = 0);
    ~BitcoinGUI();

    /** Set the client model.
        The client model represents the part of the core that communicates with the P2P network, and is wallet-agnostic.
    */
    void setClientModel(ClientModel* clientModel);

#ifdef ENABLE_WALLET
    /** Set the wallet model.
        The wallet model represents a bitcoin wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    bool addWallet(const QString& name, WalletModel* walletModel);
    bool setCurrentWallet(const QString& name);
    void removeAllWallets();
#endif // ENABLE_WALLET
    bool enableWallet;
    bool fMultiSend = false;

protected:
    void changeEvent(QEvent* e);
    void closeEvent(QCloseEvent* event);
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);
    bool eventFilter(QObject* object, QEvent* event);

private:
    ClientModel* clientModel;
    WalletFrame* walletFrame;

#ifdef ENABLE_UPDATER
    QtLuxUpdater::UpdateController *controller;
#endif

    UnitDisplayStatusBarControl* unitDisplayControl;
    QLabel* labelStakingIcon;
    QLabel* labelWalletEncryptionIcon;
    QPushButton* pushButtonWalletHDStatusIcon;
    QPushButton* labelConnectionsIcon;
    QPushButton * pushButtonTelegram;
    QPushButton * pushButtonTwitter;
    QPushButton * pushButtonGithub;
    QPushButton * pushButtonDiscord;
	QPushButton * pushButtonHelp;
    QLabel* labelBlocksIcon;
    QLabel* progressBarLabel;
    QProgressBar* progressBar;
    QProgressDialog* progressDialog;

    QMenuBar* appMenuBar;
    QAction* overviewAction;
    QAction* historyAction;
    QAction* masternodeAction;
    QAction* quitAction;
    QAction* sendCoinsAction;
    QAction* usedSendingAddressesAction;
    QAction* usedReceivingAddressesAction;
    QAction* signMessageAction;
    QAction* verifyMessageAction;
    QAction* bip38ToolAction;
    QAction* aboutAction;
    QAction* checkForUpdateAction;
    QAction* receiveCoinsAction;
    QAction* optionsAction;
    QAction* toggleHideAction;
    QAction* encryptWalletAction;
    QAction* backupWalletAction;
    QAction* restoreWalletAction;
    QAction* changePassphraseAction;
    QAction* unlockWalletAction;
    QAction* lockWalletAction;
    QAction* aboutQtAction;
    QAction* openInfoAction;
    QAction* openRPCConsoleAction;
    QAction* openNetworkAction;
    QAction* openPeersAction;
    QAction* openRepairAction;
    QAction* openConfEditorAction;
    QAction* openMNConfEditorAction;
    QAction* showBackupsAction;
    QAction* openAction;
    QAction* openBlockExplorerAction;
    QAction* showHelpMessageAction;
    QAction* multiSendAction;
    QAction* smartContractAction;
    QAction* LSRTokenAction;
    QAction* openHexAddressAction;

    QSystemTrayIcon* trayIcon;
    QMenu* trayIconMenu;
    Notificator* notificator;
    RPCConsole* rpcConsole;
    HelpMessageDialog* helpMessageDialog;
    BlockExplorer* explorerWindow;
    HexAddressConverter* hexAddressWindow;
    ModalOverlay* modalOverlay;
    UpdateDialog* updateDialog;

    /** Keep track of previous number of blocks, to detect progress */
    int prevBlocks;
    int spinnerFrame;
    const PlatformStyle* platformStyle;

    /** Create the main UI actions. */
    void createActions();
    /** Create the menu bar and sub-menus. */
    void createMenuBar();
    /** Create the toolbars */
    void createToolBars();
    /** Create system tray icon and notification */
    void createTrayIcon(const NetworkStyle* networkStyle);
    /** Create system tray menu (or setup the dock menu) */
    void createTrayIconMenu();

    /** Update UI with latest network info from model. */
    void updateNetworkState();

    /** Enable or disable all wallet-related actions */
    void setWalletActionsEnabled(bool enabled);

    /** Connect core signals to GUI client */
    void subscribeToCoreSignals();
    /** Disconnect core signals from GUI client */
    void unsubscribeFromCoreSignals();

    void updateHeadersSyncProgressLabel();
Q_SIGNALS:
    /** Signal raised when a URI was entered or dragged to the GUI */
    void receivedURI(const QString& uri);
    /** Restart handling */
    void requestedRestart(QStringList args);

public Q_SLOTS:
    /** Set number of connections shown in the UI */
    void setNumConnections(int count);
    /** Set network state shown in the UI */
    void setNetworkActive(bool networkActive);
    /** Get restart command-line parameters and request restart */
    void handleRestart(QStringList args);
    /** Set number of blocks and last block date shown in the UI */
    void setNumBlocks(int count, const QDateTime& lastBlockDate, double nVerificationProgress, bool headers);
    /** Set additional data sync status shown in the UI */
    //void setAdditionalDataSyncProgress(double nSyncProgress);

    /** Notify the user of an event from the core network or transaction handling code.
       @param[in] title     the message box / notification title
       @param[in] message   the displayed text
       @param[in] style     modality and style definitions (icon and used buttons - buttons only for message boxes)
                            @see CClientUIInterface::MessageBoxFlags
       @param[in] ret       pointer to a bool that will be modified to whether Ok was clicked (modal only)
    */
    void message(const QString& title, const QString& message, unsigned int style, bool* ret = NULL);

    void setStakingStatus();

#ifdef ENABLE_WALLET

    /** Set the hd-enabled status as shown in the UI.
     @param[in] status            current hd enabled status
     @see WalletModel::EncryptionStatus
     */
    void setHDStatus(int hdEnabled);

    /** Set the encryption status as shown in the UI.
       @param[in] status            current encryption status
       @see WalletModel::EncryptionStatus
    */
    void setEncryptionStatus(int status);

    bool handlePaymentRequest(const SendCoinsRecipient& recipient);

    /** Show incoming transaction notification for new transactions. */
    void incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label);

    /** Show incoming token transaction notification for new transactions. */
    void incomingTokenTransaction(const QString& date, const QString& amount, const QString& type, const QString& address, const QString& label, const QString& title);

#endif // ENABLE_WALLET

private Q_SLOTS:
#ifdef ENABLE_WALLET
    /** Switch to overview (home) page */
    void gotoOverviewPage();
    /** Switch to history (transactions) page */
    void gotoHistoryPage();
    /** Switch to Explorer Page */
    void gotoBlockExplorerPage();
    /** Switch to masternode page */
    void gotoMasternodePage();
    /** Switch to smart contract page */
    void gotoSmartContractPage();
    /** Switch to LSRToken page page */
    void gotoLSRTokenPage(bool toAddTokenPage = false);
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

    /** Show open dialog */
    void openClicked();

#endif // ENABLE_WALLET
    /** Show configuration dialog */
    void optionsClicked();

    /** Show about dialog */
    void aboutClicked();

    /** Show updater dialog if any */
    void updaterClicked();

    /** Show help message dialog */
    void showHelpMessageClicked();

    /** Show debug window */
    void showDebugWindow();

    /** Show debug window and set focus to the console */
    void showDebugWindowActivateConsole();

#ifndef Q_OS_MAC
    /** Handle tray icon clicked */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#endif

    /** Show window if hidden, unminimize when minimized, rise when obscured or show if hidden and fToggleHidden is true */
    void showNormalIfMinimized(bool fToggleHidden = false);
    /** Simply calls showNormalIfMinimized(true) for use in SLOT() macro */
    void toggleHidden();

    /** called by a timer to check if fRequestShutdown has been set **/
    void detectShutdown();

    /** Show progress dialog e.g. for verifychain */
    void showProgress(const QString& title, int nProgress);

    /** When hideTrayIcon setting is changed in OptionsModel hide or show the icon accordingly. */
    void setTrayIconVisible(bool);

    /** Toggle networking */
    void toggleNetworkActive();

    void showModalOverlay();
};

class UnitDisplayStatusBarControl : public QLabel
{
    Q_OBJECT

public:
    explicit UnitDisplayStatusBarControl(const PlatformStyle* platformStyle);
    /** Lets the control know about the Options Model (and its signals) */
    void setOptionsModel(OptionsModel* optionsModel);

protected:
    /** So that it responds to left-button clicks */
    void mousePressEvent(QMouseEvent* event);

private:
    OptionsModel* optionsModel;
    QMenu* menu;

    /** Shows context menu with Display Unit options by the mouse coordinates */
    void onDisplayUnitsClicked(const QPoint& point);
    /** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
    void createContextMenu();

private Q_SLOTS:
    /** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
    void updateDisplayUnit(int newUnits);
    /** Tells underlying optionsModel to update its current display unit. */
    void onMenuSelection(QAction* action);
};

#endif // BITCOIN_QT_BITCOINGUI_H
