// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miningdialog.h"
#include "guiutil.h"
#include "ui_miningdialog.h"

#include "transactiontablemodel.h"

#include <QModelIndex>
#include <QSettings>
#include <QString>

MiningDialog::MiningDialog(QWidget* parent) : QMainWindow(parent),
                                              ui(new Ui::MiningDialog),
                                              m_NeverShown(true),
                                              m_HistoryIndex(0)
{
    ui->setupUi(this);

    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());

    //QString desc = "Tra la la";//idx.data(TransactionTableModel::LongDescriptionRole).toString();
    //ui->detailText->setHtml(desc);
}

MiningDialog::~MiningDialog()
{
    delete ui;
}
