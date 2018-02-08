// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_TRANSACTIONDESCDIALOG_H
#define BITCOIN_QT_TRANSACTIONDESCDIALOG_H

#include <QMainWindow>

namespace Ui
{
class MiningDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog showing transaction details. */
class MiningDialog : public QMainWindow
{
    Q_OBJECT

public:
    explicit MiningDialog(QWidget* parent = 0);
    ~MiningDialog();

private:
    Ui::MiningDialog* ui;
    bool m_NeverShown;
    int m_HistoryIndex;
    QStringList m_History;
};

#endif // BITCOIN_QT_TRANSACTIONDESCDIALOG_H
