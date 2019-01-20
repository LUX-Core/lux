#include "luxgatebidpanel.h"
#include "luxgatedialog.h"
#include "luxgatebidsmodel.h"
#include "ui_luxgatebidpanel.h"

LuxgateBidPanel::LuxgateBidPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateBidPanel)
{
    ui->setupUi(this);
    ui->labelTotalCurrency->setText(" ( " + curCurrencyPair.quoteCurrency + " )");
    ui->labelAveragePriceCurrency->setText(" ( " + curCurrencyPair.baseCurrency
    + "/" + curCurrencyPair.quoteCurrency + " )");
}

void LuxgateBidPanel::setModel(WalletModel *model)
{
    wal_model = model;
    OptionsModel * opt_model = wal_model->getOptionsModel();
    //init tableview
    {
        auto tableModel =new LuxgateBidsModel(opt_model->getBidsAsksDecimals(), this);
        ui->tableViewBids->setModel(tableModel);
        QHeaderView * HeaderView = ui->tableViewBids->horizontalHeader();
        HeaderView->setSectionResizeMode(QHeaderView::Stretch);
        HeaderView->setSectionsMovable(true);
        HeaderView->setFixedHeight(30);
        connect(opt_model, &OptionsModel::bidsAsksDecimalsChanged,
                tableModel, &LuxgateBidsModel::slotSetDecimals);
    }
}

LuxgateBidPanel::~LuxgateBidPanel()
{
    delete ui;
}
