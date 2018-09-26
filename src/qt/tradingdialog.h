//
// Created by 216k155 on 12/26/17.
//

#ifndef LUX_TRADINGDIALOG_H
#define LUX_TRADINGDIALOG_H

#include <QWidget>
#include <QObject>
#include <QTableWidget>
#include <stdint.h>
#include "ui_tradingdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "qcustomplot.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>

namespace Ui {
    class tradingDialog;
}
class WalletModel;

class tradingDialog : public QWidget
{
    Q_OBJECT

public:
    explicit tradingDialog(QWidget *parent = 0);
    ~tradingDialog();

    void setModel(WalletModel *model);

private slots:

    void InitTrading();
    void on_TradingTabWidget_tabBarClicked(int index);
    void ParseAndPopulateOrderBookTables(QString Response);
    void ParseAndPopulatePriceChart(QString Response);
    void ParseAndPopulateMarketHistoryTable(QString Response);
    void ParseAndPopulateAccountHistoryTable(QString Response);
    void ParseAndPopulateOpenOrdersTable(QString Response);
    void UpdaterFunction();
    void CreateOrderBookTables(QTableWidget& Table,QStringList TableHeader);
    void DisplayBalance(QLabel &BalanceLabel,QLabel &Available, QLabel &Pending, QString Currency,QString Response);
    void DisplayBalance(QLabel &BalanceLabel, QLabel &BalanceLabel2, QString Response, QString Response2);
    void DisplayBalance(QLabel &BalanceLabel, QString Response);
    void ActionsOnSwitch(int index);
    void CancelOrderSlot(int row, int col);
    void BidInfoInsertSlot(int row, int col);
    void AskInfoInsertSlot(int row, int col);
    void on_UpdateKeys_clicked(bool Save=true, bool Load=true);
    void on_LoadKeys_clicked();
    void on_SaveKeys_clicked();
    void on_GenDepositBTN_clicked();
    void showMarketHistoryWhenReplyFinished();
    void showBalanceOfLUXOnTradingTab();
    void showBalanceOfBTCOnTradingTab();
    void showOrderBookOnTradingTab();
    void showMarketHistoryOnTradingTab();
    void showBalanceOfBTCOnSendTab();
    void showBalanceOfLUXOnSendTab();
    void showBalanceOfLUXOnBalanceTab();
    void showBalanceOfBTCOnBalanceTab();
    void showOpenOrders();
    void showAccountHistory();
    void slotReadyRead();
    void slotError1(QNetworkReply::NetworkError e);

    void CalculateBuyCostLabel();
    void on_Buy_Max_Amount_clicked();
    void on_BuyBidcomboBox_currentIndexChanged(const QString &arg1);
    void on_UnitsInput_textChanged(const QString &arg1);
    void on_BuyBidPriceEdit_textChanged(const QString &arg1);
    void on_BuyLUX_clicked();

    void CalculateSellCostLabel();
    void on_Sell_Max_Amount_clicked();
    void on_SellBidcomboBox_currentIndexChanged(const QString &arg1);
    void on_UnitsInputLUX_textChanged(const QString &arg1);
    void on_SellBidPriceEdit_textChanged(const QString &arg1);
    void on_SellLUXBTN_clicked();

    void CalculateCSReceiveLabel();
    void on_CSUnitsInput_textChanged(const QString &arg1);
    void on_CSUnitsBtn_clicked();
    void on_CS_Max_Amount_clicked();

    void on_Withdraw_Max_Amount_clicked();
    void on_WithdrawUnitsBtn_clicked();

    void on_KeyPasteButton_clicked();
    void on_SecretPasteButton_clicked();
    void on_CSPasteButton_clicked();
    void on_WithdrawPasteButton_clicked();
    void on_DepositCopyButton_clicked();

    int SetExchangeInfoTextLabels();

    QString CryptopiaTimeStampToReadable(QString DateTime);
    QString CryptopiaIntegerTimeStampToReadable(int DateTime);
    QString CancelOrder(QString Orderid);
    QString BuyLUX(QString OrderType, double Quantity, double Rate);
    QString SellLUX(QString OrderType, double Quantity, double Rate);
    QString Withdraw(double Amount, QString Address, QString Coin);
    void GetMarketHistory();
    QString GetMarketSummary();
    QString GetOrderBook();
    QString GetOpenOrders();
    QString GetAccountHistory();
    QString GetBalance(QString Currency);
    QString GetDepositAddress();
    unsigned char* HMAC_SHA256_SIGNER(QString UrlToSign,QString Secretkey);
    QString sendRequest(QString url, QString method = "GET", QString body = QString("{\"market\":\"LUX/BTC\"}"));// "{\"gender\":\"male\"}");
    void sendRequest1(QString url, std::function<void (void)> funcForCallAfterReceiveResponse, QString method = "GET", QString body = QString("{\"market\":\"LUX/BTC\"}"));// "{\"gender\":\"male\"}");
    //QString sendRequest(QString url);
    string encryptDecrypt(string toEncrypt, string password);
    char * base64(const unsigned char *input, int length);
    string url_encode(const string &value);
    QJsonObject GetResultObjectFromJSONObject(QString response);
    QJsonObject GetResultObjectFromJSONArray(QString response);
    QJsonArray  GetResultArrayFromJSONObject(QString response);

public slots:


private:
    Ui::tradingDialog *ui;
    //Socket *socket;
    int timerid;
    QTimer *timer;
    QString ApiKey;
    QString SecretKey;
    WalletModel *model;
    QCustomPlot* derPlot;
    QCPFinancial *ohlc;
    QNetworkAccessManager *qmanager;

};

#endif //LUX_TRADINGDIALOG_H
