
#ifndef TRADINGDIALOG_H
#define TRADINGDIALOG_H

#include <QDialog>
#include <QObject>
#include <stdint.h>
#include "ui_tradingdialog.h"
#include "clientmodel.h"
#include "walletmodel.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include <QJsonObject>
#include <QJsonArray>

namespace Ui {
class tradingDialog;
}
class WalletModel;

class tradingDialog : public QDialog
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
    void on_UpdateKeys_clicked(bool Save=true, bool Load=true);
    void on_LoadKeys_clicked();
    void on_SaveKeys_clicked();
    void on_GenDepositBTN_clicked();

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
    QString CancelOrder(QString Orderid);
    QString BuyLUX(QString OrderType, double Quantity, double Rate);
    QString SellLUX(QString OrderType, double Quantity, double Rate);
    QString Withdraw(double Amount, QString Address, QString Coin);
    QString GetMarketHistory();
    QString GetMarketSummary();
    QString GetOrderBook();
    QString GetOpenOrders();
    QString GetAccountHistory();
    QString GetBalance(QString Currency);
    QString GetDepositAddress();
    QString HMAC_SHA512_SIGNER(QString UrlToSign,QString Secretkey);
    QString sendRequest(QString url);
    string encryptDecrypt(string toEncrypt, string password);
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


};

#endif // TRADINGDIALOG_H
