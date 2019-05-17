#include "luxgatecreateorderpanel.h"
#include "ui_luxgatecreateorderpanel.h"

#include "luxgategui_global.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "rpcprotocol.h"
#include "core_io.h"
#include "luxgate/blockchainclient.h"

#include <QRegExpValidator>

LuxgateCreateOrderPanel::LuxgateCreateOrderPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateCreateOrderPanel)
{
    ui->setupUi(this);
    // ui->pushButtonBuySell->setEnabled(false);
    // ui->pushButtonBuySell->setToolTip("Invalid Price");
    ui->labelPriceDimension->setText(curCurrencyPair.baseCurrency + "/" + curCurrencyPair.quoteCurrency);
    ui->labelQuantityDimension->setText(curCurrencyPair.baseCurrency);
    ui->labelTotalDimension->setText(curCurrencyPair.quoteCurrency);

    //set validators
    auto regExp = new QRegExpValidator(QRegExp("^\\d*\\.\\d*$"), this);
    ui->lineEditPrice->setValidator(regExp);
    ui->lineEditQuantity->setValidator(regExp);
    ui->lineEditTotal->setValidator(regExp);

    // Signals-slots connections

    // The slots parse user-typed value to internal 
    connect(ui->lineEditPrice, &QLineEdit::textChanged, this, &LuxgateCreateOrderPanel::slotPriceChanged);
    connect(ui->lineEditQuantity, &QLineEdit::textChanged, this, &LuxgateCreateOrderPanel::slotQuantityChanged);
    // Recalculate total after changing amount or quantity
    connect(ui->lineEditPrice, &QLineEdit::textChanged, this, &LuxgateCreateOrderPanel::slotCalcNewTotal);
    connect(ui->lineEditQuantity, &QLineEdit::textChanged, this, &LuxgateCreateOrderPanel::slotCalcNewTotal);

    connect(ui->pushButtonBuySell, &QPushButton::clicked, this, &LuxgateCreateOrderPanel::slotBuySellClicked);

    currentPrice = 0;
    currentQuantity = 0;
    ui->lineEditQuantity->setText("0.0");
    ui->lineEditPrice->setText("0.0");
}


AmountParsingResult AmountFromQString(const QString& value, CAmount& amount)
{
    if (!ParseFixedPoint(value.toStdString(), 8, &amount))
        return INVALID;
    if (!MoneyRange(amount))
        return OUT_OF_RANGE;
    return OK;
}

void LuxgateCreateOrderPanel::updateWidgetToolTip(const AmountParsingResult result, QWidget* widget)
{
    switch (result) {
        case OK: widget->setToolTip(""); break;
        case INVALID: widget->setToolTip("Invalid number format"); break;
        case OUT_OF_RANGE: widget->setToolTip("The number is out of range"); break;
    }
}
void LuxgateCreateOrderPanel::slotPriceChanged(const QString& text)
{
    auto status = AmountFromQString(text, currentPrice);
    ui->pushButtonBuySell->setEnabled(status == OK);
    updateWidgetToolTip(status, ui->lineEditPrice);
}

void LuxgateCreateOrderPanel::slotQuantityChanged(const QString& text)
{
    auto status = AmountFromQString(text, currentQuantity);
    ui->pushButtonBuySell->setEnabled(status == OK);
    updateWidgetToolTip(status, ui->lineEditQuantity);
}

void LuxgateCreateOrderPanel::slotBuySellClicked()
{
    UniValue params;
    params.setArray();
    params.push_backV({
            UniValue("LUX/BTC"), 
            UniValue(bBuy ? "buy" : "sell"),
            UniValue(Luxgate::StrFromAmount(currentQuantity)),
            UniValue(Luxgate::StrFromAmount(currentPrice))
            });
    try {
        auto orderId = createorder(params, false);
        LogPrintf("GUI: order has been created: %s\n", orderId.write());
    } catch (UniValue e) {
        QMessageBox::critical(0, "Runaway exception",
                QString("A error occured while sending new order\n\n") + QString::fromStdString(e["message"].getValStr()));
    }
}

void LuxgateCreateOrderPanel::slotCalcNewTotal()
{
    ui->lineEditTotal->setText(QString::fromStdString(Luxgate::StrFromAmount(currentQuantity * currentPrice / COIN)));
}

void LuxgateCreateOrderPanel::setModel(WalletModel *model)
{
    OptionsModel * opt_model = model->getOptionsModel();
    decimals = opt_model->getLuxgateDecimals();
    connect(opt_model, &OptionsModel::luxgateDecimalsChanged,
            this, [this](Luxgate::Decimals decimals_){decimals=decimals_;});
}

void LuxgateCreateOrderPanel::priceChangeUpdateGUI()
{
    float realValue = ui->lineEditPrice->text().toDouble();
    ui->lineEditPrice->setProperty("Status", "Normal");
    if((realValue > qMax(average, highLow) && !bBuy)
        || (realValue < qMin(average, highLow) && bBuy))
        ui->lineEditPrice->setProperty("Status", "Expensive");
    if((realValue < qMin(average, highLow) && !bBuy)
       || (realValue > qMax(average, highLow) && bBuy))
        ui->lineEditPrice->setProperty("Status", "Cheap");

    ui->lineEditPrice->style()->unpolish(ui->lineEditPrice);
    ui->lineEditPrice->style()->polish(ui->lineEditPrice);
}

void LuxgateCreateOrderPanel::setBuySell(bool _bBuy)
{
    bBuy = _bBuy;
    if(bBuy) {
        ui->pushButtonBuySell->setText("BUY");
        ui->labelFeeDimension->setText(curCurrencyPair.baseCurrency);
    }
    else {
        ui->pushButtonBuySell->setText("SELL");
        ui->labelFeeDimension->setText(curCurrencyPair.quoteCurrency);
    }



    //get balance
    if(bBuy) {
        ClientPtr client;
        if (luxgate.FindClient("BTC", client)) {
            double dbBalance = 0;
            if (client->GetBalance(dbBalance))
                ui->lineEditTotal->setText(QString::number(dbBalance));
        }
    }

}

float LuxgateCreateOrderPanel::minSliderPriceReal()
{
    if(bBuy)
        return  highLow - (average - highLow)/2;
    else
        return average - (highLow - average)/2;
}

float LuxgateCreateOrderPanel::maxSliderPriceReal()
{
    if(bBuy)
        return  average + (average - highLow)/2;
    else
        return  highLow + (highLow - average)/2;
}

void LuxgateCreateOrderPanel::slotValueSliderPriceChanged(int value)
{
    float realValue = (maxSliderPriceReal() - minSliderPriceReal()) * value/100 + minSliderPriceReal();
    ui->lineEditPrice->setText(QString::number(realValue,'f',decimals.price));

    priceChangeUpdateGUI();
}

LuxgateCreateOrderPanel::~LuxgateCreateOrderPanel()
{
    delete ui;
}
