#include "luxgateorderbookpanel.h"
#include "ui_luxgateorderbookpanel.h"

#include "luxgategui_global.h"
#include "bitmexnetwork.h"
#include "optionsmodel.h"
#include "walletmodel.h"
#include "math.h"

#include <QHBoxLayout>
#include <QStringList>

extern BitMexNetwork * bitMexNetw;

#define defNotGroupText "Don't group"

LuxgateOrderBookPanel::LuxgateOrderBookPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateOrderBookPanel)
{
    ui->setupUi(this);

    ui->labelTitleLeft->setText("BIDS");
    ui->labelTitleLeft->setProperty("OrderColor", "BIDS");
    ui->labelTitleLeft->style()->unpolish(ui->labelTitleLeft);
    ui->labelTitleLeft->style()->polish(ui->labelTitleLeft);
    ui->labelTitleRight->setText("ASKS");
    ui->labelTitleRight->setProperty("OrderColor", "ASKS");
    ui->labelTitleRight->style()->unpolish(ui->labelTitleRight);
    ui->labelTitleRight->style()->polish(ui->labelTitleRight);

    ui->comboBoxRowsNum->addItems(QStringList() << "5" << "10" << "20"
                                                << "50" << "100" << "500" << "1000");
    ui->comboBoxRowsNum->setCurrentText("1000");
    connect(ui->comboBoxRowsNum, QOverload<const QString &>::of(&QComboBox::activated),
            this, [=](const QString &text){slotRowsDisplayChanged(text);});

    connect(ui->comboBoxGroup, QOverload<const QString &>::of(&QComboBox::activated),
            this, [=](const QString &text){slotGroupsChanged(text);});

    connect(ui->pushButtonReplaceBidsAsks, &QPushButton::clicked,
            this, &LuxgateOrderBookPanel::slotReplaceBidsAsks);

    //bitmex
    connect(bitMexNetw, &BitMexNetwork::sigBXBT_Index,
            this, &LuxgateOrderBookPanel::slotBXBT_Index);
    connect(bitMexNetw, &BitMexNetwork::sigIndices,
            this, [this](const LuxgateIndices & data)
            {
                QString color;
                QString array;
                if("PlusTick"==data.lastTickDirection) {
                    color =  "#267E00";
                    array = "<b>&#8593; </b>";
                }
                if("MinusTick"==data.lastTickDirection) {
                    color =  "#red";
                    array = "<b>&#8595; </b>";
                }
                ui->labelLastPrice->setText("<font color=\"" + color + "\">" + array + "</font><span style=\"font-family: " + qApp->font().family() +",monospace\"><font color=\"" + color + "\"><b>"
                                            + QString::number(data.dbLastPrice, 'f',decimals.price) + "</b></font></span>");
                ui->labelMarkPrice->setText("<span style=\"font-family: " + qApp->font().family() +",monospace\"><font color=\"black\"><b>"
                + QString::number(data.dbMarkPrice, 'f',decimals.price) + "</b></font></span>");
            });

}

void LuxgateOrderBookPanel::slotGroupsChanged (const QString &text)
{
    if(tr(defNotGroupText) == text) {
        ui->frameBids->slotGroup(false);
        ui->frameAsks->slotGroup(false);
    }
    else {
        ui->frameBids->slotGroup(true, text.toDouble());
        ui->frameAsks->slotGroup(true, text.toDouble());
    }
}

void LuxgateOrderBookPanel::slotRowsDisplayChanged (const QString &text)
{
    int rows = text.toInt();
    ui->frameBids->setRowsDisplayed(rows);
    ui->frameAsks->setRowsDisplayed(rows);
}

void LuxgateOrderBookPanel::slotBXBT_Index(double dbIndex)
{
    ui->labelBXBT_Index->setText("<span style=\"font-family: " + qApp->font().family() +",monospace\"><font color=\"grey\"><b>"
    + QString::number(dbIndex, 'f',decimals.price) + "</b></font></span>");
}

void LuxgateOrderBookPanel::setModel(WalletModel *model)
{
    wal_model = model;
    ui->frameAsks->setData(false, model);
    ui->frameBids->setData(true, model);
    slotReplaceBidsAsks();
    slotReplaceBidsAsks();

    OptionsModel * opt_model = wal_model->getOptionsModel();
    slotDecimalsChanged(opt_model->getLuxgateDecimals());

    connect(opt_model, &OptionsModel::luxgateDecimalsChanged,
            this, &LuxgateOrderBookPanel::slotDecimalsChanged);
}

void LuxgateOrderBookPanel::slotDecimalsChanged(const Luxgate::Decimals & decimals_)
{
    decimals = decimals_;
    ui->comboBoxGroup->clear();
    double factor = pow(0.1, decimals.price);
    ui->comboBoxGroup->addItems(QStringList() << tr(defNotGroupText)
                                              << QString::number(factor*5,'f',decimals.price)
                                              << QString::number(factor*10,'f',decimals.price)
                                              << QString::number(factor*25,'f',decimals.price)
                                              << QString::number(factor*50,'f',decimals.price)
                                              << QString::number(factor*100,'f',decimals.price)
                                              << QString::number(factor*250,'f',decimals.price)
                                              << QString::number(factor*500,'f',decimals.price));
    ui->comboBoxGroup->setCurrentIndex(0);
}

void LuxgateOrderBookPanel::slotReplaceBidsAsks()
{
    QString prevLeft = ui->labelTitleLeft->text();
    QString prevRight = ui->labelTitleRight->text();
    ui->labelTitleLeft->setText(prevRight);
    ui->labelTitleRight->setText(prevLeft);
    if("BIDS" == ui->labelTitleLeft->property("OrderColor").toString()) {
        ui->labelTitleLeft->setProperty("OrderColor", "ASKS");
        ui->labelTitleRight->setProperty("OrderColor", "BIDS");
    }
    else {
        ui->labelTitleLeft->setProperty("OrderColor", "BIDS");
        ui->labelTitleRight->setProperty("OrderColor", "ASKS");
    }
    ui->labelTitleLeft->style()->unpolish(ui->labelTitleLeft);
    ui->labelTitleLeft->style()->polish(ui->labelTitleLeft);
    ui->labelTitleRight->style()->unpolish(ui->labelTitleRight);
    ui->labelTitleRight->style()->polish(ui->labelTitleRight);

    auto mainLayout = ui->widgetMain->layout();
    int asksIndex = mainLayout->indexOf(ui->frameAsks);
    mainLayout->removeWidget(ui->frameAsks);
    mainLayout->removeWidget(ui->frameBids);
    delete mainLayout;
    QHBoxLayout * newMainLayout = new QHBoxLayout(this);
    if(0 == asksIndex)
    {
        newMainLayout->addWidget(ui->frameBids);
        newMainLayout->addWidget(ui->frameAsks);
        ui->frameAsks->setOrientation(false);
        ui->frameBids->setOrientation(true);
    }
    else
    {
        newMainLayout->addWidget(ui->frameAsks);
        newMainLayout->addWidget(ui->frameBids);
        ui->frameAsks->setOrientation(true);
        ui->frameBids->setOrientation(false);
    }
    newMainLayout->setStretch(0,1);
    newMainLayout->setStretch(1,1);
    ui->widgetMain->setLayout(newMainLayout);

}

LuxgateOrderBookPanel::~LuxgateOrderBookPanel()
{
    delete ui;
}
