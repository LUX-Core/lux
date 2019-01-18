
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
#include <QBrush>
#include <QItemDelegate>
#include <QRegExpValidator>
#include <QRegExp>

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


class LuxgateConfigDelegate : public QItemDelegate
{
public:
    LuxgateConfigDelegate(QObject *parent);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QWidget * createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
private slots:
    void commitAndCloseEditor();
};

LuxgateConfigDelegate::LuxgateConfigDelegate(QObject *parent) :
        QItemDelegate(parent)
{}

void LuxgateConfigDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    painter->setRenderHint(QPainter::Antialiasing, true);
    QStyleOptionViewItem opt = option;
    if(!index.model()->data(index, LuxgateConfigModel::ValidRole).toBool()) {
        QColor col;
        col.setNamedColor("#990012");
        opt.palette.setColor(QPalette::Highlight, col);
    }
    QItemDelegate::paint(painter, opt, index);
    if(LuxgateConfigModel::SwapSupportColumn == index.column())
    {
        QPen oldPen = painter->pen();
        QBrush oldBrush =  painter->brush();
        QColor col;
        if(index.model()->data(index).toBool())
            col = Qt::green;
        else
            col = Qt::red;
        painter->setPen(QPen(Qt::black));
        painter->setBrush(QBrush(col));
        painter->drawEllipse(option.rect.center(),9,9);
        painter->setPen(oldPen);
        painter->setBrush(oldBrush);

    }
}

QWidget * LuxgateConfigDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QLineEdit * editor = new QLineEdit(parent);
    editor->setProperty("Column", index.column());
    connect(editor, &QLineEdit::editingFinished,
            this, &LuxgateConfigDelegate::commitAndCloseEditor);

    if(LuxgateConfigModel::PortColumn == index.column())
        editor->setValidator(new QRegExpValidator(QRegExp("^[1-9]\\d*$"), editor));
    if(LuxgateConfigModel::HostColumn == index.column())
        editor->setInputMask("000.000.000.000;_");
    if(LuxgateConfigModel::Zmq_pub_raw_tx_endpointColumn == index.column())
        editor->setInputMask("000.000.000.000:0000000000;_");
    return editor;
}

void LuxgateConfigDelegate::commitAndCloseEditor()
{
    QLineEdit *editor = qobject_cast<QLineEdit *>(sender());

    emit commitData(editor);
    emit closeEditor(editor);
}

void LuxgateConfigDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
{
    int iColumn = index.column();
    bool bSetModel = true;
    QLineEdit * lineEdit = qobject_cast<QLineEdit *>(editor);
    if(LuxgateConfigModel::Zmq_pub_raw_tx_endpointColumn == iColumn)
    {
        QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
        QRegExp ipRegex ("^" + ipRange
                         + "\\." + ipRange
                         + "\\." + ipRange
                         + "\\." + ipRange + "\\:" + "[1-9]\\d*$");
        QRegExpValidator *val = new QRegExpValidator(ipRegex, lineEdit);
        int pos = 0;
        QString text = lineEdit->text();
        if(QValidator::Acceptable != val->validate(text, pos))
            bSetModel = false;
    }
    if(LuxgateConfigModel::HostColumn == iColumn)
    {
        QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
        QRegExp ipRegex ("^" + ipRange
                         + "\\." + ipRange
                         + "\\." + ipRange
                         + "\\." + ipRange + "$");
        QRegExpValidator *ipValidator = new QRegExpValidator(ipRegex, lineEdit);
        int pos = 0;
        QString text = lineEdit->text();
        if(QValidator::Acceptable != ipValidator->validate(text, pos))
            bSetModel = false;
    }
    if(bSetModel)
        QItemDelegate::setModelData(editor, model, index);
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
        ui->tableViewConfiguration->setItemDelegate(new LuxgateConfigDelegate(ui->tableViewConfiguration));
        ui->tableViewConfiguration->setStyleSheet("");
        QHeaderView * HeaderView = ui->tableViewConfiguration->horizontalHeader();
        HeaderView->setSectionsMovable(true);
        HeaderView->setSectionResizeMode(QHeaderView::Interactive);
        HeaderView->resizeSection(LuxgateConfigModel::TickerColumn, 50);
        HeaderView->resizeSection(LuxgateConfigModel::HostColumn, 80);
        HeaderView->resizeSection(LuxgateConfigModel::PortColumn, 50);
        HeaderView->resizeSection(LuxgateConfigModel::SwapSupportColumn, 50);
        HeaderView->resizeSection(LuxgateConfigModel::Zmq_pub_raw_tx_endpointColumn, 80);
        int rpc_width = (HeaderView->length() - (50*3+80*2) -5) /2;
        HeaderView->resizeSection(LuxgateConfigModel::RpcuserColumn, rpc_width);
        HeaderView->resizeSection(LuxgateConfigModel::RpcpasswordColumn, rpc_width);
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
                tr("No rows selected!"));
        return;
    }
    modelConfig->removeRows(selectRows.first().row(), 1);
    ui->labelConfigStatus->setProperty("status", "info");
    ui->labelConfigStatus->style()->unpolish(ui->labelConfigStatus);
    ui->labelConfigStatus->style()->polish(ui->labelConfigStatus);
    ui->labelConfigStatus->setText(tr("Unsaved configuration changes"));
}

void LuxgateDialog::slotConfigDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    if(LuxgateConfigModel::SwapSupportColumn == topLeft.column()
       &&
       LuxgateConfigModel::SwapSupportColumn == bottomRight.column())
        return;

    if(roles.contains(Qt::EditRole)
        ||
       roles.contains(Qt::DisplayRole))
    {
        ui->labelConfigStatus->setProperty("status", "info");
        ui->labelConfigStatus->style()->unpolish(ui->labelConfigStatus);
        ui->labelConfigStatus->style()->polish(ui->labelConfigStatus);
        ui->labelConfigStatus->setText(tr("Unsaved configuration changes"));

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
        if(conf.ticker == "Lux")
            continue;
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
    auto modelConfig = qobject_cast<LuxgateConfigModel *>(ui->tableViewConfiguration->model());
    //all configs are valid
    bool bValid = true;
    int iEditingCol = modelConfig->columnCount();
    int iEditingRow = modelConfig->rowCount();
    //fill in configs and bValid
    {
        for(int iR=0; iR<modelConfig->rowCount(); iR++)
        {
            BlockchainConfig conf = qvariant_cast<BlockchainConfigQt>(modelConfig->data(iR, 0, LuxgateConfigModel::AllDataRole)).toBlockchainConfig();
            configs[conf.ticker] = conf;

            //check Valid configs
            auto validItems = isValidBlockchainConfig(conf);
            if(!validItems.bValid) {

                if(!validItems.bTickerValid)
                    iEditingCol = qMin(iEditingCol, static_cast<int>(LuxgateConfigModel::TickerColumn));
                modelConfig->setData(iR, LuxgateConfigModel::TickerColumn, validItems.bTickerValid, LuxgateConfigModel::ValidRole);

                if(!validItems.bHostValid)
                    iEditingCol = qMin(iEditingCol, static_cast<int>(LuxgateConfigModel::HostColumn));
                modelConfig->setData(iR, LuxgateConfigModel::HostColumn, validItems.bHostValid, LuxgateConfigModel::ValidRole);


                if(!validItems.bPortValid)
                    iEditingCol = qMin(iEditingCol, static_cast<int>(LuxgateConfigModel::PortColumn));
                modelConfig->setData(iR, LuxgateConfigModel::PortColumn, validItems.bPortValid, LuxgateConfigModel::ValidRole);

                if(!validItems.bRpcUserValid)
                    iEditingCol = qMin(iEditingCol, static_cast<int>(LuxgateConfigModel::RpcuserColumn));
                modelConfig->setData(iR, LuxgateConfigModel::RpcuserColumn, validItems.bRpcUserValid, LuxgateConfigModel::ValidRole);

                if(!validItems.bRpcPasswordValid)
                    iEditingCol = qMin(iEditingCol, static_cast<int>(LuxgateConfigModel::RpcpasswordColumn));
                modelConfig->setData(iR, LuxgateConfigModel::RpcpasswordColumn, validItems.bRpcPasswordValid, LuxgateConfigModel::ValidRole);

                if(!validItems.bZmqEndpointValid)
                    iEditingCol = qMin(iEditingCol, static_cast<int>(LuxgateConfigModel::Zmq_pub_raw_tx_endpointColumn));

                if(iEditingCol != modelConfig->columnCount())
                    iEditingRow = iR;

                modelConfig->setData(iR, LuxgateConfigModel::Zmq_pub_raw_tx_endpointColumn, validItems.bZmqEndpointValid, LuxgateConfigModel::ValidRole);
                bValid = false;
            }
            else {
                for(int iC=0; iC<modelConfig->columnCount(); iC++){
                    modelConfig->setData(iR, iC, true, LuxgateConfigModel::ValidRole);
                }
            }
            //check that ticker is unical
            auto tickerList = modelConfig->match(modelConfig->index(LuxgateConfigModel::TickerColumn,0), Qt::DisplayRole, QString::fromStdString(conf.ticker), -1);
            if(tickerList.size() > 1 && conf.ticker != "") {
                iEditingCol = qMin(iEditingCol, static_cast<int>(LuxgateConfigModel::TickerColumn));
                bValid = false;
                foreach(auto index, tickerList)
                {
                    modelConfig->setData(index, false, LuxgateConfigModel::ValidRole);
                    iEditingRow = qMin(iEditingRow,index.row());
                }
            }
        }
    }

    if(!bValid)
    {
        QMessageBox::critical(this, tr("Luxgate"), tr("Configuration is not valid!"));

        if(iEditingCol != modelConfig->columnCount()) {
            ui->tableViewConfiguration->edit(modelConfig->index(iEditingRow, iEditingCol));
        }
        ui->labelConfigStatus->setProperty("status", "error");
        ui->labelConfigStatus->style()->unpolish(ui->labelConfigStatus);
        ui->labelConfigStatus->style()->polish(ui->labelConfigStatus);
        ui->labelConfigStatus->setText(tr("Configuration is not valid!"));
        return;
    }

    if(ChangeLuxGateConfiguration(configs))
    {
        update();//fill in swap supported
        for (auto it : blockchainClientPool)
        {
            Ticker ticker =  it.first;
            auto client =  it.second;
            auto list = modelConfig->match(modelConfig->index(0,0), Qt::DisplayRole, QString::fromStdString(ticker), -1);
            if(!list.isEmpty())
            {
                foreach(auto index, list)
                {
                    if(LuxgateConfigModel::TickerColumn == index.column())
                    {
                        modelConfig->setData(modelConfig->index(index.row(), LuxgateConfigModel::SwapSupportColumn),
                                client->IsSwapSupported());
                        break;
                    }
                }
            }
        }
        ui->pushButtonResetConfig->setEnabled(false);
        ui->pushButtonChangeConfig->setEnabled(false);
        ui->labelConfigStatus->clear();
    }
    else
    {
        QMessageBox::critical(this, tr("Luxgate"), tr("Error while changing configuration"));
        ui->labelConfigStatus->setProperty("status", "error");
        ui->labelConfigStatus->style()->unpolish(ui->labelConfigStatus);
        ui->labelConfigStatus->style()->polish(ui->labelConfigStatus);
        ui->labelConfigStatus->setText(tr("Error while changing configuration!"));
    }

}

void LuxgateDialog::slotClickAddConfiguration()
{
    auto modelConfig = qobject_cast<LuxgateConfigModel*>(ui->tableViewConfiguration->model());
    modelConfig->insertRows(modelConfig->rowCount(), 1);
    ui->labelConfigStatus->setProperty("status", "info");
    ui->labelConfigStatus->style()->unpolish(ui->labelConfigStatus);
    ui->labelConfigStatus->style()->polish(ui->labelConfigStatus);
    ui->labelConfigStatus->setText(tr("Unsaved configuration changes"));

    ui->pushButtonResetConfig->setEnabled(true);
    ui->pushButtonChangeConfig->setEnabled(true);
}

LuxgateDialog::~LuxgateDialog()
{
    delete ui;
}
