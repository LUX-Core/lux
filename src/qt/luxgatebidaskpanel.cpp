#include "luxgatebidaskpanel.h"
#include "ui_luxgatebidaskpanel.h"

#include "luxgategui_global.h"
#include "luxgatebidsasksmodel.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "luxgatepricedelegate.h"


LuxgateBidAskPanel::LuxgateBidAskPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateBidAskPanel)
{
    ui->setupUi(this);
    ui->widgetTotalData->hide();
    ui->labelTotalCurrency->setText(" " + curCurrencyPair.baseCurrency);
    ui->labelAveragePriceCurrency->setText(" " + curCurrencyPair.baseCurrency
                                           + "/" + curCurrencyPair.quoteCurrency);
}

void LuxgateBidAskPanel::setRowsDisplayed (int RowsDisplayed_)
{
    auto tableModel = qobject_cast<LuxgateBidsAsksModel *>(ui->tableViewBidsAsks->model());
    tableModel->setRowsDisplayed(RowsDisplayed_);
}

void LuxgateBidAskPanel::slotGroup(bool bGroup, double dbStep)
{
    auto tableModel = qobject_cast<LuxgateBidsAsksModel *>(ui->tableViewBidsAsks->model());
    tableModel->slotGroup(bGroup, dbStep);
}

void LuxgateBidAskPanel::setData(bool bBids_, WalletModel *model)
{
    bBids = bBids_;
    wal_model = model;
    OptionsModel * opt_model = wal_model->getOptionsModel();
    //init tableview
    {
        auto tableModel =new LuxgateBidsAsksModel(bBids, opt_model->getLuxgateDecimals(), this);
        ui->tableViewBidsAsks->setModel(tableModel);

        QHeaderView * HeaderView = ui->tableViewBidsAsks->horizontalHeader();
        HeaderView->setSectionResizeMode(QHeaderView::Stretch);
        HeaderView->setSectionsMovable(true);
        HeaderView->setFixedHeight(30);

        connect(opt_model, &OptionsModel::luxgateDecimalsChanged,
                tableModel, &LuxgateBidsAsksModel::slotSetDecimals);

        ui->tableViewBidsAsks->setItemDelegateForColumn(tableModel->columnNum(LuxgateBidsAsksModel::PriceColumn),
                                                        new LuxgatePriceDelegate(this));

        ui->tableViewBidsAsks->setCopyColumns(QMap<int, int>{
                {LuxgateTableView::PriceColumn, tableModel->columnNum(LuxgateBidsAsksModel::PriceColumn)},
                {LuxgateTableView::SizeColumn, tableModel->columnNum(LuxgateBidsAsksModel::SizeColumn)},
                {LuxgateTableView::TotalColumn, tableModel->columnNum(LuxgateBidsAsksModel::TotalColumn)}});
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

void LuxgateBidAskPanel::setOrientation(bool bLeft) {
    //change orientation in widgetTotalData
    auto totalLayout = ui->widgetTotalData->layout();
    totalLayout->removeWidget(ui->widgetAverage);
    totalLayout->removeWidget(ui->widgetNumAsks);
    totalLayout->removeWidget(ui->widgetTotal);
    delete totalLayout;
    QHBoxLayout *newTotalLayout = new QHBoxLayout(this);
    if (bLeft) {
        newTotalLayout->addWidget(ui->widgetAverage);
        newTotalLayout->addWidget(ui->widgetTotal);
        newTotalLayout->addWidget(ui->widgetNumAsks);
    } else {
        newTotalLayout->addWidget(ui->widgetNumAsks);
        newTotalLayout->addWidget(ui->widgetTotal);
        newTotalLayout->addWidget(ui->widgetAverage);
    }
    newTotalLayout->setStretch(0, 1);
    newTotalLayout->setStretch(1, 1);
    newTotalLayout->setStretch(2, 1);
    ui->widgetTotalData->setLayout(newTotalLayout);

    auto tableModel = qobject_cast<LuxgateBidsAsksModel *>(ui->tableViewBidsAsks->model());
    ui->tableViewBidsAsks->setItemDelegateForColumn(tableModel->columnNum(LuxgateBidsAsksModel::PriceColumn),
                                                    ui->tableViewBidsAsks->itemDelegate());
    tableModel->setOrientation(bLeft);
    ui->tableViewBidsAsks->setItemDelegateForColumn(tableModel->columnNum(LuxgateBidsAsksModel::PriceColumn),
                                                    new LuxgatePriceDelegate(this));
}

LuxgateBidAskPanel::~LuxgateBidAskPanel()
{
    delete ui;
}
