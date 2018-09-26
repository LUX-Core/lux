//
// Created by 216k155 on 12/26/17.
//

#include "tradingdialog.h"
#include "ui_tradingdialog.h"
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

tradingDialog::tradingDialog(QWidget *parent) :
        QWidget(parent),
        ui(new Ui::tradingDialog),
        model(0)
{
    qmanager = new QNetworkAccessManager(this);
    ui->setupUi(this);
    timerid = 0;
    qDebug() <<  "Expected this";


    QVBoxLayout* plotLay = new QVBoxLayout();
    ui->priceChart->setLayout(plotLay);

    derPlot = new QCustomPlot();
    plotLay->addWidget(derPlot);

    ohlc = new QCPFinancial(derPlot->xAxis, derPlot->yAxis);
    ohlc->setName("OHLC");
    ohlc->setChartStyle(QCPFinancial::csOhlc);

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
    connect (ui->BidsTable, SIGNAL(cellClicked(int,int)), this, SLOT(BidInfoInsertSlot(int, int)));
    connect (ui->AsksTable, SIGNAL(cellClicked(int,int)), this, SLOT(AskInfoInsertSlot(int, int)));
    /*OrderBook Table Init*/

    /*Market History Table Init*/
    ui->MarketHistoryTable->setColumnCount(5);
    ui->MarketHistoryTable->verticalHeader()->setVisible(false);
    ui->MarketHistoryTable->setHorizontalHeaderLabels(QStringList()<<"DATE"<<"BUY/SELL"<<"BID/ASK"<<"TOTAL UNITS(LUX)"<<"TOTAL COST(BTC)");
    ui->MarketHistoryTable->setRowCount(0);
    int Cellwidth =  ui->MarketHistoryTable->width() / 5;
    ui->MarketHistoryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->MarketHistoryTable->horizontalHeader()->resizeSection(1,Cellwidth);
    ui->MarketHistoryTable->horizontalHeader()->resizeSection(2,Cellwidth);
    ui->MarketHistoryTable->horizontalHeader()->resizeSection(3,Cellwidth);
    ui->MarketHistoryTable->horizontalHeader()->resizeSection(4,Cellwidth);
    ui->MarketHistoryTable->horizontalHeader()->resizeSection(5,Cellwidth);
    ui->MarketHistoryTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->MarketHistoryTable->horizontalHeader()->setStyleSheet("QHeaderView::section, QHeaderView::section * {font-weight :bold;}");
    /*Market History Table Init*/

    /*Account History Table Init*/
    ui->TradeHistoryTable->setColumnCount(6);
    ui->TradeHistoryTable->verticalHeader()->setVisible(false);
    ui->TradeHistoryTable->setHorizontalHeaderLabels(QStringList() << "Date Time" << "Exchange" << "OrderType" << "QTY" << "Price" << "PricePerUnit" );
    ui->TradeHistoryTable->setRowCount(0);
    Cellwidth =  ui->TradeHistoryTable->width() / 6;
    ui->TradeHistoryTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(1,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(2,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(3,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(4,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(5,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(6,Cellwidth);
    /*ui->TradeHistoryTable->horizontalHeader()->resizeSection(7,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(8,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(9,Cellwidth);*/
    ui->TradeHistoryTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->TradeHistoryTable->horizontalHeader()->setStyleSheet("QHeaderView::section, QHeaderView::section * {font-weight :bold;}");
    /*Account History Table Init*/

    /*Open Orders Table*/
    ui->OpenOrdersTable->setColumnCount(9);
    ui->OpenOrdersTable->verticalHeader()->setVisible(false);
    ui->OpenOrdersTable->setHorizontalHeaderLabels(QStringList() << "OrderId" << "Date Time" << "Exchange" << "OrderType" << "QTY" << "QTY_Rem" << "Price" << "PricePerUnit" << "Cancel Order");
    ui->OpenOrdersTable->setRowCount(0);
    Cellwidth =  ui->OpenOrdersTable->width() / 8;
    ui->OpenOrdersTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(2,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(3,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(4,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(5,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(6,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(7,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(8,Cellwidth);
    //ui->OpenOrdersTable->horizontalHeader()->resizeSection(9,Cellwidth);
    ui->OpenOrdersTable->setColumnHidden(0,false);
    ui->OpenOrdersTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->OpenOrdersTable->horizontalHeader()->setStyleSheet("QHeaderView::section, QHeaderView::section * {font-weight :bold;}");

    connect (ui->OpenOrdersTable, SIGNAL(cellClicked(int,int)), this, SLOT(CancelOrderSlot(int, int)));
    /*Open Orders Table*/
    InitTrading();

}

void tradingDialog::InitTrading()
{       //todo - add internet connection/socket error checking.

    //Get default exchange info for the qlabels
    UpdaterFunction();
    qDebug() << "Updater called";
    if(this->timerid == 0)
    {
        //Timer is not set,lets create one.
        this->timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(UpdaterFunction()));
        this->timer->start(5000);
        this->timerid = this->timer->timerId();
    }

}

void tradingDialog::UpdaterFunction(){
    //LUXst get the main exchange info in order to populate qLabels in maindialog. then get data
    //required for the current tab.

    int Retval = SetExchangeInfoTextLabels();

    if (Retval == 0){
        ActionsOnSwitch(-1);
    }
}

QString tradingDialog::GetMarketSummary(){

    QString Response = sendRequest("https://www.cryptopia.co.nz/api/GetMarket/LUX_BTC");
    return Response;
}

QString tradingDialog::GetOrderBook(){

    QString  Response = sendRequest("https://www.cryptopia.co.nz/api/GetMarketOrders/LUX_BTC");
    return Response;
}

void tradingDialog::GetMarketHistory(){
    std::function<void(void)> f = std::bind(&tradingDialog::showMarketHistoryWhenReplyFinished, this);//&tradingDialog::showMarketHistoryWhenReplyFinished;//std::bind(&tradingDialog::showMarketHistoryWhenReplyFinished, this);

    sendRequest1(QString("https://www.cryptopia.co.nz/api/GetMarketHistory/LUX_BTC"), f);
}

QString tradingDialog::CancelOrder(QString OrderId){

    QString URL = "https://www.cryptopia.co.nz/api/CancelTrade";

    QString Response = sendRequest(URL, "POST", QString("{\"Type\":\"trade\", \"OrderId\":") + OrderId + QString("}"));
    return Response;
}

QString tradingDialog::BuyLUX(QString OrderType, double Quantity, double Rate){

    QString str = "";
    QString URL = "https://www.cryptopia.co.nz/api/SubmitTrade";

    QJsonObject stats_obj;
    stats_obj["Market"] = "LUX/BTC";
    stats_obj["Type"] = "Buy";
    stats_obj["Amount"] = Quantity;
    stats_obj["Rate"] = Rate;

    QJsonDocument doc(stats_obj);
    QString param_str(doc.toJson(QJsonDocument::Compact));


    QString Response = sendRequest(URL, "POST", param_str);
    return Response;
}

QString tradingDialog::SellLUX(QString OrderType, double Quantity, double Rate){

    QString str = "";
    QString URL = "https://www.cryptopia.co.nz/api/SubmitTrade";

    QJsonObject stats_obj;
    stats_obj["Market"] = "LUX/BTC";
    stats_obj["Type"] = "Sell";
    stats_obj["Amount"] = Quantity;
    stats_obj["Rate"] = Rate;

    QJsonDocument doc(stats_obj);
    QString param_str(doc.toJson(QJsonDocument::Compact));


    QString Response = sendRequest(URL, "POST", param_str);
    return Response;
}

QString tradingDialog::Withdraw(double Amount, QString Address, QString Coin){

    QString str = "";
    QString URL = "https://www.cryptopia.co.nz/api/SubmitWithdraw";

    char tmp_nonce[255];
    timeval curTime;
    gettimeofday(&curTime, NULL);
    long nonce = curTime.tv_usec;
    sprintf(tmp_nonce, "%ld", nonce);

    QJsonObject stats_obj;
    stats_obj["Currency"] = Coin;
    stats_obj["Address"] = Address;
    stats_obj["PaymentId"] = QString(tmp_nonce);
    stats_obj["Amount"] = Amount;

    QJsonDocument doc(stats_obj);
    QString param_str(doc.toJson(QJsonDocument::Compact));

    QString Response = sendRequest(URL, "POST", param_str);
    return Response;
}

QString tradingDialog::GetOpenOrders(){
    QString URL = "https://www.cryptopia.co.nz/api/GetOpenOrders";

    QString Response = sendRequest(URL, "POST");

    return Response;
}

QString tradingDialog::GetBalance(QString Currency){

    QString URL = "https://www.cryptopia.co.nz/api/GetBalance";

    QString Response = sendRequest(URL, "POST", QString("{\"Currency\":\"") + Currency + QString("\"}"));
    return Response;
}

QString tradingDialog::GetDepositAddress(){

    QString URL = "https://www.cryptopia.co.nz/api/GetDepositAddress";
    QString Response = sendRequest(URL, "POST", QString("{\"Currency\":\"LUX\"}"));
    return Response;
}

QString tradingDialog::GetAccountHistory(){

    QString URL = "https://www.cryptopia.co.nz/api/GetTradeHistory";

    QString Response = sendRequest(URL, "POST");
    return Response;
}

int tradingDialog::SetExchangeInfoTextLabels(){
    //Get the current exchange information + information for the current open tab if required.
    QString str = "";
    QString Response = GetMarketSummary();

    //Set the labels, parse the json result to get values.
    QJsonObject obj = GetResultObjectFromJSONObject(Response);

    //set labels to richtext to use css.
    ui->Bid->setTextFormat(Qt::RichText);
    ui->Ask->setTextFormat(Qt::RichText);
    ui->volumet->setTextFormat(Qt::RichText);
    ui->volumebtc->setTextFormat(Qt::RichText);

    ui->Ask->setText("<b>Ask:</b> <span style='font-weight:bold; font-size:12px; color:Red'>" + str.number(obj["AskPrice"].toDouble(),'i',8) + "</span> BTC");

    ui->Bid->setText("<b>Bid:</b> <span style='font-weight:bold; font-size:12px; color:Green;'>" + str.number(obj["BidPrice"].toDouble(),'i',8) + "</span> BTC");

    ui->volumet->setText("<b>LUX Volume:</b> <span style='font-weight:bold; font-size:12px; color:blue;'>" + str.number(obj["Volume"].toDouble(),'i',8) + "</span> LUX");

    ui->volumebtc->setText("<b>BTC Volume:</b> <span style='font-weight:bold; font-size:12px; color:blue;'>" + str.number(obj["BaseVolume"].toDouble(),'i',8) + "</span> BTC");

    obj.empty();

    return 0;
}

void tradingDialog::CreateOrderBookTables(QTableWidget& Table,QStringList TableHeader){

    Table.setColumnCount(4);
    Table.verticalHeader()->setVisible(false);

    Table.setHorizontalHeaderLabels(TableHeader);

    int Cellwidth =  Table.width() / 4;

    Table.horizontalHeader()->resizeSection(1,Cellwidth); // column 1, width 50
    Table.horizontalHeader()->resizeSection(2,Cellwidth);
    Table.horizontalHeader()->resizeSection(3,Cellwidth);
    Table.horizontalHeader()->resizeSection(4,Cellwidth);

    Table.setRowCount(0);

    Table.horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    Table.horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    Table.horizontalHeader()->setStyleSheet("QHeaderView::section, QHeaderView::section * { font-weight :bold;}");
}

void tradingDialog::DisplayBalance(QLabel &BalanceLabel,QLabel &Available, QLabel &Pending, QString Currency,QString Response){

    QString str;

    BalanceLabel.setTextFormat(Qt::RichText);
    Available.setTextFormat(Qt::RichText);
    Pending.setTextFormat(Qt::RichText);

    //Set the labels, parse the json result to get values.
    QJsonObject ResultObject = GetResultObjectFromJSONArray(Response);//GetResultObjectFromJSONObject(Response);

    BalanceLabel.setText("<span style='font-weight:bold; font-size:11px; color:green'>" + str.number( ResultObject["Total"].toDouble(),'i',8) + "</span> " + Currency);
    Available.setText("<span style='font-weight:bold; font-size:11px; color:green'>" + str.number( ResultObject["Available"].toDouble(),'i',8) + "</span> " +Currency);
    Pending.setText("<span style='font-weight:bold; font-size:11px; color:green'>" + str.number( ResultObject["Unconfirmed"].toDouble(),'i',8) + "</span> " +Currency);
}

void tradingDialog::DisplayBalance(QLabel &BalanceLabel, QString Response){

    QString str;

    //Set the labels, parse the json result to get values.
    QJsonObject ResultObject = GetResultObjectFromJSONArray(Response);
    BalanceLabel.setStyleSheet("font-weight:bold; font-size:12px; color:green");
    BalanceLabel.setText(str.number(ResultObject["Available"].toDouble(),'i',8));
}

void tradingDialog::DisplayBalance(QLabel &BalanceLabel, QLabel &BalanceLabel2, QString Response, QString Response2){

    QString str;
    QString str2;

    //Set the labels, parse the json result to get values.
    QJsonObject ResultObject = GetResultObjectFromJSONArray(Response);
    QJsonObject ResultObject2 = GetResultObjectFromJSONArray(Response2);

    BalanceLabel.setStyleSheet("font-weight:bold; font-size:12px; color:green");
    BalanceLabel2.setStyleSheet("font-weight:bold; font-size:12px; color:green");

    BalanceLabel.setText(str.number(ResultObject["Available"].toDouble(),'i',8));
    BalanceLabel2.setText(str2.number(ResultObject2["Available"].toDouble(),'i',8));
}

void tradingDialog::ParseAndPopulateOpenOrdersTable(QString Response){

    int itteration = 0, RowCount = 0;

    QJsonArray jsonArray = GetResultArrayFromJSONObject(Response);
    QJsonObject obj;

    ui->OpenOrdersTable->setRowCount(0);

    foreach (const QJsonValue & value, jsonArray)
    {
        QString str = "";
        obj = value.toObject();

        RowCount = ui->OpenOrdersTable->rowCount();

        ui->OpenOrdersTable->insertRow(RowCount);
        ui->OpenOrdersTable->setItem(itteration, 0, new QTableWidgetItem(str.number(obj["OrderId"].toDouble(),'i',0)));
        ui->OpenOrdersTable->setItem(itteration, 1, new QTableWidgetItem(CryptopiaTimeStampToReadable(obj["TimeStamp"].toString())));
        ui->OpenOrdersTable->setItem(itteration, 2, new QTableWidgetItem(obj["Market"].toString()));
        ui->OpenOrdersTable->setItem(itteration, 3, new QTableWidgetItem(obj["Type"].toString()));
        ui->OpenOrdersTable->setItem(itteration, 4, new QTableWidgetItem(str.number(obj["Amount"].toDouble(),'i',8)));
        ui->OpenOrdersTable->setItem(itteration, 5, new QTableWidgetItem(str.number(obj["Remaining"].toDouble(),'i',8)));
        ui->OpenOrdersTable->setItem(itteration, 6, new QTableWidgetItem(str.number(obj["Total"].toDouble(),'i',8)));
        ui->OpenOrdersTable->setItem(itteration, 7, new QTableWidgetItem(str.number(obj["Rate"].toDouble(),'i',8)));
        ui->OpenOrdersTable->setItem(itteration, 8, new QTableWidgetItem(tr("Cancel Order")));

        //Handle the cancel link in open orders table
        QTableWidgetItem* CancelCell;
        CancelCell= ui->OpenOrdersTable->item(itteration, 8);    //Set the wtablewidget item to the cancel cell item.
        CancelCell->setForeground(QColor::fromRgb(255,0,0));      //make this item red.
        CancelCell->setTextAlignment(Qt::AlignCenter);
        itteration++;
    }
    obj.empty();
}

void tradingDialog::BidInfoInsertSlot(int row, int col){

    QString BuyBidPriceString = ui->BidsTable->model()->data(ui->BidsTable->model()->index(row,3)).toString();
    QString UnitsString = ui->BidsTable->model()->data(ui->BidsTable->model()->index(row,2)).toString();
    ui->BuyBidPriceEdit->setText(BuyBidPriceString);
    ui->UnitsInput->setText(UnitsString);



}

void tradingDialog::AskInfoInsertSlot(int row, int col){

    QString SellBidPriceString = ui->AsksTable->model()->data(ui->AsksTable->model()->index(row,0)).toString();
    QString UnitsLUXString = ui->AsksTable->model()->data(ui->AsksTable->model()->index(row,1)).toString();
    ui->SellBidPriceEdit->setText(SellBidPriceString);
    ui->UnitsInputLUX->setText(UnitsLUXString);



}

void tradingDialog::CancelOrderSlot(int row, int col){

    QString OrderId = ui->OpenOrdersTable->model()->data(ui->OpenOrdersTable->model()->index(row,0)).toString();
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this,"Cancel Order","Are you sure you want to cancel the order?",QMessageBox::Yes|QMessageBox::No);

    if (reply == QMessageBox::Yes) {

        QString Response = CancelOrder(OrderId);

        QJsonDocument jsonResponse = QJsonDocument::fromJson(Response.toUtf8());
        QJsonObject ResponseObject = jsonResponse.object();

        if (ResponseObject["Success"].toBool() == false){

            QMessageBox::information(this,"Failed To Cancel Order",ResponseObject["Error"].toString());

        }else if (ResponseObject["Success"].toBool() == true){
            ui->OpenOrdersTable->model()->removeRow(row);
            QMessageBox::information(this,"Success","You're order was cancelled.");
        }
    } else {
        qDebug() << "Do Nothing";
    }
}

void tradingDialog::ParseAndPopulateAccountHistoryTable(QString Response){

    int itteration = 0, RowCount = 0;

    QJsonArray jsonArray   = GetResultArrayFromJSONObject(Response);
    QJsonObject obj;

    ui->TradeHistoryTable->setRowCount(0);

    foreach (const QJsonValue & value, jsonArray)
    {
        QString str = "";
        obj = value.toObject();

        RowCount = ui->TradeHistoryTable->rowCount();

        ui->TradeHistoryTable->insertRow(RowCount);
        ui->TradeHistoryTable->setItem(itteration, 0, new QTableWidgetItem(CryptopiaTimeStampToReadable(obj["TimeStamp"].toString())));
        ui->TradeHistoryTable->setItem(itteration, 1, new QTableWidgetItem(obj["Market"].toString()));
        ui->TradeHistoryTable->setItem(itteration, 2, new QTableWidgetItem(obj["Type"].toString()));
        ui->TradeHistoryTable->setItem(itteration, 3, new QTableWidgetItem(str.number(obj["Amount"].toDouble(),'i',8)));
        ui->TradeHistoryTable->setItem(itteration, 4, new QTableWidgetItem(str.number(obj["Total"].toDouble(),'i',8)));
        ui->TradeHistoryTable->setItem(itteration, 5, new QTableWidgetItem(str.number(obj["Rate"].toDouble(),'i',8)));
        itteration++;
    }

    obj.empty();
}


void tradingDialog::ParseAndPopulateOrderBookTables(QString OrderBook){

    QString str;
    QJsonObject obj;
    QJsonObject ResultObject = GetResultObjectFromJSONObject(OrderBook);


    int BuyItteration = 0,SellItteration = 0, BidRows = 0, AskRows = 0;

    QJsonArray  BuyArray  = ResultObject.value("Buy").toArray();                //get buy/sell object from result object
    QJsonArray  SellArray = ResultObject.value("Sell").toArray();               //get buy/sell object from result object

    double LUXSupply = 0;
    double LUXDemand = 0;
    double BtcSupply = 0;
    double BtcDemand = 0;

    ui->AsksTable->setRowCount(0);

    foreach (const QJsonValue & value, SellArray)
    {
        obj = value.toObject();

        double x = obj["Price"].toDouble(); //would like to use int64 here
        double y = obj["Volume"].toDouble();
        double a = (x * y);

        LUXSupply += y;
        BtcSupply += a;

        AskRows = ui->AsksTable->rowCount();
        ui->AsksTable->insertRow(AskRows);
        ui->AsksTable->setItem(SellItteration, 0, new QTableWidgetItem(str.number(x,'i',8)));
        ui->AsksTable->setItem(SellItteration, 1, new QTableWidgetItem(str.number(y,'i',8)));
        ui->AsksTable->setItem(SellItteration, 2, new QTableWidgetItem(str.number(a,'i',8)));
        ui->AsksTable->setItem(SellItteration, 3, new QTableWidgetItem(str.number(BtcSupply,'i',8)));
        SellItteration++;
    }

    ui->BidsTable->setRowCount(0);

    foreach (const QJsonValue & value, BuyArray)
    {
        obj = value.toObject();

        double x = obj["Price"].toDouble(); //would like to use int64 here
        double y = obj["Volume"].toDouble();
        double a = (x * y);

        LUXDemand += y;
        BtcDemand += a;

        BidRows = ui->BidsTable->rowCount();
        ui->BidsTable->insertRow(BidRows);
        ui->BidsTable->setItem(BuyItteration, 0, new QTableWidgetItem(str.number(BtcDemand,'i',8)));
        ui->BidsTable->setItem(BuyItteration, 1, new QTableWidgetItem(str.number(a,'i',8)));
        ui->BidsTable->setItem(BuyItteration, 2, new QTableWidgetItem(str.number(y,'i',8)));
        ui->BidsTable->setItem(BuyItteration, 3, new QTableWidgetItem(str.number(x,'i',8)));
        BuyItteration++;
    }

    ui->LUXSupply->setText("<b>Supply:</b> <span style='font-weight:bold; font-size:12px; color:blue'>" + str.number(LUXSupply,'i',8) + "</span><b> LUX</b>");
    ui->BtcSupply->setText("<span style='font-weight:bold; font-size:12px; color:blue'>" + str.number(BtcSupply,'i',8) + "</span><b> BTC</b>");
    ui->AsksCount->setText("<b>Ask's :</b> <span style='font-weight:bold; font-size:12px; color:blue'>" + str.number(ui->AsksTable->rowCount()) + "</span>");

    ui->LUXDemand->setText("<b>Demand:</b> <span style='font-weight:bold; font-size:12px; color:blue'>" + str.number(LUXDemand,'i',8) + "</span><b> LUX</b>");
    ui->BtcDemand->setText("<span style='font-weight:bold; font-size:12px; color:blue'>" + str.number(BtcDemand,'i',8) + "</span><b> BTC</b>");
    ui->BidsCount->setText("<b>Bid's :</b> <span style='font-weight:bold; font-size:12px; color:blue'>" + str.number(ui->BidsTable->rowCount()) + "</span>");
    obj.empty();
}


void tradingDialog::ParseAndPopulateMarketHistoryTable(QString Response){

    int itteration = 0, RowCount = 0;
    QJsonArray    jsonArray    = GetResultArrayFromJSONObject(Response);
    QJsonObject   obj;

    ui->MarketHistoryTable->setRowCount(0);

    foreach (const QJsonValue & value, jsonArray)
    {
        QString str = "";
        obj = value.toObject();
        RowCount = ui->MarketHistoryTable->rowCount();

        ui->MarketHistoryTable->insertRow(RowCount);
        ui->MarketHistoryTable->setItem(itteration, 0, new QTableWidgetItem(CryptopiaIntegerTimeStampToReadable(obj["Timestamp"].toInt())));
        ui->MarketHistoryTable->setItem(itteration, 1, new QTableWidgetItem(obj["Type"].toString()));
        ui->MarketHistoryTable->setItem(itteration, 2, new QTableWidgetItem(str.number(obj["Price"].toDouble(),'i',8)));
        ui->MarketHistoryTable->setItem(itteration, 3, new QTableWidgetItem(str.number(obj["Amount"].toDouble(),'i',8)));
        ui->MarketHistoryTable->setItem(itteration, 4, new QTableWidgetItem(str.number(obj["Total"].toDouble(),'i',8)));
        ui->MarketHistoryTable->item(itteration,1)->setBackgroundColor((obj["Type"] == QStringLiteral("Buy")) ? (QColor (150, 191, 70,255)) : ( QColor (201, 119, 153,255)));
        itteration++;
    }
    obj.empty();
}


void tradingDialog::ParseAndPopulatePriceChart(QString Response){

    int itteration = 0;
    QJsonArray    jsonArray    = GetResultArrayFromJSONObject(Response);
    QJsonObject   obj;
    double binSize = 60*15;

    QVector<double> time(jsonArray.size()), price(jsonArray.size());

    foreach (const QJsonValue & value, jsonArray)
    {
        QString str = "";
        obj = value.toObject();


        time[itteration] = obj["Timestamp"].toDouble();
        price[itteration] = obj["Price"].toDouble();

        itteration++;
    }
    obj.empty();


    std::reverse(price.begin(), price.end());
    std::reverse(time.begin(), time.end());

    double min_time = *std::min_element(time.constBegin(), time.constEnd());
    double max_time = *std::max_element(time.constBegin(), time.constEnd());

    double min_price = *std::min_element(price.constBegin(), price.constEnd());
    double max_price = *std::max_element(price.constBegin(), price.constEnd());
    double diff_price = max_price - min_price;

    ohlc->data()->set(QCPFinancial::timeSeriesToOhlc(time, price, binSize/3.0, min_time)); // divide binSize by 3 just to make the ohlc bars a bit denser
    ohlc->setWidth(binSize*0.2);
    ohlc->setTwoColored(true);

    derPlot->xAxis->setLabel("Time (GMT)");
    derPlot->yAxis->setLabel("LUX Price in BTC");

    QCPAxisRect *volumeAxisRect = new QCPAxisRect(derPlot);
    QSharedPointer<QCPAxisTickerDateTime> dateTimeTicker(new QCPAxisTickerDateTime);
    dateTimeTicker->setDateTimeSpec(Qt::UTC);
    dateTimeTicker->setDateTimeFormat("dd.MM.yy(hh:mm)");
    volumeAxisRect->axis(QCPAxis::atBottom)->setTicker(dateTimeTicker);
    volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
    derPlot->xAxis->setBasePen(Qt::NoPen);
    derPlot->xAxis->setTicker(dateTimeTicker);
    derPlot->rescaleAxes();
    derPlot->xAxis->setRange(min_time, max_time+5);
    derPlot->xAxis->scaleRange(1.025, derPlot->xAxis->range().center());
    derPlot->yAxis->setRange(min_price-0.2*diff_price, max_price+0.2*diff_price);

    derPlot->replot();


}

void tradingDialog::ActionsOnSwitch(int index = -1){

    QString Response = "";
    QString Response2 = "";
    QString Response3 = "";
    QString Response4 = "";

    if(index == -1){
        index = ui->TradingTabWidget->currentIndex();
    }
    std::function<void(void)> f;

    switch (index){
        case 0:    //buy tab is active

            f = std::bind(&tradingDialog::showBalanceOfLUXOnTradingTab, this);
            sendRequest1(QString("https://www.cryptopia.co.nz/api/GetBalance"), f, "POST", QString("{\"Currency\":\"") + "LUX" + QString("\"}"));

            f = std::bind(&tradingDialog::showBalanceOfBTCOnTradingTab, this);
            sendRequest1(QString("https://www.cryptopia.co.nz/api/GetBalance"), f, "POST", QString("{\"Currency\":\"") + "BTC" + QString("\"}"));

            f = std::bind(&tradingDialog::showOrderBookOnTradingTab, this);
            sendRequest1("https://www.cryptopia.co.nz/api/GetMarketOrders/LUX_BTC", f);

            f = std::bind(&tradingDialog::showMarketHistoryOnTradingTab, this);
            sendRequest1(QString("https://www.cryptopia.co.nz/api/GetMarketHistory/LUX_BTC"), f);

            break;

        case 1: //Cross send tab active

            f = std::bind(&tradingDialog::showBalanceOfLUXOnSendTab, this);
            sendRequest1(QString("https://www.cryptopia.co.nz/api/GetBalance"), f, "POST", QString("{\"Currency\":\"") + "LUX" + QString("\"}"));

            f = std::bind(&tradingDialog::showBalanceOfBTCOnSendTab, this);
            sendRequest1(QString("https://www.cryptopia.co.nz/api/GetBalance"), f, "POST", QString("{\"Currency\":\"") + "BTC" + QString("\"}"));

            break;

        case 2://market history tab
            GetMarketHistory();
            break;

        case 3: //open orders tab

            f = std::bind(&tradingDialog::showOpenOrders, this);
            sendRequest1("https://www.cryptopia.co.nz/api/GetOpenOrders", f, "POST");

            break;

        case 4://account history tab

            f = std::bind(&tradingDialog::showAccountHistory, this);
            sendRequest1("https://www.cryptopia.co.nz/api/GetTradeHistory", f, "POST");

            break;

        case 5://show balance tab

            f = std::bind(&tradingDialog::showBalanceOfLUXOnBalanceTab, this);
            sendRequest1(QString("https://www.cryptopia.co.nz/api/GetBalance"), f, "POST", QString("{\"Currency\":\"") + "LUX" + QString("\"}"));

            f = std::bind(&tradingDialog::showBalanceOfBTCOnBalanceTab, this);
            sendRequest1(QString("https://www.cryptopia.co.nz/api/GetBalance"), f, "POST", QString("{\"Currency\":\"") + "BTC" + QString("\"}"));

            break;

        case 6:

            break;

    }

}

void tradingDialog::on_TradingTabWidget_tabBarClicked(int index)
{
    //tab was clicked, interrupt the timer and restart after action completed.

    this->timer->stop();

    ActionsOnSwitch(index);

    this->timer->start();
}


QString tradingDialog::sendRequest(QString url, QString method, QString body){

    QString Response = "";
    QString Secret   = this->SecretKey;

    string signature;
    string hmacsignature;
    string headerValue;

    // create custom temporary event loop on stack
    QEventLoop eventLoop;

    // "quit()" the event-loop, when the network request "finished()"
    QNetworkAccessManager mgr;
    QObject::connect(&mgr, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));

    // the HTTP request
    QNetworkRequest req = QNetworkRequest(QUrl(url));

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = NULL;
    if(method == "GET") {
        reply = mgr.get(req);
    } else if(method == "POST") {

        string API_KEY = this->ApiKey.toStdString();
        string SECRET_KEY = this->SecretKey.toStdString();

        unsigned char digest[16];
        const char* string = body.toLatin1().data();

        MD5_CTX ctx;
        MD5_Init(&ctx);
        MD5_Update(&ctx, string, strlen(string));
        MD5_Final(digest, &ctx);

        char mdString[33];
        for (int i = 0; i < 16; i++)
            sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);

        timeval curTime;
        gettimeofday(&curTime, NULL);
        long nonce = curTime.tv_usec;


        char *requestContentBase64String = base64(digest, sizeof(digest));


        signature += API_KEY;
        signature += "POST";
        signature += url_encode(url.toStdString());
        signature += to_string(nonce);
        signature += requestContentBase64String;

        const unsigned char * hmacsignature = HMAC_SHA256_SIGNER(QString(signature.c_str()), QString(SECRET_KEY.c_str()));

        QString hmacsignature_base64 = base64(hmacsignature, 32);

        headerValue = "amx " + API_KEY + ":" + hmacsignature_base64.toStdString().c_str() + ":" + to_string(nonce);

        req.setRawHeader("Authorization", headerValue.data());
        req.setRawHeader("Content-Type", "application/json");

        reply = mgr.post(req, body.toUtf8());
    }

    eventLoop.exec(); // blocks stack until "finished()" has been called

    if (!reply)
    {
        return "Error";
    }

    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" <<reply->errorString();
        Response = "Error";
    }
    reply->close();
    reply->deleteLater();

    return Response;
}

void tradingDialog::sendRequest1(QString url, std::function<void (void)> funcForCallAfterReceiveResponse, QString method, QString body){

    QString Response = "";
    QString Secret   = this->SecretKey;

    string signature;
    string hmacsignature;
    string headerValue;

    // the HTTP request
    QNetworkRequest req = QNetworkRequest(QUrl(url));

    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply *reply = NULL;
    //reply = qmanager->get(req);

    //printf("REPLY STARTED %s\n",url.toStdString().c_str());

    //funcForCallAfterReceiveResponse();



    if(method == "GET") {
        reply = qmanager->get(req);
    } else if(method == "POST") {

        string API_KEY = this->ApiKey.toStdString();
        string SECRET_KEY = this->SecretKey.toStdString();

        unsigned char digest[16];
        const char* string = body.toLatin1().data();

        MD5_CTX ctx;
        MD5_Init(&ctx);
        MD5_Update(&ctx, string, strlen(string));
        MD5_Final(digest, &ctx);

        char mdString[33];
        for (int i = 0; i < 16; i++)
            sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);

        timeval curTime;
        gettimeofday(&curTime, NULL);
        long nonce = curTime.tv_usec;

        char *requestContentBase64String = base64(digest, sizeof(digest));

        signature += API_KEY;
        signature += "POST";
        signature += url_encode(url.toStdString());
        signature += to_string(nonce);
        signature += requestContentBase64String;

        const unsigned char * hmacsignature = HMAC_SHA256_SIGNER(QString(signature.c_str()), QString(SECRET_KEY.c_str()));

        QString hmacsignature_base64 = base64(hmacsignature, 32);

        headerValue = "amx " + API_KEY + ":" + hmacsignature_base64.toStdString().c_str() + ":" + to_string(nonce);

        req.setRawHeader("Authorization", headerValue.data());
        req.setRawHeader("Content-Type", "application/json");

        reply = qmanager->post(req, body.toUtf8());
    }

    if (!reply) return;

    connect( reply, &QNetworkReply::finished, this, [reply, this, funcForCallAfterReceiveResponse](){
        funcForCallAfterReceiveResponse();
    });


}

void tradingDialog::slotReadyRead() {
    printf("slotReadyRead\n");
}

void tradingDialog::slotError1(QNetworkReply::NetworkError e) {
    printf("slotError\n");
}


void tradingDialog::showMarketHistoryWhenReplyFinished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        ParseAndPopulateMarketHistoryTable(Response);
    }
}

void tradingDialog::showBalanceOfLUXOnTradingTab() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        DisplayBalance(*ui->LUXAvailableLabel, Response);
    }
}

void tradingDialog::showBalanceOfBTCOnTradingTab() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        DisplayBalance(*ui->BtcAvailableLabel, Response);
    }
}

void tradingDialog::showOrderBookOnTradingTab() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        ParseAndPopulateOrderBookTables(Response);
    }
}

void tradingDialog::showMarketHistoryOnTradingTab() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        ParseAndPopulatePriceChart(Response);
    }
}

void tradingDialog::showBalanceOfLUXOnSendTab() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        DisplayBalance(*ui->CryptopiaLUXLabel, Response);
    }
}

void tradingDialog::showBalanceOfBTCOnSendTab() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        DisplayBalance(*ui->CryptopiaBTCLabel, Response);
    }
}

void tradingDialog::showBalanceOfLUXOnBalanceTab() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        DisplayBalance(*ui->LUXBalanceLabel,*ui->LUXAvailableLabel_2,*ui->LUXPendingLabel, QString::fromUtf8("LUX"),Response);
    }
}

void tradingDialog::showBalanceOfBTCOnBalanceTab() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        DisplayBalance(*ui->BitcoinBalanceLabel,*ui->BitcoinAvailableLabel,*ui->BitcoinPendingLabel, QString::fromUtf8("BTC"),Response);
    }
}

void tradingDialog::showOpenOrders() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        ParseAndPopulateOpenOrdersTable(Response);
    }
}

void tradingDialog::showAccountHistory() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    QString Response = "";
    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" << reply->errorString();
        Response = "Error";
    }

    reply->deleteLater();
    if(Response.size() > 0 && Response != "Error"){
        ParseAndPopulateAccountHistoryTable(Response);
    }
}

char * tradingDialog::base64(const unsigned char *input, int length)
{
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    (void)BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char *buff = (char *)malloc(bptr->length);
    memcpy(buff, bptr->data, bptr->length-1);
    buff[bptr->length-1] = 0;

    BIO_free_all(b64);

    return buff;
}


string tradingDialog::url_encode(const string &value) {
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        string::value_type c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << uppercase;
        escaped << '%' << setw(2) << int((unsigned char) c);
        escaped << nouppercase;
    }

    string escaped_str = escaped.str();
    //std::transform(escaped_str.begin(), escaped_str.end(), escaped_str.begin(), tolower);
    for(unsigned int i = 0; i < escaped_str.length(); ++i) {
        escaped_str[i] = tolower(escaped_str[i]);
    }

    return escaped_str;
}


QString tradingDialog::CryptopiaIntegerTimeStampToReadable(int DateTime){

    QDateTime dt;
    dt.setTime_t(DateTime);

    //Reconstruct time and date in our own format, one that QDateTime will recognise.
    QString DisplayDate = dt.toString("yyyy-MM-dd hh:mm:ss A");

    return DisplayDate;
}

QString tradingDialog::CryptopiaTimeStampToReadable(QString DateTime){
    //Seperate Time and date.
    int TPos = DateTime.indexOf("T");
    int sPos = DateTime.indexOf(".");
    QDateTime Date = QDateTime::fromString(DateTime.left(TPos),"yyyy-MM-dd"); //format to convert from
    DateTime.remove(sPos,sizeof(DateTime));
    DateTime.remove(0,TPos+1);
    QDateTime Time = QDateTime::fromString(DateTime.right(TPos),"hh:mm:ss");

    //Reconstruct time and date in our own format, one that QDateTime will recognise.
    QString DisplayDate = Date.toString("dd/MM/yyyy") + " " + Time.toString("hh:mm:ss A"); //formats to convert to

    return DisplayDate;
}

void tradingDialog::CalculateBuyCostLabel(){

    double price    = ui->BuyBidPriceEdit->text().toDouble();
    double Quantity = ui->UnitsInput->text().toDouble();
    double cost = ((price * Quantity) + ((price * Quantity / 100) * 0.25));

    QString Str = "";
    ui->BuyCostLabel->setText("<span style='font-weight:bold; font-size:12px; color:red'>" + Str.number(cost,'i',8) + "</span>");
}

void tradingDialog::CalculateSellCostLabel(){

    double price    = ui->SellBidPriceEdit->text().toDouble();
    double Quantity = ui->UnitsInputLUX->text().toDouble();
    double cost = ((price * Quantity) - ((price * Quantity / 100) * 0.25));

    QString Str = "";
    ui->SellCostLabel->setText("<span style='font-weight:bold; font-size:12px; color:green'>" + Str.number(cost,'i',8) + "</span>");
}

void tradingDialog::CalculateCSReceiveLabel(){

    //calculate amount of currency than can be transferred to bitcoin
    QString balance = GetBalance("LUX");
    QString buyorders = GetOrderBook();

    QJsonObject BuyObject = GetResultObjectFromJSONObject(buyorders);
    QJsonObject BalanceObject =  GetResultObjectFromJSONArray(balance);
    QJsonObject obj;

    QJsonDocument doc(BuyObject);
    QString param_str1(doc.toJson(QJsonDocument::Compact));

    QJsonDocument doc2(BalanceObject);
    QString param_str2(doc2.toJson(QJsonDocument::Compact));

    double AvailableLUX = BalanceObject["Available"].toDouble();
    double Quantity = ui->CSUnitsInput->text().toDouble();
    double Received = 0;
    double Qty = 0;
    double Price = 0;
    QJsonArray  BuyArray  = BuyObject.value("Buy").toArray();                //get buy/sell object from result object

    // For each buy order
    foreach (const QJsonValue & value, BuyArray)
    {
        obj = value.toObject();

        double x = obj["Price"].toDouble(); //would like to use int64 here
        double y = obj["Volume"].toDouble();
        // If
        if ( ((Quantity / x) - y) > 0 )
        {
            Price = x;
            Received += ((Price * y) - ((Price * y / 100) * 0.25));
            Qty += y;
            Quantity -= ((Price * y) - ((Price * y / 100) * 0.25));
        } else {
            Price = x;
            Received += ((Price * (Quantity / x)) - ((Price * (Quantity / x) / 100) * 0.25));
            Qty += (Quantity / x);
            Quantity -= 0;
            break;
        }
    }


    QString ReceiveStr = "";
    QString DumpStr = "";
    QString TotalStr = "";

    if ( Qty < AvailableLUX )
    {
        ui->CSReceiveLabel->setStyleSheet("font-weight:bold; font-size:12px; color:green");
        ui->CSDumpLabel->setStyleSheet("font-weight:bold; font-size:12px; color:red");
        ui->CSTotalLabel->setStyleSheet("font-weight:bold; font-size:12px; color:red");
        ui->CSReceiveLabel->setText(ReceiveStr.number((ui->CSUnitsInput->text().toDouble() - 0.0002),'i',8));
        ui->CSDumpLabel->setText(DumpStr.number(Price,'i',8));
        ui->CSTotalLabel->setText(TotalStr.number(Qty,'i',8));
    } else {
        ReceiveStr = "N/A";
        TotalStr = "N/A";
        DumpStr = "N/A";
        ui->CSReceiveLabel->setStyleSheet("font-weight:bold; font-size:12px; color:red");
        ui->CSDumpLabel->setStyleSheet("font-weight:bold; font-size:12px; color:red");
        ui->CSTotalLabel->setStyleSheet("font-weight:bold; font-size:12px; color:red");
        ui->CSReceiveLabel->setText(ReceiveStr);
        ui->CSDumpLabel->setText(DumpStr);
        ui->CSTotalLabel->setText(TotalStr);
    }
}

void tradingDialog::on_UpdateKeys_clicked(bool Save, bool Load)
{
    this->ApiKey    = ui->ApiKeyInput->text();
    this->SecretKey = ui->SecretKeyInput->text();


    QJsonDocument jsonResponse = QJsonDocument::fromJson(GetAccountHistory().toUtf8()); //get json from str.
    QJsonObject ResponseObject = jsonResponse.object();                                 //get json obj

    if ( ResponseObject.value("Success").toBool() == false){
        QMessageBox::information(this,"API Configuration Failed","Api configuration was unsuccesful.");

    }else if ( ResponseObject.value("Success").toBool() == true && Load){
        QMessageBox::information(this,"API Configuration Complete","Your API keys have been loaded and the connection has been successfully configured and tested.");
        ui->ApiKeyInput->setEchoMode(QLineEdit::Password);
        ui->SecretKeyInput->setEchoMode(QLineEdit::Password);
        ui->PasswordInput->setText("");
        ui->TradingTabWidget->setTabEnabled(0,true);
        ui->TradingTabWidget->setTabEnabled(1,true);
        ui->TradingTabWidget->setTabEnabled(3,true);
        ui->TradingTabWidget->setTabEnabled(4,true);
        ui->TradingTabWidget->setTabEnabled(5,true);
    }else if ( ResponseObject.value("Success").toBool() == true && Save){
        QMessageBox::information(this,"API Configuration Complete","Your API keys have been saved and the connection has been successfully configured and tested.");
        ui->ApiKeyInput->setEchoMode(QLineEdit::Password);
        ui->SecretKeyInput->setEchoMode(QLineEdit::Password);
        ui->PasswordInput->setText("");
        ui->TradingTabWidget->setTabEnabled(0,true);
        ui->TradingTabWidget->setTabEnabled(1,true);
        ui->TradingTabWidget->setTabEnabled(3,true);
        ui->TradingTabWidget->setTabEnabled(4,true);
        ui->TradingTabWidget->setTabEnabled(5,true);
    }else{
        QMessageBox::information(this,"API Configuration Complete","Api connection has been successfully configured and tested.");
        ui->ApiKeyInput->setEchoMode(QLineEdit::Password);
        ui->SecretKeyInput->setEchoMode(QLineEdit::Password);
        ui->PasswordInput->setText("");
        ui->TradingTabWidget->setTabEnabled(0,true);
        ui->TradingTabWidget->setTabEnabled(1,true);
        ui->TradingTabWidget->setTabEnabled(3,true);
        ui->TradingTabWidget->setTabEnabled(4,true);
        ui->TradingTabWidget->setTabEnabled(5,true);
    }

}

string tradingDialog::encryptDecrypt(string toEncrypt, string password) {

    char * key = new char [password.size()+1];
    std::strcpy (key, password.c_str());
    key[password.size()] = '\0'; // don't forget the terminating 0
    string output = toEncrypt;

    for (unsigned int i = 0; i < toEncrypt.size(); i++)
        output[i] = toEncrypt[i] ^ key[i % (strlen(key) / sizeof(char))];
    return output;
}



void tradingDialog::on_SaveKeys_clicked()
{
    bool fSuccess = true;
    boost::filesystem::path pathConfigFile = GetDataDir() / "APIcache.txt";
    boost::filesystem::ofstream stream (pathConfigFile.string(), ios::out | ios::trunc);

    // Qstring to string
    string password = ui->PasswordInput->text().toStdString();

    if (password.length() <= 6){
        QMessageBox::information(this,"Error !","Your password is too short !");
        fSuccess = false;
        stream.close();
    }

    // qstrings to utf8, add to byteArray and convert to const char for stream
    string Secret = ui->SecretKeyInput->text().toStdString();
    string Key = ui->ApiKeyInput->text().toStdString();
    string ESecret = "";
    string EKey = "";

    if (stream.is_open() && fSuccess)
    {
        ESecret = EncodeBase64(encryptDecrypt(Secret, password));
        EKey = EncodeBase64(encryptDecrypt(Key, password));
        stream << ESecret << '\n';
        stream << EKey;
        stream.close();
    }
    if (fSuccess) {
        bool Save = true;
        on_UpdateKeys_clicked(Save);
    }

}

void tradingDialog::on_LoadKeys_clicked()
{
    bool fSuccess = true;
    boost::filesystem::path pathConfigFile = GetDataDir() / "APIcache.txt";
    boost::filesystem::ifstream stream (pathConfigFile.string());

    // Qstring to string
    string password = ui->PasswordInput->text().toUtf8().constData();

    if (password.length() <= 6){
        QMessageBox::information(this,"Error !","Your password is too short !");
        fSuccess = false;
        stream.close();
    }

    QString DSecret = "";
    QString DKey = "";

    if (stream.is_open() && fSuccess)
    {
        int i =0;
        for ( std::string line; std::getline(stream,line); )
        {
            if (i == 0 ){
                DSecret = QString::fromStdString(encryptDecrypt(DecodeBase64(line), password).c_str());
                ui->SecretKeyInput->setText(DSecret);
            } else if (i == 1){
                DKey = QString::fromStdString(encryptDecrypt(DecodeBase64(line), password).c_str());
                ui->ApiKeyInput->setText(DKey);
            }
            i++;
        }
        stream.close();
    }
    if (fSuccess) {
        bool Save = false;
        bool Load = true;
        on_UpdateKeys_clicked(Save, Load);
    }

}

void tradingDialog::on_GenDepositBTN_clicked()
{
    QString response         =  GetDepositAddress();
    QJsonObject ResultObject =  GetResultObjectFromJSONObject(response);
    ui->DepositAddressLabel->setText(ResultObject["Address"].toString());
}

void tradingDialog::on_Sell_Max_Amount_clicked()
{
    //calculate amount of BTC that can be gained from selling LUX available balance
    QString responseA = GetBalance("LUX");
    QString str;
    QJsonObject ResultObject =  GetResultObjectFromJSONArray(responseA);

    double AvailableLUX = ResultObject["Available"].toDouble();

    ui->UnitsInputLUX->setText(str.number(AvailableLUX,'i',8));
}

void tradingDialog::on_Buy_Max_Amount_clicked()
{
    //calculate amount of currency than can be brought with the BTC balance available
    QString responseA = GetBalance("BTC");
    QString responseB = GetMarketSummary();
    QString str;

    QJsonObject ResultObject =  GetResultObjectFromJSONArray(responseA);
    QJsonObject ResultObj    =  GetResultObjectFromJSONObject(responseB);

    //Get the Bid ask or last value from combo
    //QString value = ui->BuyBidcomboBox->currentText();

    double AvailableBTC = ResultObject["Available"].toDouble();
    double CurrentASK   = ResultObj["AskPrice"].toDouble();
    double Result = (AvailableBTC / CurrentASK);
    double percentofnumber = (Result * 0.0025);

    Result = Result - percentofnumber;
    ui->UnitsInput->setText(str.number(Result,'i',8));
}

void tradingDialog::on_CS_Max_Amount_clicked()
{
    double Quantity = ui->CryptopiaLUXLabel->text().toDouble();
    double Received = 0;
    double Qty = 0;
    double Price = 0;
    QString buyorders = GetOrderBook();
    QJsonObject BuyObject = GetResultObjectFromJSONObject(buyorders);
    QJsonObject obj;
    QString str;

    QJsonArray  BuyArray  = BuyObject.value("Buy").toArray();                //get buy/sell object from result object



    // For each buy order
    foreach (const QJsonValue & value, BuyArray)
    {
        obj = value.toObject();

        double x = obj["Price"].toDouble(); //would like to use int64 here
        double y = obj["Volume"].toDouble();


        if ( (Quantity - y) > 0 )
        {
            Price = x;
            Received += ((Price * y) - ((Price * y / 100) * 0.25));
            Qty += y;
            Quantity -= y;

        } else {
            Price = x;
            Received += ((Price * Quantity) - ((Price * Quantity / 100) * 0.25));
            Qty += Quantity;

            if ((Quantity * x) < 0.00055){
                Quantity = (0.00055 / x);
            }
            break;
        }
    }

    ui->CSUnitsInput->setText(str.number(Received,'i',8));
}

void tradingDialog::on_Withdraw_Max_Amount_clicked()
{
    //calculate amount of currency than can be brought with the BTC balance available
    QString responseA = GetBalance("LUX");
    QString str;

    QJsonObject ResultObject =  GetResultObjectFromJSONArray(responseA);

    double AvailableLUX = ResultObject["Available"].toDouble();

    ui->WithdrawUnitsInput->setText(str.number(AvailableLUX,'i',8));
}

QJsonObject tradingDialog::GetResultObjectFromJSONObject(QString response){

    QJsonDocument jsonResponse = QJsonDocument::fromJson(response.toUtf8());          //get json from str.
    QJsonObject  ResponseObject = jsonResponse.object();                              //get json obj
    QJsonObject  ResultObject   = ResponseObject.value(QString("Data")).toObject(); //get result object

    return ResultObject;
}

QJsonObject tradingDialog::GetResultObjectFromJSONArray(QString response){

    QJsonDocument jsonResponsea = QJsonDocument::fromJson(response.toUtf8());
    QJsonObject   jsonObjecta   = jsonResponsea.object();
    QJsonArray    jsonArraya    = jsonObjecta["Data"].toArray();
    QJsonObject   obj;

    foreach (const QJsonValue & value, jsonArraya)
    {
        obj = value.toObject();
    }

    return obj;
}

QJsonArray tradingDialog::GetResultArrayFromJSONObject(QString response){

    QJsonDocument jsonResponse = QJsonDocument::fromJson(response.toUtf8());
    QJsonObject   jsonObject   = jsonResponse.object();
    QJsonArray    jsonArray    = jsonObject["Data"].toArray();

    return jsonArray;
}

unsigned char* tradingDialog::HMAC_SHA256_SIGNER(QString UrlToSign, QString Secret){

    QString retval = "";

    QByteArray byteArray = UrlToSign.toUtf8();
    const char* URL = byteArray.constData();

    const EVP_MD *md = EVP_sha256();
    unsigned char* digest = NULL;

    // Using sha512 hash engine here.
    digest = HMAC(md, (const void *)DecodeBase64(string(Secret.toStdString())).c_str(), strlen( DecodeBase64(string(Secret.toStdString())).c_str()), (unsigned char*) URL, strlen( URL), NULL, NULL);

    // Be careful of the length of string with the choosen hash engine. SHA1 produces a 20-byte hash value which rendered as 40 characters.
    // Change the length accordingly with your choosen hash engine
    char mdString[65] = { 0 };

    for(int i = 0; i < 32; i++){
        sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);
    }
    retval = mdString;

    return digest;
}

void tradingDialog::on_SellBidcomboBox_currentIndexChanged(const QString &arg1)
{
    QString response = GetMarketSummary();
    QJsonObject ResultObject = GetResultObjectFromJSONArray(response);
    QString Str;

    //Get the Bid ask or last value from combo
    ui->SellBidPriceEdit->setText(Str.number(ResultObject[arg1].toDouble(),'i',8));

    CalculateSellCostLabel(); //update cost
}

void tradingDialog::on_BuyBidcomboBox_currentIndexChanged(const QString &arg1)
{
    QString response = GetMarketSummary();
    QJsonObject ResultObject = GetResultObjectFromJSONArray(response);
    QString Str;

    QJsonDocument doc(ResultObject);
    QString param_str(doc.toJson(QJsonDocument::Compact));

    //Get the Bid ask or last value from combo
    ui->BuyBidPriceEdit->setText(Str.number(ResultObject[arg1].toDouble(),'i',8));

    CalculateBuyCostLabel(); //update cost
}

void tradingDialog::on_BuyLUX_clicked()
{
    double Rate;
    double Quantity;

    Rate     = ui->BuyBidPriceEdit->text().toDouble();
    Quantity = ui->UnitsInput->text().toDouble();

    QString OrderType = "Limit";
    QString Order;

    if(OrderType == "Limit"){Order = "buylimit";}else if (OrderType == "Market"){ Order = "buymarket";}

    QString Msg = "Are you sure you want to buy ";
    Msg += ui->UnitsInput->text();
    Msg += "LUX @ ";
    Msg += ui->BuyBidPriceEdit->text();
    Msg += " BTC Each";

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this,"Buy Order",Msg,QMessageBox::Yes|QMessageBox::No);

    if (reply == QMessageBox::Yes) {

        QString Response =  BuyLUX(Order,Quantity,Rate);

        QJsonDocument jsonResponse = QJsonDocument::fromJson(Response.toUtf8());          //get json from str.
        QJsonObject  ResponseObject = jsonResponse.object();                              //get json obj

        if (ResponseObject["Success"].toBool() == false){
            QMessageBox::information(this,"Buy Order Failed",ResponseObject["Error"].toString());

        }else if (ResponseObject["Success"].toBool() == true){
            QMessageBox::information(this,"Buy Order Initiated","You Placed an order");
        }
    }else{
        //do nothing
    }
}

void tradingDialog::on_SellLUXBTN_clicked()
{
    double Rate;
    double Quantity;

    Rate     = ui->SellBidPriceEdit->text().toDouble();
    Quantity = ui->UnitsInputLUX->text().toDouble();

    QString OrderType = "Limit";
    QString Order;

    if(OrderType == "Limit"){Order = "selllimit";}else if (OrderType == "Market"){ Order = "sellmarket";}

    QString Msg = "Are you sure you want to Sell ";
    Msg += ui->UnitsInputLUX->text();
    Msg += " LUX @ ";
    Msg += ui->SellBidPriceEdit->text();
    Msg += " BTC Each";

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this,"Sell Order",Msg,QMessageBox::Yes|QMessageBox::No);

    if (reply == QMessageBox::Yes) {

        QString Response =  SellLUX(Order,Quantity,Rate);
        QJsonDocument jsonResponse = QJsonDocument::fromJson(Response.toUtf8());          //get json from str.
        QJsonObject  ResponseObject = jsonResponse.object();                              //get json obj

        if (ResponseObject["Success"].toBool() == false){
            QMessageBox::information(this,"Sell Order Failed",ResponseObject["Error"].toString());

        }else if (ResponseObject["Success"].toBool() == true){
            QMessageBox::information(this,"Sell Order Initiated","You Placed an order");
        }
    }else{
        //do nothing
    }
}

void tradingDialog::on_CSUnitsBtn_clicked()
{
    double Quantity = ui->CSUnitsInput->text().toDouble();
    double Rate = ui->CSDumpLabel->text().toDouble();
    double Received = 0;
    double Qty = 0;
    double Price = 0;
    double Add = 0;

    QString buyorders = GetOrderBook();
    QJsonObject BuyObject = GetResultObjectFromJSONObject(buyorders);
    QJsonObject obj;
    QString Astr;
    QString Qstr;
    QString Rstr;
    QString Coin = "BTC";
    QString Msg = "Are you sure you want to Send ";
    Msg += Qstr.number((Quantity - 0.0002),'i',8);
    Msg += " BTC to ";
    Msg += ui->CSUnitsAddress->text()+"/// "+ui->CSDumpLabel->text()+" ///";
    Msg += ", DUMPING your coins at ";
    Msg += Rstr.number(Rate,'i',8);
    Msg += " satoshis ?";

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this,"Cross-Send",Msg,QMessageBox::Yes|QMessageBox::No);

    if(reply != QMessageBox::Yes)
    {
        return;
    }

    /*WalletModel::UnlockContext ctx(model->requestUnlock()); //TODO: some magic here -- temporarily commented
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        return;
    }*/

    QString Order = "selllimit";
    QJsonArray  BuyArray  = BuyObject.value("Buy").toArray();                //get buy/sell object from result object

    // For each buy order
    foreach (const QJsonValue & value, BuyArray)
    {
        obj = value.toObject();

        double x = obj["Price"].toDouble(); //would like to use int64 here
        double y = obj["Volume"].toDouble();
        // If
        if ( ((Quantity / x) - y) > 0 )
        {
            Price = x;
            Received += ((Price * y) - ((Price * y / 100) * 0.25));
            Qty += y;
            Quantity -= ((Price * y) - ((Price * y / 100) * 0.25));

            QString SellResponse = SellLUX(Order,y,x);
            QJsonDocument SelljsonResponse = QJsonDocument::fromJson(SellResponse.toUtf8());          //get json from str.
            QJsonObject SellResponseObject = SelljsonResponse.object();                              //get json obj

            if (SellResponseObject["Success"].toBool() == false){
                if (SellResponseObject["Error"].toString() == "DUST_TRADE_DISALLOWED_MIN_VALUE_50K_SAT"){
                    Add += y;
                    continue;
                }
                QMessageBox::information(this,"sFailed",SellResponse);
                break;
            }
            MilliSleep(100);

        } else {
            Price = x;
            Received += ((Price * (Quantity / x)) - ((Price * (Quantity / x) / 100) * 0.25));
            Qty += (Quantity / x);
            if (Add > 0)
                Quantity += (Add * x);
            if (Quantity < 0.00051){
                Quantity = 0.00051;
            }
            QString SellResponse = SellLUX(Order,(Quantity / x),x);
            QJsonDocument SelljsonResponse = QJsonDocument::fromJson(SellResponse.toUtf8());          //get json from str.
            QJsonObject SellResponseObject = SelljsonResponse.object();                              //get json obj

            if (SellResponseObject["Success"].toBool() == false){
                QMessageBox::information(this,"sFailed",SellResponse);

            } else if (SellResponseObject["Success"].toBool() == true){
                MilliSleep(5000);
                QString Response = Withdraw(ui->CSUnitsInput->text().toDouble(),ui->CSUnitsAddress->text(),Coin);
                QJsonDocument jsonResponse = QJsonDocument::fromJson(Response.toUtf8());          //get json from str.
                QJsonObject ResponseObject = jsonResponse.object();                              //get json obj

                if (ResponseObject["Success"].toBool() == false){
                    MilliSleep(5000);
                    QString Response = Withdraw(ui->CSUnitsInput->text().toDouble(),ui->CSUnitsAddress->text(),Coin);
                    QJsonDocument jsonResponse = QJsonDocument::fromJson(Response.toUtf8());          //get json from str.
                    QJsonObject ResponseObject = jsonResponse.object();

                    if (ResponseObject["Success"].toBool() == false){
                        QMessageBox::information(this,"Failed",ResponseObject["Error"].toString());
                    } else if (ResponseObject["Success"].toBool() == true){
                        QMessageBox::information(this,"Success","<center>Cross-Send Successful</center>\n Sold "+Astr.number(Qty,'i',4)+" LUX for "+Qstr.number((ui->CSUnitsInput->text().toDouble()-0.0002),'i',8)+" BTC");
                    }
                } else if (ResponseObject["Success"].toBool() == true){
                    QMessageBox::information(this,"Success","<center>Cross-Send Successful</center>\n Sold "+Astr.number(Qty,'i',4)+" LUX for "+Qstr.number((ui->CSUnitsInput->text().toDouble()-0.0002),'i',8)+" BTC");
                }
            }
            break;
        }
    }
}

void tradingDialog::on_WithdrawUnitsBtn_clicked()
{
    double Quantity = ui->WithdrawUnitsInput->text().toDouble();
    QString Qstr;
    QString Coin = "LUX";
    QString Msg = "Are you sure you want to Withdraw ";
    Msg += Qstr.number((Quantity - 0.02),'i',8);
    Msg += " LUX to ";
    Msg += ui->WithdrawAddress->text();
    Msg += " ?";

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this,"Withdraw",Msg,QMessageBox::Yes|QMessageBox::No);

    if(reply != QMessageBox::Yes)
    {
        return;
    }

    /*WalletModel::UnlockContext ctx(model->requestUnlock());  // TODO: it's magic-check in future
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        return;
    }*/

    QString Response =  Withdraw(Quantity, ui->WithdrawAddress->text(), Coin);
    QJsonDocument jsonResponse = QJsonDocument::fromJson(Response.toUtf8());          //get json from str.
    QJsonObject  ResponseObject = jsonResponse.object();                              //get json obj

    if (ResponseObject["Success"].toBool() == false){
        QMessageBox::information(this,"Failed",ResponseObject["Error"].toString());

    }else if (ResponseObject["Success"].toBool() == true){
        QMessageBox::information(this,"Success","Withdrawal Successful !");
    }
}

void tradingDialog::on_UnitsInputLUX_textChanged(const QString &arg1)
{
    CalculateSellCostLabel(); //update cost
}

void tradingDialog::on_UnitsInput_textChanged(const QString &arg1)
{
    CalculateBuyCostLabel(); //update cost
}

void tradingDialog::on_BuyBidPriceEdit_textChanged(const QString &arg1)
{
    CalculateBuyCostLabel(); //update cost
}

void tradingDialog::on_SellBidPriceEdit_textChanged(const QString &arg1)
{
    CalculateSellCostLabel();
}

void tradingDialog::on_CSUnitsInput_textChanged(const QString &arg1)
{
    CalculateCSReceiveLabel(); //update cost
}

void tradingDialog::on_CSPasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->CSUnitsAddress->setText(QApplication::clipboard()->text());
}

void tradingDialog::on_WithdrawPasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->WithdrawAddress->setText(QApplication::clipboard()->text());
}

void tradingDialog::on_SecretPasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->SecretKeyInput->setText(QApplication::clipboard()->text());
}

void tradingDialog::on_KeyPasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->ApiKeyInput->setText(QApplication::clipboard()->text());
}

void setClipboard(const QString& str)
{
    QApplication::clipboard()->setText(str, QClipboard::Clipboard);
    QApplication::clipboard()->setText(str, QClipboard::Selection);
}

void tradingDialog::on_DepositCopyButton_clicked()
{
    setClipboard(ui->DepositAddressLabel->text());
}

void tradingDialog::setModel(WalletModel *model)
{
    this->model = model;
}

tradingDialog::~tradingDialog()
{
    delete ui;
}
