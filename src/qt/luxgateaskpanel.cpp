#include "luxgateaskpanel.h"
#include "luxgatedialog.h"
#include "luxgateasksmodel.h"
#include "ui_luxgateaskpanel.h"
#include "walletmodel.h"
#include "optionsmodel.h"

LuxgateAskPanel::LuxgateAskPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateAskPanel)
{
    ui->setupUi(this);
    ui->labelTotalCurrency->setText(" ( " + curCurrencyPair.baseCurrency + " )");
    ui->labelAveragePriceCurrency->setText(" ( " + curCurrencyPair.baseCurrency
    + "/" + curCurrencyPair.quoteCurrency + " )");
}

void LuxgateAskPanel::setModel(WalletModel *model)
{
    wal_model = model;
    OptionsModel * opt_model = wal_model->getOptionsModel();
    //init tableview
    {
        auto tableModel =new LuxgateAsksModel(opt_model->getBidsAsksDecimals(), this);
        ui->tableViewAsks->setModel(tableModel);
        QHeaderView * HeaderView = ui->tableViewAsks->horizontalHeader();
        HeaderView->setSectionResizeMode(QHeaderView::Stretch);
        HeaderView->setSectionsMovable(true);
        HeaderView->setFixedHeight(30);
        connect(opt_model, &OptionsModel::bidsAsksDecimalsChanged,
                tableModel, &LuxgateAsksModel::slotSetDecimals);
    }
}

LuxgateAskPanel::~LuxgateAskPanel()
{
    delete ui;
}
