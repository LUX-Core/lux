// Copyright (c) 2018 The Luxcore Developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "walletview.h"
#include "createcontract.h"
#include "addressbookpage.h"
#include "askpassphrasedialog.h"
#include "bip38tooldialog.h"
#include "bitcoingui.h"
#include "blockexplorer.h"
#include "clientmodel.h"
#include "guiutil.h"
#include "masternodeconfig.h"
#include "multisenddialog.h"
#include "optionsmodel.h"
#include "overviewpage.h"
#include "platformstyle.h"
#include "receivecoinsdialog.h"
#include "sendcoinsdialog.h"
#include "signverifymessagedialog.h"
#include "transactiontablemodel.h"
#include "tokentransactiontablemodel.h"
#include "transactionview.h"
#include "walletmodel.h"
#include "smartcontract.h"
#include "lsrtoken.h"
#include "restoredialog.h"

#include "ui_interface.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>


WalletView::WalletView(const PlatformStyle* platformStyle, QWidget* parent) : QStackedWidget(parent),
                                                                             clientModel(0),
                                                                             walletModel(0),
                                                                             platformStyle(platformStyle)
{
    // Create tabs
    overviewPage = new OverviewPage(platformStyle, this);
    explorerWindow = new BlockExplorer(platformStyle, this);
    transactionsPage = new QWidget(this);
    smartContractPage = new SmartContract(this);
    LSRTokenPage = new LSRToken(this);
    QVBoxLayout* vbox = new QVBoxLayout();
    QHBoxLayout* hbox_buttons = new QHBoxLayout();
    transactionView = new TransactionView(platformStyle, this);
    vbox->addWidget(transactionView);
    QPushButton *exportButton = new QPushButton(tr("&Export"), this);
    exportButton->setToolTip(tr("Export the data in the current tab to a file"));
    if (platformStyle->getImagesOnButtons()) {
        exportButton->setIcon(platformStyle->SingleColorIcon(":/icons/export"));
    }
    hbox_buttons->addStretch();

    // Sum of selected transactions
    QLabel* transactionSumLabel = new QLabel();                // Label
    transactionSumLabel->setObjectName("transactionSumLabel"); // Label ID as CSS-reference
    transactionSumLabel->setText(tr("Selected amount:"));
    hbox_buttons->addWidget(transactionSumLabel);

    transactionSum = new QLabel();                   // Amount
    transactionSum->setObjectName("transactionSum"); // Label ID as CSS-reference
    transactionSum->setMinimumSize(200, 8);
    transactionSum->setTextInteractionFlags(Qt::TextSelectableByMouse);
    hbox_buttons->addWidget(transactionSum);

    hbox_buttons->addWidget(exportButton);
    vbox->addLayout(hbox_buttons);
    transactionsPage->setLayout(vbox);

    receiveCoinsPage = new ReceiveCoinsDialog(platformStyle);
    sendCoinsPage = new SendCoinsDialog(platformStyle);

    addWidget(overviewPage);
    addWidget(transactionsPage);

    addWidget(receiveCoinsPage);
    addWidget(sendCoinsPage);
    addWidget(explorerWindow);
    addWidget(smartContractPage);   // Testing
    addWidget(LSRTokenPage);

    QSettings settings;

    masternodeManagerPage = new MasternodeManager();
    addWidget(masternodeManagerPage);

    // Clicking on a transaction on the overview pre-selects the transaction on the transaction history page
    connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

    // Clicking on a token on the overview pre-selects the token on the LSR Token page
    connect(overviewPage, &OverviewPage::tokenClicked, LSRTokenPage, &LSRToken::focusToken);

    connect(overviewPage, SIGNAL(outOfSyncWarningClicked()), this, SLOT(requestedSyncWarningInfo()));

    // Double-clicking on a transaction on the transaction history page shows details
    connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

    // Update wallet with sum of selected transactions
    connect(transactionView, SIGNAL(trxAmount(QString)), this, SLOT(trxAmount(QString)));

    // Clicking on "Export" allows to export the transaction list
    connect(exportButton, SIGNAL(clicked()), transactionView, SLOT(exportClicked()));

    // Pass through messages from sendCoinsPage
    connect(sendCoinsPage, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

    // Pass through messages from transactionView
    connect(transactionView, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));
}

WalletView::~WalletView()
{
}

void WalletView::setBitcoinGUI(BitcoinGUI* gui)
{
    if (gui) {
        // Clicking on a transaction on the overview page simply sends you to transaction history page
        connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)),
                gui, SLOT(gotoHistoryPage()));

        // Clicking on a token on the overview page simply sends you to LSR Token page
        connect(overviewPage,SIGNAL(tokenClicked(QModelIndex)),
                gui, SLOT(gotoLSRTokenPage()));

        // Receive and report messages
        connect(this, SIGNAL(message(QString, QString, unsigned int)), gui, SLOT(message(QString, QString, unsigned int)));

        // Pass through encryption status changed signals
        connect(this, SIGNAL(encryptionStatusChanged(int)), gui, SLOT(setEncryptionStatus(int)));

        // Pass through transaction notifications
        connect(this, SIGNAL(incomingTransaction(QString, int, CAmount, QString, QString, QString)), gui, SLOT(incomingTransaction(QString, int, CAmount, QString, QString, QString)));

        // Connect HD enabled state signal
        connect(this, SIGNAL(hdEnabledStatusChanged(int)), gui, SLOT(setHDStatus(int)));

        // Pass through token transaction notifications
        connect(this, SIGNAL(incomingTokenTransaction(QString,QString,QString,QString,QString,QString)), gui, SLOT(incomingTokenTransaction(QString,QString,QString,QString,QString,QString)));

        // Clicking on add token button sends you to add token page
        connect(overviewPage, SIGNAL(addTokenClicked(bool)), gui, SLOT(gotoLSRTokenPage(bool)));

    }
}

void WalletView::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;

    overviewPage->setClientModel(clientModel);
    sendCoinsPage->setClientModel(clientModel);
    smartContractPage->setClientModel(clientModel);
    LSRTokenPage->setClientModel(clientModel);
    QSettings settings;

    masternodeManagerPage->setClientModel(clientModel);
}

void WalletView::setWalletModel(WalletModel* walletModel)
{
    this->walletModel = walletModel;

    // Put transaction list in tabs
    transactionView->setModel(walletModel);
    overviewPage->setWalletModel(walletModel);
    smartContractPage->setModel(walletModel);
    LSRTokenPage->setModel(walletModel);
    QSettings settings;

    masternodeManagerPage->setWalletModel(walletModel);
    receiveCoinsPage->setModel(walletModel);
    sendCoinsPage->setModel(walletModel);

    if (walletModel) {
        // Receive and pass through messages from wallet model
        connect(walletModel, SIGNAL(message(QString, QString, unsigned int)), this, SIGNAL(message(QString, QString, unsigned int)));

        // Handle changes in encryption status
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SIGNAL(encryptionStatusChanged(int)));
        updateEncryptionStatus();

        // update HD status
        Q_EMIT hdEnabledStatusChanged(walletModel->hdEnabled());

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex, int, int)),
            this, SLOT(processNewTransaction(QModelIndex, int, int)));

        // Balloon pop-up for new token transaction
        connect(walletModel->getTokenTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(processNewTokenTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));

        // Show progress dialog
        connect(walletModel, SIGNAL(showProgress(QString, int)), this, SLOT(showProgress(QString, int)));
    }
}

void WalletView::processNewTransaction(const QModelIndex& parent, int start, int /*end*/)
{
    // Prevent balloon-spam when initial block download is in progress
    if (!walletModel || !clientModel || clientModel->inInitialBlockDownload())
        return;

    TransactionTableModel* ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    QString date = ttm->index(start, TransactionTableModel::Date, parent).data().toString();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent).data(Qt::EditRole).toULongLong();
    QString type = ttm->index(start, TransactionTableModel::Type, parent).data().toString();
    QString address = ttm->index(start, TransactionTableModel::ToAddress, parent).data().toString();
    QString label = ttm->index(start, TransactionTableModel::LabelRole, parent).data().toString();

    Q_EMIT incomingTransaction(date, walletModel->getOptionsModel()->getDisplayUnit(), amount, type, address, label);
}

// ----------------------------------------------------------------------------------------------- //
void WalletView::processNewTokenTransaction(const QModelIndex &parent, int start, int /*end*/)
{
    // Prevent balloon-spam when initial block download is in progress
    if (!walletModel || !clientModel || clientModel->inInitialBlockDownload())
        return;

    TokenTransactionTableModel* ttm = walletModel->getTokenTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    QString date = ttm->index(start, TokenTransactionTableModel::Date, parent).data().toString();
    QString amount(ttm->index(start, TokenTransactionTableModel::Amount, parent).data(TokenTransactionTableModel::FormattedAmountWithUnitRole).toString());
    QString type = ttm->index(start, TokenTransactionTableModel::Type, parent).data().toString();
    //QModelIndex index = ttm->index(start, 0, parent);
    QString address = ttm->index(start, TokenTransactionTableModel::AddressRole, parent).data().toString();
    QString label = ttm->index(start, TokenTransactionTableModel::LabelRole, parent).data().toString();
    QString title = ttm->index(start, TokenTransactionTableModel::TypeRole, parent).data().toString();

    Q_EMIT incomingTokenTransaction(date, amount, type, address, label, title);
}

void WalletView::gotoOverviewPage()
{
    setCurrentWidget(overviewPage);
}

void WalletView::gotoHistoryPage()
{
    setCurrentWidget(transactionsPage);
}

void WalletView::gotoBlockExplorerPage()
{
    setCurrentWidget(explorerWindow);
}

void WalletView::gotoMasternodePage()
{
    QSettings settings;
    setCurrentWidget(masternodeManagerPage);
}
void WalletView::gotoSmartContractPage()
{
    setCurrentWidget(smartContractPage);       //Testing
}

void WalletView::gotoLSRTokenPage(bool toAddTokenPage)
{
    setCurrentWidget(LSRTokenPage);
    if(toAddTokenPage)
        LSRTokenPage->on_goToAddTokenPage();
}

void WalletView::gotoReceiveCoinsPage()
{
    setCurrentWidget(receiveCoinsPage);
}

void WalletView::gotoSendCoinsPage(QString addr)
{
    setCurrentWidget(sendCoinsPage);

    if (!addr.isEmpty())
        sendCoinsPage->setAddress(addr);
}

void WalletView::gotoSignMessageTab(QString addr)
{
    // calls show() in showTab_SM()
    SignVerifyMessageDialog* signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_SM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_SM(addr);
}

void WalletView::gotoVerifyMessageTab(QString addr)
{
    // calls show() in showTab_VM()
    SignVerifyMessageDialog* signVerifyMessageDialog = new SignVerifyMessageDialog(platformStyle, this);
    signVerifyMessageDialog->setAttribute(Qt::WA_DeleteOnClose);
    signVerifyMessageDialog->setModel(walletModel);
    signVerifyMessageDialog->showTab_VM(true);

    if (!addr.isEmpty())
        signVerifyMessageDialog->setAddress_VM(addr);
}

void WalletView::gotoBip38Tool()
{
    Bip38ToolDialog* bip38ToolDialog = new Bip38ToolDialog(platformStyle, this);
    //bip38ToolDialog->setAttribute(Qt::WA_DeleteOnClose);
    bip38ToolDialog->setModel(walletModel);
    bip38ToolDialog->showTab_ENC(true);
}

void WalletView::gotoMultiSendDialog()
{
    MultiSendDialog* multiSendDialog = new MultiSendDialog(platformStyle, this);
    multiSendDialog->setModel(walletModel);
    multiSendDialog->show();
}

bool WalletView::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    return sendCoinsPage->handlePaymentRequest(recipient);
}

void WalletView::showOutOfSyncWarning(bool fShow)
{
    overviewPage->showOutOfSyncWarning(fShow);
}

void WalletView::updateEncryptionStatus()
{
    Q_EMIT encryptionStatusChanged(walletModel->getEncryptionStatus());
}

void WalletView::encryptWallet(bool status)
{
    if (!walletModel)
        return;
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt : AskPassphraseDialog::Decrypt, this, walletModel);
//  dlg.setModel(walletModel);
    dlg.exec();

    updateEncryptionStatus();
}

void WalletView::backupWallet()
{
    QString filename = GUIUtil::getSaveFileName(this,
        tr("Backup Wallet"), QString(),
        tr("Wallet (*.dat)"), NULL);

    if (filename.isEmpty())
        return;

    if (!walletModel->backupWallet(filename)) {
        Q_EMIT message(tr("Backup Failed"), tr("There was an error trying to save the wallet data to %1.").arg(filename),
            CClientUIInterface::MSG_ERROR);
    } else {
        Q_EMIT message(tr("Backup Successful"), tr("The wallet data was successfully saved to %1.").arg(filename),
            CClientUIInterface::MSG_INFORMATION);
    }
}

void WalletView::restoreWallet()
{
    RestoreDialog dlg(this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void WalletView::changePassphrase()
{
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this, walletModel);
//  dlg.setModel(walletModel);
    dlg.exec();
}

void WalletView::unlockWallet()
{
    if (!walletModel)
        return;
    // Unlock wallet when requested by wallet model

    if (walletModel->getEncryptionStatus() == WalletModel::Locked || walletModel->getEncryptionStatus() == WalletModel::UnlockedForStakingOnly) {
        AskPassphraseDialog dlg(AskPassphraseDialog::UnlockAnonymize, this, walletModel);
//      dlg.setModel(walletModel);
        dlg.exec();
    }
}

void WalletView::lockWallet()
{
    if (!walletModel)
        return;

    walletModel->setWalletLocked(true);
}

void WalletView::usedSendingAddresses()
{
    if (!walletModel)
        return;
    AddressBookPage* dlg = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::SendingTab, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModel(walletModel->getAddressTableModel());
    dlg->show();
}

void WalletView::usedReceivingAddresses()
{
    if (!walletModel)
        return;
    AddressBookPage* dlg = new AddressBookPage(platformStyle, AddressBookPage::ForEditing, AddressBookPage::ReceivingTab, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModel(walletModel->getAddressTableModel());
    dlg->show();
}

void WalletView::showProgress(const QString& title, int nProgress)
{
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButtonText(tr("Cancel"));
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    } else if (progressDialog) {
        if (progressDialog->wasCanceled()) {
            getWalletModel()->getCurrentWallet()->AbortRescan();
        } else {
            progressDialog->setValue(nProgress);
        }
    }
}

void WalletView::requestedSyncWarningInfo()
{
    Q_EMIT outOfSyncWarningClicked();
}

/** Update wallet with the sum of the selected transactions */
void WalletView::trxAmount(QString amount)
{
    transactionSum->setText(amount);
}
