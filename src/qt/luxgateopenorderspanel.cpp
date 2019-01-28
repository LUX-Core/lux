#include "luxgateopenorderspanel.h"
#include "ui_luxgateopenorderspanel.h"

#include "luxgategui_global.h"
#include "luxgateopenordersmodel.h"
#include "guiutil.h"
#include "walletmodel.h"
#include "optionsmodel.h"

#include <QPixmap>
#include <QPushButton>

LuxgateOpenOrdersPanel::LuxgateOpenOrdersPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateOpenOrdersPanel)
{
    ui->setupUi(this);
    ui->labelBaseTotalCurrency->setPixmap(QPixmap(GUIUtil::currencyIcon(curCurrencyPair.baseCurrency))
    .scaledToHeight(ui->labelBaseTotalCurrency->height(),Qt::SmoothTransformation));
    ui->labelQuoteTotalCurrency->setPixmap(QPixmap(GUIUtil::currencyIcon(curCurrencyPair.quoteCurrency))
    .scaledToHeight(ui->labelBaseTotalCurrency->height(), Qt::SmoothTransformation));
}

void LuxgateOpenOrdersPanel::setModel(WalletModel *model)
{
    wal_model = model;
    OptionsModel * opt_model = wal_model->getOptionsModel();
    //init tableview
    {
        auto tableModel =new LuxgateOpenOrdersModel(opt_model->getLuxgateDecimals(), this);
        ui->tableViewOpenOrders->setModel(tableModel);
        QHeaderView * HeaderView = ui->tableViewOpenOrders->horizontalHeader();
        HeaderView->setSectionResizeMode(QHeaderView::Stretch);
        HeaderView->setSectionsMovable(true);
        HeaderView->setFixedHeight(30);
        connect(opt_model, &OptionsModel::luxgateDecimalsChanged,
                tableModel, &LuxgateOpenOrdersModel::slotSetDecimals);
        updateButtonsCancel();
    }

}

void LuxgateOpenOrdersPanel::slotCancelClicked()
{
    int n=0;
    n++;
}

void LuxgateOpenOrdersPanel::updateButtonsCancel()
{
    auto tableModel =ui->tableViewOpenOrders->model();
    auto buttonsList = ui->tableViewOpenOrders->findChildren<QPushButton *>();
    foreach(auto but, buttonsList)
        but->deleteLater();
    for(int iR=0; iR<tableModel->rowCount(); iR++) {
        auto button = new QPushButton(this);
        button->setText("Cancel");
        button->setIcon(QIcon(":/icons/quit"));
        button->setText("Cancel");
        button->setProperty("OpenOrdersCancel", true);
        ui->tableViewOpenOrders->setIndexWidget(tableModel->index(iR, LuxgateOpenOrdersModel::CancelColumn),
                                                button);
        connect(button, &QPushButton::clicked,
                this, &LuxgateOpenOrdersPanel::slotCancelClicked);
    }
}

LuxgateOpenOrdersPanel::~LuxgateOpenOrdersPanel()
{
    delete ui;
}
