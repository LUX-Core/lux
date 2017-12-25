#include "tradingdialog.h"
#include "ui_tradingdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include <qmessagebox.h>
#include <qtimer.h>
#include <rpcserver.h>
#include </home/ilya/lux/lux/src/crypto/rfc6979_hmac_sha256.h>
#include </home/ilya/lux/lux/src/utilstrencodings.h>

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

#include <openssl/hmac.h>
#include <stdlib.h>
#include "util.h"
#include <openssl/x509.h>
//#include <chrono>


#include <string.h>
#include <openssl/md5.h>

#include <cctype>
#include <iomanip>
#include <sstream>

#include <iostream>
#include <algorithm>

using namespace std;

tradingDialog::tradingDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::tradingDialog),
    model(0)
{
    ui->setupUi(this);
    timerid = 0;
    qDebug() <<  "Expected this";
    printf("Plot: sdfsdfsdf:");
    
    ui->BtcAvailableLabel->setTextFormat(Qt::RichText);
    ui->LUXAvailableLabel->setTextFormat(Qt::RichText);
    ui->BuyCostLabel->setTextFormat(Qt::RichText);
    ui->SellCostLabel->setTextFormat(Qt::RichText);
    ui->CryptopiaBTCLabel->setTextFormat(Qt::RichText);
    ui->CryptopiaLUXLabel->setTextFormat(Qt::RichText);
    ui->CSDumpLabel->setTextFormat(Qt::RichText);
    ui->CSTotalLabel->setTextFormat(Qt::RichText);
    ui->CSReceiveLabel->setTextFormat(Qt::RichText);

    //Set tabs to inactive
    ui->TradingTabWidget->setTabEnabled(0,false);
    ui->TradingTabWidget->setTabEnabled(1,false);
    ui->TradingTabWidget->setTabEnabled(3,false);
    ui->TradingTabWidget->setTabEnabled(4,false);
    ui->TradingTabWidget->setTabEnabled(5,false);

//    connect(ui->TradingTabWidget, SIGNAL(tabBarClicked(int)), this, SLOT(on_TradingTabWidget_tabBarClicked(int)));

    // Listen for keypress
    connect(ui->PasswordInput, SIGNAL(returnPressed()),ui->LoadKeys,SIGNAL(clicked()));

    /*OrderBook Table Init*/
    CreateOrderBookTables(*ui->BidsTable,QStringList() << "SUM(BTC)" << "TOTAL(BTC)" << "LUX(SIZE)" << "BID(BTC)");
    CreateOrderBookTables(*ui->AsksTable,QStringList() << "ASK(BTC)" << "LUX(SIZE)" << "TOTAL(BTC)" << "SUM(BTC)");
    /*OrderBook Table Init*/

    /*Market History Table Init*/
    ui->MarketHistoryTable->setColumnCount(5);
    ui->MarketHistoryTable->verticalHeader()->setVisible(false);
    ui->MarketHistoryTable->setHorizontalHeaderLabels(QStringList()<<"DATE"<<"BUY/SELL"<<"BID/ASK"<<"TOTAL UNITS(LUX)"<<"TOTAL COST(BTC");
    ui->MarketHistoryTable->setRowCount(0);
    int Cellwidth =  ui->MarketHistoryTable->width() / 5;
    ui->MarketHistoryTable->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
    ui->MarketHistoryTable->horizontalHeader()->resizeSection(1,Cellwidth);
    ui->MarketHistoryTable->horizontalHeader()->resizeSection(2,Cellwidth);
    ui->MarketHistoryTable->horizontalHeader()->resizeSection(3,Cellwidth);
    ui->MarketHistoryTable->horizontalHeader()->resizeSection(4,Cellwidth);
    ui->MarketHistoryTable->horizontalHeader()->resizeSection(5,Cellwidth);
    ui->MarketHistoryTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->MarketHistoryTable->horizontalHeader()->setStyleSheet("QHeaderView::section, QHeaderView::section * {font-weight :bold;}");
    /*Market History Table Init*/

    /*Account History Table Init*/
  
