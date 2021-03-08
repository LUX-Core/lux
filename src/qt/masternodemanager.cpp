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
#include "guiutil.h"

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
#include <QSettings>
#include <QHeaderView>

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

    subscribeToCoreSignals();

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(updateNodeList()));
    timer->start(20000);
    fFilterUpdated = true;
    nTimeFilterUpdated = GetTime();
    updateNodeList();
    connect(ui->filterLineEdit, SIGNAL(textChanged(const QString &)), this, SLOT(wait()));
	
    int columnAddressWidth = 200;
    int columnrankItemWidth = 60;
    int columnProtocolWidth = 60;
    int columnActiveWidth = 80;
    int columnActiveSecondsWidth = 130;
    int columnLastSeenWidth = 130;
		
    ui->tableWidget->setColumnWidth(0, columnAddressWidth); // Address
    ui->tableWidget->setColumnWidth(1, columnrankItemWidth); // Rank
    ui->tableWidget->setColumnWidth(2, columnProtocolWidth); // protocol
    ui->tableWidget->setColumnWidth(3, columnActiveWidth); // status
    ui->tableWidget->setColumnWidth(4, columnActiveSecondsWidth); // time active
    ui->tableWidget->setColumnWidth(5, columnLastSeenWidth); // last seen
	ui->tableWidget->verticalHeader()->setVisible(false); // hide numberic row
	
	QSettings settings;

	if (settings.value("theme").toString() == "dark grey") {
		QString styleSheet = ".QTabWidget { border-left: 0px !important; border-bottom: 0px !important; "
								"border-right: 0px !important; marging-left: -1px !important; "
								"marging-bottom: -1px !important; marging-right: -1px !important; } "
								".QTableWidget {background-color: #262626; alternate-background-color:#424242; "
								"gridline-color: #fff5f5; border: 1px solid #fff5f5; color: #fff; min-height:2em; }";
		ui->tableWidget->setStyleSheet(styleSheet);
		ui->tableWidgetPMN->setStyleSheet(styleSheet);
		ui->tableWidget_2->setStyleSheet(styleSheet);
	} else if (settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QTabWidget { border-left: 0px !important; border-bottom: 0px !important; "
								"border-right: 0px !important; marging-left: -1px !important; "
								"marging-bottom: -1px !important; marging-right: -1px !important; } "
								".QTableWidget {background-color: #031d54; alternate-background-color:#0D2A64; "
								"gridline-color: #fff5f5; border: 1px solid #fff5f5; color: #fff; min-height:2em; }";										
		ui->tableWidget->setStyleSheet(styleSheet);
		ui->tableWidgetPMN->setStyleSheet(styleSheet);
		ui->tableWidget_2->setStyleSheet(styleSheet);
	} else { 
		//code here
	}
	
}

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
    static int64_t nTimeListUpdated = GetTime();

    // to prevent high cpu usage update only once in MASTERNODELIST_UPDATE_SECONDS seconds
    // or MASTERNODELIST_FILTER_COOLDOWN_SECONDS seconds after filter was last changed
    int64_t nSecondsToWait = fFilterUpdated ? nTimeFilterUpdated - GetTime() + MASTERNODELIST_FILTER_COOLDOWN_SECONDS : nTimeListUpdated - GetTime() + MASTERNODELIST_UPDATE_SECONDS;

    if (fFilterUpdated) ui->countLabel->setText(tr("Please wait... ") + nSecondsToWait);
    if (nSecondsToWait > 0) return;
    nTimeListUpdated = GetTime();
    fFilterUpdated = false;

    TRY_LOCK(cs_masternodes, lockMasternodes);
    if(!lockMasternodes)
        return;

    QString strToFilter;
    ui->countLabel->setText("Updating...");
    ui->tableWidget->setSortingEnabled(false);
    ui->tableWidget->clearContents();
    ui->tableWidget->setRowCount(0);

    for (CMasterNode &mn : vecMasternodes)
    {
        if (ShutdownRequested()) return;

    // populate list
    // Address, Rank, protocol, status, time active, last seen, Pub Key
	
    QTableWidgetItem *addressItem = new QTableWidgetItem(QString::fromStdString(mn.addr.ToString())); // Address
    QTableWidgetItem *rankItem = new QTableWidgetItem(QString::number(GetMasternodeRank(mn.vin, chainActive.Height()))); // Rank
    QTableWidgetItem *protocolItem = new QTableWidgetItem(QString::number(mn.protocolVersion)); // Protocol
    QTableWidgetItem *activeItem = new QTableWidgetItem(mn.IsEnabled() ? tr("Enabled") : tr("Disabled")); // Status
    QTableWidgetItem *activeSecondsItem = new QTableWidgetItem(seconds_to_DHMS((qint64)(mn.lastTimeSeen - mn.now))); // Time Active
    QTableWidgetItem *lastSeenItem = new QTableWidgetItem(QString::fromStdString(DateTimeStrFormat("%Y-%m-%d %H:%M:%S", mn.lastTimeSeen))); // Last Seen
	
	
    CScript pubkey;
        pubkey = GetScriptForDestination(mn.pubkey.GetID());
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
        ui->tableWidget->setItem(0, 0, addressItem);
        ui->tableWidget->setItem(0, 1, rankItem);
		ui->tableWidget->setItem(0, 2, protocolItem);
        ui->tableWidget->setItem(0, 3, activeItem);
        ui->tableWidget->setItem(0, 4, activeSecondsItem);
        ui->tableWidget->setItem(0, 5, lastSeenItem);
        ui->tableWidget->setItem(0, 6, pubkeyItem);
		
    }

    ui->countLabel->setText(QString::number(ui->tableWidget->rowCount()));
    ui->tableWidget->setSortingEnabled(true);
    
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
    ui->countLabel->setText(QString::fromStdString(strprintf("Please wait... %d", MASTERNODELIST_FILTER_COOLDOWN_SECONDS)));
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

	QMessageBox* msg = new QMessageBox();
	
	QSettings settings;

	if (settings.value("theme").toString() == "dark grey") {
		QString styleSheet = ".QMessageBox { background-color: #262626; color:#fff; border: 1px solid #fff5f5;"
								"min-width: 150px; min-width: 250px; }"
								"QMessageBox QPushButton { background-color: #262626; color:#fff; "
								"border: 1px solid #fff5f5; padding-left:10px; "
								"padding-right:10px; min-height:25px; min-width:75px; }"
								"QMessageBox QPushButton::hover { background-color:#fff5f5; color:#262626; }"
								"QMessageBox QLabel {color:#fff;}";

		msg->setStyleSheet(styleSheet);
		if(result)
		{
			msg->setText("<span style=\"color:#fff;\"> LUX Node at " + QString::fromStdString(c.sAddress) + " started. </span>");
		}
		else
		{
			msg->setText("<span style=\"color:#fff;\"> Error: " + QString::fromStdString(errorMessage) + "</span>");
		}
	} else if (settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QMessageBox { background-color: #031d54; color:#fff; border: 1px solid #fff5f5; "
								"min-width: 150px; min-width: 250px; }"
								"QMessageBox QPushButton { background-color: #031d54; color:#fff; "
								"border: 1px solid #fff5f5; padding-left:10px; "
								"padding-right:10px; min-height:25px; min-width:75px; }"
								"QMessageBox QPushButton::hover { background-color:#fff5f5; color:#031d54; }";
								"QMessageBox QLabel {color:#fff;}";
							
		msg->setStyleSheet(styleSheet);
		if(result)
		{
			msg->setText("<span style=\"color:#fff;\"> LUX Node at " + QString::fromStdString(c.sAddress) + " started. </span>");
		}
		else
		{
			msg->setText("<span style=\"color:#fff;\"> Error: " + QString::fromStdString(errorMessage) + "</span>");
		}
	} else { 
		if(result)
		{
			msg->setText("LUX Node at " + QString::fromStdString(c.sAddress) + " started.");
		}
		else
		{
			msg->setText("Error: " + QString::fromStdString(errorMessage));
		}
	}
	
    msg->exec();
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
    QMessageBox* msg = new QMessageBox();
	
	QSettings settings;

	if (settings.value("theme").toString() == "dark grey") {
		QString styleSheet = ".QMessageBox { background-color: #262626; color:#fff; border: 1px solid #fff5f5;"
								"min-width: 150px; min-width: 250px; }"
								"QMessageBox QPushButton { background-color: #262626; color:#fff; "
								"border: 1px solid #fff5f5; padding-left:10px; "
								"padding-right:10px; min-height:25px; min-width:75px; }"
								"QMessageBox QPushButton::hover { background-color:#fff5f5; color:#262626; }"
								"QMessageBox QLabel {color:#fff;}";

		msg->setStyleSheet(styleSheet);
		if(result)
		{
			msg->setText("<span style=\"color:#fff;\"> LUX Node at " + QString::fromStdString(c.sAddress) + " stopped. </span>");
		}
		else
		{
			msg->setText("<span style=\"color:#fff;\"> Error: " + QString::fromStdString(errorMessage) + "</span>");
		}
	} else if (settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QMessageBox { background-color: #031d54; color:#fff; border: 1px solid #fff5f5; "
								"min-width: 150px; min-width: 250px; }"
								"QMessageBox QPushButton { background-color: #031d54; color:#fff; "
								"border: 1px solid #fff5f5; padding-left:10px; "
								"padding-right:10px; min-height:25px; min-width:75px; }"
								"QMessageBox QPushButton::hover { background-color:#fff5f5; color:#031d54; }";
								"QMessageBox QLabel {color:#fff;}";
							
		msg->setStyleSheet(styleSheet);
		if(result)
		{
			msg->setText("<span style=\"color:#fff;\"> LUX Node at " + QString::fromStdString(c.sAddress) + " stopped. </span>");
		}
		else
		{
			msg->setText("<span style=\"color:#fff;\"> Error: " + QString::fromStdString(errorMessage) + "</span>");
		}
	} else { 
		if(result)
		{
			msg->setText("LUX Node at " + QString::fromStdString(c.sAddress) + " stopped.");
		}
		else
		{
			msg->setText("Error: " + QString::fromStdString(errorMessage));
		}
	}

    msg->exec();
}

void MasternodeManager::on_startAllButton_clicked()
{
    std::string results;
	
	results = "There are not masternodes setup";
	
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

    QMessageBox* msg = new QMessageBox();
	
	QSettings settings;

	if (settings.value("theme").toString() == "dark grey") {
		QString styleSheet = ".QMessageBox { background-color: #262626; color:#fff; border: 1px solid #fff5f5;"
								"min-width: 150px; min-width: 250px; }"
								"QMessageBox QPushButton { background-color: #262626; color:#fff; "
								"border: 1px solid #fff5f5; padding-left:10px; "
								"padding-right:10px; min-height:25px; min-width:75px; }"
								"QMessageBox QPushButton::hover { background-color:#fff5f5; color:#262626; }"
								"QMessageBox QLabel {color:#fff;}";

		msg->setStyleSheet(styleSheet);
		msg->setText("<span style=\"color:#fff;\">" +QString::fromStdString(results) + "</span>");
	} else if (settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QMessageBox { background-color: #031d54; color:#fff; border: 1px solid #fff5f5; "
								"min-width: 150px; min-width: 250px; }"
								"QMessageBox QPushButton { background-color: #031d54; color:#fff; "
								"border: 1px solid #fff5f5; padding-left:10px; "
								"padding-right:10px; min-height:25px; min-width:75px; }"
								"QMessageBox QPushButton::hover { background-color:#fff5f5; color:#031d54; }";
								"QMessageBox QLabel {color:#fff;}";
							
		msg->setStyleSheet(styleSheet);
		msg->setText("<span style=\"color:#fff;\">" +QString::fromStdString(results) + "</span>");
	} else { 
		msg->setText(QString::fromStdString(results));
	}
    msg->exec();
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

    QMessageBox* msg = new QMessageBox();
	
	QSettings settings;

	if (settings.value("theme").toString() == "dark grey") {
		QString styleSheet = ".QMessageBox { background-color: #262626; color:#fff; border: 1px solid #fff5f5;"
								"min-width: 150px; min-width: 250px; }"
								"QMessageBox QPushButton { background-color: #262626; color:#fff; "
								"border: 1px solid #fff5f5; padding-left:10px; "
								"padding-right:10px; min-height:25px; min-width:75px; }"
								"QMessageBox QPushButton::hover { background-color:#fff5f5; color:#262626; }"
								"QMessageBox QLabel {color:#fff;}";

		msg->setStyleSheet(styleSheet);
		msg->setText("<span style=\"color:#fff;\">" +QString::fromStdString(results) + "</span>");
	} else if (settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QMessageBox { background-color: #031d54; color:#fff; border: 1px solid #fff5f5; "
								"min-width: 150px; min-width: 250px; }"
								"QMessageBox QPushButton { background-color: #031d54; color:#fff; "
								"border: 1px solid #fff5f5; padding-left:10px; "
								"padding-right:10px; min-height:25px; min-width:75px; }"
								"QMessageBox QPushButton::hover { background-color:#fff5f5; color:#031d54; }";
								"QMessageBox QLabel {color:#fff;}";

		msg->setStyleSheet(styleSheet);
		msg->setText("<span style=\"color:#fff;\">" +QString::fromStdString(results) + "</span>");
	} else { 
		msg->setText(QString::fromStdString(results));
	}
    msg->exec();
}
