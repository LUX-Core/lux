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
    /*ui->TradingTabWidget->setTabEnabled(0,false);
    ui->TradingTabWidget->setTabEnabled(1,false);
    ui->TradingTabWidget->setTabEnabled(3,false);
    ui->TradingTabWidget->setTabEnabled(4,false);
    ui->TradingTabWidget->setTabEnabled(5,false);*/

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
    ui->TradeHistoryTable->setColumnCount(9);
    ui->TradeHistoryTable->verticalHeader()->setVisible(false);
    ui->TradeHistoryTable->setHorizontalHeaderLabels(QStringList() << "Date Time" << "Exchange" << "OrderType"  << "Limit" << "QTY" << "QTY_Rem" << "Price" << "PricePerUnit" << "Closed");
    ui->TradeHistoryTable->setRowCount(0);
    Cellwidth =  ui->TradeHistoryTable->width() / 9;
    ui->TradeHistoryTable->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(1,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(2,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(3,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(4,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(5,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(6,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(7,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(8,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->resizeSection(9,Cellwidth);
    ui->TradeHistoryTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->TradeHistoryTable->horizontalHeader()->setStyleSheet("QHeaderView::section, QHeaderView::section * {font-weight :bold;}");
    /*Account History Table Init*/

    /*Open Orders Table*/
    ui->OpenOrdersTable->setColumnCount(10);
    ui->OpenOrdersTable->verticalHeader()->setVisible(false);
    ui->OpenOrdersTable->setHorizontalHeaderLabels(QStringList() << "OrderId" << "Date Time" << "Exchange" << "OrderType"  << "Limit" << "QTY" << "QTY_Rem" << "Price" << "PricePerUnit" << "Cancel Order");
    ui->OpenOrdersTable->setRowCount(0);
    Cellwidth =  ui->TradeHistoryTable->width() / 9;
    ui->OpenOrdersTable->horizontalHeader()->setResizeMode(QHeaderView::Stretch);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(2,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(3,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(4,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(5,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(6,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(7,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(8,Cellwidth);
    ui->OpenOrdersTable->horizontalHeader()->resizeSection(9,Cellwidth);
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

QString tradingDialog::GetMarketHistory(){
      //QString Response = sendRequest(QString("https://jsonplaceholder.typicode.com/posts/1/"));
    QString Response = sendRequest(QString("https://www.cryptopia.co.nz/api/GetMarketHistory/LUX_BTC"));
      return Response;
}

QString tradingDialog::CancelOrder(QString OrderId){

        QString URL = "https://www.cryptopia.co.nz/api/CancelTrade";
                URL += this->ApiKey;
                URL += "&nonce=12345434&uuid=";
                URL += OrderId;

        QString Response = sendRequest(URL);
        return Response;
}

QString tradingDialog::BuyLUX(QString OrderType, double Quantity, double Rate){

    QString str = "";
    QString URL = "https://www.cryptopia.co.nz";
            URL += OrderType;
            URL += "/api/SubmitTrade";
            URL += this->ApiKey;
            URL += "&nonce=12345434&market=BTC-LUX&quantity=";
            URL += str.number(Quantity,'i',8);
            URL += "&rate=";
            URL += str.number(Rate,'i',8);

    QString Response = sendRequest(URL);
    return Response;
}

QString tradingDialog::SellLUX(QString OrderType, double Quantity, double Rate){

    QString str = "";
    QString URL = "https://www.cryptopia.co.nz";
            URL += OrderType;
            URL += "/api/GetMarket/LUX_BTC";
            URL += this->ApiKey;
            URL += "&nonce=12345434&market=BTC-LUX&quantity=";
            URL += str.number(Quantity,'i',8);
            URL += "&rate=";
            URL += str.number(Rate,'i',8);

    QString Response = sendRequest(URL);
    return Response;
}

QString tradingDialog::Withdraw(double Amount, QString Address, QString Coin){

    QString str = "";
    QString URL = "https://www.cryptopia.co.nz";
            URL += this->ApiKey;
            URL += "/api/SubmitWithdraw";
            URL += Coin;
            URL += "&quantity=";
            URL += str.number(Amount,'i',8);
            URL += "&address=";
            URL += Address;
            URL += "&nonce=12345434";

    QString Response = sendRequest(URL);
     return Response;
}

QString tradingDialog::GetOpenOrders(){
    QString URL = "https://www.cryptopia.co.nz/api/GetOpenOrders";
            //URL += this->ApiKey;
            //URL += "&nonce=12345434&market=BTC-LUX";

    QString Response = sendRequest(URL, "POST");
    //LogPrintf("RESPPONDSE!!!!!! ", Response.toStdString().c_str());
    LogPrintf("REFFFFFFFFFFFFFF!!!!!!!!!!!!!!! %s \n",Response.toStdString().c_str());
    return Response;
}

QString tradingDialog::GetBalance(QString Currency){

    QString URL = "https://www.cryptopia.co.nz/api/GetBalance";
            //URL += this->ApiKey;
            //URL += "&nonce=12345434&currency=";
            //URL += Currency;

    QString Response = sendRequest(URL, "POST", QString("{\"Currency\":\"LUX\"}"));
     return Response;
}

QString tradingDialog::GetDepositAddress(){

    QString URL = "https://www.cryptopia.co.nz/api/GetDepositAddress";
            URL += this->ApiKey;
            URL += "&nonce=12345434&currency=LUX";

    QString Response = sendRequest(URL);
    return Response;
}

QString tradingDialog::GetAccountHistory(){

    QString URL = "https://www.cryptopia.co.nz/api/GetTradeHistory";
           // URL += this->ApiKey;
           // URL += "&nonce=12345434&market=BTC-LUX&count=10";

    QString Response = sendRequest(URL, "POST");
    return Response;
}

int tradingDialog::SetExchangeInfoTextLabels(){
    //Get the current exchange information + information for the current open tab if required.
    QString str = "";
    QString Response = GetMarketSummary();

    //Set the labels, parse the json result to get values.
    QJsonObject obj = GetResultObjectFromJSONArray(Response);

    //set labels to richtext to use css.
    ui->Bid->setTextFormat(Qt::RichText);
    ui->Ask->setTextFormat(Qt::RichText);
    ui->volumet->setTextFormat(Qt::RichText);
    ui->volumebtc->setTextFormat(Qt::RichText);

    ui->Ask->setText("<b>Ask:</b> <span style='font-weight:bold; font-size:12px; color:Red'>" + str.number(obj["Ask"].toDouble(),'i',8) + "</span> BTC");

    ui->Bid->setText("<b>Bid:</b> <span style='font-weight:bold; font-size:12px; color:Green;'>" + str.number(obj["Bid"].toDouble(),'i',8) + "</span> BTC");

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

    Table.horizontalHeader()->setResizeMode(QHeaderView::Stretch);
    Table.horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    Table.horizontalHeader()->setStyleSheet("QHeaderView::section, QHeaderView::section * { font-weight :bold;}");
}

void tradingDialog::DisplayBalance(QLabel &BalanceLabel,QLabel &Available, QLabel &Pending, QString Currency,QString Response){

    QString str;

    BalanceLabel.setTextFormat(Qt::RichText);
    Available.setTextFormat(Qt::RichText);
    Pending.setTextFormat(Qt::RichText);

    //Set the labels, parse the json result to get values.
    QJsonObject ResultObject = GetResultObjectFromJSONObject(Response);

    BalanceLabel.setText("<span style='font-weight:bold; font-size:11px; color:green'>" + str.number( ResultObject["Balance"].toDouble(),'i',8) + "</span> " + Currency);
    Available.setText("<span style='font-weight:bold; font-size:11px; color:green'>" + str.number( ResultObject["Available"].toDouble(),'i',8) + "</span> " +Currency);
    Pending.setText("<span style='font-weight:bold; font-size:11px; color:green'>" + str.number( ResultObject["Pending"].toDouble(),'i',8) + "</span> " +Currency);
}

void tradingDialog::DisplayBalance(QLabel &BalanceLabel, QString Response){

    QString str;

    //Set the labels, parse the json result to get values.
    QJsonObject ResultObject = GetResultObjectFromJSONObject(Response);

    BalanceLabel.setText("<span style='font-weight:bold; font-size:12px; color:green'>" + str.number(ResultObject["Available"].toDouble(),'i',8) + "</span>");
}

void tradingDialog::DisplayBalance(QLabel &BalanceLabel, QLabel &BalanceLabel2, QString Response, QString Response2){

    QString str;
    QString str2;

    //Set the labels, parse the json result to get values.
    QJsonObject ResultObject = GetResultObjectFromJSONObject(Response);
    QJsonObject ResultObject2 = GetResultObjectFromJSONObject(Response2);

    BalanceLabel.setText("<span style='font-weight:bold; font-size:12px; color:green'>" + str.number(ResultObject["Available"].toDouble(),'i',8) + "</span>");
    BalanceLabel2.setText("<span style='font-weight:bold; font-size:12px; color:green'>" + str2.number(ResultObject2["Available"].toDouble(),'i',8) + "</span>");
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
            ui->OpenOrdersTable->setItem(itteration, 0, new QTableWidgetItem(obj["OrderUuid"].toString()));
            ui->OpenOrdersTable->setItem(itteration, 1, new QTableWidgetItem(CryptopiaTimeStampToReadable(obj["Opened"].toString())));
            ui->OpenOrdersTable->setItem(itteration, 2, new QTableWidgetItem(obj["Exchange"].toString()));
            ui->OpenOrdersTable->setItem(itteration, 3, new QTableWidgetItem(obj["OrderType"].toString()));
            ui->OpenOrdersTable->setItem(itteration, 4, new QTableWidgetItem(str.number(obj["Limit"].toDouble(),'i',8)));
            ui->OpenOrdersTable->setItem(itteration, 5, new QTableWidgetItem(str.number(obj["Quantity"].toDouble(),'i',8)));
            ui->OpenOrdersTable->setItem(itteration, 6, new QTableWidgetItem(str.number(obj["QuantityRemaining"].toDouble(),'i',8)));
            ui->OpenOrdersTable->setItem(itteration, 7, new QTableWidgetItem(str.number(obj["Price"].toDouble(),'i',8)));
            ui->OpenOrdersTable->setItem(itteration, 8, new QTableWidgetItem(str.number(obj["PricePerUnit"].toDouble(),'i',8)));
            ui->OpenOrdersTable->setItem(itteration, 9, new QTableWidgetItem(tr("Cancel Order")));

            //Handle the cancel link in open orders table
            QTableWidgetItem* CancelCell;
            CancelCell= ui->OpenOrdersTable->item(itteration, 9);    //Set the wtablewidget item to the cancel cell item.
            CancelCell->setForeground(QColor::fromRgb(255,0,0));      //make this item red.
            CancelCell->setTextAlignment(Qt::AlignCenter);
            itteration++;
        }
        obj.empty();
}


void tradingDialog::CancelOrderSlot(int row, int col){

   QString OrderId = ui->OpenOrdersTable->model()->data(ui->OpenOrdersTable->model()->index(row,0)).toString();
   QMessageBox::StandardButton reply;
   reply = QMessageBox::question(this,"Cancel Order","Are you sure you want to cancel the order?",QMessageBox::Yes|QMessageBox::No);

   if (reply == QMessageBox::Yes) {

          QString Response = CancelOrder(OrderId);

          QJsonDocument jsonResponse = QJsonDocument::fromJson(Response.toUtf8());
          QJsonObject ResponseObject = jsonResponse.object();

              if (ResponseObject["success"].toBool() == false){

                            QMessageBox::information(this,"Failed To Cancel Order",ResponseObject["message"].toString());

                  }else if (ResponseObject["success"].toBool() == true){
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
            ui->TradeHistoryTable->setItem(itteration, 1, new QTableWidgetItem(obj["Exchange"].toString()));
            ui->TradeHistoryTable->setItem(itteration, 2, new QTableWidgetItem(obj["OrderType"].toString()));
            ui->TradeHistoryTable->setItem(itteration, 3, new QTableWidgetItem(str.number(obj["Limit"].toDouble(),'i',8)));
            ui->TradeHistoryTable->setItem(itteration, 4, new QTableWidgetItem(str.number(obj["Quantity"].toDouble(),'i',8)));
            ui->TradeHistoryTable->setItem(itteration, 5, new QTableWidgetItem(str.number(obj["QuantityRemaining"].toDouble(),'i',8)));
            ui->TradeHistoryTable->setItem(itteration, 6, new QTableWidgetItem(str.number(obj["Price"].toDouble(),'i',8)));
            ui->TradeHistoryTable->setItem(itteration, 7, new QTableWidgetItem(str.number(obj["PricePerUnit"].toDouble(),'i',8)));
            ui->TradeHistoryTable->setItem(itteration, 8, new QTableWidgetItem(obj["Closed"].toString()));
            itteration++;
        }

        obj.empty();
}


void tradingDialog::ParseAndPopulateOrderBookTables(QString OrderBook){

    QString str;
    QJsonObject obj;
    QJsonObject ResultObject = GetResultObjectFromJSONObject(OrderBook);

    int BuyItteration = 0,SellItteration = 0, BidRows = 0, AskRows = 0;

    QJsonArray  BuyArray  = ResultObject.value("buy").toArray();                //get buy/sell object from result object
    QJsonArray  SellArray = ResultObject.value("sell").toArray();               //get buy/sell object from result object

    double LUXSupply = 0;
    double LUXDemand = 0;
    double BtcSupply = 0;
    double BtcDemand = 0;

    ui->AsksTable->setRowCount(0);

    foreach (const QJsonValue & value, SellArray)
    {
        obj = value.toObject();

        double x = obj["Rate"].toDouble(); //would like to use int64 here
        double y = obj["Quantity"].toDouble();
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

        double x = obj["Rate"].toDouble(); //would like to use int64 here
        double y = obj["Quantity"].toDouble();
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

void tradingDialog::ActionsOnSwitch(int index = -1){

    QString Response = "";
    QString Response2 = "";
    QString Response3 = "";

    if(index == -1){
       index = ui->TradingTabWidget->currentIndex();
    }

    switch (index){
                case 0:    //buy tab is active

                    Response = GetBalance("BTC");
                    Response2 = GetBalance("LUX");
                    Response3 = GetOrderBook();

                    if((Response.size() > 0 && Response != "Error") && (Response2.size() > 0 && Response2 != "Error")){
                        DisplayBalance(*ui->BtcAvailableLabel, *ui->LUXAvailableLabel, Response, Response2);
                    }
                    if ((Response3.size() > 0 && Response3 != "Error")) {
                        ParseAndPopulateOrderBookTables(Response3);
                    }

                break;

                case 1: //Cross send tab active
                    Response = GetBalance("LUX");
                    Response2 = GetBalance("BTC");
                    if((Response.size() > 0 && Response != "Error") && (Response2.size() > 0 && Response2 != "Error")){
                        DisplayBalance(*ui->CryptopiaLUXLabel, *ui->CryptopiaBTCLabel, Response, Response2);
                    }

                break;

                case 2://market history tab
                    Response = GetMarketHistory();
                    if(Response.size() > 0 && Response != "Error"){
                        ParseAndPopulateMarketHistoryTable(Response);
                    }
                break;

                case 3: //open orders tab
                    Response = GetOpenOrders();
                    if(Response.size() > 0 && Response != "Error"){
                        ParseAndPopulateOpenOrdersTable(Response);
                    }

                break;

                case 4://account history tab
                    Response = GetAccountHistory();
                    if(Response.size() > 0 && Response != "Error"){
                        ParseAndPopulateAccountHistoryTable(Response);
                    }
                break;

                case 5://show balance tab
                    Response = GetBalance("BTC");
                    if(Response.size() > 0 && Response != "Error"){
                        DisplayBalance(*ui->BitcoinBalanceLabel,*ui->BitcoinAvailableLabel,*ui->BitcoinPendingLabel, QString::fromUtf8("BTC"),Response);
                    }

                    Response = GetBalance("LUX");
                    if(Response.size() > 0 && Response != "Error"){
                        DisplayBalance(*ui->LUXBalanceLabel,*ui->LUXAvailableLabel_2,*ui->LUXPendingLabel, QString::fromUtf8("LUX"),Response);
                    }
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

    QNetworkReply *reply;
    if(method == "GET") {
        reply = mgr.get(req);
    } else if(method == "POST") {

        string API_KEY = this->ApiKey.toStdString();//"0449b9396e5e46da86f762562684c8c5";
        string SECRET_KEY = this->SecretKey.toStdString();//"h8K8zJYa0hVPyrOJL/O8Uox7DQW3ybdL46qVO/zNK/s=";

        unsigned char digest[16];
        const char* string = body.toLatin1().data();

        MD5_CTX ctx;
        MD5_Init(&ctx);
        MD5_Update(&ctx, string, strlen(string));
        MD5_Final(digest, &ctx);

        char mdString[33];
        for (int i = 0; i < 16; i++)
            sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);

        time_t nonce = std::time(0);

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

        LogPrintf("MY HEADER -- %s\n", req.rawHeader("Authorization").toStdString().c_str());

        LogPrintf("my str: %s %s %d -> %s %s -lala %s =baba %s --hjhj %s ## %s ^^--\n\n %s \n\n--^^\n\n", string, body.toStdString().c_str(), strlen(string), mdString, digest, requestContentBase64String, signature, hmacsignature, hmacsignature_base64.toStdString().c_str(), DecodeBase64(SECRET_KEY));

        reply = mgr.post(req, body.toUtf8());
    }

    eventLoop.exec(); // blocks stack until "finished()" has been called

    if (reply->error() == QNetworkReply::NoError) {
        //success
        Response = reply->readAll();

    }
    else{
        //failure
        qDebug() << "Failure" <<reply->errorString();
        Response = "Error";
      //QMessageBox::information(this,"Error",reply->errorString());
        }
    reply->close();
    reply->deleteLater();

     return Response;
}


static const char reverse_table[128] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
};

::std::string base64_decode(const ::std::string &ascdata)
{
    using ::std::string;
    string retval;
    const string::const_iterator last = ascdata.end();
    int bits_collected = 0;
    unsigned int accumulator = 0;

    for (string::const_iterator i = ascdata.begin(); i != last; ++i) {
        const int c = *i;
        if (::std::isspace(c) || c == '=') {
            // Skip whitespace and padding. Be liberal in what you accept.
            continue;
        }
        if ((c > 127) || (c < 0) || (reverse_table[c] > 63)) {
            throw ::std::invalid_argument("This contains characters not legal in a base64 encoded string.");
        }
        accumulator = (accumulator << 6) | reverse_table[c];
        bits_collected += 6;
        if (bits_collected >= 8) {
            bits_collected -= 8;
            retval += static_cast<char>((accumulator >> bits_collected) & 0xffu);
        }
    }
    return retval;
}


void tradingDialog::hmac_sha256(
        const unsigned char *text,      /* pointer to data stream        */
        int                 text_len,   /* length of data stream         */
        const unsigned char *key,       /* pointer to authentication key */
        int                 key_len,    /* length of authentication key  */
        void                *digest)    /* caller digest to be filled in */
{
    unsigned char k_ipad[65];   /* inner padding -
                                 * key XORd with ipad
                                 */
    unsigned char k_opad[65];   /* outer padding -
                                 * key XORd with opad
                                 */
    unsigned char tk[SHA256_DIGEST_LENGTH];
    unsigned char tk2[SHA256_DIGEST_LENGTH];
    unsigned char bufferIn[1024];
    unsigned char bufferOut[1024];
    int           i;

    /* if key is longer than 64 bytes reset it to key=sha256(key) */
    if ( key_len > 64 ) {
        SHA256( key, key_len, tk );
        key     = tk;
        key_len = SHA256_DIGEST_LENGTH;
    }

    /*
     * the HMAC_SHA256 transform looks like:
     *
     * SHA256(K XOR opad, SHA256(K XOR ipad, text))
     *
     * where K is an n byte key
     * ipad is the byte 0x36 repeated 64 times
     * opad is the byte 0x5c repeated 64 times
     * and text is the data being protected
     */

    /* start out by storing key in pads */
    memset( k_ipad, 0, sizeof k_ipad );
    memset( k_opad, 0, sizeof k_opad );
    memcpy( k_ipad, key, key_len );
    memcpy( k_opad, key, key_len );

    /* XOR key with ipad and opad values */
    for ( i = 0; i < 64; i++ ) {
        k_ipad[i] ^= 0x36;
        k_opad[i] ^= 0x5c;
    }

    /*
     * perform inner SHA256
     */
    memset( bufferIn, 0x00, 1024 );
    memcpy( bufferIn, k_ipad, 64 );
    memcpy( bufferIn + 64, text, text_len );

    SHA256( bufferIn, 64 + text_len, tk2 );

    /*
     * perform outer SHA256
     */
    memset( bufferOut, 0x00, 1024 );
    memcpy( bufferOut, k_opad, 64 );
    memcpy( bufferOut + 64, tk2, SHA256_DIGEST_LENGTH );

    SHA256( bufferOut, 64 + SHA256_DIGEST_LENGTH, (unsigned char*) digest );
}

char * tradingDialog::base64(const unsigned char *input, int length)
{
    BIO *bmem, *b64;
    BUF_MEM *bptr;

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);

    char *buff = (char *)malloc(bptr->length);
    memcpy(buff, bptr->data, bptr->length-1);
    buff[bptr->length-1] = 0;

    BIO_free_all(b64);

    return buff;
}

unsigned char * tradingDialog::unbase64(unsigned char *input, int length)
{
    BIO *b64, *bmem;

    unsigned char *buffer = (unsigned char *)malloc(length);
    memset(buffer, 0, length);

    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new_mem_buf(input, length);
    bmem = BIO_push(b64, bmem);

    BIO_read(bmem, buffer, length);

    BIO_free_all(bmem);

    return buffer;
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
    QJsonObject BalanceObject =  GetResultObjectFromJSONObject(balance);
    QJsonObject obj;

    double AvailableLUX = BalanceObject["Available"].toDouble();
    double Quantity = ui->CSUnitsInput->text().toDouble();
    double Received = 0;
    double Qty = 0;
    double Price = 0;
    QJsonArray  BuyArray  = BuyObject.value("buy").toArray();                //get buy/sell object from result object

    // For each buy order
    foreach (const QJsonValue & value, BuyArray)
    {
        obj = value.toObject();

        double x = obj["Rate"].toDouble(); //would like to use int64 here
        double y = obj["Quantity"].toDouble();
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
        ui->CSReceiveLabel->setText("<span style='font-weight:bold; font-size:12px; color:green'>" + ReceiveStr.number((ui->CSUnitsInput->text().toDouble() - 0.0002),'i',8) + "</span>");
        ui->CSDumpLabel->setText("<span style='font-weight:bold; font-size:12px; color:red'>" + DumpStr.number(Price,'i',8) + "</span>");
        ui->CSTotalLabel->setText("<span style='font-weight:bold; font-size:12px; color:red'>" + TotalStr.number(Qty,'i',8) + "</span>");
    } else {
        ReceiveStr = "N/A";
        TotalStr = "N/A";
        DumpStr = "N/A";
        ui->CSReceiveLabel->setText("<span style='font-weight:bold; font-size:12px; color:red'>" + ReceiveStr + "</span>");
        ui->CSDumpLabel->setText("<span style='font-weight:bold; font-size:12px; color:red'>" + DumpStr + "</span>");
        ui->CSTotalLabel->setText("<span style='font-weight:bold; font-size:12px; color:red'>" + TotalStr + "</span>");
    }
}

void tradingDialog::on_UpdateKeys_clicked(bool Save, bool Load)
{
  this->ApiKey    = ui->ApiKeyInput->text();
  this->SecretKey = ui->SecretKeyInput->text();


   QJsonDocument jsonResponse = QJsonDocument::fromJson(GetAccountHistory().toUtf8()); //get json from str.
   QJsonObject ResponseObject = jsonResponse.object();                                 //get json obj
    LogPrintf("API KEY: %s SECRET KEY: %s ACC_HIST: %s CONF: %s\n", this->ApiKey.toStdString().c_str(), this->SecretKey.toStdString().c_str(), GetAccountHistory().toStdString().c_str(), GetDataDir());

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
        output[i] = toEncrypt[i] ^ key[i % (sizeof(key) / sizeof(char))];
    return output;
}

void tradingDialog::on_SaveKeys_clicked()
{
    bool fSuccess = true;
    boost::filesystem::path pathConfigFile = GetDataDir() / "APIcache.txt";
    boost::filesystem::ofstream stream (pathConfigFile.string(), ios::out | ios::trunc);

    // Qstring to string
    string password = ui->PasswordInput->text().toUtf8().constData();

    if (password.length() <= 6){
        QMessageBox::information(this,"Error !","Your password is too short !");
        fSuccess = false;
        stream.close();
    }

    // qstrings to utf8, add to byteArray and convert to const char for stream
    string Secret = ui->SecretKeyInput->text().toUtf8().constData();
    string Key = ui->ApiKeyInput->text().toUtf8().constData();
    string ESecret = "";
    string EKey = "";

    if (stream.is_open() && fSuccess)
    {
        ESecret = encryptDecrypt(Secret, password);
        EKey = encryptDecrypt(Key, password);
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

    LogPrintf("YOU CLICKED ON LOAD KEYS!!!\n");

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
                DSecret = QString::fromUtf8(encryptDecrypt(line, password).c_str());
                ui->SecretKeyInput->setText(DSecret);
            } else if (i == 1){
                DKey = QString::fromUtf8(encryptDecrypt(line, password).c_str());
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
    QJsonObject ResultObject =  GetResultObjectFromJSONObject(responseA);

    double AvailableLUX = ResultObject["Available"].toDouble();

    ui->UnitsInputLUX->setText(str.number(AvailableLUX,'i',8));
}

void tradingDialog::on_Buy_Max_Amount_clicked()
{
    //calculate amount of currency than can be brought with the BTC balance available
    QString responseA = GetBalance("BTC");
    QString responseB = GetMarketSummary();
    QString str;

    QJsonObject ResultObject =  GetResultObjectFromJSONObject(responseA);
    QJsonObject ResultObj    =  GetResultObjectFromJSONArray(responseB);

    //Get the Bid ask or last value from combo
    QString value = ui->BuyBidcomboBox->currentText();

    double AvailableBTC = ResultObject["Available"].toDouble();
    double CurrentASK   = ResultObj[value].toDouble();
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

    QJsonArray  BuyArray  = BuyObject.value("buy").toArray();                //get buy/sell object from result object

    // For each buy order
    foreach (const QJsonValue & value, BuyArray)
    {
        obj = value.toObject();

        double x = obj["Rate"].toDouble(); //would like to use int64 here
        double y = obj["Quantity"].toDouble();
        // If
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

    QJsonObject ResultObject =  GetResultObjectFromJSONObject(responseA);

    double AvailableLUX = ResultObject["Available"].toDouble();

    ui->WithdrawUnitsInput->setText(str.number(AvailableLUX,'i',8));
}

QJsonObject tradingDialog::GetResultObjectFromJSONObject(QString response){

    QJsonDocument jsonResponse = QJsonDocument::fromJson(response.toUtf8());          //get json from str.
    QJsonObject  ResponseObject = jsonResponse.object();                              //get json obj
    QJsonObject  ResultObject   = ResponseObject.value(QString("result")).toObject(); //get result object

  return ResultObject;
}

QJsonObject tradingDialog::GetResultObjectFromJSONArray(QString response){

    QJsonDocument jsonResponsea = QJsonDocument::fromJson(response.toUtf8());
    QJsonObject   jsonObjecta   = jsonResponsea.object();
    QJsonArray    jsonArraya    = jsonObjecta["result"].toArray();
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

    LogPrintf("HMAC PARAMS --- %s /////// \n\n\n%s\n\n\n %d\n", UrlToSign.toStdString().c_str(), Secret.toStdString().c_str(), Secret.length());

    QString retval = "";

    QByteArray byteArray = UrlToSign.toUtf8();
    const char* URL = byteArray.constData();

    //QByteArray byteArrayB = Secret.toUtf8();
    //const char* Secretkey = byteArrayB.constData();

    //const char *Secretkey = Secret.toStdString().c_str();//"h8K8zJYa0hVPyrOJL/O8Uox7DQW3ybdL46qVO/zNK/s=";
    //char URL[] = "0449b9396e5e46da86f762562684c8c5POSThttps%3a%2f%2fwww.cryptopia.co.nz%2fapi%2fgetopenorders1513658996UpnpMVCjZBnRwucbpMyiVw==";

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
    LogPrintf("HMAC!!!! ------ %s %s\n", retval.toStdString().c_str(), Secret.toStdString().c_str());
    //qDebug() << "HMAC digest:"<< retval;

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

        if (ResponseObject["success"].toBool() == false){
            QMessageBox::information(this,"Buy Order Failed",ResponseObject["message"].toString());

        }else if (ResponseObject["success"].toBool() == true){
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

        if (ResponseObject["success"].toBool() == false){
            QMessageBox::information(this,"Sell Order Failed",ResponseObject["message"].toString());

        }else if (ResponseObject["success"].toBool() == true){
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
            Msg += ui->CSUnitsAddress->text();
            Msg += ", DUMPING your coins at ";
            Msg += Rstr.number(Rate,'i',8);
            Msg += " satoshis ?";

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this,"Cross-Send",Msg,QMessageBox::Yes|QMessageBox::No);

    if(reply != QMessageBox::Yes)
    {
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        return;
    }

        QString Order = "selllimit";
        QJsonArray  BuyArray  = BuyObject.value("buy").toArray();                //get buy/sell object from result object

        // For each buy order
        foreach (const QJsonValue & value, BuyArray)
        {
            obj = value.toObject();

            double x = obj["Rate"].toDouble(); //would like to use int64 here
            double y = obj["Quantity"].toDouble();
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

                if (SellResponseObject["success"].toBool() == false){
                    if (SellResponseObject["message"].toString() == "DUST_TRADE_DISALLOWED_MIN_VALUE_50K_SAT"){
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

                if (SellResponseObject["success"].toBool() == false){
                    QMessageBox::information(this,"sFailed",SellResponse);

                } else if (SellResponseObject["success"].toBool() == true){
                    MilliSleep(5000);
                    QString Response = Withdraw(ui->CSUnitsInput->text().toDouble(),ui->CSUnitsAddress->text(),Coin);
                    QJsonDocument jsonResponse = QJsonDocument::fromJson(Response.toUtf8());          //get json from str.
                    QJsonObject ResponseObject = jsonResponse.object();                              //get json obj

                    if (ResponseObject["success"].toBool() == false){
                        MilliSleep(5000);
                        QString Response = Withdraw(ui->CSUnitsInput->text().toDouble(),ui->CSUnitsAddress->text(),Coin);
                        QJsonDocument jsonResponse = QJsonDocument::fromJson(Response.toUtf8());          //get json from str.
                        QJsonObject ResponseObject = jsonResponse.object();

                        if (ResponseObject["success"].toBool() == false){
                            QMessageBox::information(this,"Failed",ResponseObject["message"].toString());
                        } else if (ResponseObject["success"].toBool() == true){
                            QMessageBox::information(this,"Success","<center>Cross-Send Successful</center>\n Sold "+Astr.number(Qty,'i',4)+" LUX for "+Qstr.number((ui->CSUnitsInput->text().toDouble()-0.0002),'i',8)+" BTC");
                        }
                    } else if (ResponseObject["success"].toBool() == true){
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

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        return;
    }

        QString Response =  Withdraw(Quantity, ui->WithdrawAddress->text(), Coin);
        QJsonDocument jsonResponse = QJsonDocument::fromJson(Response.toUtf8());          //get json from str.
        QJsonObject  ResponseObject = jsonResponse.object();                              //get json obj

        if (ResponseObject["success"].toBool() == false){
            QMessageBox::information(this,"Failed",ResponseObject["message"].toString());

        }else if (ResponseObject["success"].toBool() == true){
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
