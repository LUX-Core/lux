// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactiondescdialog.h"
#include "guiutil.h"
#include "ui_transactiondescdialog.h"

#include "transactiontablemodel.h"

#include <QModelIndex>
#include <QSettings>
#include <QString>

TransactionDescDialog::TransactionDescDialog(const QModelIndex& idx, QWidget* parent) : QDialog(parent),
                                                                                        ui(new Ui::TransactionDescDialog)
{
    Qt::WindowFlags flags;
    ui->setupUi(this);
    flags = this->windowFlags() ^ Qt::WindowContextHelpButtonHint;
    this->setWindowFlags(flags);
    this->setModal(true);
    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());
    setWindowTitle(tr("Details for %1").arg(idx.data(TransactionTableModel::TxIDRole).toString()));
    QString desc = idx.data(TransactionTableModel::LongDescriptionRole).toString();
    ui->detailText->setHtml(desc);
}

TransactionDescDialog::~TransactionDescDialog()
{
    delete ui;
}
