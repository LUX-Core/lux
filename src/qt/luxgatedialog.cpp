
#include "luxgatedialog.h"
#include "luxgateconfigmodel.h"
#include "luxgate/luxgate.h"
#include "luxgate/lgconfig.h"
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
#include <QStringList>
#include <QHeaderView>
#include <QVector>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QFile>

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

boost::filesystem::path PathFromQString(const QString & filePath)
{
#ifdef _WIN32
    auto * wptr = reinterpret_cast<const wchar_t*>(filePath.utf16());
    return boost::filesystem::path(wptr, wptr + filePath.size());
#else
    return boost::filesystem::path(filePath.toStdString());
#endif
}

QString QStringFromPath(const boost::filesystem::path & filePath)
{
#ifdef _WIN32
    return QString::fromStdWString(filePath.generic_wstring());
#else
    return QString::fromStdString(filePath.native());
#endif
}

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
        auto configModel = new LuxgateConfigModel(this);
        ui->tableViewConfiguration->setModel(configModel);

        QHeaderView * HeaderView = ui->tableViewConfiguration->horizontalHeader();
        HeaderView->setSectionResizeMode(QHeaderView::Stretch);

        slotClickResetConfiguration();
        ui->pushButtonResetConfig->setEnabled(false);
        ui->pushButtonChangeConfig->setEnabled(false);

        connect(ui->pushButtonResetConfig, &QPushButton::clicked,
                this, &LuxgateDialog::slotClickResetConfiguration);
        connect(ui->pushButtonChangeConfig, &QPushButton::clicked,
                this, &LuxgateDialog::slotClickChangeConfig);
        connect(configModel, &LuxgateConfigModel::dataChanged,
                this, &LuxgateDialog::slotConfigDataChanged);
        connect(ui->pushButtonAddConfiguration, &QPushButton::clicked,
                this, &LuxgateDialog::slotClickAddConfiguration);
        connect(ui->pushButtonRemoveConfiguration, &QPushButton::clicked,
                this, &LuxgateDialog::slotClickRemoveConfiguration);
        connect(ui->pushButtonImportConfig, &QPushButton::clicked,
                this, &LuxgateDialog::slotClickImportConfiguration);
        connect(ui->pushButtonExportConfig, &QPushButton::clicked,
                this, &LuxgateDialog::slotClickExportConfiguration);
    }
}

void LuxgateDialog::slotClickRemoveConfiguration()
{
    auto modelConfig = qobject_cast<LuxgateConfigModel *>(ui->tableViewConfiguration->model());
    auto selectRows = ui->tableViewConfiguration->selectionModel()->selectedRows();
    if(selectRows.isEmpty())
    {
        QMessageBox::critical(this, tr("Luxgate"),
                tr("Not one row selected!"));
        return;
    }
    modelConfig->removeRows(selectRows.first().row(), 1);
}

void LuxgateDialog::slotConfigDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    if(roles.contains(Qt::EditRole)
        ||
       roles.contains(Qt::DisplayRole))
    {
        ui->pushButtonResetConfig->setEnabled(true);
        ui->pushButtonChangeConfig->setEnabled(true);
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

void LuxgateDialog::slotClickImportConfiguration()
{
    QString openFileName = QFileDialog::getOpenFileName(this, tr("Import Luxgate Configuration"),
            QStandardPaths::writableLocation(QStandardPaths::DataLocation),
            tr("*.json"));
    if(!openFileName.isEmpty())
    {
        fillInTableWithConfig(openFileName);
        ui->pushButtonResetConfig->setEnabled(true);
        ui->pushButtonChangeConfig->setEnabled(true);
    }
}

void LuxgateDialog::slotClickExportConfiguration()
{
    QString newFileName = QFileDialog::getSaveFileName(this, tr("Export Luxgate Configuration"),
                                                        QStandardPaths::writableLocation(QStandardPaths::DataLocation),
                                                        tr("*.json"));
    QFile::remove(newFileName);
    QFile::copy(QStringFromPath(GetLuxGateConfigFile()),
                newFileName);
}

//strConfig - path to import config, if strConfig == "" use standard config file
void LuxgateDialog::fillInTableWithConfig(QString strConfig)
{
    auto modelConfig = qobject_cast<LuxgateConfigModel*>(ui->tableViewConfiguration->model());
    modelConfig->removeRows(0, modelConfig->rowCount());
    // Read LuxGate config
    std::map<std::string, BlockchainConfig> config = ReadLuxGateConfigFile(PathFromQString(strConfig));
    for (auto it : config)
    {
        BlockchainConfigQt conf(it.second);
        modelConfig->insertRows(modelConfig->rowCount(), 1);
        modelConfig->setData(modelConfig->rowCount()-1, 0,
                             QVariant::fromValue(conf),
                             LuxgateConfigModel::AllDataRole);
    }
}

void LuxgateDialog::slotClickResetConfiguration()
{
    fillInTableWithConfig();
    ui->pushButtonResetConfig->setEnabled(false);
    ui->pushButtonChangeConfig->setEnabled(false);
}

void LuxgateDialog::slotClickChangeConfig()
{
    std::map<std::string, BlockchainConfig> configs;
    //fill in configs
    {
        auto model = qobject_cast<LuxgateConfigModel *>(ui->tableViewConfiguration->model());
        for(int iR=0; iR<model->rowCount(); iR++)
        {
            BlockchainConfig conf = qvariant_cast<BlockchainConfigQt>(model->data(iR, 0, LuxgateConfigModel::AllDataRole)).toBlockchainConfig();
            configs[conf.ticker] = conf;
        }
    }
    //check Valid configs
    for (auto it : configs)
    {
        if(!isValidBlockchainConfig(it.second).bValid)
        {
            QMessageBox::critical(this, tr("Luxgate"), tr("Configuration is not valid!"));
            return;
        }
    }
    bool bChange = ChangeLuxGateConfiguration(configs);
    if(bChange)
    {
        ui->pushButtonResetConfig->setEnabled(false);
        ui->pushButtonChangeConfig->setEnabled(false);
    }
    else
    {
        QMessageBox::critical(this, tr("Luxgate"), tr("Error while changing configuration"));
    }
}

void LuxgateDialog::slotClickAddConfiguration()
{
    auto modelConfig = qobject_cast<LuxgateConfigModel*>(ui->tableViewConfiguration->model());
    modelConfig->insertRows(modelConfig->rowCount(), 1);
    ui->pushButtonResetConfig->setEnabled(true);
    ui->pushButtonChangeConfig->setEnabled(true);
}

LuxgateDialog::~LuxgateDialog()
{
    delete ui;
}
