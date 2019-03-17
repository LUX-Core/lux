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

    //signals-slots connections

    // Two-way binding of slider and price value
    connect(ui->horizontalSliderPrice, &QSlider::valueChanged, this, &LuxgateCreateOrderPanel::slotValueSliderPriceChanged);
    connect(ui->lineEditPrice, &QLineEdit::textEdited, this, &LuxgateCreateOrderPanel::slotPriceEditChanged);
    // The slots parse user-typed value to internal 
    connect(ui->lineEditPrice, &QLineEdit::textChanged, this, &LuxgateCreateOrderPanel::slotPriceChanged);
    connect(ui->lineEditQuantity, &QLineEdit::textChanged, this, &LuxgateCreateOrderPanel::slotQuantityChanged);
    // Recalculate total after changing amount or quantity
    connect(ui->lineEditPrice, &QLineEdit::textChanged, this, &LuxgateCreateOrderPanel::slotCalcNewTotal);
    connect(ui->lineEditQuantity, &QLineEdit::textChanged, this, &LuxgateCreateOrderPanel::slotCalcNewTotal);

    connect(ui->pushButtonBuySell, &QPushButton::clicked, this, &LuxgateCreateOrderPanel::slotBuySellClicked);
}


AmountParsingResult AmountFromQString(const QString& value, CAmount& amount)
{
    if (!ParseFixedPoint(value.toStdString(), 8, &amount))
        return INVALID;
    if (!MoneyRange(amount))
        return OUT_OF_RANGE;
    return OK;
}

std::string StrFromAmount(const CAmount& amount)
{
    bool sign = amount < 0;
    int64_t n_abs = (sign ? -amount : amount);
    int64_t quotient = n_abs / COIN;
    int64_t remainder = n_abs % COIN;
    return strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder);
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
            UniValue(StrFromAmount(currentQuantity)),
            UniValue(StrFromAmount(currentPrice))
            });
    try {
        auto orderId = createorder(params, false);
        LogPrintf("GUI: order has been created: %s\n", orderId.write());
    } catch (UniValue e) {
        QMessageBox::critical(0, "Runaway exception",
                QString("A error occured while sending new order\n\n") + QString::fromStdString(e["message"].getValStr()));
    }
}

void LuxgateCreateOrderPanel::slotPriceEditChanged(const QString & text)
{
    float value = text.toDouble();
    int sliderValue =  100 * (value - minSliderPriceReal())/(maxSliderPriceReal() - minSliderPriceReal());
    bool oldState = ui->horizontalSliderPrice->blockSignals(true);
    ui->horizontalSliderPrice->setValue(sliderValue);
    ui->horizontalSliderPrice->blockSignals(oldState);
    priceChangeUpdateGUI();
}

void LuxgateCreateOrderPanel::slotCalcNewTotal()
{
    ui->lineEditTotal->setText(QString::fromStdString(StrFromAmount(currentQuantity * currentPrice / COIN)));
}

void LuxgateCreateOrderPanel::setModel(WalletModel *model)
{
    OptionsModel * opt_model = model->getOptionsModel();
    decimals = opt_model->getLuxgateDecimals();
    connect(opt_model, &OptionsModel::luxgateDecimalsChanged,
            this, [this](Luxgate::Decimals decimals_){decimals=decimals_;});
}

void LuxgateCreateOrderPanel::slotUpdateAverageHighLowBidAsk(float average_, float highLow_)
{
    average = average_;
    highLow = highLow_;

    auto formatText = [](QString priceText, QString color, QString tytleText)
    {
        return "<html><head/><body><p style=\"line-height:10\" align=\"center\">" +
                priceText + "</p><p style=\"line-height:10\" align=\"center\">" + tytleText + "</font></p></body></html>";
    };

    if(!bBuy) {
        ui->labelLeftPrice->setText(formatText(Luxgate::priceFormattedText(QString::number(average,'f',decimals.price), true), "#267E00", "Average bid"));
        ui->labelRightPrice->setText(formatText(Luxgate::priceFormattedText(QString::number(highLow,'f',decimals.price), true), "#267E00", "Highest bid"));
    }
    else {
        ui->labelLeftPrice->setText(formatText(Luxgate::priceFormattedText(QString::number(highLow,'f',decimals.price), false), "red", "Lowest ask"));
        ui->labelRightPrice->setText(formatText(Luxgate::priceFormattedText(QString::number(average,'f',decimals.price), false), "red", "Average ask"));
    }
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

    //test
    if(bBuy)
        slotUpdateAverageHighLowBidAsk(0.00010000, 0.00030000);
    else
        slotUpdateAverageHighLowBidAsk(0.00010000, 0.00030000);


    //get balance
    if(bBuy) {
        if (blockchainClientPool.count("BTC")) {
            double dbBalance = 0;
            bool bBalance = blockchainClientPool["BTC"]->GetBalance(dbBalance);
            if (bBalance)
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
