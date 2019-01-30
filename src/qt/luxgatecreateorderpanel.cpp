#include "luxgatecreateorderpanel.h"
#include "ui_luxgatecreateorderpanel.h"

#include "luxgategui_global.h"
#include "walletmodel.h"
#include "optionsmodel.h"

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

    //signals-slots connections
    connect(ui->horizontalSliderPrice, &QSlider::valueChanged,
            this, &LuxgateCreateOrderPanel::slotValueSliderPriceChanged);
    connect(ui->lineEditPrice, &QLineEdit::textEdited,
            this, &LuxgateCreateOrderPanel::slotPriceEditChanged);
    connect(ui->lineEditPrice, &QLineEdit::textChanged,
            this, &LuxgateCreateOrderPanel::slotCalcNewTotal);
    connect(ui->lineEditQuantity, &QLineEdit::textChanged,
            this, &LuxgateCreateOrderPanel::slotCalcNewTotal);
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
    auto price = ui->lineEditPrice->text().toDouble();
    auto quantity = ui->lineEditQuantity->text().toDouble();
    ui->lineEditTotal->setText(QString::number(price*quantity,'f',decimals.price));
    ui->pushButtonBuySell->setEnabled(true);
    ui->pushButtonBuySell->setToolTip("");
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
        slotUpdateAverageHighLowBidAsk(0.00017949, 0.00010979);
    else
        slotUpdateAverageHighLowBidAsk(0.00009364, 0.00010803);
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
