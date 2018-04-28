#include "sendtokenpage.h"
#include "ui_sendtokenpage.h"

#include "main.h"
#include "clientmodel.h"
#include "optionsmodel.h"
#include "utilmoneystr.h"
#include "token.h"
#include "bitcoinunits.h"
#include "wallet.h"
#include "guiutil.h"
#include "sendcoinsdialog.h"

static const CAmount SINGLE_STEP = static_cast<const CAmount>(0.00000001 * COIN);

struct SelectedToken {
    std::string address;
    std::string sender;
    std::string symbol;
    int8_t decimals;
    std::string balance;
};

SendTokenPage::SendTokenPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SendTokenPage),
    m_model(0),
    m_clientModel(0),
    m_tokenABI(0),
    m_selectedToken(0)
{
    ui->setupUi(this);
    ui->labelPayTo->setToolTip(tr("The address that will receive the tokens."));
    ui->labelAmount->setToolTip(tr("The amount in LSR Token to send."));
    ui->labelDescription->setToolTip(tr("Optional description for transaction."));
    m_tokenABI = new Token();
    m_selectedToken = new SelectedToken();

    // Set defaults
    ui->lineEditGasPrice->setValue(DEFAULT_GAS_PRICE);
    ui->lineEditGasPrice->setSingleStep(SINGLE_STEP);
    ui->lineEditGasLimit->setMaximum(DEFAULT_GAS_LIMIT_OP_SEND);
    ui->lineEditGasLimit->setValue(DEFAULT_GAS_LIMIT_OP_SEND);
    ui->confirmButton->setEnabled(false);

    // Connect signals with slots
    connect(ui->lineEditPayTo, SIGNAL(textChanged(QString)), SLOT(on_updateConfirmButton()));
    connect(ui->lineEditAmount, SIGNAL(textChanged(QString)), SLOT(on_updateConfirmButton()));
    connect(ui->confirmButton, SIGNAL(clicked()), SLOT(on_confirmClicked()));
}

SendTokenPage::~SendTokenPage()
{
    delete ui;

    if(m_tokenABI)
        delete m_tokenABI;
    m_tokenABI = 0;
}

void SendTokenPage::setModel(WalletModel *_model)
{
    m_model = _model;
}

void SendTokenPage::setClientModel(ClientModel *_clientModel)
{
    m_clientModel = _clientModel;

    if (m_clientModel)
    {
        connect(m_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(on_numBlocksChanged()));
        on_numBlocksChanged();
    }
}

void SendTokenPage::clearAll()
{
    ui->lineEditPayTo->setText("");
    ui->lineEditAmount->clear();
    ui->lineEditDescription->setText("");
    ui->lineEditGasLimit->setValue(DEFAULT_GAS_LIMIT_OP_SEND);
    ui->lineEditGasPrice->setValue(DEFAULT_GAS_PRICE);
}

void SendTokenPage::on_clearButton_clicked()
{
    clearAll();
}

void SendTokenPage::on_numBlocksChanged()
{
    if(m_clientModel)
    {
        uint64_t blockGasLimit = 0;
        uint64_t minGasPrice = 0;
        uint64_t nGasPrice = 0;
        m_clientModel->getGasInfo(blockGasLimit, minGasPrice, nGasPrice);

        ui->labelGasLimit->setToolTip(tr("Gas limit: Default = %1, Max = %2.").arg(DEFAULT_GAS_LIMIT_OP_SEND).arg(blockGasLimit));
        ui->labelGasPrice->setToolTip(tr("Gas price: LUX/gas unit. Default = %1, Min = %2.").arg(QString::fromStdString(FormatMoney(DEFAULT_GAS_PRICE))).arg(QString::fromStdString(FormatMoney(minGasPrice))));
        ui->lineEditGasPrice->setMinimum(minGasPrice);
        ui->lineEditGasLimit->setMaximum(blockGasLimit);
    }
}

void SendTokenPage::on_updateConfirmButton()
{
    bool enabled = true;
    if(ui->lineEditPayTo->text().isEmpty() || ui->lineEditAmount->text().isEmpty())
    {
        enabled = false;
    }

    ui->confirmButton->setEnabled(enabled);
}

void SendTokenPage::on_confirmClicked()
{
    if(m_model && m_model->isUnspentAddress(m_selectedToken->sender))
    {
        int unit = m_model->getOptionsModel()->getDisplayUnit();
        uint64_t gasLimit = ui->lineEditGasLimit->value();
        CAmount gasPrice = ui->lineEditGasPrice->value();

        m_tokenABI->setAddress(m_selectedToken->address);
        m_tokenABI->setSender(m_selectedToken->sender);
        m_tokenABI->setGasLimit(QString::number(gasLimit).toStdString());
        m_tokenABI->setGasPrice(BitcoinUnits::format(unit, gasPrice, false, BitcoinUnits::separatorNever).toStdString());

        std::string toAddress = ui->lineEditPayTo->text().toStdString();
        std::string amountToSend = ui->lineEditAmount->text().toStdString();
        QString amountFormated = BitcoinUnits::formatToken(m_selectedToken->decimals, ui->lineEditAmount->value(), false, BitcoinUnits::separatorAlways);

        QString questionString = tr("Are you sure you want to send? <br /><br />");
        questionString.append(tr("<b>%1 %2 </b> to ")
                              .arg(amountFormated).arg(QString::fromStdString(m_selectedToken->symbol)));
        questionString.append(tr("<br />%3 <br />")
                              .arg(QString::fromStdString(toAddress)));

        SendConfirmationDialog confirmationDialog(tr("Confirm send token."), questionString, 3, this);
        confirmationDialog.exec();
        QMessageBox::StandardButton retval = (QMessageBox::StandardButton)confirmationDialog.result();
        if(retval == QMessageBox::Yes)
        {
            m_tokenABI->transfer(toAddress, amountToSend, true);
            clearAll();
        }
    }
    else
    {
        QString message = tr("To send %1 you need LUX in address <br /> %2.")
                .arg(QString::fromStdString(m_selectedToken->symbol)).arg(QString::fromStdString(CBitcoinAddress(m_selectedToken->sender).ToString()));

        QMessageBox::warning(this, tr("Send token"), message);
    }
}

void SendTokenPage::setTokenData(std::string address, std::string sender, std::string symbol, int8_t decimals, std::string balance)
{
    m_selectedToken->address = address;
    m_selectedToken->sender = sender;
    m_selectedToken->symbol = symbol;
    m_selectedToken->decimals = decimals;
    m_selectedToken->balance = balance;

    ui->lineEditAmount->setDecimalUnits(decimals);
    ui->lineEditAmount->setTotalSupply(int256_t(balance));
}
