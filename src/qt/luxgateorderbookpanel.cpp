#include "luxgateorderbookpanel.h"
#include "ui_luxgateorderbookpanel.h"
#include "luxgategui_global.h"

#include <QHBoxLayout>
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

    connect(ui->pushButtonReplaceBidsAsks, &QPushButton::clicked,
            this, &LuxgateOrderBookPanel::slotReplaceBidsAsks);
}

void LuxgateOrderBookPanel::setModel(WalletModel *model)
{
    wal_model = model;
    ui->frameAsks->setData(false, model);
    ui->frameBids->setData(true, model);
    slotReplaceBidsAsks();
    slotReplaceBidsAsks();
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
