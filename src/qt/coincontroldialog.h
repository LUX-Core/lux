// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_COINCONTROLDIALOG_H
#define BITCOIN_QT_COINCONTROLDIALOG_H

#include "amount.h"

#include <QAbstractButton>
#include <QAction>
#include <QDialog>
#include <QList>
#include <QMenu>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QTreeWidgetItem>
class WalletModel;
class ClientModel;
class PlatformStyle;

class CCoinControl;
class CTxMemPool;

namespace Ui
{
class CoinControlDialog;
}

class CCoinControlWidgetItem : public QTreeWidgetItem
{
public:
    CCoinControlWidgetItem(QTreeWidget *parent, int type = Type) : QTreeWidgetItem(parent, type) {}
    CCoinControlWidgetItem(int type = Type) : QTreeWidgetItem(type) {}
    CCoinControlWidgetItem(QTreeWidgetItem *parent, int type = Type) : QTreeWidgetItem(parent, type) {}

    bool operator<(const QTreeWidgetItem &other) const;
};

class CoinControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CoinControlDialog(const PlatformStyle *platformStyle, QWidget* parent = 0);
    ~CoinControlDialog();
    void CheckDialogLablesUpdated();
    void setModel(WalletModel* model);
    void setClientModel(ClientModel* clientModel);

    // static because also called from sendcoinsdialog
    static void updateLabels(WalletModel*, QDialog*);
    static QString getPriorityLabel(double dPriority, double mempoolEstimatePriority);

    static QList<CAmount> payAmounts;
    static CCoinControl* coinControl;
    static int nSplitBlockDummy;
    static bool fSubtractFeeFromAmount;

private:
    Ui::CoinControlDialog* ui;
    WalletModel* model;
    ClientModel* clientModel;
    int sortColumn;
    Qt::SortOrder sortOrder;
    
    QMenu* contextMenu;
    QTreeWidgetItem* contextMenuItem;
    QAction* copyTransactionHashAction;
    QAction* lockAction;
    QAction* unlockAction;
    const PlatformStyle* platformStyle;
    QStringList toogleLockList;
    QString strPad(QString, int, QString);
    void sortView(int, Qt::SortOrder);
    void updateView();

    enum {
        COLUMN_CHECKBOX = 0,
        COLUMN_AMOUNT,
        COLUMN_LABEL,
        COLUMN_ADDRESS,
        COLUMN_DATE,
        COLUMN_CONFIRMATIONS,
        COLUMN_PRIORITY,
        COLUMN_TXHASH,
        COLUMN_VOUT_INDEX,
        COLUMN_PRIORITY_INT64,
        COLUMN_DARKSEND_ROUNDS,
        COLUMN_DATE_INT64
    };

    friend class CCoinControlWidgetItem;

private Q_SLOTS:
    void showMenu(const QPoint&);
    void copyAmount();
    void copyLabel();
    void copyAddress();
    void copyTransactionHash();
    void lockCoin();
    void unlockCoin();
    void clipboardQuantity();
    void clipboardAmount();
    void clipboardFee();
    void clipboardAfterFee();
    void clipboardBytes();
    void clipboardPriority();
    void clipboardLowOutput();
    void clipboardChange();
    void radioTreeMode(bool);
    void radioListMode(bool);
    void toggled(bool);
    void viewItemChanged(QTreeWidgetItem*, int);
    void headerSectionClicked(int);
    void buttonBoxClicked(QAbstractButton*);
    void buttonSelectAllClicked();
    void HideInputAutoSelection();
    void ShowInputAutoSelection();
    unsigned int SIZE_OF_TX();
    void tree_view_data_to_double(QString &str, double &val);
    bool TX_size_limit();
    void greater();
    void Less();
    void Equal();
    void select_50();
    void select_100();
    void select_250();
    void buttonToggleLockClicked();
    void updateLabelLocked();
public Q_SLOTS:
    void updateInfoInDialog();
};

#endif // BITCOIN_QT_COINCONTROLDIALOG_H
