#include "sendtokenpage.h"
#include "ui_sendtokenpage.h"

#include "main.h"
#include "clientmodel.h"
#include "optionsmodel.h"
#include "utilmoneystr.h"
#include "token.h"
#include "bitcoinunits.h"
#include "wallet.h"
#include "walletmodel.h"
#include "guiutil.h"
#include "sendcoinsdialog.h"
#include "createcontract.h"
#include "sendtocontract.h"
#include "bitcoinaddressvalidator.h"
#include "timedata.h"
#include "uint256.h"

#include <QSettings>

static const CAmount SINGLE_STEP = 0.00000001*COIN;

struct SelectedToken {
    std::string address;
    std::string sender;
    std::string symbol;
    int8_t decimals;
    std::string balance;
    SelectedToken():
            decimals(0)
    {}
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
    ui->lineEditPayTo->setToolTip(tr("The address that will receive the tokens."));
    ui->lineEditAmount->setToolTip(tr("The amount in LSR Token to send."));
    ui->lineEditDescription->setToolTip(tr("Optional description for transaction."));
    m_tokenABI = new Token();
    m_selectedToken = new SelectedToken();

    // Set defaults
    ui->lineEditGasPrice->setValue(DEFAULT_GAS_PRICE);
    ui->lineEditGasPrice->setSingleStep(SINGLE_STEP);
    ui->lineEditGasLimit->setMaximum(DEFAULT_GAS_LIMIT_OP_SEND);
    ui->lineEditGasLimit->setValue(DEFAULT_GAS_LIMIT_OP_SEND);
    ui->confirmButton->setEnabled(false);
	connect(ui->lineEditGasPrice,
        &BitcoinAmountField::valueChanged,
        [&](){
            ui->labelAvailInputInfo->setText(GUIUtil::handleAvailableInputInfo(m_model, ui->lineEditGasPrice->value()));
        }
    );  
	QSettings settings;
	
	if (settings.value("theme").toString() == "dark grey") {
		QString styleSheet = ".QPushButton { background-color: #262626; color:#fff; border: 1px solid #fff5f5; "
								"padding-left:10px; padding-right:10px; min-height:25px; min-width:75px; }"
								".QPushButton:hover { background-color:#fff5f5 !important; color:#262626 !important; }";					
		ui->confirmButton->setStyleSheet(styleSheet);
		ui->clearButton->setStyleSheet(styleSheet);
	} else if (settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QPushButton { background-color: #031d54; color:#fff; border: 1px solid #fff5f5; "
								"padding-left:10px; padding-right:10px; min-height:25px; min-width:75px; }"
								".QPushButton:hover { background-color:#fff5f5; color:#031d54; }";					
		ui->confirmButton->setStyleSheet(styleSheet);
		ui->clearButton->setStyleSheet(styleSheet);
	} else { 
		//code here
	}

    // Connect signals with slots
    connect(ui->lineEditPayTo, SIGNAL(textChanged(QString)), SLOT(on_updateConfirmButton()));
    connect(ui->lineEditAmount, SIGNAL(valueChanged()), SLOT(on_updateConfirmButton()));
    connect(ui->confirmButton, SIGNAL(clicked()), SLOT(on_confirmClicked()));

    ui->lineEditPayTo->setCheckValidator(new BitcoinAddressCheckValidator(parent));
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
    connect(m_model, &WalletModel::balanceChanged, 
        [&](const CAmount& balance,
            const CAmount&,
            const CAmount&,
            const CAmount&,
            const CAmount&,
            const CAmount&,
            const CAmount&){
                ui->labelAvailInputInfo->setText(GUIUtil::handleAvailableInputInfo(m_model, ui->lineEditGasPrice->value()));
        }
    );
}

void SendTokenPage::setClientModel(ClientModel *_clientModel)
{
    m_clientModel = _clientModel;

    if (m_clientModel)
    {
        connect(m_clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(on_numBlocksChanged(int)));
        on_numBlocksChanged(1);
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

bool SendTokenPage::isValidAddress()
{
    ui->lineEditPayTo->checkValidity();
    return ui->lineEditPayTo->isValid();
}

bool SendTokenPage::isDataValid()
{
    bool dataValid = true;

    if(!isValidAddress())
        dataValid = false;
    if(!ui->lineEditAmount->validate())
        dataValid = false;
    if(ui->lineEditAmount->value(0) <= 0)
    {
        ui->lineEditAmount->setValid(false);
        dataValid = false;
    }
    return dataValid;
}

void SendTokenPage::on_clearButton_clicked()
{
    clearAll();
}

void SendTokenPage::on_numBlocksChanged(int newHeight)
{
    if(m_clientModel)
    {
        if (lastUpdatedHeight < newHeight) {
            uint64_t blockGasLimit = 0;
            uint64_t minGasPrice = 0;
            uint64_t nGasPrice = 0;
            m_clientModel->getGasInfo(blockGasLimit, minGasPrice, nGasPrice);

            ui->lineEditGasLimit->setToolTip(
                    tr("Gas limit: Default = %1, Max = %2.").arg(DEFAULT_GAS_LIMIT_OP_SEND).arg(blockGasLimit));
            ui->lineEditGasPrice->setToolTip(tr("Gas price: LUX/gas unit. Default = %1, Min = %2.").arg(
                    QString::fromStdString(FormatMoney(DEFAULT_GAS_PRICE))).arg(
                    QString::fromStdString(FormatMoney(minGasPrice))));
            ui->lineEditGasPrice->setMinimum(minGasPrice);
            ui->lineEditGasLimit->setMaximum(blockGasLimit);
            lastUpdatedHeight = newHeight;
        }
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
    if(!isDataValid()) return;

    WalletModel::UnlockContext ctx(m_model->requestUnlock());
    if(!ctx.isValid()) {
        return;
    }

    if(m_model && m_model->isUnspentAddress(m_selectedToken->sender))
    {
        int unit = m_model->getOptionsModel()->getDisplayUnit();
        uint64_t gasLimit = ui->lineEditGasLimit->value();
        CAmount gasPrice = ui->lineEditGasPrice->value();
        std::string label = ui->lineEditDescription->text().trimmed().toStdString();

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
            if(m_tokenABI->transfer(toAddress, amountToSend, true))
            {
                CTokenTx tokenTx;
                tokenTx.strContractAddress = m_selectedToken->address;
                tokenTx.strSenderAddress = m_selectedToken->sender;
                tokenTx.strReceiverAddress = toAddress;
                dev::u256 nValue(amountToSend);
                tokenTx.nValue = u256Touint(nValue);
                tokenTx.transactionHash = uint256S(m_tokenABI->getTxId());
                tokenTx.strLabel = label;
                m_model->addTokenTxEntry(tokenTx);
            }
            clearAll();
        }
    }
    else
    {
        QString message = tr("To send %1 you need LUX in address <br /> %2.")
                .arg(QString::fromStdString(m_selectedToken->symbol)).arg(QString::fromStdString(m_selectedToken->sender));

        QMessageBox::warning(this, tr("Send token"), message);
    }
}

void SendTokenPage::setTokenData(std::string address, std::string sender, std::string symbol, int8_t decimals, std::string balance)
{
    // Update data with the current token
    int decimalDiff = decimals - m_selectedToken->decimals;
    m_selectedToken->address = address;
    m_selectedToken->sender = sender;
    m_selectedToken->symbol = symbol;
    m_selectedToken->decimals = decimals;
    m_selectedToken->balance = balance;

    // Convert values for different number of decimals
    int256_t totalSupply(balance);
    int256_t value(ui->lineEditAmount->value());
    if(value != 0)
    {
        for(int i = 0; i < decimalDiff; i++)
        {
            value *= 10;
        }
        for(int i = decimalDiff; i < 0; i++)
        {
            value /= 10;
        }
    }
    if(value > totalSupply)
    {
        value = totalSupply;
    }

    // Update the amount field with the current token data
    ui->lineEditAmount->clear();
    ui->lineEditAmount->setDecimalUnits(decimals);
    ui->lineEditAmount->setTotalSupply(totalSupply);
    if(value != 0)
    {
        ui->lineEditAmount->setValue(value);
    }
}
