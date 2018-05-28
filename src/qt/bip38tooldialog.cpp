// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bip38tooldialog.h"
#include "ui_bip38tooldialog.h"

#include "addressbookpage.h"
#include "guiutil.h"
#include "walletmodel.h"

#include "base58.h"
#include "bip38.h"
#include "init.h"
#include "wallet.h"
#include "../script/standard.h"

#include <string>
#include <vector>

#include <QClipboard>

Bip38ToolDialog::Bip38ToolDialog(QWidget* parent) : QDialog(parent),
                                                    ui(new Ui::Bip38ToolDialog),
                                                    model(0)
{
    ui->setupUi(this);

#if QT_VERSION >= 0x040700
    ui->decryptedKeyOut_DEC->setPlaceholderText(tr("Click \"Decrypt Key\" to compute key"));
#endif

    GUIUtil::setupAddressWidget(ui->addressIn_ENC, this);
    ui->addressIn_ENC->installEventFilter(this);
    ui->passphraseIn_ENC->installEventFilter(this);
    ui->encryptedKeyOut_ENC->installEventFilter(this);
    ui->encryptedKeyIn_DEC->installEventFilter(this);
    ui->passphraseIn_DEC->installEventFilter(this);
    ui->decryptedKeyOut_DEC->installEventFilter(this);
}

Bip38ToolDialog::~Bip38ToolDialog()
{
    delete ui;
}

void Bip38ToolDialog::setModel(WalletModel* model)
{
    this->model = model;
}

void Bip38ToolDialog::setAddress_ENC(const QString& address)
{
    ui->addressIn_ENC->setText(address);
    ui->passphraseIn_ENC->setFocus();
}

void Bip38ToolDialog::setAddress_DEC(const QString& address)
{
    ui->encryptedKeyIn_DEC->setText(address);
    ui->passphraseIn_DEC->setFocus();
}

void Bip38ToolDialog::showTab_ENC(bool fShow)
{
    ui->tabWidget->setCurrentIndex(0);
    if (fShow)
        this->show();
}

void Bip38ToolDialog::showTab_DEC(bool fShow)
{
    ui->tabWidget->setCurrentIndex(1);
    if (fShow)
        this->show();
}

void Bip38ToolDialog::on_addressBookButton_ENC_clicked()
{
    if (model && model->getAddressTableModel()) {
        AddressBookPage dlg(AddressBookPage::ForSelection, AddressBookPage::ReceivingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec()) {
            setAddress_ENC(dlg.getReturnValue());
        }
    }
}

void Bip38ToolDialog::on_pasteButton_ENC_clicked()
{
    setAddress_ENC(QApplication::clipboard()->text());
}

QString specialChar = "\"@!#$%&'()*+,-./:;<=>?`{|}~^_[]\\";
QString validChar = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz" + specialChar;
bool isValidPassphrase(QString strPassphrase, QString& strInvalid)
{
    for (int i = 0; i < strPassphrase.size(); i++) {
        if (!validChar.contains(strPassphrase[i], Qt::CaseSensitive)) {
            if (QString("\"'").contains(strPassphrase[i]))
                continue;

            strInvalid = strPassphrase[i];
            return false;
        }
    }

    return true;
}

void Bip38ToolDialog::on_encryptKeyButton_ENC_clicked()
{
    if (!model)
        return;

    QString qstrPassphrase = ui->passphraseIn_ENC->text();
    QString strInvalid;
    if (!isValidPassphrase(qstrPassphrase, strInvalid)) {
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("The entered passphrase is invalid. ") + strInvalid + QString(" is not valid") + QString(" ") + tr("Allowed: 0-9,a-z,A-Z,") + specialChar);
        return;
    }

    CTxDestination addr = DecodeDestination(ui->addressIn_ENC->text().toStdString());
    if (!IsValidDestination(addr)) {
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("The entered address is invalid.") + QString(" ") + tr("Please check the address and try again."));
        return;
    }

    const CKeyID *keyID = boost::get<CKeyID>(&addr);
    if (!keyID) {
        ui->addressIn_ENC->setValid(false);
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("The entered address does not refer to a key.") + QString(" ") + tr("Please check the address and try again."));
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock(true));
    if (!ctx.isValid()) {
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("Wallet unlock was cancelled."));
        return;
    }

    CKey key;
    if (!pwalletMain->GetKey(*keyID, key)) {
        ui->statusLabel_ENC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_ENC->setText(tr("Private key for the entered address is not available."));
        return;
    }

    std::string encryptedKey = BIP38_Encrypt(EncodeDestination(addr), qstrPassphrase.toStdString(), key.GetPrivKey_256());
    ui->encryptedKeyOut_ENC->setText(QString::fromStdString(encryptedKey));
}

void Bip38ToolDialog::on_copyKeyButton_ENC_clicked()
{
    GUIUtil::setClipboard(ui->encryptedKeyOut_ENC->text());
}

void Bip38ToolDialog::on_clearButton_ENC_clicked()
{
    ui->addressIn_ENC->clear();
    ui->passphraseIn_ENC->clear();
    ui->encryptedKeyOut_ENC->clear();
    ui->statusLabel_ENC->clear();

    ui->addressIn_ENC->setFocus();
}

CKey key;
void Bip38ToolDialog::on_decryptKeyButton_DEC_clicked()
{
    string strPassphrase = ui->passphraseIn_DEC->text().toStdString();
    string strKey = ui->encryptedKeyIn_DEC->text().toStdString();

    uint256 privKey;
    bool fCompressed;
    if (!BIP38_Decrypt(strPassphrase, strKey, privKey, fCompressed)) {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Failed to decrypt.") + QString(" ") + tr("Please check the key and passphrase and try again."));
        return;
    }

    key.Set(privKey.begin(), privKey.end(), fCompressed);
    CPubKey pubKey = key.GetPubKey();
    CTxDestination address(pubKey.GetID());

    ui->decryptedKeyOut_DEC->setText(QString::fromStdString(HexStr(privKey)));
    ui->addressOut_DEC->setText(QString::fromStdString(EncodeDestination(address)));
}

void Bip38ToolDialog::on_importAddressButton_DEC_clicked()
{
    WalletModel::UnlockContext ctx(model->requestUnlock(true));
    if (!ctx.isValid()) {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Wallet unlock was cancelled."));
        return;
    }

    CTxDestination address = DecodeDestination(ui->addressOut_DEC->text().toStdString());
    CPubKey pubkey = key.GetPubKey();

    if (!IsValidDestination(address) || !key.IsValid() || EncodeDestination(CTxDestination(pubkey.GetID())) != EncodeDestination(address)) {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Data Not Valid.") + QString(" ") + tr("Please try again."));
        return;
    }

    CKeyID vchAddress = pubkey.GetID();
    {
        ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel_DEC->setText(tr("Please wait while key is imported"));

        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBook(vchAddress, "", "receive");

        // Don't throw error in case a key is already there
        if (pwalletMain->HaveKey(vchAddress)) {
            ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
            ui->statusLabel_DEC->setText(tr("Key Already Held By Wallet"));
            return;
        }

        pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;

        if (!pwalletMain->AddKeyPubKey(key, pubkey)) {
            ui->statusLabel_DEC->setStyleSheet("QLabel { color: red; }");
            ui->statusLabel_DEC->setText(tr("Error Adding Key To Wallet"));
            return;
        }

        // whenever a key is imported, we need to scan the whole chain
        pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'
        pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true);
    }

    ui->statusLabel_DEC->setStyleSheet("QLabel { color: green; }");
    ui->statusLabel_DEC->setText(tr("Successfully Added Private Key To Wallet"));
}

void Bip38ToolDialog::on_clearButton_DEC_clicked()
{
    ui->encryptedKeyIn_DEC->clear();
    ui->decryptedKeyOut_DEC->clear();
    ui->passphraseIn_DEC->clear();
    ui->statusLabel_DEC->clear();

    ui->encryptedKeyIn_DEC->setFocus();
}

bool Bip38ToolDialog::eventFilter(QObject* object, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::FocusIn) {
        if (ui->tabWidget->currentIndex() == 0) {
            /* Clear status message on focus change */
            ui->statusLabel_ENC->clear();

            /* Select generated signature */
            if (object == ui->encryptedKeyOut_ENC) {
                ui->encryptedKeyOut_ENC->selectAll();
                return true;
            }
        } else if (ui->tabWidget->currentIndex() == 1) {
            /* Clear status message on focus change */
            ui->statusLabel_DEC->clear();
        }
    }
    return QDialog::eventFilter(object, event);
}
