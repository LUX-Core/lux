#include <boost/asio/connect.hpp>
#include "addtokenpage.h"
#include "ui_addtokenpage.h"
#include "guiconstants.h"
#include "wallet.h"
#include "walletmodel.h"
#include "token.h"
#include "clientmodel.h"

AddTokenPage::AddTokenPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AddTokenPage),
    m_tokenABI(0),
    m_model(0),
    m_clientModel(0)
{
    ui->setupUi(this);
    m_tokenABI = new Token();

    connect(ui->lineEditContractAddress, SIGNAL(textChanged(const QString &)), this, SLOT(on_addressChanged()));
}

AddTokenPage::~AddTokenPage()
{
    delete ui;

    if(m_tokenABI)
        delete m_tokenABI;
    m_tokenABI = 0;
}

void AddTokenPage::setClientModel(ClientModel *clientModel)
{
    m_clientModel = clientModel;
    if (m_clientModel)
    {
        connect(m_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(on_numBlocksChanged()));
        on_numBlocksChanged();
    }
}

void AddTokenPage::clearAll()
{
    ui->lineEditContractAddress->setText("");
    ui->lineEditTokenName->setText("");
    ui->lineEditTokenSymbol->setText("");
    ui->lineEditDecimals->setText("");
    ui->lineEditSenderAddress->setCurrentIndex(-1);
}

void AddTokenPage::setModel(WalletModel *_model)
{
    m_model = _model;
}

void AddTokenPage::on_clearButton_clicked()
{
    clearAll();
}

void AddTokenPage::on_confirmButton_clicked()
{
    CTokenInfo tokenInfo;
    tokenInfo.strContractAddress = ui->lineEditContractAddress->text().toStdString();
    tokenInfo.strTokenName = ui->lineEditTokenName->text().toStdString();
    tokenInfo.strTokenSymbol = ui->lineEditTokenSymbol->text().toStdString();
    tokenInfo.nDecimals = ui->lineEditDecimals->text().toInt();
    tokenInfo.strSenderAddress = ui->lineEditSenderAddress->currentText().toStdString();

    if(m_model)
    {
        m_model->AddTokenEntry(tokenInfo);
    }

    clearAll();
}

void AddTokenPage::on_addressChanged()
{
    bool enableConfirm = false;
    QString tokenAddress = ui->lineEditContractAddress->text();
    if(m_tokenABI)
    {
        m_tokenABI->setAddress(tokenAddress.toStdString());
        std::string name, symbol, decimals;
        bool ret = m_tokenABI->name(name);
        ret &= m_tokenABI->symbol(symbol);
        ret &= m_tokenABI->decimals(decimals);
        ui->lineEditTokenName->setText(QString::fromStdString(name));
        ui->lineEditTokenSymbol->setText(QString::fromStdString(symbol));
        ui->lineEditDecimals->setText(QString::fromStdString(decimals));
        enableConfirm = ret;
    }
    ui->confirmButton->setEnabled(enableConfirm);
}

void AddTokenPage::on_numBlocksChanged()
{
    ui->lineEditSenderAddress->on_refresh();
}
