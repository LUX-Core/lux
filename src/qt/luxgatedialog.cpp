
#include "luxgatedialog.h"
#include "luxgateconfigmodel.h"
#include "luxgate/luxgate.h"
#include "ui_luxgatedialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include <qmessagebox.h>
#include <qtimer.h>
#include <rpcserver.h>
#include "crypto/rfc6979_hmac_sha256.h"
#include "utilstrencodings.h"
#include "qcustomplot.h"

#include <QClipboard>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

#include <QUrl>
#include <QUrlQuery>
#include <QVariant>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>
#include <QJsonArray>
#include <QTime>
#include <QVBoxLayout>
#include <QDockWidget>

#include <openssl/hmac.h>
#include <stdlib.h>
#include "util.h"
#include "luxgateconfigmodel.h"
#include "../luxgate/lgconfig.h"
#include <openssl/x509.h>


#include <string.h>
#include <openssl/md5.h>

#include <cctype>
#include <iomanip>
#include <sstream>

#include <iostream>
#include <algorithm>
#include <functional>

using namespace std;

LuxgateDialog::LuxgateDialog(QWidget *parent) :
        QMainWindow(parent),
        ui(new Ui::LuxgateDialog),
        model(0)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Widget);
    setContentsMargins(10,10,10,10);
    moveWidgetsToDocks();

    //init configuration
    {
        ui->tableViewConfiguration->setModel(new LuxgateConfigModel(this));
        connect(ui->pushButtonResetConfig, &QPushButton::clicked,
                this, &LuxgateDialog::slotClickResetConfiguration);
        connect(ui->pushButtonChangeConfig, &QPushButton::clicked,
                this, &LuxgateDialog::slotClickChangeConfig);
        slotClickResetConfiguration();
    }
}

void LuxgateDialog::moveWidgetsToDocks()
{
    //Left
    auto dockBidPanel = createDock(ui->bidpanel, "bidpanel");
    auto dockAskPanel = createDock(ui->askpanel_2, "askpanel_2");
    auto dockConfigPanel = createDock(ui->configpanel, "Configuration");
    auto dockChart = createDock(ui->tabCharts, "tabCharts");
    auto dockOrderBook = createDock(ui->tabOrderBook, "tabOrderBook");
    auto dockOpenOrders = createDock(ui->tabOpenOrders, "tabOpenOrders");
    auto dockBalances= createDock(ui->tabBalances, "tabBalances");
    addDockWidget(Qt::LeftDockWidgetArea, dockBidPanel);
    splitDockWidget(dockBidPanel, dockChart, Qt::Horizontal);
    splitDockWidget(dockBidPanel, dockAskPanel, Qt::Vertical);
    splitDockWidget(dockAskPanel, dockConfigPanel, Qt::Vertical);
    tabifyDockWidget(dockChart, dockOrderBook);
    tabifyDockWidget(dockChart, dockOpenOrders);
    tabifyDockWidget(dockChart, dockBalances);
    setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);

    //Right
    auto dockActionPanel = createDock(ui->action_panel_3, "action_panel_3");
    auto dockStatusPanel = createDock(ui->status_panel, "status_panel");
    auto dockSwapPanel = createDock(ui->swap_panel, "swap_panel");
    addDockWidget(Qt::RightDockWidgetArea, dockActionPanel);
    addDockWidget(Qt::RightDockWidgetArea, dockStatusPanel);
    addDockWidget(Qt::RightDockWidgetArea, dockSwapPanel);

    //remove Central
    QDockWidget* centralDockNULL = new QDockWidget(this);
    centralDockNULL->setFixedWidth(0);
    setCentralWidget(centralDockNULL);
}

QDockWidget* LuxgateDialog::createDock(QWidget* widget, const QString& title)
{
    QDockWidget* dock = new QDockWidget(this);
    widget->setProperty("IsDockable", true);
    if(widget->layout())
    {
        QMargins widgetMargins = widget->layout()->contentsMargins();
        widgetMargins.setTop(widgetMargins.top() + 2);
        widget->layout()->setContentsMargins(widgetMargins);
    }
    dock->setWidget(widget);
    dock->setWindowTitle(title);

    return dock;
}

void LuxgateDialog::setModel(WalletModel *model)
{
    this->model = model;
}

void LuxgateDialog::slotClickResetConfiguration()
{
    auto model = qobject_cast<LuxgateConfigModel*>(ui->tableViewConfiguration->model());
    model->removeRows(0, model->rowCount());
    // Read LuxGate config
    std::map<std::string, BlockchainConfig> config = ReadLuxGateConfigFile();
    for (auto it : config)
    {
        BlockchainConfig conf = it.second;
        model->insertRows(model->rowCount(), 1);
        model->setData(model->rowCount(), 0,
                QVariant::fromValue(conf),
                LuxgateConfigModel::AllDataRole);
    }
}

void LuxgateDialog::slotClickChangeConfig()
{
    std::map<std::string, BlockchainConfig> configs;
    //fill in configs
    {
        auto model = qobject_cast<LuxgateConfigModel *>(ui->tableViewConfiguration->model());
        for(int iR=0; iR<model->rowCount(); iR++)
        {
            BlockchainConfig conf = qvariant_cast<BlockchainConfig>(model->data(iR, 0, LuxgateConfigModel::AllDataRole));
            configs[conf.ticker] = conf;
        }
    }
    ChangeLuxGateConfiguration(configs);
}

LuxgateDialog::~LuxgateDialog()
{
    delete ui;
}
