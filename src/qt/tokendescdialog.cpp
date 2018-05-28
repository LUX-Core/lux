#include "tokendescdialog.h"
#include "ui_tokendescdialog.h"

#include "tokenfilterproxy.h"

#include <QModelIndex>

TokenDescDialog::TokenDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TokenDescDialog)
{
    ui->setupUi(this);
    setWindowTitle(tr("Details for %1").arg(idx.data(TokenTransactionTableModel::TxHashRole).toString()));
    QString desc = idx.data(TokenTransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
}

TokenDescDialog::~TokenDescDialog()
{
    delete ui;
}
