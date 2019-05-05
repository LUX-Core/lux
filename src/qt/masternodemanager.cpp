#include "masternodemanager.h"
#include "ui_masternodemanager.h"
#include "addeditluxnode.h"
#include "luxnodeconfigdialog.h"

#include "sync.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "activemasternode.h"
#include "masternodeconfig.h"
#include "masternode.h"
#include "walletdb.h"
#include "wallet.h"
#include "init.h"
#include "rpcserver.h"
#include <boost/lexical_cast.hpp>
#include <fstream>

using namespace std;

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QTimer>
#include <QDebug>
#include <QScrollArea>
#include <QScroller>
#include <QDateTime>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QThread>
#include <QScrollBar>
#include <QMessageBox>

MasternodeManager::MasternodeManager(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MasternodeManager),
    clientModel(0),
    walletModel(0)
{
    ui->setupUi(this);

    ui->editButton->setEnabled(false);
    ui->getConfigButton->setEnabled(false);
    ui->startButton->setEnabled(false);
    ui->stopButton->setEnabled(false);
    //ui->copyAddressButton->setEnabled(false);

    int columnAddressWidth = 160;
    int columnrankItemWidth = 40;
    int columnActiveWidth = 60;
    int columnActiveSecondsWidth = 130;
    int columnLastSeenWidth = 130;
    int columnProtocolWidth = 60;

    // setup widths for the main table
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->setColumnWidth(Address, columnAddressWidth);
    ui->tableWidget->setColumnWidth(Rank, columnrankItemWidth);
    ui->tableWidget->setColumnWidth(Active, columnActiveWidth);
    ui->tableWidget->setColumnWidth(ActiveSecs, columnActiveSecondsWidth);
    ui->tableWidget->setColumnWidth(LastSeen, columnLastSeenWidth);
    ui->tableWidget->setColumnWidth(Protocol, columnProtocolWidth);

    ui->tableWidget->sortByColumn(Rank, Qt::AscendingOrder);
    ui->tableWidget->setSortingEnabled(true);

    // setup widths for parallel table
    ui->tableWidget_2->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui->tableWidget_2->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget_2->setColumnWidth(Address, columnAddressWidth);
    ui->tableWidget_2->setColumnWidth(Rank, columnrankItemWidth);
    ui->tableWidget_2->setColumnWidth(Active, columnActiveWidth);
    ui->tableWidget_2->setColumnWidth(ActiveSecs, columnActiveSecondsWidth);
    ui->tableWidget_2->setColumnWidth(LastSeen, columnLastSeenWidth);
    ui->tableWidget_2->setColumnWidth(Protocol, columnProtocolWidth);

    ui->tableWidget_2->sortByColumn(Address, Qt::AscendingOrder);
    ui->tableWidget_2->setSortingEnabled(true);

    subscribeToCoreSignals();

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
    timer->start(20000);
    fFilterUpdated = true;
    nTimeFilterUpdated = GetTime();
    updateNodeList();
    connect(ui->filterLineEdit, SIGNAL(textChanged(const QString &)), this, SLOT(wait()));

}

class SortedWidgetItem : public QTableWidgetItem
{
public:
    bool operator <( const QTableWidgetItem& other ) const
    {
        return (data(Qt::UserRole) < other.data(Qt::UserRole));
    }
};

MasternodeManager::~MasternodeManager()
{
    delete timer;
    delete ui;
}

static void NotifyLuxNodeUpdated(MasternodeManager *page, CLuxNodeConfig nodeConfig)
{
    // alias, address, privkey, collateral address
    QString alias = QString::fromStdString(nodeConfig.sAlias);
    QString addr = QString::fromStdString(nodeConfig.sAddress);
    QString privkey = QString::fromStdString(nodeConfig.sMasternodePrivKey);
    QString collateral = QString::fromStdString(nodeConfig.sCollateralAddress);
    
    QMetaObject::invokeMethod(page, "updateLuxNode", Qt::QueuedConnection,
                              Q_ARG(QString, alias),
                              Q_ARG(QString, addr),
                              Q_ARG(QString, privkey),
                              Q_ARG(QString, collateral)
                              );
}

void MasternodeManager::subscribeToCoreSignals()
{
    // Connect signals to core
    uiInterface.NotifyLuxNodeChanged.connect(boost::bind(&NotifyLuxNodeUpdated, this, _1));
}

void MasternodeManager::unsubscribeFromCoreSignals()
{
    // Disconnect signals from core
    uiInterface.NotifyLuxNodeChanged.disconnect(boost::bind(&NotifyLuxNodeUpdated, this, _1));
}

void MasternodeManager::on_tableWidget_2_itemSelectionChanged()
{
    if(ui->tableWidget_2->selectedItems().count() > 0)
    {
        ui->editButton->setEnabled(true);
        ui->getConfigButton->setEnabled(true);
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(true);
        //ui->copyAddressButton->setEnabled(true);
    }
}

void MasternodeManager::updateLuxNode(QString alias, QString addr, QString privkey, QString collateral)
{
    LOCK(cs_lux);
   
    ui->tableWidget_2->insertRow(0);

    QTableWidgetItem *aliasItem = new QTableWidgetItem(alias);
    QTableWidgetItem *addrItem = new QTableWidgetItem(addr);
    QTableWidgetItem *statusItem = new QTableWidgetItem("");
    QTableWidgetItem *collateralItem = new QTableWidgetItem(collateral);

    ui->tableWidget_2->setItem(0, 0, aliasItem);
    ui->tableWidget_2->setItem(0, 1, addrItem);
    ui->tableWidget_2->setItem(0, 2, statusItem);
    ui->tableWidget_2->setItem(0, 3, collateralItem);
}

static QString seconds_to_DHMS(quint32 duration)
{
    QString res;
    int seconds = (int) (duration % 60);
    duration /= 60;
    int minutes = (int) (duration % 60);
    duration /= 60;
    int hours = (int) (duration % 24);
    int days = (int) (duration / 24);
    if((hours == 0)&&(days == 0))
        return res.sprintf("%02dm:%02ds", minutes, seconds);
    if (days == 0)
        return res.sprintf("%02dh:%02dm:%02ds", hours, minutes, seconds);
    return res.sprintf("%dd %02dh:%02dm:%02ds", days, hours, minutes, seconds);
}

void MasternodeManager::wait()
{
    QTimer::singleShot(3000, this, SLOT(updateNodeList()));
}

void MasternodeManager::updateNodeList()
{
    if (chainActive.Tip() == NULL) return; // don't segfault if the gui boots before the wallet's ready

    QString strToFilter;
    ui->countLabel->setText("Updating...");
    ui->tableWidget->setSortingEnabled(false);
    ui->tableWidget->setUpdatesEnabled(false); // disable sorting and gui updates while doing this so as to not incur thousands of redraws
    ui->tableWidget->clearContents();
    ui->tableWidget->setRowCount(0);

    // lock masternodes every time, and make a quick copy before doing expensive UI stuff during the lock period.
    vector<CMasterNode*> vecMasternodesCopy;
    {
        LOCK(cs_masternodes);
        vecMasternodesCopy = vecMasternodes;
    }

    for (CMasterNode* mn : vecMasternodesCopy)
    {
        if (ShutdownRequested()) return;

    // populate list
    // Address, Rank, Active, Active Seconds, Last Seen, Pub Key
    QTableWidgetItem *activeItem = new QTableWidgetItem(mn->IsEnabled() ? tr("yes") : tr("no"));
    QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(mn->addr.ToString()));
    SortedWidgetItem *protocolItem = new SortedWidgetItem();
    protocolItem->setData(Qt::UserRole, mn->protocolVersion);
    protocolItem->setData(Qt::DisplayRole, QString::number(mn->protocolVersion));
    SortedWidgetItem *rankItem = new SortedWidgetItem();
    int rank = GetMasternodeRank(mn->vin, chainActive.Height());
    rankItem->setData(Qt::UserRole, rank > 0 ? rank : 99999);
    rankItem->setData(Qt::DisplayRole, rank > 0 ? QString::number(rank) : QString(""));
    SortedWidgetItem *activeSecondsItem = new SortedWidgetItem();
    activeSecondsItem->setData(Qt::UserRole, (qint64)(mn->lastTimeSeen - mn->now));
    activeSecondsItem->setData(Qt::DisplayRole, seconds_to_DHMS((qint64)(mn->lastTimeSeen - mn->now)));
    QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M:%S", mn->lastTimeSeen)));


    CScript pubkey;
    pubkey = GetScriptForDestination(mn->pubkey.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);
    QTableWidgetItem *pubkeyItem = new QTableWidgetItem(QString::fromStdString(EncodeDestination(address1)));

        if (strCurrentFilter != "") {
            strToFilter = addressItem->text() + " " +
                          rankItem->text() + " " +
                          protocolItem->text() + " " +
                          activeItem->text() + " " +
                          activeSecondsItem->text() + " " +
                          lastSeenItem->text() + " " +
                          pubkeyItem->text();
            if (!strToFilter.contains(strCurrentFilter)) continue;
        }
		
	    ui->tableWidget->insertRow(0);
        ui->tableWidget->setItem(0, Address, addressItem);
        ui->tableWidget->setItem(0, Rank, rankItem);
	    ui->tableWidget->setItem(0, Protocol, protocolItem);
        ui->tableWidget->setItem(0, Active, activeItem);
        ui->tableWidget->setItem(0, ActiveSecs, activeSecondsItem);
        ui->tableWidget->setItem(0, LastSeen, lastSeenItem);
        ui->tableWidget->setItem(0, Pubkey, pubkeyItem);
		
    }

    ui->countLabel->setText(QString::number(ui->tableWidget->rowCount()));
    ui->tableWidget->setSortingEnabled(true);
    ui->tableWidget->setUpdatesEnabled(true);
    
    if(pwalletMain)
    {
        LOCK(cs_lux);
        for (PAIRTYPE(std::string, CLuxNodeConfig) lux : pwalletMain->mapMyLuxNodes)
        {
            updateLuxNode(QString::fromStdString(lux.second.sAlias), QString::fromStdString(lux.second.sAddress), QString::fromStdString(lux.second.sMasternodePrivKey), QString::fromStdString(lux.second.sCollateralAddress));
        }
    }
}

void MasternodeManager::on_filterLineEdit_textChanged(const QString& strFilterIn)
{
    strCurrentFilter = strFilterIn;
    nTimeFilterUpdated = GetTime();
    fFilterUpdated = true;
    updateNodeList();
    // ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", MASTERNODELIST_FILTER_COOLDOWN_SECONDS)));
}

void MasternodeManager::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if (model) {
        // try to update list when masternode count changes
        connect(clientModel, SIGNAL(strMasternodesChanged(QString)), this, SLOT(updateNodeList()));
    }
}

void MasternodeManager::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        setPMNsVisible(model->getOptionsModel()->getParallelMasternodes());
        connect(model->getOptionsModel(), SIGNAL(parallelMasternodesChanged(bool)), this, SLOT(setPMNsVisible(bool)));
    }
}

void MasternodeManager::setPMNsVisible(bool visible)
{
    ui->tableWidgetPMN->setVisible(visible);
    ui->labelPMN->setVisible(visible);
}

void MasternodeManager::on_createButton_clicked()
{
    AddEditLuxNode* aenode = new AddEditLuxNode();
    aenode->exec();
}
#if 0
void MasternodeManager::on_copyAddressButton_clicked()
{
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string sCollateralAddress = ui->tableWidget_2->item(r, 3)->text().toStdString();

    QApplication::clipboard()->setText(QString::fromStdString(sCollateralAddress));
}
#endif
void MasternodeManager::on_editButton_clicked()
{
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string sAddress = ui->tableWidget_2->item(r, 1)->text().toStdString();

    // get existing config entry
}

void MasternodeManager::on_getConfigButton_clicked()
{
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string sAddress = ui->tableWidget_2->item(r, 1)->text().toStdString();
    CLuxNodeConfig c = pwalletMain->mapMyLuxNodes[sAddress];
    std::string sPrivKey = c.sMasternodePrivKey;
    LuxNodeConfigDialog* d = new LuxNodeConfigDialog(this, QString::fromStdString(sAddress), QString::fromStdString(sPrivKey));
    d->exec();
}

void MasternodeManager::on_removeButton_clicked()
{
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QMessageBox::StandardButton confirm;
    confirm = QMessageBox::question(this, "Delete LUX Node?", "Are you sure you want to delete this LUX node configuration?", QMessageBox::Yes|QMessageBox::No);

    if(confirm == QMessageBox::Yes)
    {
        QModelIndex index = selected.at(0);
        int r = index.row();
        std::string sAddress = ui->tableWidget_2->item(r, 1)->text().toStdString();
        CLuxNodeConfig c = pwalletMain->mapMyLuxNodes[sAddress];
        CWalletDB walletdb(pwalletMain->strWalletFile);
        pwalletMain->mapMyLuxNodes.erase(sAddress);
        walletdb.EraseLuxNodeConfig(c.sAddress);
        ui->tableWidget_2->clearContents();
        ui->tableWidget_2->setRowCount(0);
        for (PAIRTYPE(std::string, CLuxNodeConfig) lux : pwalletMain->mapMyLuxNodes)
        {
            updateLuxNode(QString::fromStdString(lux.second.sAlias), QString::fromStdString(lux.second.sAddress), QString::fromStdString(lux.second.sMasternodePrivKey), QString::fromStdString(lux.second.sCollateralAddress));
        }
    }
}

void MasternodeManager::on_startButton_clicked()
{
    // start the node
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string sAddress = ui->tableWidget_2->item(r, 1)->text().toStdString();
    CLuxNodeConfig c = pwalletMain->mapMyLuxNodes[sAddress];

    std::string errorMessage;
    bool result = activeMasternode.RegisterByPubKey(c.sAddress, c.sMasternodePrivKey, c.sCollateralAddress, errorMessage);

    QMessageBox msg;
    if(result)
        msg.setText("LUX Node at " + QString::fromStdString(c.sAddress) + " started.");
    else
        msg.setText("Error: " + QString::fromStdString(errorMessage));

    msg.exec();
}

void MasternodeManager::on_stopButton_clicked()
{
    // start the node
    QItemSelectionModel* selectionModel = ui->tableWidget_2->selectionModel();
    QModelIndexList selected = selectionModel->selectedRows();
    if(selected.count() == 0)
        return;

    QModelIndex index = selected.at(0);
    int r = index.row();
    std::string sAddress = ui->tableWidget_2->item(r, 1)->text().toStdString();
    CLuxNodeConfig c = pwalletMain->mapMyLuxNodes[sAddress];

    std::string errorMessage;
    bool result = activeMasternode.StopMasterNode(c.sAddress, c.sMasternodePrivKey, errorMessage);
    QMessageBox msg;
    if(result)
    {
        msg.setText("LUX Node at " + QString::fromStdString(c.sAddress) + " stopped.");
    }
    else
    {
        msg.setText("Error: " + QString::fromStdString(errorMessage));
    }
    msg.exec();
}

void MasternodeManager::on_startAllButton_clicked()
{
    std::string results;
    for (PAIRTYPE(std::string, CLuxNodeConfig) lux : pwalletMain->mapMyLuxNodes)
    {
        CLuxNodeConfig c = lux.second;
	std::string errorMessage;
        bool result = activeMasternode.RegisterByPubKey(c.sAddress, c.sMasternodePrivKey, c.sCollateralAddress, errorMessage);
	if(result)
	{
   	    results += c.sAddress + ": STARTED\n";
	}	
	else
	{
	    results += c.sAddress + ": ERROR: " + errorMessage + "\n";
	}
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(results));
    msg.exec();
}

void MasternodeManager::on_stopAllButton_clicked()
{
    std::string results;
    for (PAIRTYPE(std::string, CLuxNodeConfig) lux : pwalletMain->mapMyLuxNodes)
    {
        CLuxNodeConfig c = lux.second;
	std::string errorMessage;
        bool result = activeMasternode.StopMasterNode(c.sAddress, c.sMasternodePrivKey, errorMessage);
	if(result)
	{
   	    results += c.sAddress + ": STOPPED\n";
	}	
	else
	{
	    results += c.sAddress + ": ERROR: " + errorMessage + "\n";
	}
    }

    QMessageBox msg;
    msg.setText(QString::fromStdString(results));
    msg.exec();
}
