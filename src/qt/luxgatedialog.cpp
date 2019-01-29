
#include "luxgatedialog.h"
#include "luxgategui_global.h"
#include "ui_luxgatedialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include <qmessagebox.h>
#include <qtimer.h>
#include <rpcserver.h>
#include "crypto/rfc6979_hmac_sha256.h"
#include "utilstrencodings.h"
#include "qcustomplot.h"
#include "luxgateconfigpanel.h"
#include "luxgatebidaskpanel.h"
#include "luxgateopenorderspanel.h"
#include "luxgateorderbookpanel.h"
#include "luxgatehistorypanel.h"
#include "tradingplot.h"
#include "luxgatecreateorderpanel.h"

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

Luxgate::CurrencyPair curCurrencyPair ("LUX", "BTC");

LuxgateDialog::LuxgateDialog(QWidget *parent) :
        QMainWindow(parent),
        ui(new Ui::LuxgateDialog),
        model(0)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Widget);
    setContentsMargins(10,10,10,10);
    createWidgetsAndDocks();
}

void LuxgateDialog::createWidgetsAndDocks()
{
    confPanel = new LuxgateConfigPanel(this);
    dockConfigPanel = createDock(confPanel, "Configuration");

    chartsPanel = new TradingPlot(this);
    dockChart = createDock(chartsPanel, "Charts");

    openOrders = new LuxgateOpenOrdersPanel(this);
    dockOpenOrdersPanel = createDock(openOrders, "Your Open Orders");

    orderBookPanel = new LuxgateOrderBookPanel(this);
    dockOrderBookPanel = createDock(orderBookPanel, "Order Book");

    historyPanel = new LuxgateHistoryPanel(this);
    dockHistoryPanel = createDock(historyPanel, "Trades History");

    addDockWidget(Qt::LeftDockWidgetArea, dockConfigPanel);

    tabifyDockWidget(dockConfigPanel, dockChart);
    tabifyDockWidget(dockChart, dockOrderBookPanel);
    tabifyDockWidget(dockChart, dockOpenOrdersPanel);
    tabifyDockWidget(dockChart, dockHistoryPanel);
    setTabPosition(Qt::LeftDockWidgetArea, QTabWidget::North);

    //Right
    createBuyOrderPanel = new LuxgateCreateOrderPanel(this);
    createBuyOrderPanel->setBidAsk(true);
    createBuyOrderPanel->setObjectName("createBuyOrderPanel");
    dockBuyOrderPanel = createDock(createBuyOrderPanel, "Buy " + curCurrencyPair.baseCurrency);

    createSellOrderPanel = new LuxgateCreateOrderPanel(this);
    createSellOrderPanel->setBidAsk(false);
    createSellOrderPanel->setObjectName("createSellOrderPanel");
    dockSellOrderPanel = createDock(createSellOrderPanel, "Sell " + curCurrencyPair.baseCurrency);

    addDockWidget(Qt::RightDockWidgetArea, dockBuyOrderPanel);
    addDockWidget(Qt::RightDockWidgetArea, dockSellOrderPanel);
/*    auto dockActionPanel = createDock(ui->action_panel_3, "Action");
    auto dockStatusPanel = createDock(ui->status_panel, "Status");
    auto dockSwapPanel = createDock(ui->swap_panel, "Swap");
    addDockWidget(Qt::RightDockWidgetArea, dockActionPanel);
    addDockWidget(Qt::RightDockWidgetArea, dockStatusPanel);
    addDockWidget(Qt::RightDockWidgetArea, dockSwapPanel);
*/
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
    orderBookPanel->setModel(model);
    openOrders->setModel(model);
    historyPanel->setModel(model);
    OptionsModel * opt_model = model->getOptionsModel();

    dockConfigPanel->setHidden(opt_model->getHideConfigWidget());
    connect(opt_model, &OptionsModel::hideConfigWidget,
            dockConfigPanel, &QDockWidget::setHidden);

    dockOpenOrdersPanel->setHidden(opt_model->getHideOpenOrdersWidget());
    connect(opt_model, &OptionsModel::hideOpenOrdersWidget,
            dockOpenOrdersPanel, &QDockWidget::setHidden);

    dockOrderBookPanel->setHidden(opt_model->getHideOrderBookWidget());
    connect(opt_model, &OptionsModel::hideOrderBookWidget,
            dockOrderBookPanel, &QDockWidget::setHidden);

    dockHistoryPanel->setHidden(opt_model->getHideTradesHistoryWidget());
    connect(opt_model, &OptionsModel::hideTradesHistoryWidget,
            dockHistoryPanel, &QDockWidget::setHidden);

    dockBuyOrderPanel->setHidden(opt_model->getHideBuyWidget());
    connect(opt_model, &OptionsModel::hideBuyWidget,
            dockBuyOrderPanel, &QDockWidget::setHidden);

    dockSellOrderPanel->setHidden(opt_model->getHideSellWidget());
    connect(opt_model, &OptionsModel::hideSellWidget,
            dockSellOrderPanel, &QDockWidget::setHidden);

    dockChart->setHidden(opt_model->getHideChartsWidget());
    connect(opt_model, &OptionsModel::hideChartsWidget,
            dockChart, &QDockWidget::setHidden);
}

LuxgateDialog::~LuxgateDialog()
{
    delete ui;
}
