#include "multisenddialog.h"
#include "addressbookpage.h"
#include "base58.h"
#include "init.h"
#include "ui_multisenddialog.h"
#include "walletmodel.h"
#include "platformstyle.h"
#include <QLineEdit>
#include <QMessageBox>
#include <QStyle>
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

MultiSendDialog::MultiSendDialog(const PlatformStyle *platformStyle,QWidget* parent) : QDialog(parent),
                                                    ui(new Ui::MultiSendDialog),
                                                    model(0),
                                                    platformStyle(platformStyle)
{
    ui->setupUi(this);
    updateCheckBoxes();
}

MultiSendDialog::~MultiSendDialog()
{
    delete ui;
}

void MultiSendDialog::setModel(WalletModel* model)
{
    this->model = model;
}

void MultiSendDialog::setAddress(const QString& address)
{
    setAddress(address, ui->multiSendAddressEdit);
}

void MultiSendDialog::setAddress(const QString& address, QLineEdit* addrEdit)
{
    addrEdit->setText(address);
    addrEdit->setFocus();
}

void MultiSendDialog::updateCheckBoxes()
{
    ui->multiSendStakeCheckBox->setChecked(pwalletMain->fMultiSendStake);
    ui->multiSendMasternodeCheckBox->setChecked(pwalletMain->fMultiSendMasternodeReward);
}

void MultiSendDialog::on_addressBookButton_clicked()
{
    if (model && model->getAddressTableModel()) {
        AddressBookPage dlg(platformStyle, AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
        dlg.setModel(model->getAddressTableModel());
        if (dlg.exec())
            setAddress(dlg.getReturnValue(), ui->multiSendAddressEdit);
    }
}

void MultiSendDialog::on_viewButton_clicked()
{
    std::pair<std::string, int> pMultiSend;
    QString strMultiSendPrint = "";
    if (pwalletMain->isMultiSendEnabled()) {
        if (pwalletMain->fMultiSendStake)
            strMultiSendPrint += tr("MultiSend enabled for Stakes") + "\n";
        else if (pwalletMain->fMultiSendStake)
            strMultiSendPrint += tr("MultiSend enabled for Masternode Rewards") + "\n";
    } else
        strMultiSendPrint += tr("MultiSend is not active") + "\n";

    for (int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++) {
        pMultiSend = pwalletMain->vMultiSend[i];
        strMultiSendPrint += QString(pMultiSend.first.c_str());
        strMultiSendPrint += " - ";
        strMultiSendPrint += QString::fromStdString(itostr(pMultiSend.second));
        strMultiSendPrint += "% \n";
    }
    ui->message->setProperty("status", "ok");
    ui->message->style()->unpolish(ui->message);
    ui->message->style()->polish(ui->message);
    ui->message->setText(strMultiSendPrint);
    return;
}

void MultiSendDialog::on_addButton_clicked()
{
    bool fValidConversion = false;
    std::string strAddress = ui->multiSendAddressEdit->text().toStdString();
    if (!IsValidDestination(DecodeDestination(strAddress))) {
        ui->message->setProperty("status", "error");
        ui->message->style()->unpolish(ui->message);
        ui->message->style()->polish(ui->message);
        ui->message->setText(tr("The entered address:\n") + ui->multiSendAddressEdit->text() + tr(" is invalid.\nPlease check the address and try again."));
        ui->multiSendAddressEdit->setFocus();
        return;
    }
    int nMultiSendPercent = ui->multiSendPercentEdit->text().toInt(&fValidConversion, 10);
    int nSumMultiSend = 0;
    for (int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++)
        nSumMultiSend += pwalletMain->vMultiSend[i].second;
    if (nSumMultiSend + nMultiSendPercent > 100) {
        ui->message->setProperty("status", "error");
        ui->message->style()->unpolish(ui->message);
        ui->message->style()->polish(ui->message);
        ui->message->setText(tr("The total amount of your MultiSend vector is over 100% of your stake reward\n"));
        ui->multiSendAddressEdit->setFocus();
        return;
    }
    if (!fValidConversion || nMultiSendPercent > 100 || nMultiSendPercent <= 0) {
        ui->message->setProperty("status", "error");
        ui->message->style()->unpolish(ui->message);
        ui->message->style()->polish(ui->message);
        ui->message->setText(tr("Please Enter 1 - 100 for percent."));
        ui->multiSendPercentEdit->setFocus();
        return;
    }
    std::pair<std::string, int> pMultiSend;
    pMultiSend.first = strAddress;
    pMultiSend.second = nMultiSendPercent;
    pwalletMain->vMultiSend.push_back(pMultiSend);
    ui->message->setProperty("status", "ok");
    ui->message->style()->unpolish(ui->message);
    ui->message->style()->polish(ui->message);
    std::string strMultiSendPrint = "";
    for (int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++) {
        pMultiSend = pwalletMain->vMultiSend[i];
        strMultiSendPrint += pMultiSend.first.c_str();
        strMultiSendPrint += " - ";
        strMultiSendPrint += boost::lexical_cast<string>(pMultiSend.second);
        strMultiSendPrint += "% \n";
    }
    CWalletDB walletdb(pwalletMain->strWalletFile);
    walletdb.WriteMultiSend(pwalletMain->vMultiSend);
    ui->message->setText(tr("MultiSend Vector\n") + QString(strMultiSendPrint.c_str()));
    return;
}

void MultiSendDialog::on_deleteButton_clicked()
{
    std::vector<std::pair<std::string, int> > vMultiSendTemp = pwalletMain->vMultiSend;
    std::string strAddress = ui->multiSendAddressEdit->text().toStdString();
    bool fRemoved = false;
    for (int i = 0; i < (int)pwalletMain->vMultiSend.size(); i++) {
        if (pwalletMain->vMultiSend[i].first == strAddress) {
            pwalletMain->vMultiSend.erase(pwalletMain->vMultiSend.begin() + i);
            fRemoved = true;
        }
    }
    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.EraseMultiSend(vMultiSendTemp))
        fRemoved = false;
    if (!walletdb.WriteMultiSend(pwalletMain->vMultiSend))
        fRemoved = false;

    if (fRemoved)
        ui->message->setText(tr("Removed ") + QString(strAddress.c_str()));
    else
        ui->message->setText(tr("Could not locate address\n"));

    updateCheckBoxes();

    return;
}

void MultiSendDialog::on_activateButton_clicked()
{
    QString qstr = "";
    if (pwalletMain->vMultiSend.size() < 1)
        qstr = tr("Unable to activate MultiSend, check MultiSend vector") + "\n";
    else if (!(ui->multiSendStakeCheckBox->isChecked() || ui->multiSendMasternodeCheckBox->isChecked())) {
        qstr = tr("Need to select to send on stake and/or masternode rewards") + "\n";
    } else if (IsValidDestination(DecodeDestination(pwalletMain->vMultiSend[0].first))) {
        pwalletMain->fMultiSendStake = ui->multiSendStakeCheckBox->isChecked();
        pwalletMain->fMultiSendMasternodeReward = ui->multiSendMasternodeCheckBox->isChecked();

        CWalletDB walletdb(pwalletMain->strWalletFile);
        if (!walletdb.WriteMSettings(pwalletMain->fMultiSendStake, pwalletMain->fMultiSendMasternodeReward, pwalletMain->nLastMultiSendHeight))
            qstr = tr("MultiSend activated but writing settings to DB failed") + "\n";
        else
            qstr = tr("MultiSend activated") + "\n";
    } else
        qstr = tr("First address is not valid") + "\n";
    ui->message->setProperty("status", "ok");
    ui->message->style()->unpolish(ui->message);
    ui->message->style()->polish(ui->message);
    ui->message->setText(qstr);
    return;
}

void MultiSendDialog::on_disableButton_clicked()
{
    QString qstr = "";
    pwalletMain->setMultiSendDisabled();
    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.WriteMSettings(false, false, pwalletMain->nLastMultiSendHeight))
    {
        qstr = tr("MultiSend disabled but writing settings to DB failed") + "\n";
        ui->message->setProperty("status", "error");
    }
    else
    {
        qstr = tr("MultiSend disabled") + "\n";
        ui->message->setProperty("status", "ok");
    }
    ui->message->style()->unpolish(ui->message);
    ui->message->style()->polish(ui->message);
    ui->message->setText(qstr);
    return;
}
