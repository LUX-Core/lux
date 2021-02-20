// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoingui.h"

#include "bitcoinunits.h"
#include "chainparams.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "modaloverlay.h"
#include "miner.h"
#include "networkstyle.h"
#include "notificator.h"
#include "openuridialog.h"
#include "optionsdialog.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "rpcconsole.h"
#include "utilitydialog.h"
#include "stake.h"
#include "main.h"
#include "hexaddressconverter.h"

#ifdef ENABLE_WALLET
#include "blockexplorer.h"
#include "walletframe.h"
#include "walletmodel.h"
#include "wallet.h"
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "init.h"
#include "masternodemanager.h"
#include "ui_interface.h"
#include "util.h"
#include "updatedialog.h"

#include <iostream>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QIcon>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QToolButton>
#include <QDockWidget>
#include <QSizeGrip>
#include <QDesktopServices>
#include <QProcess>

#if QT_VERSION < 0x050000
#include <QTextDocument>
#include <QUrl>
#else
#include <QUrlQuery>
#endif

const std::string BitcoinGUI::DEFAULT_UIPLATFORM =
#if defined(Q_OS_MAC)
        "macosx"
#elif defined(Q_OS_WIN)
        "windows"
#else
        "other"
#endif
        ;

const QString BitcoinGUI::DEFAULT_WALLET = "~Default";



BitcoinGUI::BitcoinGUI(const PlatformStyle *platformStyle, const NetworkStyle* networkStyle, QWidget* parent) : QMainWindow(parent),
                                                                            enableWallet(false),
                                                                            clientModel(0),
                                                                            walletFrame(0),
                                                                            unitDisplayControl(0),
                                                                            timerStakingIcon(0),
                                                                            labelStakingIcon(0),
                                                                            labelWalletEncryptionIcon(0),
                                                                            pushButtonWalletHDStatusIcon(0),
                                                                            labelConnectionsIcon(0),
                                                                            labelBlocksIcon(0),
                                                                            progressBarLabel(0),
                                                                            progressBar(0),
                                                                            progressDialog(0),
                                                                            appMenuBar(0),
                                                                            overviewAction(0),
                                                                            historyAction(0),
                                                                            masternodeAction(0),
                                                                            quitAction(0),
                                                                            sendCoinsAction(0),
                                                                            usedSendingAddressesAction(0),
                                                                            usedReceivingAddressesAction(0),
                                                                            signMessageAction(0),
                                                                            verifyMessageAction(0),
                                                                            bip38ToolAction(0),
                                                                            aboutAction(0),
                                                                            checkForUpdateAction(0),
                                                                            receiveCoinsAction(0),
                                                                            optionsAction(0),
                                                                            toggleHideAction(0),
                                                                            encryptWalletAction(0),
                                                                            backupWalletAction(0),
                                                                            restoreWalletAction(0),
                                                                            changePassphraseAction(0),
                                                                            aboutQtAction(0),
                                                                            openRPCConsoleAction(0),
                                                                            openAction(0),
                                                                            showHelpMessageAction(0),
                                                                            multiSendAction(0),
                                                                            smartContractAction(0),
                                                                            LSRTokenAction(0),
                                                                            trayIcon(0),
                                                                            trayIconMenu(0),
                                                                            notificator(0),
                                                                            rpcConsole(0),
                                                                            helpMessageDialog(0),
                                                                            explorerWindow(0),
                                                                            hexAddressWindow(0),
                                                                            modalOverlay(0),
                                                                            updateDialog(0),
                                                                            prevBlocks(0),
                                                                            spinnerFrame(0),
                                                                            platformStyle(platformStyle)

{
    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());
    resize(1200, 750);
    QString windowTitle = tr("Luxcore") + " - ";
	QSettings settings;

#ifdef ENABLE_UPDATER
    controller = new QtLuxUpdater::UpdateController(QStringLiteral("v5.3.0"), this);
    controller->setDetailedUpdateInfo(true);
#endif

#ifdef ENABLE_WALLET
    enableWallet = WalletModel::isWalletEnabled();
#endif // ENABLE_WALLET
    if (enableWallet) {
        windowTitle += tr("Wallet");
    } else {
        windowTitle += tr("Node");
    }
    QString userWindowTitle = QString::fromStdString(GetArg("-windowtitle", ""));
    if (!userWindowTitle.isEmpty()) windowTitle += " - " + userWindowTitle;
    windowTitle += " " + networkStyle->getTitleAddText();
#ifndef Q_OS_MAC
    QApplication::setWindowIcon(networkStyle->getAppIcon());
    setWindowIcon(networkStyle->getAppIcon());
#else
    MacDockIconHandler::instance()->setIcon(networkStyle->getAppIcon());
#endif
    setWindowTitle(windowTitle);

#if defined(Q_OS_MAC) && QT_VERSION < 0x050000
    // This property is not implemented in Qt 5. Setting it has no effect.
    // A replacement API (QtMacUnifiedToolBar) is available in QtMacExtras.
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    rpcConsole = new RPCConsole(platformStyle, enableWallet ? this : 0);
    helpMessageDialog = new HelpMessageDialog(this, HelpMessageDialog::cmdline);
    hexAddressWindow = new HexAddressConverter(this);
#ifdef ENABLE_WALLET
    if (enableWallet) {
        /** Create wallet frame*/
        walletFrame = new WalletFrame(platformStyle, this);
        explorerWindow = new BlockExplorer(platformStyle, this);
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
    }

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    // Create status bar
    statusBar();

    // Disable size grip because it looks ugly and nobody needs it
    statusBar()->setSizeGripEnabled(false);

	// Status bar notification icons
    QFrame* frameBlocks = new QFrame();

    frameBlocks->setContentsMargins(0, 0, 0, 0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout* frameBlocksLayout = new QHBoxLayout(frameBlocks);

    frameBlocksLayout->setContentsMargins(6, 0, 6, 0);
    frameBlocksLayout->setSpacing(6);
    unitDisplayControl = new UnitDisplayStatusBarControl(platformStyle);
    unitDisplayControl->setCursor(Qt::PointingHandCursor);
    labelStakingIcon = new QLabel();
    labelWalletEncryptionIcon = new QLabel();
    pushButtonWalletHDStatusIcon = new QPushButton();
	
    QString styleSheet = ".QPushButton { padding-left:2px !important; " 
							"padding-right:2px !important; " 
							"border:0 !important; "
							"min-height:5px !important; " 
							"min-width:5px !important; }"
							".QPushButton::hover {background-color: transparent !important; }";
							
    pushButtonWalletHDStatusIcon->setMinimumSize(30, STATUSBAR_ICONSIZE);
    pushButtonWalletHDStatusIcon->setMaximumSize(30, STATUSBAR_ICONSIZE);
    pushButtonWalletHDStatusIcon->setIconSize(QSize(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
    pushButtonWalletHDStatusIcon->setCursor(Qt::PointingHandCursor);
    pushButtonWalletHDStatusIcon->setStyleSheet(styleSheet);
    connect(pushButtonWalletHDStatusIcon, &QPushButton::clicked, this, [this]() {
        int hdEnabled = this->pushButtonWalletHDStatusIcon->property("hdEnabled").toInt();
        if(!hdEnabled) {
            if (!pwalletMain->IsCrypted()) 
            {
                auto button = QMessageBox::warning(this, "HD Wallet",
                        tr("Do you really want to enable hd-wallet? You will no more be able to disable it. "
                           "If you select \"Yes\" the application will restart to upgrade the wallet..."),
                        QMessageBox::Yes | QMessageBox::No);

                if (QMessageBox::Yes == button) {
                    QStringList args;
                    args << "-usehd" << "-upgradewallet";
                    emit requestedRestart(args);
                }
           }
           else
           {
                QMessageBox::warning(this, "HD Wallet",
                    tr("Your wallet cannot be upgraded because it is encrypted. "
                     "In order to use the HD \"Hierarchical Deterministic\" feature you will need to import "
                     "your private keys into a new/old unencrypted wallet, then attempt to upgrade."));
           }
        }
    });

    labelConnectionsIcon = new QPushButton();
    labelConnectionsIcon->setFlat(true); // Make the button look like a label, but clickable
	labelConnectionsIcon->setStyleSheet(styleSheet);
    labelConnectionsIcon->setMaximumSize(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE);


    labelBlocksIcon = new GUIUtil::ClickableLabel();
    if (enableWallet) {
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(unitDisplayControl);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelWalletEncryptionIcon);
        // Temporarily disable HD wallet upgrade button
        // frameBlocksLayout->addWidget(pushButtonWalletHDStatusIcon);
    }
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelStakingIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    //status bar social media icons
    QFrame* frameSocMedia = new QFrame();
    //fill in frameSocMedia
    {
        frameSocMedia->setContentsMargins(0, 0, 0, 0);
        frameSocMedia->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
        QHBoxLayout* frameLayout = new QHBoxLayout(frameSocMedia);
        frameLayout->setContentsMargins(6, 0, 6, 0);
        frameLayout->setSpacing(6);
		
		pushButtonTelegram = new QPushButton(frameSocMedia);
        pushButtonTelegram->setToolTip(tr("Go to")+" Telegram");
        connect(pushButtonTelegram, &QPushButton::clicked,
                this, [](){QDesktopServices::openUrl(QUrl("https://t.me/LUXcoreOfficial"));});
        pushButtonTelegram->setIcon(QIcon(QPixmap(":/icons/telegram").scaledToHeight(STATUSBAR_ICONSIZE,Qt::SmoothTransformation)));

        pushButtonDiscord = new QPushButton(frameSocMedia);
        pushButtonDiscord->setToolTip(tr("Go to")+" Discord");
        connect(pushButtonDiscord, &QPushButton::clicked,
                this, [](){QDesktopServices::openUrl(QUrl("https://discord.gg/ndUg9va"));});
        pushButtonDiscord->setIcon(QIcon(":/icons/discord").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        pushButtonTwitter = new QPushButton(frameSocMedia);
        pushButtonTwitter->setToolTip(tr("Go to")+" Twitter");
        connect(pushButtonTwitter, &QPushButton::clicked,
                this, [](){QDesktopServices::openUrl(QUrl("https://twitter.com/LUX_Coin"));});
        pushButtonTwitter->setIcon(QIcon(":/icons/twitter").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        pushButtonGithub = new QPushButton(frameSocMedia);
        pushButtonGithub->setToolTip(tr("Go to")+" GitHub");
        connect(pushButtonGithub, &QPushButton::clicked,
                this, [](){QDesktopServices::openUrl(QUrl("https://github.com/lux-core"));});
        pushButtonGithub->setIcon(QIcon(":/icons/github").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        pushButtonHelp = new QPushButton(frameSocMedia);
        pushButtonHelp->setToolTip(tr("Go to")+" Documentation Hub");
        connect(pushButtonHelp, &QPushButton::clicked,
                this, [](){QDesktopServices::openUrl(QUrl("https://docs.luxcore.io/"));});
        pushButtonHelp->setIcon(QIcon(":/icons/hub").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));	
		
        auto buttons = frameSocMedia->findChildren<QPushButton* >();
		
		
		if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
			QString styleSheet = ".QPushButton { padding-left:2px !important; " 
							"padding-right:2px !important; " 
							"border:0 !important; "
							"min-height:5px !important; " 
							"min-width:5px !important; }"
							".QPushButton::hover {background-color: transparent !important; }";
		} else { 
			QString styleSheet = ".QPushButton { background-color: transparent;"
                                        "border: none;"
                                        "qproperty-text: \"\" }";
		}
						
        foreach(auto but, buttons)
        {
            frameLayout->addWidget(but);
            but->setMinimumSize(30, STATUSBAR_ICONSIZE);
            but->setMaximumSize(30, STATUSBAR_ICONSIZE);
            but->setIconSize(QSize(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            but->setCursor(Qt::PointingHandCursor);
            but->setStyleSheet(styleSheet);
        }
    }
	
	
    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(true);
	
	
	progressBar = new GUIUtil::ProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
	progressBar->setVisible(true);
	
	if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QLabel{ color: #fff; margin-left: 60px !important;" 
								"margin-right: 10px; border: 0px solid #fff5f5; min-width: 100px !important;" 
								"min-height:5px !iomportant; max-height: 8px !important; }"
								".ProgressBar { border: 1px solid #fff5f5 !important; "
								"min-height:3px !iomportant; max-height: 8px !important; }";
		progressBarLabel->setStyleSheet(styleSheet);
		progressBar->setStyleSheet(styleSheet);
	} else if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QLabel { color: #fff; margin-left-left: 60px !important;" 
								"margin-right: 10px; border: 0px solid #fff5f5; min-width: 100px !important;" 
								"min-height:5px !iomportant; max-height: 8px !important; }"
								".ProgressBar { border: 1px solid #fff5f5 !important; "
								"min-height:3px !iomportant; max-height: 8px !important; }";
		progressBarLabel->setStyleSheet(styleSheet);
		progressBar->setStyleSheet(styleSheet);
	}else { 
		QString styleSheet = ".QLabel#progressBarLabel { color: #ffffff; margin-left: 10px; "
								"margin-right: 10px; border: 0px solid #ff0000 } "
								".ProgressBar#progressBarLabel { border: 0px solid #ff0000; }";
		progressBarLabel->setStyleSheet(styleSheet);
		progressBar->setStyleSheet(styleSheet);
	}
	
    

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if (curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle") {
        
    }
	
	QFrame* horizontalSpacerFrame1 = new QFrame(); // blank frames that will fix the location of the other items on the statusBar
	QFrame* horizontalSpacerFrame2 = new QFrame();
	horizontalSpacerFrame1->setGeometry(0,0,48,5);
	horizontalSpacerFrame2->setGeometry(0,0,2,5);
	
	statusBar()->addWidget(horizontalSpacerFrame1);
    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameSocMedia);
    statusBar()->addPermanentWidget(frameBlocks);
	statusBar()->addPermanentWidget(horizontalSpacerFrame2);


    // Jump directly to tabs in RPC-console
    connect(openInfoAction, SIGNAL(triggered()), rpcConsole, SLOT(showInfo()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(showConsole()));
    connect(openNetworkAction, SIGNAL(triggered()), rpcConsole, SLOT(showNetwork()));
    connect(openPeersAction, SIGNAL(triggered()), rpcConsole, SLOT(showPeers()));
    connect(openRepairAction, SIGNAL(triggered()), rpcConsole, SLOT(showRepair()));
    connect(openConfEditorAction, SIGNAL(triggered()), rpcConsole, SLOT(showConfEditor()));
    connect(openMNConfEditorAction, SIGNAL(triggered()), rpcConsole, SLOT(showMNConfEditor()));
    connect(showBackupsAction, SIGNAL(triggered()), rpcConsole, SLOT(showBackups()));
    connect(labelConnectionsIcon, SIGNAL(clicked()), rpcConsole, SLOT(showPeers()));

    // Get restart command-line parameters and handle restart
    connect(rpcConsole, SIGNAL(handleRestart(QStringList)), this, SLOT(handleRestart(QStringList)));


    connect(openBlockExplorerAction, SIGNAL(triggered()), explorerWindow, SLOT(show()));

    connect(openHexAddressAction, SIGNAL(triggered()), hexAddressWindow, SLOT(show()));

    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), explorerWindow, SLOT(hide()));

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    // Subscribe to notifications from core
    subscribeToCoreSignals();

    setStakingStatus();
    timerStakingIcon = new QTimer(this);
    connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(setStakingStatus()));
    timerStakingIcon->start(10000);

    modalOverlay = new ModalOverlay(this->centralWidget());

    updateDialog = new UpdateDialog(this);

    if(fCheckUpdates && updateDialog->newUpdateAvailable())
    {
        QString url = "https://github.com/LUX-Core/lux/releases";
        QString link = QString("<a href=\\\"\"+ url +\"\\\">\"+ url +\"</a>").arg(NEW_RELEASES, NEW_RELEASES);
        QString message(tr("New lux-qt version available: <br /> %1. <br />").arg(link));
        QMessageBox::information(this, tr("Check for updates"), message);
    }

#ifdef ENABLE_WALLET
    if(enableWallet) {
        connect(walletFrame, SIGNAL(requestedSyncWarningInfo()), this, SLOT(showModalOverlay()));
        connect(labelBlocksIcon, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
        connect(progressBar, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
    }
#endif
    setMinimumHeight(height());
}

BitcoinGUI::~BitcoinGUI() {
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();
    delete timerStakingIcon;
    GUIUtil::saveWindowGeometry("nWindow", this);
    if (trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif
}

void BitcoinGUI::createActions() {
	
    QSettings settings;	
    QActionGroup* tabGroup = new QActionGroup(this);
	
    if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {	
        overviewAction = new QAction(QIcon(":/icons/overview_red"), tr("&Overview"), this);
        sendCoinsAction = new QAction(QIcon(":/icons/send_red"), tr("&Send"), this);
        receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses_red"), tr("&Receive"), this);
        historyAction = new QAction(QIcon(":/icons/history_red"), tr("&Transactions"), this);
        LSRTokenAction = new QAction(QIcon(":/icons/lsrtoken_red"), tr("&LSR Token"), this);
        masternodeAction = new QAction(QIcon(":/icons/masternodes_red"), tr("&Masternodes"), this);
        smartContractAction = new QAction(QIcon(":/icons/smartcontract_red"), tr("&Smart Contracts"), this);
    } else {
        overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
        sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send"), this);
        receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive"), this);
        historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
        LSRTokenAction = new QAction(QIcon(":/icons/lsrtoken"), tr("&LSR Token"), this);
        masternodeAction = new QAction(QIcon(":/icons/masternodes"), tr("&Masternodes"), this);
        smartContractAction = new QAction(QIcon(":/icons/smartcontract"), tr("&Smart Contracts"), this);
    }

    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    tabGroup->addAction(overviewAction);
	
    sendCoinsAction->setStatusTip(tr("Send coins to a LUX address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and lux: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    tabGroup->addAction(receiveCoinsAction);

    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    tabGroup->addAction(historyAction);
	
    LSRTokenAction->setStatusTip(tr("LSR Token (send, receive or add Token in list)"));
    LSRTokenAction->setToolTip(LSRTokenAction->statusTip());
    LSRTokenAction->setCheckable(true);
    tabGroup->addAction(LSRTokenAction);
	
    masternodeAction->setStatusTip(tr("Browse masternodes"));
    masternodeAction->setToolTip(masternodeAction->statusTip());
    masternodeAction->setCheckable(true);
    tabGroup->addAction(masternodeAction);
	
    smartContractAction->setStatusTip(tr("Smart Contracts Actions"));
    smartContractAction->setToolTip(smartContractAction->statusTip());
    smartContractAction->setCheckable(true);
    tabGroup->addAction(smartContractAction);
	
#ifdef Q_OS_MAC

    overviewAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_1));
    sendCoinsAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_2));
    receiveCoinsAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_3));
	historyAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_4));
    LSRTokenAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_5));
    masternodeAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_7));
    smartContractAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_8));

#else

    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    LSRTokenAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    masternodeAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_7));
    smartContractAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_8));
	
#endif

    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(LSRTokenAction, SIGNAL(triggered()), this, SLOT(gotoLSRTokenPage()));
    connect(masternodeAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(masternodeAction, SIGNAL(triggered()), this, SLOT(gotoMasternodePage()));
    connect(smartContractAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(smartContractAction, SIGNAL(triggered()), this, SLOT(gotoSmartContractPage()));


	//SETUP THE CORRECT ICON IF THEME IS DIFFERENT THAN DEFAULT
	if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {	
		
		quitAction = new QAction(QIcon(":/icons/quit-light"), tr("E&xit"), this);
		optionsAction = new QAction(QIcon(":/icons/options-white"), tr("&Options..."), this);
		aboutAction = new QAction(QIcon(":/icons/luxcoin_black-white"), tr("&About Luxcore"), this);
#ifdef ENABLE_UPDATER
		// Check for update menu item
		checkForUpdateAction = new QAction(QIcon(":/icons/update_black"), tr("Check for &Update"), this);
#endif	
#if QT_VERSION < 0x050000
		aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#else
		aboutQtAction = new QAction(QIcon(":/qt-project.org/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#endif
		toggleHideAction = new QAction(QIcon(":/icons/luxcoin_black-white"), tr("&Show / Hide"), this);
		encryptWalletAction = new QAction(QIcon(":/icons/lock_closed_black-white"), tr("&Encrypt Wallet..."), this);
		backupWalletAction = new QAction(QIcon(":/icons/backup-white"), tr("&Backup Wallet..."), this);
		changePassphraseAction = new QAction(QIcon(":/icons/key-white"), tr("&Change Passphrase..."), this);
		restoreWalletAction = new QAction(QIcon(":/icons/restore-white"), tr("&Restore Wallet..."), this);
		unlockWalletAction = new QAction(tr("&Unlock Wallet..."), this); //no icon
		lockWalletAction = new QAction(tr("&Lock Wallet"), this); //no icon
		signMessageAction = new QAction(QIcon(":/icons/sign-white"), tr("Sign &message..."), this);
		verifyMessageAction = new QAction(QIcon(":/icons/verified-white"), tr("&Verify message..."), this);
		bip38ToolAction = new QAction(QIcon(":/icons/key-white"), tr("&BIP38 tool"), this);
		multiSendAction = new QAction(QIcon(":/icons/edit"), tr("&MultiSend"), this);
		openInfoAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Information"), this);
		openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow-white"), tr("&Debug console"), this);
		openNetworkAction = new QAction(QIcon(":/icons/network_monitor-white"), tr("&Network Monitor"), this);
		openPeersAction = new QAction(QIcon(":/icons/peer-white"), tr("&Peers list"), this);
		openRepairAction = new QAction(QIcon(":/icons/wallet_repair-white"), tr("Wallet &Repair"), this);
		openConfEditorAction = new QAction(QIcon(":/icons/edit"), tr("Open Wallet &Configuration File"), this);
		openMNConfEditorAction = new QAction(QIcon(":/icons/edit"), tr("Open &Masternode Configuration File"), this);
		showBackupsAction = new QAction(QIcon(":/icons/auto_backup"), tr("Show Automatic &Backups"), this);
		openConfEditorAction = new QAction(QIcon(":/icons/conf-white"), tr("Edit &Configuration File"), this);
		usedSendingAddressesAction = new QAction(QIcon(":/icons/address-book-white"), tr("&Sending addresses..."), this);
		usedReceivingAddressesAction = new QAction(QIcon(":/icons/address-book-white"), tr("&Receiving addresses..."), this);
		openAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_FileIcon), tr("Open payment request | &URI..."), this);
		openBlockExplorerAction = new QAction(QIcon(":/icons/explorer"), tr("&Blockchain explorer"), this);
		openHexAddressAction = new QAction(QIcon(":/icons/editcopy"), tr("&Hex Address Converter"), this);
		showHelpMessageAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Command-line options"), this);
		
	} else {
		
		quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
		optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
		aboutAction = new QAction(QIcon(":/icons/luxcoin_black"), tr("&About Luxcore"), this);
#ifdef ENABLE_UPDATER
		// Check for update menu item
		checkForUpdateAction = new QAction(QIcon(":/icons/update_black"), tr("Check for &Update"), this);
#endif	
#if QT_VERSION < 0x050000
		aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#else
		aboutQtAction = new QAction(QIcon(":/qt-project.org/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#endif
		toggleHideAction = new QAction(QIcon(":/icons/luxcoin_black"), tr("&Show / Hide"), this);
		encryptWalletAction = new QAction(QIcon(":/icons/lock_closed_black"), tr("&Encrypt Wallet..."), this);
		backupWalletAction = new QAction(QIcon(":/icons/backup"), tr("&Backup Wallet..."), this);
		changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
		restoreWalletAction = new QAction(QIcon(":/icons/restore"), tr("&Restore Wallet..."), this);
		unlockWalletAction = new QAction(tr("&Unlock Wallet..."), this);
		lockWalletAction = new QAction(tr("&Lock Wallet"), this);
		signMessageAction = new QAction(QIcon(":/icons/sign"), tr("Sign &message..."), this);
		verifyMessageAction = new QAction(QIcon(":/icons/verified"), tr("&Verify message..."), this);
		bip38ToolAction = new QAction(QIcon(":/icons/key"), tr("&BIP38 tool"), this);
		multiSendAction = new QAction(QIcon(":/icons/edit"), tr("&MultiSend"), this);
		openInfoAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Information"), this);
		openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug console"), this);
		openNetworkAction = new QAction(QIcon(":/icons/network_monitor"), tr("&Network Monitor"), this);
		openPeersAction = new QAction(QIcon(":/icons/peer"), tr("&Peers list"), this);
		openRepairAction = new QAction(QIcon(":/icons/wallet_repair"), tr("Wallet &Repair"), this);
		openConfEditorAction = new QAction(QIcon(":/icons/edit"), tr("Open Wallet &Configuration File"), this);
		openMNConfEditorAction = new QAction(QIcon(":/icons/edit"), tr("Open &Masternode Configuration File"), this);
		showBackupsAction = new QAction(QIcon(":/icons/auto_backup"), tr("Show Automatic &Backups"), this);
		openConfEditorAction = new QAction(QIcon(":/icons/conf"), tr("Edit &Configuration File"), this);
		usedSendingAddressesAction = new QAction(QIcon(":/icons/address-book"), tr("&Sending addresses..."), this);
		usedReceivingAddressesAction = new QAction(QIcon(":/icons/address-book"), tr("&Receiving addresses..."), this);
		openAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_FileIcon), tr("Open payment request | &URI..."), this);
		openBlockExplorerAction = new QAction(QIcon(":/icons/explorer"), tr("&Blockchain explorer"), this);
		openHexAddressAction = new QAction(QIcon(":/icons/editcopy"), tr("&Hex Address Converter"), this);
		showHelpMessageAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Command-line options"), this);
			
	}
    
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
	
    aboutAction->setStatusTip(tr("Show information about Luxcore"));
    aboutAction->setMenuRole(QAction::AboutRole);

#ifdef ENABLE_UPDATER
    // Check for update menu item
    checkForUpdateAction->setStatusTip(tr("Check whether there is an updated wallet from Luxcore"));
    checkForUpdateAction->setMenuRole(QAction::NoRole);
#endif

    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
	
    optionsAction->setStatusTip(tr("Modify configuration options for LUX"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
	
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));
	
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);

    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));  
    restoreWalletAction->setStatusTip(tr("Restore wallet from another location"));
    unlockWalletAction->setToolTip(tr("Unlock wallet"));
    signMessageAction->setStatusTip(tr("Sign messages with your LUX addresses to prove you own them"));
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified LUX addresses"));
    bip38ToolAction->setToolTip(tr("Encrypt and decrypt private keys using a passphrase"));
    
    multiSendAction->setToolTip(tr("MultiSend Settings"));
    multiSendAction->setCheckable(true);

    openInfoAction->setStatusTip(tr("Show diagnostic information"));
    openRPCConsoleAction->setStatusTip(tr("Open debugging console"));
    openNetworkAction->setStatusTip(tr("Show network monitor"));
    openPeersAction->setStatusTip(tr("Show peers info"));
    openRepairAction->setStatusTip(tr("Show wallet repair options"));
    openConfEditorAction->setStatusTip(tr("Open configuration file"));
    openMNConfEditorAction->setStatusTip(tr("Open Masternode configuration file"));
    showBackupsAction->setStatusTip(tr("Show automatically created wallet backups"));
    openConfEditorAction->setStatusTip(tr("Edit configuration file"));
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));
    openAction->setStatusTip(tr("Open a LUX: URI or payment request"));
    openBlockExplorerAction->setStatusTip(tr("Block explorer window"));
    openHexAddressAction->setStatusTip(tr("Converter for LUX Smart Contract addresses"));

    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the Luxcore help message to get a list with possible LUX command-line options"));


    connect(qApp, SIGNAL(aboutToQuit()), qApp, SLOT(quit()));
    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
#ifdef ENABLE_UPDATER
    connect(checkForUpdateAction, SIGNAL(triggered()), this, SLOT(updaterClicked()));
#endif
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), this, SLOT(showDebugWindow()));
    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), rpcConsole, SLOT(hide()));

#ifdef ENABLE_WALLET
    if (walletFrame) {
        connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));
        connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));
        connect(restoreWalletAction, SIGNAL(triggered()), walletFrame, SLOT(restoreWallet()));
        connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));
        connect(unlockWalletAction, SIGNAL(triggered()), walletFrame, SLOT(unlockWallet()));
        connect(lockWalletAction, SIGNAL(triggered()), walletFrame, SLOT(lockWallet()));
        connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
        connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
        connect(bip38ToolAction, SIGNAL(triggered()), this, SLOT(gotoBip38Tool()));
        connect(usedSendingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedSendingAddresses()));
        connect(usedReceivingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedReceivingAddresses()));
        connect(openAction, SIGNAL(triggered()), this, SLOT(openClicked()));
        connect(multiSendAction, SIGNAL(triggered()), this, SLOT(gotoMultiSendDialog()));
    }
#endif // ENABLE_WALLET
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), this, SLOT(showDebugWindowActivateConsole()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D), this, SLOT(showDebugWindow()));
}

void BitcoinGUI::createMenuBar() {
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu* file = appMenuBar->addMenu(tr("&File"));
    if (walletFrame) {
        file->addAction(openAction);
        file->addAction(backupWalletAction);
        file->addAction(restoreWalletAction);
        file->addSeparator();
        file->addAction(signMessageAction);
        file->addAction(verifyMessageAction);
        file->addSeparator();
        file->addAction(usedSendingAddressesAction);
        file->addAction(usedReceivingAddressesAction);
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu* settings = appMenuBar->addMenu(tr("&Settings"));
    if (walletFrame) {
        settings->addAction(encryptWalletAction);
        settings->addAction(changePassphraseAction);
        settings->addAction(unlockWalletAction);
        settings->addAction(lockWalletAction);
        settings->addSeparator();
        settings->addAction(openConfEditorAction);
        settings->addAction(openMNConfEditorAction);
        settings->addSeparator();
    }

    settings->addAction(optionsAction);

    if (walletFrame) {
        QMenu* tools = appMenuBar->addMenu(tr("&Tools"));
        tools->addAction(openRPCConsoleAction);
        tools->addSeparator();
        tools->addAction(openNetworkAction);
        tools->addAction(openPeersAction);
        tools->addSeparator();
        tools->addAction(openRepairAction);
        tools->addAction(showBackupsAction);
        tools->addAction(bip38ToolAction);
        tools->addAction(multiSendAction);
        tools->addSeparator();
        tools->addAction(openHexAddressAction);
        tools->addAction(openBlockExplorerAction);
#ifdef ENABLE_UPDATER
        tools->addSeparator();
        tools->addAction(checkForUpdateAction);
        tools->addSeparator();
#endif
    }

    QMenu* help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openInfoAction);
    help->addAction(showHelpMessageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
    help->addSeparator();
}

void BitcoinGUI::createToolBars() {
    if (walletFrame) {
		
		QWidget* separator = new QWidget(this);
		separator->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		
        QToolBar* toolbar = new QToolBar(tr("Tabs toolbar"));
        toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
        toolbar->setMovable(false);
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addAction(overviewAction);
        toolbar->addAction(sendCoinsAction);
        toolbar->addAction(receiveCoinsAction);
        toolbar->addAction(historyAction);
        toolbar->addAction(LSRTokenAction);
				
        QSettings settings;

        toolbar->addAction(masternodeAction);
        toolbar->addAction(smartContractAction);
		toolbar->addWidget(separator);
		toolbar->setMovable(false); // remove unused icon in upper left corner
        overviewAction->setChecked(true);

		if (settings.value("theme").toString() == "dark grey") {
			QString styleSheet = ".toolbar {  border: 1px solid #fff5f5 !important; } "
							".QToolButton {  background-color: transparent; margin-left: -1px !important; padding-top: 1px !important; "
							" border: 1px solid #fff5f5 !important; "
							" min-height: 35px !important; min-width: 85px !important; text-align:center !important; }" 
							".QToolButton::hover { background-color: #fff5f5; margin-bottom: -1px !important; } "
							".QToolButton:checked, .QToolButton::hover:selected { background-color: transparent; color: #fff !important; margin-bottom: -1px !important;} "
							".QWidget { border-bottom: 1px solid #fff5f5 !important; }";
			toolbar->setStyleSheet(styleSheet);
			separator->setStyleSheet(styleSheet);
		} else if (settings.value("theme").toString() == "dark blue") {
			QString styleSheet = ".toolbar {  border: 1px solid #fff5f5 !important; } "
							".QToolButton {  background-color: transparent; margin-left: -1px !important; padding-top: 1px !important; "
							" border: 1px solid #fff5f5 !important; "
							" min-height: 35px !important; min-width: 85px !important; text-align:center !important; }" 
							".QToolButton::hover { background-color: #fff5f5; margin-bottom: -1px !important;} "
							".QToolButton:checked, .QToolButton::hover:selected { background-color: transparent; color: #fff !important; margin-bottom: -1px !important;} "
							".QWidget { border-bottom: 1px solid #fff5f5 !important; }";
			toolbar->setStyleSheet(styleSheet);
			separator->setStyleSheet(styleSheet);
		} else { 
			//code here
		}
		
        /** Create additional container for toolbar and walletFrame and make it the central widget.
            This is a workaround mostly for toolbar styling on Mac OS but should work fine for every other OSes too.
        */
        QVBoxLayout* layout = new QVBoxLayout;
        layout->addWidget(toolbar);
        layout->addWidget(walletFrame);
        layout->setSpacing(0);
        layout->setContentsMargins(QMargins());
        QWidget* containerWidget = new QWidget();
        containerWidget->setLayout(layout);
		
		if (settings.value("theme").toString() == "dark grey") {
			QString styleSheet = ".QToolButton { background-color: #262626 !important; color:#fff !important; "
								"padding-left:5px; padding-right:5px;} "
								".QToolButton::hover { background-color:#fff5f5; color:#262626; }";
			containerWidget->setStyleSheet(styleSheet);
		} else if (settings.value("theme").toString() == "dark blue") {
			QString styleSheet = ".QToolButton { background-color: #031d54 !important; color:#fff !important; "
								"padding-left:5px; padding-right:5px;} "
								".QToolButton::hover { background-color:#fff5f5; color:#031d54; }";
			containerWidget->setStyleSheet(styleSheet);
		} else { 
			//code here
		}
		
        setMinimumSize(200, 200);
        setCentralWidget(containerWidget);
    }
}

void BitcoinGUI::setClientModel(ClientModel* clientModel) {
    this->clientModel = clientModel;
    if (clientModel) {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        updateNetworkState();
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));
        connect(clientModel, SIGNAL(networkActiveChanged(bool)), this, SLOT(setNetworkActive(bool)));

        modalOverlay->setKnownBestHeight(clientModel->getHeaderTipHeight(), QDateTime::fromTime_t(clientModel->getHeaderTipTime()));
        setNumBlocks(clientModel->getNumBlocks(), clientModel->getLastBlockDate(), clientModel->getVerificationProgress(NULL), false);
        connect(clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(setNumBlocks(int,QDateTime,double,bool)));

        //connect(clientModel, SIGNAL(additionalDataSyncProgressChanged(double)), this, SLOT(setAdditionalDataSyncProgress(double)));

        // Receive and report messages from client model
        connect(clientModel, SIGNAL(message(QString, QString, unsigned int)), this, SLOT(message(QString, QString, unsigned int)));

        // Show progress dialog
        connect(clientModel, SIGNAL(showProgress(QString, int)), this, SLOT(showProgress(QString, int)));

        rpcConsole->setClientModel(clientModel);
#ifdef ENABLE_WALLET
        if (walletFrame) {
            walletFrame->setClientModel(clientModel);
        }
#endif // ENABLE_WALLET
        unitDisplayControl->setOptionsModel(clientModel->getOptionsModel());

        OptionsModel* optionsModel = clientModel->getOptionsModel();
        if(optionsModel)
        {
            // be aware of the tray icon disable state change reported by the OptionsModel object.
            connect(optionsModel,SIGNAL(hideTrayIconChanged(bool)),this,SLOT(setTrayIconVisible(bool)));

            // initialize the disable state of the tray icon with the current value in the model.
            setTrayIconVisible(optionsModel->getHideTrayIcon());
        }

        if (explorerWindow)
            explorerWindow->setClientModel(clientModel);

    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if(trayIconMenu)
        {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
        // Propagate cleared model to child objects
        rpcConsole->setClientModel(nullptr);
#ifdef ENABLE_WALLET
        if (walletFrame)
        {
            walletFrame->setClientModel(nullptr);
        }
#endif // ENABLE_WALLET
        unitDisplayControl->setOptionsModel(nullptr);

#ifdef Q_OS_MAC
        if(trayIconMenu)
        {
            // Disable context menu on dock icon
            trayIconMenu->clear();
        }
#endif
    }
}

#ifdef ENABLE_WALLET
bool BitcoinGUI::addWallet(const QString& name, WalletModel* walletModel)
{
    if (!walletFrame)
        return false;
    setWalletActionsEnabled(true);
    return walletFrame->addWallet(name, walletModel);
}

bool BitcoinGUI::setCurrentWallet(const QString& name)
{
    if (!walletFrame)
        return false;
    return walletFrame->setCurrentWallet(name);
}

void BitcoinGUI::removeAllWallets()
{
    if (!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET

void BitcoinGUI::setWalletActionsEnabled(bool enabled) {
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
    QSettings settings;

    masternodeAction->setEnabled(enabled);
    smartContractAction->setEnabled(enabled);
    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    restoreWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    bip38ToolAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
}

void BitcoinGUI::createTrayIcon(const NetworkStyle* networkStyle) {
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("Luxcore client") + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getAppIcon());
    trayIcon->show();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

void BitcoinGUI::createTrayIconMenu() {
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
        this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler* dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow*)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addAction(bip38ToolAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(openInfoAction);
    trayIconMenu->addAction(openRPCConsoleAction);
    trayIconMenu->addAction(openNetworkAction);
    trayIconMenu->addAction(openPeersAction);
    trayIconMenu->addAction(openRepairAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(openConfEditorAction);
    trayIconMenu->addAction(openMNConfEditorAction);
    trayIconMenu->addAction(showBackupsAction);
    trayIconMenu->addAction(openHexAddressAction);
    trayIconMenu->addAction(openBlockExplorerAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#endif

void BitcoinGUI::optionsClicked() {
    if (!clientModel || !clientModel->getOptionsModel())
        return;

    OptionsDialog dlg(this, enableWallet);
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();
}

void BitcoinGUI::aboutClicked() {
    if (!clientModel)
        return;

    HelpMessageDialog dlg(this, HelpMessageDialog::about);
    dlg.exec();
}

void BitcoinGUI::updaterClicked() {
    if (!clientModel)
        return;
#ifdef ENABLE_UPDATER
    controller->start(QtLuxUpdater::UpdateController::ProgressLevel);
#else
    uiInterface.ThreadSafeMessageBox("This feature is only available on official builds!",
             "Warning", CClientUIInterface::MSG_WARNING);
#endif
}

void BitcoinGUI::showDebugWindow()
{
    rpcConsole->showNormal();
    rpcConsole->show();
    rpcConsole->raise();
    rpcConsole->activateWindow();
}

void BitcoinGUI::showDebugWindowActivateConsole()
{
    rpcConsole->setTabFocus(RPCConsole::TAB_CONSOLE);
    showDebugWindow();
}

void BitcoinGUI::showHelpMessageClicked() {
    helpMessageDialog->show();
}

#ifdef ENABLE_WALLET
void BitcoinGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if (dlg.exec()) {
        Q_EMIT receivedURI(dlg.getURI());
    }
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    if (walletFrame) walletFrame->gotoOverviewPage();
}
void BitcoinGUI::gotoSmartContractPage()
{
    smartContractAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSmartContractPage();
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    if (walletFrame) walletFrame->gotoHistoryPage();
}

void BitcoinGUI::gotoMasternodePage()
{
    QSettings settings;
    masternodeAction->setChecked(true);
    if (walletFrame) walletFrame->gotoMasternodePage();
}

void BitcoinGUI::gotoLSRTokenPage(bool toAddTokenPage)
{
    LSRTokenAction->setChecked(true);
    if (walletFrame) walletFrame->gotoLSRTokenPage(toAddTokenPage);
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void BitcoinGUI::gotoSendCoinsPage(QString addr)
{
    sendCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}

void BitcoinGUI::gotoBip38Tool()
{
    if (walletFrame) walletFrame->gotoBip38Tool();
}

void BitcoinGUI::gotoMultiSendDialog()
{
    multiSendAction->setChecked(true);
    if (walletFrame)
        walletFrame->gotoMultiSendDialog();
}
void BitcoinGUI::gotoBlockExplorerPage()
{
    if (walletFrame) walletFrame->gotoBlockExplorerPage();
}

#endif // ENABLE_WALLET

void BitcoinGUI::updateNetworkState() {
    int count = clientModel->getNumConnections();
    QString icon;
    switch (count) {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    QIcon connectionItem = QIcon(icon).pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE);
    labelConnectionsIcon->setIcon(connectionItem);
    if (clientModel->getNetworkActive())
        labelConnectionsIcon->setToolTip(tr("%n active connection(s) to Luxcore network", "", count));
    else
        labelConnectionsIcon->setToolTip(tr("Network activity disabled"));
}

void BitcoinGUI::setNetworkActive(bool networkActive) {
    updateNetworkState();
}

void BitcoinGUI::setNumConnections(int count) {
    updateNetworkState();
}

void BitcoinGUI::updateHeadersSyncProgressLabel()
{
    int64_t headersTipTime = clientModel->getHeaderTipTime();
    int headersTipHeight = clientModel->getHeaderTipHeight();
    int estHeadersLeft = (GetTime() - headersTipTime) / Params().GetConsensus().nPowTargetSpacing;
#if 1
    if (estHeadersLeft > HEADER_HEIGHT_SYNC)
		
        progressBarLabel->setText(tr("Syncing Headers (%1%)...").arg(QString::number(100.0 / (headersTipHeight+estHeadersLeft)*headersTipHeight, 'f', 1)));
#endif
}

void BitcoinGUI::setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool header)
{
    if (modalOverlay) {
        if (header)
            modalOverlay->setKnownBestHeight(count, blockDate);
        else
            modalOverlay->tipUpdate(count, blockDate, nVerificationProgress);
    }
    if (!clientModel)
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbelled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BLOCK_SOURCE_NETWORK:
            hideLogMessage = true;
            if (header) {
                updateHeadersSyncProgressLabel();
                return;
            }
            progressBarLabel->setText(tr("Synchronizing with network..."));
            updateHeadersSyncProgressLabel();
            break;
        case BLOCK_SOURCE_DISK:
            if (header) {
                progressBarLabel->setText(tr("Indexing blocks on disk..."));
            } else {
                progressBarLabel->setText(tr("Processing blocks on disk..."));
            }
            break;
        case BLOCK_SOURCE_REINDEX:
            progressBarLabel->setText(tr("Reindexing blocks on disk..."));
            break;
        case BLOCK_SOURCE_NONE:
            if (header) {
                return;
            }
            // Case: not Importing, not Reindexing and no network connection
            progressBarLabel->setText(tr("Connecting to peers..."));
            break;
    }

    QString tooltip;

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    QDateTime currentDate = QDateTime::currentDateTime();
    int secs = lastBlockDate.secsTo(currentDate);

    tooltip = tr("Processed %n blocks of transaction history.", "", count);
    if(secs < 90*60) {
        hideLogMessage = false;
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(false);
            modalOverlay->showHide(true, true);
        }
#endif // ENABLE_WALLET
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);

        // notify tip changed when the sync is finished
        if(fWalletProcessingMode) {
            fWalletProcessingMode = false;
            QMetaObject::invokeMethod(clientModel, "numBlocksChanged", Qt::QueuedConnection, Q_ARG(int, count));
        }
    } else {
        QString timeBehindText = GUIUtil::formatNiceTimeOffset(secs);
        // Represent time from last generated block in human readable text
        const int HOUR_IN_SECONDS = 60 * 60;
        const int DAY_IN_SECONDS = 24 * 60 * 60;
        const int WEEK_IN_SECONDS = 7 * 24 * 60 * 60;
        const int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
        if (secs < 2 * DAY_IN_SECONDS) {
            timeBehindText = tr("%n hour(s)", "", secs / HOUR_IN_SECONDS);
        } else if (secs < 2 * WEEK_IN_SECONDS) {
            timeBehindText = tr("%n day(s)", "", secs / DAY_IN_SECONDS);
        } else if (secs < YEAR_IN_SECONDS) {
            timeBehindText = tr("%n week(s)", "", secs / WEEK_IN_SECONDS);
        } else {
            int years = secs / YEAR_IN_SECONDS;
            int remainder = secs % YEAR_IN_SECONDS;
            timeBehindText = tr("%1 and %2").arg(tr("%n year(s)", "", years)).arg(tr("%n week(s)", "", remainder / WEEK_IN_SECONDS));
        }

        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(clientModel->getVerificationProgress(NULL) * 1000000000.0 + 0.5);
        progressBar->setVisible(true);

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;

        if (count != prevBlocks) {
            labelBlocksIcon->setPixmap(QIcon(QString(":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0'))).pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }

        prevBlocks = count;

#ifdef ENABLE_WALLET
        if(walletFrame) {
            walletFrame->showOutOfSyncWarning(true);
            modalOverlay->showHide();
        }
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BitcoinGUI::message(const QString& title, const QString& message, unsigned int style, bool* ret) {
    QString strTitle = tr("Luxcore"); // default title
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    } else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            break;
        default:
            break;
        }
    }
    // Append title to "LUX - "
    if (!msgType.isEmpty())
        strTitle += " - " + msgType;

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    } else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }
    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        showNormalIfMinimized();
        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, message, buttons, this);
        int r = mBox.exec();
        if (ret != NULL)
            *ret = r == QMessageBox::Ok;
    } else
        notificator->notify((Notificator::Class)nNotifyIcon, strTitle, message);
}

void BitcoinGUI::changeEvent(QEvent* e) {
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if (e->type() == QEvent::WindowStateChange) {
        if (clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray()) {
            QWindowStateChangeEvent* wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if (!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized()) {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent* event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if(clientModel && clientModel->getOptionsModel())
    {
        if(!clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            // close rpcConsole in case it was open to make some space for the shutdown window
            rpcConsole->close();

            QApplication::quit();
        }
        else
        {
            QMainWindow::showMinimized();
            event->ignore();
        }
    }
#else
    QMainWindow::closeEvent(event);
#endif
}

#ifdef ENABLE_WALLET
void BitcoinGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label)
{
    QString msg = tr("Date: %1\n").arg(date) +
                  tr("Amount: %1\n").arg(BitcoinUnits::formatWithUnit(unit, amount, true)) +
                  tr("Type: %1\n").arg(type);
    if (!label.isEmpty())
        msg += tr("Label: %1\n").arg(label);
    else if (!address.isEmpty())
        msg += tr("Address: %1\n").arg(address);

    // On new transaction, make an info balloon
    message((amount) < 0 ? (pwalletMain->fMultiSendNotify == true ? tr("Sent MultiSend transaction") : tr("Sent transaction")) : tr("Incoming transaction"),
        msg,
        CClientUIInterface::MSG_INFORMATION);

    pwalletMain->fMultiSendNotify = false;
}

void BitcoinGUI::incomingTokenTransaction(const QString& date, const QString& amount, const QString& type, const QString& address, const QString& label, const QString& title)
{
    // On new transaction, make an info balloon
    QString msg = tr("Date: %1\n").arg(date) +
                  tr("Amount: %1\n").arg(amount) +
                  tr("Type: %1\n").arg(type);
    if (!label.isEmpty())
        msg += tr("Label: %1\n").arg(label);
    else if (!address.isEmpty())
        msg += tr("Address: %1\n").arg(address);
    message(title, msg, CClientUIInterface::MSG_INFORMATION);
}

#endif // ENABLE_WALLET

void BitcoinGUI::dragEnterEvent(QDragEnterEvent* event) {
    // Accept only URIs
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent* event) {
    if (event->mimeData()->hasUrls()) {
        Q_FOREACH (const QUrl& uri, event->mimeData()->urls()) {
            Q_EMIT receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool BitcoinGUI::eventFilter(QObject* object, QEvent* event) {
    // Catch status tip events
    if (event->type() == QEvent::StatusTip) {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}

void BitcoinGUI::setStakingStatus() {
    if (pwalletMain)
        fMultiSend = pwalletMain->isMultiSendEnabled();

    if (stake->HasStaked()) {
        labelStakingIcon->show();
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_active").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelStakingIcon->setToolTip(tr("Staking is active\n MultiSend: %1").arg(fMultiSend ? tr("Active") : tr("Not Active")));
    } else {
        labelStakingIcon->show();
        labelStakingIcon->setPixmap(QIcon(":/icons/staking_inactive").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelStakingIcon->setToolTip(tr("Staking is not active\n MultiSend: %1").arg(fMultiSend ? tr("Active") : tr("Not Active")));
    }
}

#ifdef ENABLE_WALLET
bool BitcoinGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient)) {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void BitcoinGUI::setHDStatus(int hdEnabled)
{

    pushButtonWalletHDStatusIcon->setProperty("hdEnabled", hdEnabled);
    pushButtonWalletHDStatusIcon->setIcon(QIcon(hdEnabled ? ":/icons/hd_enabled" : ":/icons/hd_disabled").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
    pushButtonWalletHDStatusIcon->setToolTip(hdEnabled ? tr("HD key generation is <b>enabled</b>") : tr("HD key generation is <b>disabled</b>"));

    // eventually disable the QLabel to set its opacity to 50%
    //pushButtonWalletHDStatusIcon->setEnabled(hdEnabled);
}

void BitcoinGUI::setEncryptionStatus(int status)
{
	QSettings settings;

    switch (status) {
    case WalletModel::Unencrypted:
        labelWalletEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelWalletEncryptionIcon->show();
		if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
			        labelWalletEncryptionIcon->setPixmap(QIcon(":/icons/lock_open-white").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
		} else {
			labelWalletEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
		}

        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::UnlockedForStakingOnly:
        labelWalletEncryptionIcon->show();
		if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
			labelWalletEncryptionIcon->setPixmap(QIcon(":/icons/lock_open-white").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
		} else {
			labelWalletEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
		}
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b> for anonymization and staking only"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelWalletEncryptionIcon->show();
		if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
			labelWalletEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed_black-white").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
		} else {
			labelWalletEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed_black").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
		}
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}
#endif // ENABLE_WALLET

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden) {
    if (!clientModel)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden()) {
        show();
        activateWindow();
    } else if (isMinimized()) {
        showNormal();
        activateWindow();
    } else if (GUIUtil::isObscured(this)) {
        raise();
        activateWindow();
    } else if (fToggleHidden)
        hide();
}

void BitcoinGUI::toggleHidden() {
    showNormalIfMinimized(true);
}

void BitcoinGUI::detectShutdown() {
    if (ShutdownRequested()) {
        if (rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}
//check this for theme 
void BitcoinGUI::showProgress(const QString& title, int nProgress) {
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    } else if (progressDialog)
        progressDialog->setValue(nProgress);
}

void BitcoinGUI::setTrayIconVisible(bool fHideTrayIcon)
{
    if (trayIcon)
    {
        trayIcon->setVisible(!fHideTrayIcon);
    }
}

void BitcoinGUI::showModalOverlay()
{
    if (modalOverlay && (progressBar->isVisible() || modalOverlay->isLayerVisible()))
        modalOverlay->toggleVisibility();
}


static bool ThreadSafeMessageBox(BitcoinGUI* gui, const std::string& message, const std::string& caption, unsigned int style) {
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
        modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(caption)),
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(unsigned int, style),
        Q_ARG(bool*, &ret));
    return ret;
}

void BitcoinGUI::subscribeToCoreSignals() {
    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
    uiInterface.ThreadSafeQuestion.connect(boost::bind(ThreadSafeMessageBox, this, _1, _3, _4));
}

void BitcoinGUI::unsubscribeFromCoreSignals() {
    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
    uiInterface.ThreadSafeQuestion.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _3, _4));
}

void BitcoinGUI::toggleNetworkActive()
{
    if (clientModel) {
        clientModel->setNetworkActive(!clientModel->getNetworkActive());
    }
}

/** Get restart command-line parameters and request restart */
void BitcoinGUI::handleRestart(QStringList args) {
    if (!ShutdownRequested())
        Q_EMIT requestedRestart(args);
}

UnitDisplayStatusBarControl::UnitDisplayStatusBarControl(const PlatformStyle *platformStyle) : optionsModel(0),
                                                             menu(0)
{
    createContextMenu();
    setToolTip(tr("Unit to show amounts in. Click to select another unit."));
}

/** So that it responds to button clicks */
void UnitDisplayStatusBarControl::mousePressEvent(QMouseEvent* event) {
    onDisplayUnitsClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void UnitDisplayStatusBarControl::createContextMenu() {
    menu = new QMenu(this);
    Q_FOREACH (BitcoinUnits::Unit u, BitcoinUnits::availableUnits()) {
        QAction* menuAction = new QAction(QString(BitcoinUnits::name(u)), this);
        menuAction->setData(QVariant(u));
        menu->addAction(menuAction);
    }
    connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(onMenuSelection(QAction*)));
}

/** Lets the control know about the Options Model (and its signals) */
void UnitDisplayStatusBarControl::setOptionsModel(OptionsModel* optionsModel) {
    if (optionsModel) {
        this->optionsModel = optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(optionsModel, SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit(int)));

        // initialize the display units label with the current value in the model.
        updateDisplayUnit(optionsModel->getDisplayUnit());
    }
}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void UnitDisplayStatusBarControl::updateDisplayUnit(int newUnits) {
    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        setPixmap(QIcon(":/icons/unit_" + BitcoinUnits::id(newUnits)).pixmap(39, STATUSBAR_ICONSIZE));
    } else {
        setPixmap(QIcon(":/icons/unit_t" + BitcoinUnits::id(newUnits)).pixmap(39, STATUSBAR_ICONSIZE));
    }
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void UnitDisplayStatusBarControl::onDisplayUnitsClicked(const QPoint& point) {
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void UnitDisplayStatusBarControl::onMenuSelection(QAction* action) {
    if (action) {
        optionsModel->setDisplayUnit(action->data());
    }
}
