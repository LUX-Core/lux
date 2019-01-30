#include "luxgatecreateorderpanel.h"
#include "ui_luxgatecreateorderpanel.h"

#include "luxgategui_global.h"

#include <QRegExpValidator>

LuxgateCreateOrderPanel::LuxgateCreateOrderPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateCreateOrderPanel)
{
    ui->setupUi(this);
    ui->pushButtonBuySell->setEnabled(false);
    ui->pushButtonBuySell->setToolTip("Invalid Price");
    ui->labelPriceDimension->setText(curCurrencyPair.baseCurrency + "/" + curCurrencyPair.quoteCurrency);
    ui->labelQuantityDimension->setText(curCurrencyPair.baseCurrency);
    ui->labelTotalDimension->setText(curCurrencyPair.quoteCurrency);

    //set validators
    auto regExp = new QRegExpValidator(QRegExp("^\\d*\\.\\d*$"), this);
    ui->lineEditPrice->setValidator(regExp);
    ui->lineEditQuantity->setValidator(regExp);
    ui->lineEditTotal->setValidator(regExp);
}

void LuxgateCreateOrderPanel::setBidAsk(bool _bBid)
{
    bBid = _bBid;
    if(bBid) {
        ui->pushButtonBuySell->setText("BUY");
        ui->labelFeeDimension->setText(curCurrencyPair.baseCurrency);
    }
    else {
        ui->pushButtonBuySell->setText("SELL");
        ui->labelFeeDimension->setText(curCurrencyPair.quoteCurrency);
    }
}

LuxgateCreateOrderPanel::~LuxgateCreateOrderPanel()
{
    delete ui;
}
