// Copyright (c) 2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_MULTISIGDIALOG_H
#define BITCOIN_QT_MULTISIGDIALOG_H

#include <QDialog>
#include <QFrame>
#include <QString>
#include <QVBoxLayout>
#include "script/script.h"
#include "primitives/transaction.h"
#include "coins.h"
#include "coincontrol.h"
#include "walletmodel.h"
#include "coincontroldialog.h"

namespace Ui
{
class MultisigDialog;
}

class MultisigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultisigDialog(QWidget* parent);
    ~MultisigDialog();
    void setModel(WalletModel* model);
    void updateCoinControl(CAmount nAmount, unsigned int nQuantity);

public slots:
    void showTab(int index);

private:
    Ui::MultisigDialog* ui;
    WalletModel* model;
    CCoinControl* coinControl;
    bool isFirstPrivKey;
    bool isFirstRawTx;
    CMutableTransaction multisigTx;

    QFrame* createAddress(int labelNumber);
    QFrame* createInput(int labelNumber);
    CCoinsViewCache getInputsCoinsViewCache(const std::vector<CTxIn>& vin);
    QString buildMultisigTxStatusString(bool fComplete, const CMutableTransaction& tx);
    bool createRedeemScript(int m, std::vector<std::string> keys, CScript& redeemRet, std::string& errorRet);
    bool createMultisigTransaction(std::vector<CTxIn> vUserIn, std::vector<CTxOut> vUserOut, string& feeStringRet, string& errorRet);
    bool signMultisigTx(CMutableTransaction& txToSign, std::string& errorMessageRet, QVBoxLayout* keyList = nullptr);
    bool addMultisig(int m, std::vector<std::string> keys);
    bool isFullyVerified(CMutableTransaction& txToVerify);

private slots:
   void deleteFrame();
   void pasteText();
   void commitMultisigTx();
   void addressBookButtonReceiving();
   void on_addAddressButton_clicked();
   void on_addMultisigButton_clicked();
   void on_addDestinationButton_clicked();
   void on_createButton_clicked();
   void on_addInputButton_clicked();
   void on_addPrivKeyButton_clicked();
   void on_signButton_clicked();
   void on_pushButtonCoinControl_clicked();
   void on_importAddressButton_clicked();
};

#endif // BITCOIN_QT_MULTISIGDIALOG_H
