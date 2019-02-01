#include "luxgatehistorypanel.h"
#include "ui_luxgatehistorypanel.h"

#include "luxgatehistorymodel.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "luxgatepricedelegate.h"

LuxgateHistoryPanel::LuxgateHistoryPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateHistoryPanel)
{
    ui->setupUi(this);
}

void LuxgateHistoryPanel::setModel(WalletModel *model)
{
    wal_model = model;
    OptionsModel * opt_model = wal_model->getOptionsModel();
    //init tableview
    {
        auto tableModel =new LuxgateHistoryModel(opt_model->getLuxgateDecimals(), this);
        ui->tableViewHistory->setModel(tableModel);
        QHeaderView * HeaderView = ui->tableViewHistory->horizontalHeader();
        HeaderView->setSectionResizeMode(QHeaderView::Stretch);
        HeaderView->setSectionsMovable(true);
        HeaderView->setFixedHeight(30);
        connect(opt_model, &OptionsModel::luxgateDecimalsChanged,
                tableModel, &LuxgateHistoryModel::slotSetDecimals);

        ui->tableViewHistory->setItemDelegateForColumn(LuxgateHistoryModel::PriceColumn,
                                                        new LuxgatePriceDelegate(this));

        ui->tableViewHistory->setCopyColumns(QMap<int, int>{
                {LuxgateTableView::PriceColumn, LuxgateHistoryModel::PriceColumn},
                {LuxgateTableView::BaseAmountColumn, LuxgateHistoryModel::BaseAmountColumn},
                {LuxgateTableView::QuoteTotalColumn, LuxgateHistoryModel::QuoteTotalColumn},
                {LuxgateTableView::DateCloseOrderColumn, LuxgateHistoryModel::DateColumn}});
    }
}

LuxgateHistoryPanel::~LuxgateHistoryPanel()
{
    delete ui;
}
