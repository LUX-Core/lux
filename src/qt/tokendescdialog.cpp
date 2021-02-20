#include "tokendescdialog.h"
#include "ui_tokendescdialog.h"
#include "guiutil.h"
#include "tokenfilterproxy.h"

#include <QModelIndex>
#include <QSettings>

TokenDescDialog::TokenDescDialog(const QModelIndex &idx, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::TokenDescDialog)
{
    ui->setupUi(this);

    setWindowTitle(tr("Details for %1").arg(idx.data(TokenTransactionTableModel::TxHashRole).toString()));
    QString desc = idx.data(TokenTransactionTableModel::LongDescriptionRole).toString();
    this->setStyleSheet(GUIUtil::loadStyleSheet());

    QSettings settings;
    if (settings.value("theme").toString() == "dark grey") {
        QString styleSheet = ".QPushButton { background-color: #262626; color:#fff; border: 1px solid #fff5f5; "
                                "padding-left:10px; padding-right:10px; min-height:25px; min-width:75px; }"
                                ".QPushButton:hover { background-color:#fff5f5 !important; color:#262626 !important; }"
                                ".QTextEdit { background-color: #262626; color:#fff; border: 1px solid #fff5f5;"
                                "padding-left:10px; padding-right:10px; min-height:25px; }";
        ui->buttonBox->setStyleSheet(styleSheet);
        ui->detailText->setStyleSheet(styleSheet);
    } else if (settings.value("theme").toString() == "dark blue") {
        QString styleSheet = ".QPushButton { background-color: #031d54; color:#fff; border: 1px solid #fff5f5; "
                                "padding-left:10px; padding-right:10px; min-height:25px; min-width:75px; }"
                                ".QPushButton:hover { background-color:#fff5f5 !important; color:#031d54 !important; }"
                                ".QTextEdit { background-color: #031d54; color:#fff; border: 1px solid #fff5f5;"
                                "padding-left:10px; padding-right:10px; min-height:25px; }";
        ui->buttonBox->setStyleSheet(styleSheet);
        ui->detailText->setStyleSheet(styleSheet);
    } else {
        //code here
    }

    ui->detailText->setHtml(desc);
}

TokenDescDialog::~TokenDescDialog()
{
    delete ui;
}
