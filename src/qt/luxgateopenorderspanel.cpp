#include "luxgateopenorderspanel.h"
#include "ui_luxgateopenorderspanel.h"

#include "luxgategui_global.h"
#include "luxgateopenordersmodel.h"
#include "luxgatepricedelegate.h"
#include "guiutil.h"
#include "walletmodel.h"
#include "optionsmodel.h"
#include "bitmexnetwork.h"

#include <QPixmap>
#include <QPushButton>

extern BitMexNetwork * bitMexNetw;

LuxgateOpenOrdersPanel::LuxgateOpenOrdersPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateOpenOrdersPanel)
{
    ui->setupUi(this);
    ui->labelBaseTotalCurrency->setPixmap(QPixmap(GUIUtil::currencyIcon(curCurrencyPair.baseCurrency))
    .scaledToHeight(ui->labelBaseTotalCurrency->height(),Qt::SmoothTransformation));
    ui->labelQuoteTotalCurrency->setPixmap(QPixmap(GUIUtil::currencyIcon(curCurrencyPair.quoteCurrency))
    .scaledToHeight(ui->labelBaseTotalCurrency->height(), Qt::SmoothTransformation));

    connect(ui->pushButtonAllCancel, &QPushButton::clicked,
            this, &LuxgateOpenOrdersPanel::slotAllCancelClicked);
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
        connect(tableModel, &LuxgateOpenOrdersModel::sigUpdateButtons,
                this, &LuxgateOpenOrdersPanel::updateButtonsCancel);
        updateButtonsCancel();

        ui->tableViewOpenOrders->setItemDelegateForColumn(LuxgateOpenOrdersModel::PriceColumn,
                                                       new LuxgatePriceDelegate(this));

        ui->tableViewOpenOrders->setCopyColumns(QMap<int, int>{
                {LuxgateTableView::PriceColumn, LuxgateOpenOrdersModel::PriceColumn},
                {LuxgateTableView::QtyColumn, LuxgateOpenOrdersModel::QtyColumn},
                {LuxgateTableView::RemainQtyColumn, LuxgateOpenOrdersModel::RemainQtyColumn},
                {LuxgateTableView::ValueColumn, LuxgateOpenOrdersModel::ValueColumn},
                {LuxgateTableView::DateOpenOrderColumn, LuxgateOpenOrdersModel::DateColumn}});
    }

}

void LuxgateOpenOrdersPanel::slotAllCancelClicked()
{
    bitMexNetw->requestCancelAllOrders();
}

void LuxgateOpenOrdersPanel::slotCancelClicked()
{
    QPushButton * but = qobject_cast<QPushButton *> (sender());
    int row = but->property("TableRow").toInt();
    auto tableModel =ui->tableViewOpenOrders->model();
    bitMexNetw->requestCancelOrder(tableModel->data(tableModel->index(row,0), LuxgateOpenOrdersModel::OrderIdRole).toByteArray());
}

void LuxgateOpenOrdersPanel::updateButtonsCancel()
{
    auto buttonsList = ui->tableViewOpenOrders->findChildren<QPushButton *>();
    foreach(auto but, buttonsList)
    but->deleteLater();

    auto tableModel =ui->tableViewOpenOrders->model();
    for(int iR=0; iR<tableModel->rowCount(); iR++) {
        auto button = new QPushButton(this);
        button->setText("Cancel");
        button->setIcon(QIcon(":/icons/quit"));
        button->setText("Cancel");
        button->setProperty("OpenOrdersCancel", true);
        button->setProperty("TableRow", iR);
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
