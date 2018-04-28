#include "sendtokenpage.h"
#include "ui_sendtokenpage.h"

//#include "validation.h"
#include "main.h"
#include "clientmodel.h"
#include "utilmoneystr.h"

static const CAmount SINGLE_STEP = static_cast<const CAmount>(0.00000001 * COIN);

SendTokenPage::SendTokenPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SendTokenPage)
{
    ui->setupUi(this);
    ui->labelPayTo->setToolTip(tr("The address that will receive the tokens."));
    ui->labelAmount->setToolTip(tr("The amount in LSR Token to send."));
    ui->labelDescription->setToolTip(tr("Optional description for transaction."));
    // Set defaults
    ui->lineEditGasPrice->setValue(DEFAULT_GAS_PRICE);
    ui->lineEditGasPrice->setSingleStep(SINGLE_STEP);
    ui->lineEditGasLimit->setMaximum(DEFAULT_GAS_LIMIT_OP_SEND);
    ui->lineEditGasLimit->setValue(DEFAULT_GAS_LIMIT_OP_SEND);
}

SendTokenPage::~SendTokenPage()
{
    delete ui;
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