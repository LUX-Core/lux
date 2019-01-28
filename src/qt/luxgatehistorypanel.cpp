#include "luxgatehistorypanel.h"
#include "luxgatehistorymodel.h"
#include "ui_luxgatehistorypanel.h"
#include "walletmodel.h"
#include "optionsmodel.h"

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
        connect(opt_model, &OptionsModel::luxgateDecimalsChanged,
                tableModel, &LuxgateHistoryModel::slotSetDecimals);
    }
}

LuxgateHistoryPanel::~LuxgateHistoryPanel()
{
    delete ui;
}
