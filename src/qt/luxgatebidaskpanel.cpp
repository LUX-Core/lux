#include "luxgatebidaskpanel.h"
#include "luxgategui_global.h"
#include "luxgatebidsasksmodel.h"
#include "ui_luxgatebidaskpanel.h"
#include "walletmodel.h"
#include "optionsmodel.h"

LuxgateBidAskPanel::LuxgateBidAskPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateBidAskPanel)
{
    ui->setupUi(this);
    ui->labelTotalCurrency->setText(" " + curCurrencyPair.baseCurrency);
    ui->labelAveragePriceCurrency->setText(" " + curCurrencyPair.baseCurrency
                                           + "/" + curCurrencyPair.quoteCurrency);
}

void LuxgateBidAskPanel::setData(bool bBids_, WalletModel *model)
{
    bBids = bBids_;
    wal_model = model;
    OptionsModel * opt_model = wal_model->getOptionsModel();
    //init tableview
    {
        auto tableModel =new LuxgateBidsAsksModel(bBids, opt_model->getBidsAsksDecimals(), this);
        ui->tableViewBidsAsks->setModel(tableModel);
        QHeaderView * HeaderView = ui->tableViewBidsAsks->horizontalHeader();
        HeaderView->setSectionResizeMode(QHeaderView::Stretch);
        HeaderView->setSectionsMovable(true);
        HeaderView->setFixedHeight(30);
        connect(opt_model, &OptionsModel::luxgateDecimalsChanged,
                tableModel, &LuxgateBidsAsksModel::slotSetDecimals);
    }

    //init other widgets
    if(bBids) {
        ui->BidAskNum->setProperty("OrderColor", "BIDS");
        ui->labelBidsAsksType->setText(" BIDS");
    }
    else {
        ui->BidAskNum->setProperty("OrderColor", "ASKS");
        ui->labelBidsAsksType->setText(" ASKS");
    }
    ui->BidAskNum->style()->unpolish(ui->BidAskNum);
    ui->BidAskNum->style()->polish(ui->BidAskNum);
    ui->labelBidsAsksType->style()->unpolish(ui->labelBidsAsksType);
    ui->labelBidsAsksType->style()->polish(ui->labelBidsAsksType);
}

void LuxgateBidAskPanel::setOrientation(bool bLeft)
{
    //change orientation in widgetTotalData
    auto totalLayout = ui->widgetTotalData->layout();
    totalLayout->removeWidget(ui->widgetAverage);
    totalLayout->removeWidget(ui->widgetNumAsks);
    totalLayout->removeWidget(ui->widgetTotal);
    delete totalLayout;
    QHBoxLayout * newTotalLayout = new QHBoxLayout(this);
    if(bLeft){
        newTotalLayout->addWidget(ui->widgetAverage);
        newTotalLayout->addWidget(ui->widgetTotal);
        newTotalLayout->addWidget(ui->widgetNumAsks);
    }
    else{
        newTotalLayout->addWidget(ui->widgetNumAsks);
        newTotalLayout->addWidget(ui->widgetTotal);
        newTotalLayout->addWidget(ui->widgetAverage);
    }
    newTotalLayout->setStretch(0,1);
    newTotalLayout->setStretch(1,1);
    newTotalLayout->setStretch(2,1);
    ui->widgetTotalData->setLayout(newTotalLayout);

    qobject_cast<LuxgateBidsAsksModel *>(ui->tableViewBidsAsks->model())->setOrientation(bLeft);
}

LuxgateBidAskPanel::~LuxgateBidAskPanel()
{
    delete ui;
}
