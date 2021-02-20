// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coincontroldialog.h"
#include "ui_coincontroldialog.h"

#include "addresstablemodel.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "init.h"
#include "optionsmodel.h"
#include "walletmodel.h"
#include "clientmodel.h"
#include "platformstyle.h"

#include "coincontrol.h"
#include "main.h"
#include "darksend.h"
#include "wallet.h"

#include <boost/assign/list_of.hpp> // for 'map_list_of()'

#include <QApplication>
#include <QCheckBox>
#include <QCursor>
#include <QDialogButtonBox>
#include <QFlags>
#include <QIcon>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>

static QString WarnInputErrorMessage = "Selecting a large numbers of inputs may take a long time. During this process, the Lux wallet may enter into a 'not responding' status. Once the operation is complete, the 'not responding' status will go away.";

using namespace std;
QList<CAmount> CoinControlDialog::payAmounts;
int CoinControlDialog::nSplitBlockDummy;
CCoinControl* CoinControlDialog::coinControl = new CCoinControl();
bool CoinControlDialog::fSubtractFeeFromAmount = false;

bool CCoinControlWidgetItem::operator<(const QTreeWidgetItem &other) const {
    int column = treeWidget()->sortColumn();
    if (column == CoinControlDialog::COLUMN_AMOUNT ||
        column == CoinControlDialog::COLUMN_DATE ||
        column == CoinControlDialog::COLUMN_CONFIRMATIONS ||
        column == CoinControlDialog::COLUMN_DARKSEND_ROUNDS)
        return data(column, Qt::UserRole).toLongLong() < other.data(column, Qt::UserRole).toLongLong();
    return QTreeWidgetItem::operator<(other);
}


CoinControlDialog::CoinControlDialog(const PlatformStyle *platformStyle, QWidget* parent) : QDialog(parent),
                                                        ui(new Ui::CoinControlDialog),
                                                        model(0),
                                                        clientModel(0),
                                                        platformStyle(platformStyle)
{
    ui->setupUi(this);

    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());

    // context menu actions
    QAction* copyAddressAction = new QAction(tr("Copy address"), this);
    QAction* copyLabelAction = new QAction(tr("Copy label"), this);
    QAction* copyAmountAction = new QAction(tr("Copy amount"), this);
    copyTransactionHashAction = new QAction(tr("Copy transaction ID"), this); // we need to enable/disable this
    lockAction = new QAction(tr("Lock unspent"), this);                       // we need to enable/disable this
    unlockAction = new QAction(tr("Unlock unspent"), this);                   // we need to enable/disable this

    // context menu
    contextMenu = new QMenu(this);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyLabelAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTransactionHashAction);
    contextMenu->addSeparator();
    contextMenu->addAction(lockAction);
    contextMenu->addAction(unlockAction);

    // context menu signals
    connect(ui->treeWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showMenu(QPoint)));
    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(copyAddress()));
    connect(copyLabelAction, SIGNAL(triggered()), this, SLOT(copyLabel()));
    connect(copyAmountAction, SIGNAL(triggered()), this, SLOT(copyAmount()));
    connect(copyTransactionHashAction, SIGNAL(triggered()), this, SLOT(copyTransactionHash()));
    connect(lockAction, SIGNAL(triggered()), this, SLOT(lockCoin()));
    connect(unlockAction, SIGNAL(triggered()), this, SLOT(unlockCoin()));

    // clipboard actions
    QAction* clipboardQuantityAction = new QAction(tr("Copy quantity"), this);
    QAction* clipboardAmountAction = new QAction(tr("Copy amount"), this);
    QAction* clipboardFeeAction = new QAction(tr("Copy fee"), this);
    QAction* clipboardAfterFeeAction = new QAction(tr("Copy after fee"), this);
    QAction* clipboardBytesAction = new QAction(tr("Copy bytes"), this);
    QAction* clipboardPriorityAction = new QAction(tr("Copy priority"), this);
    QAction* clipboardLowOutputAction = new QAction(tr("Copy dust"), this);
    QAction* clipboardChangeAction = new QAction(tr("Copy change"), this);

    connect(clipboardQuantityAction, SIGNAL(triggered()), this, SLOT(clipboardQuantity()));
    connect(clipboardAmountAction, SIGNAL(triggered()), this, SLOT(clipboardAmount()));
    connect(clipboardFeeAction, SIGNAL(triggered()), this, SLOT(clipboardFee()));
    connect(clipboardAfterFeeAction, SIGNAL(triggered()), this, SLOT(clipboardAfterFee()));
    connect(clipboardBytesAction, SIGNAL(triggered()), this, SLOT(clipboardBytes()));
    connect(clipboardPriorityAction, SIGNAL(triggered()), this, SLOT(clipboardPriority()));
    connect(clipboardLowOutputAction, SIGNAL(triggered()), this, SLOT(clipboardLowOutput()));
    connect(clipboardChangeAction, SIGNAL(triggered()), this, SLOT(clipboardChange()));

    ui->labelCoinControlQuantity->addAction(clipboardQuantityAction);
    ui->labelCoinControlAmount->addAction(clipboardAmountAction);
    ui->labelCoinControlFee->addAction(clipboardFeeAction);
    ui->labelCoinControlAfterFee->addAction(clipboardAfterFeeAction);
    ui->labelCoinControlBytes->addAction(clipboardBytesAction);
    ui->labelCoinControlPriority->addAction(clipboardPriorityAction);
    ui->labelCoinControlLowOutput->addAction(clipboardLowOutputAction);
    ui->labelCoinControlChange->addAction(clipboardChangeAction);

    // toggle tree/list mode
    connect(ui->radioTreeMode, SIGNAL(toggled(bool)), this, SLOT(radioTreeMode(bool)));
    connect(ui->radioListMode, SIGNAL(toggled(bool)), this, SLOT(radioListMode(bool)));

    // click on checkbox
    connect(ui->treeWidget, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(viewItemChanged(QTreeWidgetItem*, int)));

// click on header
#if QT_VERSION < 0x050000
    ui->treeWidget->header()->setClickable(true);
#else
    ui->treeWidget->header()->setSectionsClickable(true);
#endif
    connect(ui->treeWidget->header(), SIGNAL(sectionClicked(int)), this, SLOT(headerSectionClicked(int)));

    // ok button
    connect(ui->buttonBox, SIGNAL(clicked(QAbstractButton*)), this, SLOT(buttonBoxClicked(QAbstractButton*)));

    // (un)select all
    connect(ui->pushButtonSelectAll, SIGNAL(clicked()), this, SLOT(buttonSelectAllClicked()));

    // Toggle lock state
    connect(ui->pushButtonToggleLock, SIGNAL(clicked()), this, SLOT(buttonToggleLockClicked()));

    //selection of first 50 inputs on button pressed
    connect(ui->select_50, SIGNAL(clicked()), this, SLOT(select_50()));

    //selection of first 100 inputs on button pressed
    connect(ui->select_100, SIGNAL(clicked()), this, SLOT(select_100()));

    //selection of first 250 inputs on button pressed
    connect(ui->select_250, SIGNAL(clicked()), this, SLOT(select_250()));
    
    //Make "advanced" features visible if check (advance check box)
    connect(ui->advanced, SIGNAL(clicked(bool)), this, SLOT(toggled(bool)));
    
    //selection of all inputs greater than what the user inputs into "num_box"
    connect(ui->GreaterThan, SIGNAL(clicked()), this, SLOT(greater()));

    //selection of all inputs less than what the user inputs into "num_box"
    connect(ui->LessThan, SIGNAL(clicked()), this, SLOT(Less()));

    //selection of all inputs equal to what the user inputs into "num_box"
    connect(ui->EqualTo, SIGNAL(clicked()), this, SLOT(Equal()));

    // change coin control first column label due Qt4 bug.
    // see https://github.com/bitcoin/bitcoin/issues/5716
    ui->treeWidget->headerItem()->setText(COLUMN_CHECKBOX, QString());

	int columnCheckboxWidth = 50;
	int columnAmountWidth = 100;
	int columnLabelWidth = 160;
	int columnAddressWidth = 230;
	int columnDateWidth = 100;
	int columnConfirmationsWidth = 100;
	int columnPriority = 100;
	int columnDarkSendWidth = 150;
	
    ui->treeWidget->setColumnWidth(COLUMN_CHECKBOX, columnCheckboxWidth); 
    ui->treeWidget->setColumnWidth(COLUMN_AMOUNT, columnAmountWidth);
    ui->treeWidget->setColumnWidth(COLUMN_LABEL, columnLabelWidth);
    ui->treeWidget->setColumnWidth(COLUMN_ADDRESS, columnAddressWidth);
    ui->treeWidget->setColumnWidth(COLUMN_DATE, columnDateWidth);
    ui->treeWidget->setColumnWidth(COLUMN_CONFIRMATIONS, columnConfirmationsWidth);
    ui->treeWidget->setColumnWidth(COLUMN_PRIORITY, columnPriority);
    ui->treeWidget->setColumnWidth(COLUMN_DARKSEND_ROUNDS, columnDarkSendWidth);
	
    ui->treeWidget->setColumnHidden(COLUMN_TXHASH, true);         // store transacton hash in this column, but dont show it
    ui->treeWidget->setColumnHidden(COLUMN_VOUT_INDEX, true);     // store vout index in this column, but dont show it
    ui->treeWidget->setColumnHidden(COLUMN_PRIORITY_INT64, true);
    ui->treeWidget->setColumnHidden(COLUMN_DATE_INT64, true);     // store date int64 in this column, but dont show it
	
	QSettings settings;
	
	if (settings.value("theme").toString() == "dark grey") {
		QString styleSheet = ".CoinControlTreeWidget#treeWidget {  background-color: #262626; alternate-background-color:#424242; " 
								"gridline-color: #fff5f5; border: 1px solid #fff5f5; color: #fff; min-height:2em; }";					
		ui->treeWidget->setStyleSheet(styleSheet);
	} else if (settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".CoinControlTreeWidget#treeWidget {  background-color: #031d54; alternate-background-color:#0D2A64; " 
								"gridline-color: #fff5f5; border: 1px solid #fff5f5; color: #fff; min-height:2em; }";
		ui->treeWidget->setStyleSheet(styleSheet);
	} else { 
		//code here
	}
	
    ui->num_box->setRange(1, 9999); // set the range
    ui->select_50->setVisible(false); // set the advanced features to hidden 
    ui->select_100->setVisible(false);
    ui->select_250->setVisible(false);
    ui->GreaterThan->setVisible(false);
    ui->LessThan->setVisible(false);
    ui->EqualTo->setVisible(false);
    ui->num_box->setVisible(false);

    // default view is sorted by amount desc
    sortView(COLUMN_AMOUNT, Qt::DescendingOrder);

    // restore list mode and sortorder as a convenience feature
    
    if (settings.contains("nCoinControlMode") && !settings.value("nCoinControlMode").toBool())
        ui->radioTreeMode->click();
    if (settings.contains("nCoinControlSortColumn") && settings.contains("nCoinControlSortOrder"))
        sortView(settings.value("nCoinControlSortColumn").toInt(), ((Qt::SortOrder)settings.value("nCoinControlSortOrder").toInt()));
    if (settings.contains("toogleLockValue"))
    {
        QString toogleLockValue = settings.value("toogleLockValue").toString();
        toogleLockList = toogleLockValue.split(",");
    }
}

CoinControlDialog::~CoinControlDialog()
{
    QSettings settings;
    settings.setValue("nCoinControlMode", ui->radioListMode->isChecked());
    settings.setValue("nCoinControlSortColumn", sortColumn);
    settings.setValue("nCoinControlSortOrder", (int)sortOrder);
    QString toogleLockValue;
    QTreeWidgetItem* item;
    ui->radioListMode->click();
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        item = ui->treeWidget->topLevelItem(i);
        COutPoint outpt(uint256(item->text(COLUMN_TXHASH).toStdString()), item->text(COLUMN_VOUT_INDEX).toUInt());
        if (model->isLockedCoin(uint256(item->text(COLUMN_TXHASH).toStdString()), item->text(COLUMN_VOUT_INDEX).toUInt())) {
            if (toogleLockValue.trimmed().isEmpty()) {
                toogleLockValue = item->text(COLUMN_TXHASH);
            } else {
                toogleLockValue = toogleLockValue + "," + item->text(COLUMN_TXHASH);
            }
        }
    }
    settings.setValue("toogleLockValue", toogleLockValue);

    delete ui;
}

void CoinControlDialog::setModel(WalletModel* model)
{
    this->model = model;

    if (model && model->getOptionsModel() && model->getAddressTableModel()) {
        updateView();
        updateLabelLocked();
        CoinControlDialog::updateLabels(model, this);
        CheckDialogLablesUpdated();
    }
}

void CoinControlDialog::setClientModel(ClientModel* clientModel) {
    this->clientModel = clientModel;
    connect(clientModel, SIGNAL(numBlocksChanged()), this, SLOT(updateInfoInDialog()));
}

void CoinControlDialog::updateInfoInDialog() {
    updateView();
}

// helper function str_pad
QString CoinControlDialog::strPad(QString s, int nPadLength, QString sPadding)
{
    while (s.length() < nPadLength)
        s = sPadding + s;

    return s;
}

// ok button
void CoinControlDialog::buttonBoxClicked(QAbstractButton* button)
{
    if (ui->buttonBox->buttonRole(button) == QDialogButtonBox::AcceptRole)
        done(QDialog::Accepted); // closes the dialog
}

// (un)select all
void CoinControlDialog::buttonSelectAllClicked()
{
 Qt::CheckState state = Qt::Checked;
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != Qt::Unchecked) {
            state = Qt::Unchecked;
            break;
        }
    }
    ui->treeWidget->setEnabled(false);
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
        if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != state)
            ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, state);

    ui->treeWidget->setEnabled(true);
    if (state == Qt::Unchecked)
        coinControl->UnSelectAll(); // just to be sure
    CoinControlDialog::updateLabels(model, this);
    CheckDialogLablesUpdated();
}

void CoinControlDialog::HideInputAutoSelection() //set the advanced features to hidden
{
    ui->select_50->setVisible(false);
    ui->select_100->setVisible(false);
    ui->select_250->setVisible(false);
    ui->GreaterThan->setVisible(false);
    ui->LessThan->setVisible(false);
    ui->EqualTo->setVisible(false);
    ui->num_box->setVisible(false);
}

void CoinControlDialog::ShowInputAutoSelection() // set the advanced features to shown
{
    ui->select_50->setVisible(true);
    ui->select_100->setVisible(true);
    ui->select_250->setVisible(true);
    ui->GreaterThan->setVisible(true);
    ui->LessThan->setVisible(true);
    ui->EqualTo->setVisible(true);
    ui->num_box->setVisible(true);
}

unsigned int CoinControlDialog::SIZE_OF_TX(){
        // nPayAmount
    CAmount nPayAmount = 0;
    bool fDust = false;
    CMutableTransaction txDummy;
    Q_FOREACH (const CAmount& amount, CoinControlDialog::payAmounts) {
        nPayAmount += amount;

        if (amount > 0) {
            CTxOut txout(amount, (CScript)vector<unsigned char>(24, 0));
            txDummy.vout.push_back(txout);
            if (txout.IsDust(::minRelayTxFee))
                fDust = true;
        }
    }

    CAmount nAmount = 0;
    unsigned int nBytes = 0;
    unsigned int nBytesInputs = 0;
    unsigned int nQuantity = 0;
    int nQuantityUncompressed = 0;
    bool fAllowFree = false;
    bool fWitness = false;

    vector<COutPoint> vCoinControl;
    vector<COutput> vOutputs;
    coinControl->ListSelected(vCoinControl);
    model->getOutputs(vCoinControl, vOutputs);

    for (const COutput& out : vOutputs) {
        // unselect already spent, very unlikely scenario, this could happen
        // when selected are spent elsewhere, like rpc or another computer
        uint256 txhash = out.tx->GetHash();
        COutPoint outpt(txhash, out.i);
        if (model->isSpent(outpt)) {
            coinControl->UnSelect(outpt);
            continue;
        }

        // Quantity
        nQuantity++;


        // Bytes
        CTxDestination address;
        int witnessversion = 0;
        std::vector<unsigned char> witnessprogram;
        if (out.tx->vout[out.i].scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram))
        {
            nBytesInputs += (32 + 4 + 1 + (107 / WITNESS_SCALE_FACTOR) + 4);
            fWitness = true;
        }
        else if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) {
            CPubKey pubkey;
            CKeyID* keyid = boost::get<CKeyID>(&address);
            if (keyid && model->getPubKey(*keyid, pubkey)) {
                nBytesInputs += (pubkey.IsCompressed() ? 148 : 180);
                if (!pubkey.IsCompressed())
                    nQuantityUncompressed++;
            } else
                nBytesInputs += 148; // in all error cases, simply assume 148 here
        } else
            nBytesInputs += 148;
    }

    // calculation
    if (nQuantity > 0) {
        // Bytes
        nBytes = nBytesInputs + ((CoinControlDialog::payAmounts.size() > 0 ? CoinControlDialog::payAmounts.size() + max(1, CoinControlDialog::nSplitBlockDummy) : 2) * 34) + 10; // always assume +1 output for change here
        if (fWitness)
        {
            // there is some fudging in these numbers related to the actual virtual transaction size calculation that will keep this estimate from being exact.
            // usually, the result will be an overestimate within a couple of satoshis so that the confirmation dialog ends up displaying a slightly smaller fee.
            // also, the witness stack size value value is a variable sized integer. usually, the number of stack items will be well under the single byte var int limit.
            nBytes += 2; // account for the serialized marker and flag bytes
            nBytes += nQuantity; // account for the witness byte that holds the number of stack items for each input.
        }

    }  
    return nBytes;
}

bool CoinControlDialog::TX_size_limit()
{
    unsigned int MAX = MAX_STANDARD_TX_SIZE - 148; // small buffer
    return SIZE_OF_TX() >= MAX;
}

void CoinControlDialog::tree_view_data_to_double(QString &str, double &val){
    /*                              xxxxxxxxxx
    our tree will return "12 345.56789ABCDE" as "12 34556789ABCDE"...
    so .toDouble() not gonna work here (works just fine for 0 to 9999.99999)
    so we need to remove that space and put in our "."
    we could do this the lazy was and use 
    QRegExp space("\\s");  
    QString.remove(space); 
    but this is very slow and expensive 
    */

    if (str.length() >= 14){ // if num is 10,000 or more 
        str.remove(str.length() - 9, 7); // xx xxxxxxxxxx -> xx xxxxx
            /*QMessageBox A3;
            A3.setText(str);
            A3.exec();*/
         str.remove(str.length() - 6, 1); // xx xxxxx -> xxxxxxx
            /*QMessageBox A2;
            A2.setText(str);
            A2.exec();*/
        val = str.toDouble();
        val = val / 100;// xxxxxxx -> xxxxx.xx (we now have a usable num
    }else{
        val = str.toDouble();
    } 
}

void CoinControlDialog::greater()// select all inputs grater than "amount"
{
    int val = ui->num_box->value();   
    Qt::CheckState state = Qt::Checked;  

    QMessageBox WARN;
	WARN.setStyleSheet(GUIUtil::loadStyleSheet());
    WARN.setText(WarnInputErrorMessage);
    WARN.exec();

    ui->treeWidget->setEnabled(false);     
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        double value = 0;
        QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
        QString D = item->text(COLUMN_AMOUNT);
        tree_view_data_to_double(D,value);
        if (value > val) {
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != state) {
                ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, state);
                
            }

            if (TX_size_limit()){
                QMessageBox A;
                A.setText("MAX TX SIZE REACHED");
                A.exec();
                ui->treeWidget->setEnabled(true);
                if (state == Qt::Unchecked)
                coinControl->UnSelectAll(); // just to be sure
                CoinControlDialog::updateLabels(model, this);
                CheckDialogLablesUpdated();
                return;
            }
        }
    }
    ui->treeWidget->setEnabled(true);
    if (state == Qt::Unchecked)
    coinControl->UnSelectAll(); // just to be sure
    CoinControlDialog::updateLabels(model, this);
    CheckDialogLablesUpdated();
} 

void CoinControlDialog::Less()//select all inputs Less than "amount"
{
    QMessageBox WARN;
	WARN.setStyleSheet(GUIUtil::loadStyleSheet());
    WARN.setText(WarnInputErrorMessage);
    WARN.exec();

    int val = ui->num_box->value();   
    Qt::CheckState state = Qt::Checked;  
    ui->treeWidget->setEnabled(false);
        for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
            double value = 0;
            QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
            QString D = item->text(COLUMN_AMOUNT);
            tree_view_data_to_double(D,value);
        if (value < val) {
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != state) {
                ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, state);

                if (TX_size_limit()){
                    QMessageBox A;
                    A.setText("MAX TX SIZE REACHED");
                    A.exec();
                    ui->treeWidget->setEnabled(true);
                    if (state == Qt::Unchecked)
                    coinControl->UnSelectAll(); // just to be sure
                    CoinControlDialog::updateLabels(model, this);
                    CheckDialogLablesUpdated();
                    return;
                }
            }
        }
    } 
    ui->treeWidget->setEnabled(true);
    if (state == Qt::Unchecked)
    coinControl->UnSelectAll(); // just to be sure
    CoinControlDialog::updateLabels(model, this);
    CheckDialogLablesUpdated();
}


void CoinControlDialog::Equal() // select all inputs equal to "amount"
{
    QMessageBox WARN;
	WARN.setStyleSheet(GUIUtil::loadStyleSheet());
    WARN.setText(WarnInputErrorMessage);
    WARN.exec();


    double val = ui->num_box->value();  
    Qt::CheckState state = Qt::Checked;  
    ui->treeWidget->setEnabled(false);
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        double round = 0;
        double value = 0;
        QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
        QString D = item->text(COLUMN_AMOUNT);
        tree_view_data_to_double(D,value);
        int log = 0;
        adjusted:
        
        if (val > value) {
            round = val - value; 
            log++;
        }   
        //since we can't compare "value" and "val" directly we minus the 2 and get the difference
        if (val < value){
            round = value - val; 
            log++;
        }
        if (log == 0) {  
            val = val +0.01; 
            log++; 
            goto adjusted; 
        }   
        // in the event that the input and "val" are equal add a small amount and go through the sorting process again
        if (round < 0.5) { // if are input and "val" are within 0.5 of each other
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != state) {
                ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, state);
                if (TX_size_limit()){
                    QMessageBox A;
                    A.setText("MAX TX SIZE REACHED");
                    A.exec();
                    ui->treeWidget->setEnabled(true);
                    if (state == Qt::Unchecked)
                    coinControl->UnSelectAll(); // just to be sure
                    CoinControlDialog::updateLabels(model, this);
                    CheckDialogLablesUpdated();
                    return;
                }
            }
        }
    }
    ui->treeWidget->setEnabled(true);
    if (state == Qt::Unchecked)
    coinControl->UnSelectAll(); // just to be sure
    CoinControlDialog::updateLabels(model, this);
    CheckDialogLablesUpdated(); 
}

void CoinControlDialog::select_50() //select the first 50 inputs 
{
    if (ui->treeWidget->topLevelItemCount() > 49){ //check we have more then 50 inputs 
        Qt::CheckState state = Qt::Checked;   
        ui->treeWidget->setEnabled(false);
        for (int i = 0; i < 50; i++) {
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != state) {
                ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, state);
            }
        }
            ui->treeWidget->setEnabled(true);
            if (state == Qt::Unchecked)
            coinControl->UnSelectAll(); // just to be sure
            CoinControlDialog::updateLabels(model, this);
            CheckDialogLablesUpdated(); 
    } else { //if we have less then 50 inputs give the user dialogue to inform them of this issue  
        QMessageBox msgBox;
        msgBox.setObjectName("lockMessageBox");
        msgBox.setStyleSheet(GUIUtil::loadStyleSheet());
        msgBox.setText(tr("Please have at least 50 inputs to use this function."));
        msgBox.exec();
    }
}

void CoinControlDialog::select_100() //select the first 100 inputs 
{
    if (ui->treeWidget->topLevelItemCount() > 99){ //check we have more then 50 inputs 
        Qt::CheckState state = Qt::Checked;   
        ui->treeWidget->setEnabled(false);
        for (int i = 0; i < 50; i++) {
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != state) {
                ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, state);
                if (state == Qt::Unchecked) {
                    coinControl->UnSelectAll(); // just to be sure
                    CoinControlDialog::updateLabels(model, this);
                    CheckDialogLablesUpdated();
                }
            }
        }
            ui->treeWidget->setEnabled(true);
            if (state == Qt::Unchecked)
            coinControl->UnSelectAll(); // just to be sure
            CoinControlDialog::updateLabels(model, this);
            CheckDialogLablesUpdated(); 
    } else { //if we have less then 100 inputs give the user dialogue to inform them of this issue  
        QMessageBox msgBox;
        msgBox.setObjectName("lockMessageBox");
        msgBox.setStyleSheet(GUIUtil::loadStyleSheet());
        msgBox.setText(tr("Please have at least 100 inputs to use this function."));
        msgBox.exec();
    }
}

void CoinControlDialog::select_250() //select the first 250 inputs 
{
    if (ui->treeWidget->topLevelItemCount() > 249){ //check we have more then 50 inputs 
        Qt::CheckState state = Qt::Checked;   
        ui->treeWidget->setEnabled(false);
        for (int i = 0; i < 50; i++) {
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != state) {
                ui->treeWidget->topLevelItem(i)->setCheckState(COLUMN_CHECKBOX, state);
                ui->treeWidget->setEnabled(true);
            }
        }
            ui->treeWidget->setEnabled(true);
            if (state == Qt::Unchecked)
            coinControl->UnSelectAll(); // just to be sure
            CoinControlDialog::updateLabels(model, this);
            CheckDialogLablesUpdated(); 
    } else { //if we have less then 250 inputs give the user dialogue to inform them of this issue  
        QMessageBox msgBox;
        msgBox.setObjectName("lockMessageBox");
        msgBox.setStyleSheet(GUIUtil::loadStyleSheet());
        msgBox.setText(tr("Please have at least 250 inputs to use this function."));
        msgBox.exec();
    }
}

// Toggle lock state
void CoinControlDialog::buttonToggleLockClicked()
{
    // Works in list-mode only
    if (ui->radioListMode->isChecked()) {
        bool doLock = false;
        ui->treeWidget->setEnabled(false);

        // count selected (checked) inputs to lock, if none action will be to unlock all
        for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) == Qt::Checked) {
                doLock = true;
                break;
            }
        }

        for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
            QTreeWidgetItem* item = ui->treeWidget->topLevelItem(i);
            COutPoint outpt(uint256(item->text(COLUMN_TXHASH).toStdString()), item->text(COLUMN_VOUT_INDEX).toUInt());
            if (doLock == false && item->isDisabled()) {
                model->unlockCoin(outpt);
                item->setDisabled(false);
                item->setIcon(COLUMN_CHECKBOX, QIcon());
            } else if (doLock && item->checkState(COLUMN_CHECKBOX) == Qt::Checked) {
                model->lockCoin(outpt);
                item->setDisabled(true);
                item->setIcon(COLUMN_CHECKBOX, QIcon(":/icons/lock_closed"));
            }
            updateLabelLocked();
        }

        ui->treeWidget->setEnabled(true);
        CoinControlDialog::updateLabels(model, this);
        CheckDialogLablesUpdated();
    } else {
        QMessageBox msgBox;
        msgBox.setObjectName("lockMessageBox");
        msgBox.setStyleSheet(GUIUtil::loadStyleSheet());
        msgBox.setText(tr("Please switch to \"List mode\" to use this function."));
        msgBox.exec();
    }
}

// context menu
void CoinControlDialog::showMenu(const QPoint& point)
{
    QTreeWidgetItem* item = ui->treeWidget->itemAt(point);
    if (item) {
        contextMenuItem = item;

        // disable some items (like Copy Transaction ID, lock, unlock) for tree roots in context menu
        if (item->text(COLUMN_TXHASH).length() == 64) // transaction hash is 64 characters (this means its a child node, so its not a parent node in tree mode)
        {
            copyTransactionHashAction->setEnabled(true);
            if (model->isLockedCoin(uint256(item->text(COLUMN_TXHASH).toStdString()), item->text(COLUMN_VOUT_INDEX).toUInt())) {
                lockAction->setEnabled(false);
                unlockAction->setEnabled(true);
            } else {
                lockAction->setEnabled(true);
                unlockAction->setEnabled(false);
            }
        } else // this means click on parent node in tree mode -> disable all
        {
            copyTransactionHashAction->setEnabled(false);
            lockAction->setEnabled(false);
            unlockAction->setEnabled(false);
        }

        // show context menu
        contextMenu->exec(QCursor::pos());
    }
}

// context menu action: copy amount
void CoinControlDialog::copyAmount()
{
    GUIUtil::setClipboard(BitcoinUnits::removeSpaces(contextMenuItem->text(COLUMN_AMOUNT)));
}

// context menu action: copy label
void CoinControlDialog::copyLabel()
{
    if (ui->radioTreeMode->isChecked() && contextMenuItem->text(COLUMN_LABEL).length() == 0 && contextMenuItem->parent())
        GUIUtil::setClipboard(contextMenuItem->parent()->text(COLUMN_LABEL));
    else
        GUIUtil::setClipboard(contextMenuItem->text(COLUMN_LABEL));
}

// context menu action: copy address
void CoinControlDialog::copyAddress()
{
    if (ui->radioTreeMode->isChecked() && contextMenuItem->text(COLUMN_ADDRESS).length() == 0 && contextMenuItem->parent())
        GUIUtil::setClipboard(contextMenuItem->parent()->text(COLUMN_ADDRESS));
    else
        GUIUtil::setClipboard(contextMenuItem->text(COLUMN_ADDRESS));
}

// context menu action: copy transaction id
void CoinControlDialog::copyTransactionHash()
{
    GUIUtil::setClipboard(contextMenuItem->text(COLUMN_TXHASH));
}

// context menu action: lock coin
void CoinControlDialog::lockCoin()
{
    if (contextMenuItem->checkState(COLUMN_CHECKBOX) == Qt::Checked)
        contextMenuItem->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

    COutPoint outpt(uint256(contextMenuItem->text(COLUMN_TXHASH).toStdString()), contextMenuItem->text(COLUMN_VOUT_INDEX).toUInt());
    model->lockCoin(outpt);
    contextMenuItem->setDisabled(true);
    contextMenuItem->setIcon(COLUMN_CHECKBOX, QIcon(":/icons/lock_closed"));
    updateLabelLocked();
}

// context menu action: unlock coin
void CoinControlDialog::unlockCoin()
{
    COutPoint outpt(uint256(contextMenuItem->text(COLUMN_TXHASH).toStdString()), contextMenuItem->text(COLUMN_VOUT_INDEX).toUInt());
    model->unlockCoin(outpt);
    contextMenuItem->setDisabled(false);
    contextMenuItem->setIcon(COLUMN_CHECKBOX, QIcon());
    updateLabelLocked();
}

// copy label "Quantity" to clipboard
void CoinControlDialog::clipboardQuantity()
{
    GUIUtil::setClipboard(ui->labelCoinControlQuantity->text());
}

// copy label "Amount" to clipboard
void CoinControlDialog::clipboardAmount()
{
    GUIUtil::setClipboard(ui->labelCoinControlAmount->text().left(ui->labelCoinControlAmount->text().indexOf(" ")));
}

// copy label "Fee" to clipboard
void CoinControlDialog::clipboardFee()
{
    GUIUtil::setClipboard(ui->labelCoinControlFee->text().left(ui->labelCoinControlFee->text().indexOf(" ")).replace("~", ""));
}

// copy label "After fee" to clipboard
void CoinControlDialog::clipboardAfterFee()
{
    GUIUtil::setClipboard(ui->labelCoinControlAfterFee->text().left(ui->labelCoinControlAfterFee->text().indexOf(" ")).replace("~", ""));
}

// copy label "Bytes" to clipboard
void CoinControlDialog::clipboardBytes()
{
    GUIUtil::setClipboard(ui->labelCoinControlBytes->text().replace("~", ""));
}

// copy label "Priority" to clipboard
void CoinControlDialog::clipboardPriority()
{
    GUIUtil::setClipboard(ui->labelCoinControlPriority->text());
}

// copy label "Dust" to clipboard
void CoinControlDialog::clipboardLowOutput()
{
    GUIUtil::setClipboard(ui->labelCoinControlLowOutput->text());
}

// copy label "Change" to clipboard
void CoinControlDialog::clipboardChange()
{
    GUIUtil::setClipboard(ui->labelCoinControlChange->text().left(ui->labelCoinControlChange->text().indexOf(" ")).replace("~", ""));
}

// treeview: sort
void CoinControlDialog::sortView(int column, Qt::SortOrder order)
{
    sortColumn = column;
    sortOrder = order;
    ui->treeWidget->sortItems(column, order);
    ui->treeWidget->header()->setSortIndicator(sortColumn, sortOrder);
}

// treeview: clicked on header
void CoinControlDialog::headerSectionClicked(int logicalIndex)
{
    if (logicalIndex == COLUMN_CHECKBOX) // click on most left column -> do nothing
    {
        ui->treeWidget->header()->setSortIndicator(sortColumn, sortOrder);
    } else {
        if (sortColumn == logicalIndex)
            sortOrder = ((sortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder);
        else {
            sortColumn = logicalIndex;
            sortOrder = ((sortColumn == COLUMN_LABEL || sortColumn == COLUMN_ADDRESS) ? Qt::AscendingOrder : Qt::DescendingOrder); // if label or address then default => asc, else default => desc
        }

        sortView(sortColumn, sortOrder);
    }
}

// toggle tree mode
void CoinControlDialog::radioTreeMode(bool checked)
{
HideInputAutoSelection(); 
    if (checked && model)
        updateView();
}

// toggle list mode
void CoinControlDialog::radioListMode(bool checked)
{
    if (ui->advanced->checkState()){
        ShowInputAutoSelection();    
            }else{
            HideInputAutoSelection(); 
    }
    if (checked && model)
        updateView();
}

//toggle advaced features 
void CoinControlDialog::toggled(bool)
{
    if (ui->radioListMode->isChecked()){
        if (ui->advanced->checkState()){
        ShowInputAutoSelection();    
        }else{
        HideInputAutoSelection(); 
    }
    if (checked && model)
        updateView();
        }else{ // if not in lsit mode 
            QMessageBox msgBox;
            msgBox.setObjectName("lockMessageBox");
            msgBox.setStyleSheet(GUIUtil::loadStyleSheet());
            msgBox.setText(tr("Please switch to \"List mode\" to use this function."));
            msgBox.exec();
    }
}

// checkbox clicked by user
void CoinControlDialog::viewItemChanged(QTreeWidgetItem* item, int column)
{
    if (column == COLUMN_CHECKBOX && item->text(COLUMN_TXHASH).length() == 64) // transaction hash is 64 characters (this means its a child node, so its not a parent node in tree mode)
    {
        COutPoint outpt(uint256(item->text(COLUMN_TXHASH).toStdString()), item->text(COLUMN_VOUT_INDEX).toUInt());
        CTxIn in;
        int rounds = pwalletMain->GetInputDarkSendRounds(in);
        if (coinControl->useDarksend && rounds < nDarksendRounds)
            QMessageBox::warning(this, windowTitle(),
                                 tr("Non-anonymized input selected. <b>Luxsend will be disabled.</b><br><br>If you still want to use Luxsend, please deselect all non-nonymized inputs first and then check Luxsend checkbox again."),
                                 QMessageBox::Ok, QMessageBox::Ok);
        coinControl->useDarksend = false;


        if (item->checkState(COLUMN_CHECKBOX) == Qt::Unchecked)
            coinControl->UnSelect(outpt);
            else if (item->isDisabled()) // locked (this happens if "check all" through parent node)
                item->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
            else
                coinControl->Select(outpt);

        // selection changed -> update labels
        if (ui->treeWidget->isEnabled()){ // do not update on every click for (un)select all
            CoinControlDialog::updateLabels(model, this);
            CheckDialogLablesUpdated();
        }
    }
// todo: this is a temporary qt5 fix: when clicking a parent node in tree mode, the parent node
//       including all childs are partially selected. But the parent node should be fully selected
//       as well as the childs. Childs should never be partially selected in the first place.
//       Please remove this ugly fix, once the bug is solved upstream.
#if QT_VERSION >= 0x050000
    else if (column == COLUMN_CHECKBOX && item->childCount() > 0) {
        if (item->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked && item->child(0)->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked)
            item->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
    }
#endif
}

// return human readable label for priority number
QString CoinControlDialog::getPriorityLabel(double dPriority, double mempoolEstimatePriority)
{
    double dPriorityMedium = mempoolEstimatePriority;

    if (dPriorityMedium <= 0)
        dPriorityMedium = AllowFreeThreshold(); // not enough data, back to hard-coded

    if (dPriority / 1000000 > dPriorityMedium)
        return tr("highest");
    else if (dPriority / 100000 > dPriorityMedium)
        return tr("higher");
    else if (dPriority / 10000 > dPriorityMedium)
        return tr("high");
    else if (dPriority / 1000 > dPriorityMedium)
        return tr("medium-high");
    else if (dPriority > dPriorityMedium)
        return tr("medium");
    else if (dPriority * 10 > dPriorityMedium)
        return tr("low-medium");
    else if (dPriority * 100 > dPriorityMedium)
        return tr("low");
    else if (dPriority * 1000 > dPriorityMedium)
        return tr("lower");
    else
        return tr("lowest");
}

// shows count of locked unspent outputs
void CoinControlDialog::updateLabelLocked()
{
    vector<COutPoint> vOutpts;
    model->listLockedCoins(vOutpts);
    if (vOutpts.size() > 0) {
        ui->labelLocked->setText(tr("(%1 locked)").arg(vOutpts.size()));
        ui->labelLocked->setVisible(true);
    } else
        ui->labelLocked->setVisible(false);
}

void CoinControlDialog::CheckDialogLablesUpdated()
{
    if (this->parentWidget() == nullptr) {
        CoinControlDialog::updateLabels(model, this);
        return;
    }
    vector<COutPoint> vCoinControl;
    vector<COutput> vOutputs;
    coinControl->ListSelected(vCoinControl);
    model->getOutputs(vCoinControl, vOutputs);

    CAmount nAmount = 0;
    unsigned int nQuantity = 0;
    for (const COutput& out : vOutputs) {
        // unselect already spent, very unlikely scenario, this could happen
        // when selected are spent elsewhere, like rpc or another computer
        uint256 txhash = out.tx->GetHash();
        COutPoint outpt(txhash, out.i);
        if(model->isSpent(outpt)) {
            coinControl->UnSelect(outpt);
            continue;
        }

        // Quantity
        nQuantity++;

        // Amount
        nAmount += out.tx->vout[out.i].nValue;
    }
}

void CoinControlDialog::updateLabels(WalletModel* model, QDialog* dialog)
{
    if (!model)
        return;

    // nPayAmount
    CAmount nPayAmount = 0;
    bool fDust = false;
    CMutableTransaction txDummy;
    Q_FOREACH (const CAmount& amount, CoinControlDialog::payAmounts) {
        nPayAmount += amount;

        if (amount > 0) {
            CTxOut txout(amount, (CScript)vector<unsigned char>(24, 0));
            txDummy.vout.push_back(txout);
            if (txout.IsDust(::minRelayTxFee))
                fDust = true;
        }
    }

    QString sPriorityLabel = tr("none");
    CAmount nAmount = 0;
    CAmount nPayFee = 0;
    CAmount nAfterFee = 0;
    CAmount nChange = 0;
    unsigned int nBytes = 0;
    unsigned int nBytesInputs = 0;
    double dPriority = 0;
    double dPriorityInputs = 0;
    unsigned int nQuantity = 0;
    int nQuantityUncompressed = 0;
    bool fAllowFree = false;
    bool fWitness = false;

    vector<COutPoint> vCoinControl;
    vector<COutput> vOutputs;
    coinControl->ListSelected(vCoinControl);
    model->getOutputs(vCoinControl, vOutputs);

    for (const COutput& out : vOutputs) {
        // unselect already spent, very unlikely scenario, this could happen
        // when selected are spent elsewhere, like rpc or another computer
        uint256 txhash = out.tx->GetHash();
        COutPoint outpt(txhash, out.i);
        if (model->isSpent(outpt)) {
            coinControl->UnSelect(outpt);
            continue;
        }

        // Quantity
        nQuantity++;

        // Amount
        nAmount += out.tx->vout[out.i].nValue;

        // Priority
        dPriorityInputs += (double)out.tx->vout[out.i].nValue * (out.nDepth + 1);

        // Bytes
        CTxDestination address;
        int witnessversion = 0;
        std::vector<unsigned char> witnessprogram;
        if (out.tx->vout[out.i].scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram))
        {
            nBytesInputs += (32 + 4 + 1 + (107 / WITNESS_SCALE_FACTOR) + 4);
            fWitness = true;
        }
        else if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, address)) {
            CPubKey pubkey;
            CKeyID* keyid = boost::get<CKeyID>(&address);
            if (keyid && model->getPubKey(*keyid, pubkey)) {
                nBytesInputs += (pubkey.IsCompressed() ? 148 : 180);
                if (!pubkey.IsCompressed())
                    nQuantityUncompressed++;
            } else
                nBytesInputs += 148; // in all error cases, simply assume 148 here
        } else
            nBytesInputs += 148;
    }

    // calculation
    if (nQuantity > 0) {
        // Bytes
        nBytes = nBytesInputs + ((CoinControlDialog::payAmounts.size() > 0 ? CoinControlDialog::payAmounts.size() + max(1, CoinControlDialog::nSplitBlockDummy) : 2) * 34) + 10; // always assume +1 output for change here
        if (fWitness)
        {
            // there is some fudging in these numbers related to the actual virtual transaction size calculation that will keep this estimate from being exact.
            // usually, the result will be an overestimate within a couple of satoshis so that the confirmation dialog ends up displaying a slightly smaller fee.
            // also, the witness stack size value value is a variable sized integer. usually, the number of stack items will be well under the single byte var int limit.
            nBytes += 2; // account for the serialized marker and flag bytes
            nBytes += nQuantity; // account for the witness byte that holds the number of stack items for each input.
        }

        // in the subtract fee from amount case, we can tell if zero change already and subtract the bytes, so that fee calculation afterwards is accurate
        if (CoinControlDialog::fSubtractFeeFromAmount)
            if (nAmount - nPayAmount == 0)
                nBytes -= 34;

        // Priority
        double mempoolEstimatePriority = mempool.estimatePriority(nTxConfirmTarget);
        dPriority = dPriorityInputs / (nBytes - nBytesInputs + (nQuantityUncompressed * 29)); // 29 = 180 - 151 (uncompressed public keys are over the limit. max 151 bytes of the input are ignored for priority)
        sPriorityLabel = CoinControlDialog::getPriorityLabel(dPriority, mempoolEstimatePriority);

        // Fee
        nPayFee = (CWallet::GetMinimumFee(nBytes, nTxConfirmTarget, mempool)) + 30;
        if (nPayFee > 0 && coinControl->nMinimumTotalFee > nPayFee)
            nPayFee = coinControl->nMinimumTotalFee;

        // IX Fee
        if (coinControl->useInstanTX) nPayFee = max(nPayFee, CENT);
        // Allow free?
        double dPriorityNeeded = mempoolEstimatePriority;
        if (dPriorityNeeded <= 0)
            dPriorityNeeded = AllowFreeThreshold(); // not enough data, back to hard-coded
        fAllowFree = (dPriority >= dPriorityNeeded);

        if (fSendFreeTransactions)
            if (fAllowFree && nBytes <= MAX_FREE_TRANSACTION_CREATE_SIZE)
                nPayFee = 0;

        if (nPayAmount > 0) {
            nChange = nAmount - nPayAmount;
            if (!CoinControlDialog::fSubtractFeeFromAmount) {
                nChange -= nPayFee;
             }

            // DS Fee = overpay
            if (coinControl->useDarksend && nChange > 0) {
                nPayFee += nChange;
                nChange = 0;
            }

            // Never create dust outputs; if we would, just add the dust to the fee.
            if (nChange > 0 && nChange < CENT) {
                CTxOut txout(nChange, (CScript)vector<unsigned char>(24, 0));
                {
                    if (CoinControlDialog::fSubtractFeeFromAmount) // dust-change will be raised until no dust
                        nChange = txout.GetDustThreshold(dustRelayFee);
                    else
                    {
                        nPayFee += nChange;
                        nChange = 0;
                    }
                }
            }

            if (nChange == 0 && !CoinControlDialog::fSubtractFeeFromAmount)
                nBytes -= 34;
        }

        // after fee.
        nAfterFee = std::max<CAmount>(nAmount - nPayFee, 0);

        // if use Split UTXO
        if (CoinControlDialog::nSplitBlockDummy > 1 && CoinControlDialog::coinControl->fSplitBlock) {
            map<QString, vector<COutput> > mapCoins;
            model->listCoins(mapCoins);

            if(mapCoins.size() > 0) {
                map<QString, vector<COutput>>::iterator it = mapCoins.begin();

                SendCoinsRecipient defRecipient;
                defRecipient.address = (*it).first;
                defRecipient.label = "";
                defRecipient.amount = 100000000;
                defRecipient.message = "default";
                defRecipient.inputType = ALL_COINS;
                defRecipient.useInstanTX = CoinControlDialog::coinControl->useInstanTX;

                QList<SendCoinsRecipient> recipients;
                recipients.append(defRecipient);

                CoinControlDialog::coinControl->nSplitBlock = CoinControlDialog::nSplitBlockDummy;

                WalletModelTransaction currentTransaction(recipients);
                model->prepareTransaction(currentTransaction, CoinControlDialog::coinControl, false); // let's not sign this preparation as the wallet may be locked

                CAmount nFee = currentTransaction.getTransactionFee() + 10;
                if (nPayFee < nFee) {
                    nPayFee = nFee;
                    nAfterFee = nAmount - nFee;
                }
            }
        }
    }

    // actually update labels
    int nDisplayUnit = BitcoinUnits::LUX;
    if (model && model->getOptionsModel())
        nDisplayUnit = model->getOptionsModel()->getDisplayUnit();

    QLabel* l1 = dialog->findChild<QLabel*>("labelCoinControlQuantity");
    QLabel* l2 = dialog->findChild<QLabel*>("labelCoinControlAmount");
    QLabel* l3 = dialog->findChild<QLabel*>("labelCoinControlFee");
    QLabel* l4 = dialog->findChild<QLabel*>("labelCoinControlAfterFee");
    QLabel* l5 = dialog->findChild<QLabel*>("labelCoinControlBytes");
    QLabel* l6 = dialog->findChild<QLabel*>("labelCoinControlPriority");
    QLabel* l7 = dialog->findChild<QLabel*>("labelCoinControlLowOutput");
    QLabel* l8 = dialog->findChild<QLabel*>("labelCoinControlChange");

    // enable/disable "dust" and "change"
    dialog->findChild<QLabel*>("labelCoinControlLowOutputText")->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel*>("labelCoinControlLowOutput")->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel*>("labelCoinControlChangeText")->setEnabled(nPayAmount > 0);
    dialog->findChild<QLabel*>("labelCoinControlChange")->setEnabled(nPayAmount > 0);

    // stats
    l1->setText(QString::number(nQuantity));                            // Quantity
    l2->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nAmount));   // Amount
    l3->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nPayFee));   // Fee
    l4->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nAfterFee)); // After Fee
    l5->setText(((nBytes > 0) ? "~" : "") + QString::number(nBytes));   // Bytes
    l6->setText(sPriorityLabel);                                        // Priority
    l7->setText(fDust ? tr("yes") : tr("no"));                          // Dust
    l8->setText(BitcoinUnits::formatWithUnit(nDisplayUnit, nChange));   // Change
    if (nPayFee > 0 && !(payTxFee.GetFeePerK() > 0 && fPayAtLeastCustomFee && nBytes < 1000)) {
        l3->setText("~" + l3->text());
        l4->setText("~" + l4->text());
        if (nChange > 0)
            l8->setText("~" + l8->text());
    }

    // turn labels "red"
    l5->setStyleSheet((nBytes >= MAX_FREE_TRANSACTION_CREATE_SIZE) ? "color:red;" : ""); // Bytes >= 1000
    l6->setStyleSheet((dPriority > 0 && !fAllowFree) ? "color:red;" : "");               // Priority < "medium"
    l7->setStyleSheet((fDust) ? "color:red;" : "");                                      // Dust = "yes"

    // tool tips
    QString toolTip1 = tr("This label turns red, if the transaction size is greater than 1000 bytes.") + "<br /><br />";
    toolTip1 += tr("This means a fee of at least %1 per kB is required.").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CWallet::minTxFee.GetFeePerK())) + "<br /><br />";
    toolTip1 += tr("Can vary +/- 1 byte per input.");

    QString toolTip2 = tr("Transactions with higher priority are more likely to get included into a block.") + "<br /><br />";
    toolTip2 += tr("This label turns red, if the priority is smaller than \"medium\".") + "<br /><br />";
    toolTip2 += tr("This means a fee of at least %1 per kB is required.").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, CWallet::minTxFee.GetFeePerK()));

    QString toolTip3 = tr("This label turns red, if any recipient receives an amount smaller than %1.").arg(BitcoinUnits::formatWithUnit(nDisplayUnit, ::minRelayTxFee.GetFee(546)));

    // how many satoshis the estimated fee can vary per byte we guess wrong
    double dFeeVary;
    if (payTxFee.GetFeePerK() > 0)
        dFeeVary = (double)std::max(CWallet::minTxFee.GetFeePerK(), payTxFee.GetFeePerK()) / 1000;
    else
        dFeeVary = (double)std::max(CWallet::minTxFee.GetFeePerK(), mempool.estimateFee(nTxConfirmTarget).GetFeePerK()) / 1000;
    QString toolTip4 = tr("Can vary +/- %1 duff(s) per input.").arg(dFeeVary);

    l3->setToolTip(toolTip4);
    l4->setToolTip(toolTip4);
    l5->setToolTip(toolTip1);
    l6->setToolTip(toolTip2);
    l7->setToolTip(toolTip3);
    l8->setToolTip(toolTip4);
    dialog->findChild<QLabel*>("labelCoinControlFeeText")->setToolTip(l3->toolTip());
    dialog->findChild<QLabel*>("labelCoinControlAfterFeeText")->setToolTip(l4->toolTip());
    dialog->findChild<QLabel*>("labelCoinControlBytesText")->setToolTip(l5->toolTip());
    dialog->findChild<QLabel*>("labelCoinControlPriorityText")->setToolTip(l6->toolTip());
    dialog->findChild<QLabel*>("labelCoinControlLowOutputText")->setToolTip(l7->toolTip());
    dialog->findChild<QLabel*>("labelCoinControlChangeText")->setToolTip(l8->toolTip());

    // Insufficient funds
    QLabel* label = dialog->findChild<QLabel*>("labelCoinControlInsuffFunds");
    if (label)
        label->setVisible(nChange < 0);
}

void CoinControlDialog::updateView()
{
    if (!model || !model->getOptionsModel() || !model->getAddressTableModel())
        return;

    bool treeMode = ui->radioTreeMode->isChecked();

    ui->treeWidget->clear();
    ui->treeWidget->setEnabled(false); // performance, otherwise updateLabels would be called for every checked checkbox
    ui->treeWidget->setAlternatingRowColors(!treeMode);
    QFlags<Qt::ItemFlag> flgCheckbox = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    QFlags<Qt::ItemFlag> flgTristate = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;

    int nDisplayUnit = model->getOptionsModel()->getDisplayUnit();
    double mempoolEstimatePriority = mempool.estimatePriority(nTxConfirmTarget);

    map<QString, vector<COutput> > mapCoins;
    model->listCoins(mapCoins);

    for (PAIRTYPE(QString, vector<COutput>) coins : mapCoins) {
        CCoinControlWidgetItem *itemWalletAddress = new CCoinControlWidgetItem();
        itemWalletAddress->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);
        QString sWalletAddress = coins.first;
        QString sWalletLabel = model->getAddressTableModel()->labelForAddress(sWalletAddress);
        if (sWalletLabel.isEmpty())
            sWalletLabel = tr("(no label)");

        if (treeMode) {
            // wallet address
            ui->treeWidget->addTopLevelItem(itemWalletAddress);

            itemWalletAddress->setFlags(flgTristate);
            itemWalletAddress->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

            // label
            itemWalletAddress->setText(COLUMN_LABEL, sWalletLabel);
            itemWalletAddress->setToolTip(COLUMN_LABEL, sWalletLabel);

            // address
            itemWalletAddress->setText(COLUMN_ADDRESS, sWalletAddress);
            itemWalletAddress->setToolTip(COLUMN_ADDRESS, sWalletAddress);
        }

        CAmount nSum = 0;
        double dPrioritySum = 0;
        int nChildren = 0;
        int nInputSum = 0;
        for (const COutput& out : coins.second) {
            int nInputSize = 0;
            nSum += out.tx->vout[out.i].nValue;
            nChildren++;

            CCoinControlWidgetItem *itemOutput;
            if (treeMode)
                itemOutput = new CCoinControlWidgetItem(itemWalletAddress);
            else
                itemOutput = new CCoinControlWidgetItem(ui->treeWidget);
            itemOutput->setFlags(flgCheckbox);
            itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

            // address
            CTxDestination outputAddress;
            QString sAddress = "";
            if (ExtractDestination(out.tx->vout[out.i].scriptPubKey, outputAddress)) {
                sAddress = QString::fromStdString(EncodeDestination(outputAddress));

                // if listMode or change => show LUX address. In tree mode, address is not shown again for direct wallet address outputs
                if (!treeMode || (!(sAddress == sWalletAddress)))
                    itemOutput->setText(COLUMN_ADDRESS, sAddress);

                itemOutput->setToolTip(COLUMN_ADDRESS, sAddress);

                CPubKey pubkey;
                CKeyID* keyid = boost::get<CKeyID>(&outputAddress);
                if (keyid && model->getPubKey(*keyid, pubkey) && !pubkey.IsCompressed())
                    nInputSize = 29; // 29 = 180 - 151 (public key is 180 bytes, priority free area is 151 bytes)
            }

            // label
            if (!(sAddress == sWalletAddress)) // change
            {
                // tooltip from where the change comes from
                itemOutput->setToolTip(COLUMN_LABEL, tr("change from %1 (%2)").arg(sWalletLabel).arg(sWalletAddress));
                itemOutput->setText(COLUMN_LABEL, tr("(change)"));
            } else if (!treeMode) {
                QString sLabel = model->getAddressTableModel()->labelForAddress(sAddress);
                if (sLabel.isEmpty())
                    sLabel = tr("(no label)");
                itemOutput->setText(COLUMN_LABEL, sLabel);
            }

            // amount
            itemOutput->setText(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, out.tx->vout[out.i].nValue));
            itemOutput->setToolTip(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, out.tx->vout[out.i].nValue));
            itemOutput->setData(COLUMN_AMOUNT, Qt::UserRole, QVariant((qlonglong)out.tx->vout[out.i].nValue)); // padding so that sorting works correctly

            // date
            itemOutput->setText(COLUMN_DATE, GUIUtil::dateTimeStr(out.tx->GetTxTime()));
            itemOutput->setToolTip(COLUMN_DATE, GUIUtil::dateTimeStr(out.tx->GetTxTime()));
            itemOutput->setData(COLUMN_DATE, Qt::UserRole, QVariant((qlonglong)out.tx->GetTxTime()));

            // ds+ rounds
            CTxIn vin = CTxIn(out.tx->GetHash(), out.i);
            int rounds = pwalletMain->GetInputDarkSendRounds(vin);

            if (rounds >= 0 || fDebug)
                itemOutput->setText(COLUMN_DARKSEND_ROUNDS, QString::number(rounds));
            else
                itemOutput->setText(COLUMN_DARKSEND_ROUNDS, tr("n/a"));
            itemOutput->setData(COLUMN_DARKSEND_ROUNDS, Qt::UserRole, QVariant((qlonglong)rounds));

            // confirmations
            itemOutput->setText(COLUMN_CONFIRMATIONS, QString::number(out.nDepth));
            itemOutput->setData(COLUMN_CONFIRMATIONS, Qt::UserRole, QVariant((qlonglong)out.nDepth));

            // priority
            double dPriority = ((double)out.tx->vout[out.i].nValue / (nInputSize + 78)) * (out.nDepth + 1); // 78 = 2 * 34 + 10
            itemOutput->setText(COLUMN_PRIORITY, CoinControlDialog::getPriorityLabel(dPriority, mempoolEstimatePriority));
            itemOutput->setText(COLUMN_PRIORITY_INT64, strPad(QString::number((int64_t)dPriority), 20, " "));
            dPrioritySum += (double)out.tx->vout[out.i].nValue * (out.nDepth + 1);
            nInputSum += nInputSize;

            // transaction hash
            uint256 txhash = out.tx->GetHash();
            itemOutput->setText(COLUMN_TXHASH, QString::fromStdString(txhash.GetHex()));

            // vout index
            itemOutput->setText(COLUMN_VOUT_INDEX, QString::number(out.i));

            COutPoint outpt(txhash, out.i);
            if (toogleLockList.indexOf(QString::fromStdString(txhash.GetHex())) >= 0) {
                model->lockCoin(outpt);
                toogleLockList.removeAll(QString::fromStdString(txhash.GetHex()));
            }

            // disable locked coins
            if (model->isLockedCoin(txhash, out.i)) {
                coinControl->UnSelect(outpt); // just to be sure
                itemOutput->setDisabled(true);
                itemOutput->setIcon(COLUMN_CHECKBOX, QIcon(":/icons/lock_closed"));
            }

            // set checkbox
            if (coinControl->IsSelected(txhash, out.i))
                itemOutput->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
        }

        // amount
        if (treeMode) {
            dPrioritySum = dPrioritySum / (nInputSum + 78);
            itemWalletAddress->setText(COLUMN_CHECKBOX, "(" + QString::number(nChildren) + ")");
            itemWalletAddress->setText(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, nSum));
            itemWalletAddress->setToolTip(COLUMN_AMOUNT, BitcoinUnits::format(nDisplayUnit, nSum));
            itemWalletAddress->setData(COLUMN_AMOUNT, Qt::UserRole, QVariant((qlonglong)nSum));
            itemWalletAddress->setText(COLUMN_PRIORITY, CoinControlDialog::getPriorityLabel(dPrioritySum, mempoolEstimatePriority));
            itemWalletAddress->setText(COLUMN_PRIORITY_INT64, strPad(QString::number((int64_t)dPrioritySum), 20, " "));
        }
    }

    // expand all partially selected
    if (treeMode) {
        for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++)
            if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) == Qt::PartiallyChecked)
                ui->treeWidget->topLevelItem(i)->setExpanded(true);
    }

    // sort view
    sortView(sortColumn, sortOrder);
    ui->treeWidget->setEnabled(true);
}
