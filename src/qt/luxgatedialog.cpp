
#include "luxgatedialog.h"
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
#include "luxgatebidpanel.h"

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

CurrencyPair curCurrencyPair ("LUX", "BTC");

LuxgateDialog::LuxgateDialog(QWidget *parent) :
        QMainWindow(parent),
        ui(new Ui::LuxgateDialog),
        model(0)
{
    ui->setupUi(this);
    setWindowFlags(Qt::Widget);
    setContentsMargins(10,10,10,10);
    moveWidgetsToDocks();
}

void LuxgateDialog::moveWidgetsToDocks()
{
    dockBidPanel = createDock(ui->bidpanel, "Bids");
    dockAskPanel = createDock(ui->askspanel, "Asks");
    dockConfigPanel = createDock(ui->configpanel, "Configuration");

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
    ui->askspanel->setModel(model);
    ui->bidpanel->setModel(model);
    OptionsModel * opt_model = model->getOptionsModel();

    dockAskPanel->setHidden(opt_model->getHideAsksWidget());
    connect(opt_model, &OptionsModel::hideAsksWidget,
            dockAskPanel, &QDockWidget::setHidden);

    dockBidPanel->setHidden(opt_model->getHideBidsWidget());
    connect(opt_model, &OptionsModel::hideBidsWidget,
            dockBidPanel, &QDockWidget::setHidden);

    dockConfigPanel->setHidden(opt_model->getHideConfigWidget());
    connect(opt_model, &OptionsModel::hideConfigWidget,
            dockConfigPanel, &QDockWidget::setHidden);
}

LuxgateDialog::~LuxgateDialog()
{
    delete ui;
}
