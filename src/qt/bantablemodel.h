// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_BANTABLEMODEL_H
#define BITCOIN_QT_BANTABLEMODEL_H

#include "main.h"
#include "net.h"

#include <QAbstractTableModel>
#include <QStringList>

class ClientModel;
class BanTablePriv;

QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE

struct CCombinedBan {
    CSubNet subnet;
    CBanEntry banEntry;
};

class BannedNodeLessThan
{
public:
    BannedNodeLessThan(int nColumn, Qt::SortOrder fOrder) :
            column(nColumn), order(fOrder) {}
    bool operator()(const CCombinedBan& left, const CCombinedBan& right) const;

private:
    int column;
    Qt::SortOrder order;
};

/**
   Qt model providing information about connected peers, similar to the
   "getpeerinfo" RPC call. Used by the rpc console UI.
 */
class BanTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit BanTableModel(ClientModel *parent = 0);
    ~BanTableModel();
    void stopAutoRefresh();
    void startAutoRefresh();

    enum ColumnIndex {
        Address = 0,
        Bantime = 1
    };

    /** @name Methods overridden from QAbstractTableModel
        @{*/
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;
    void sort(int column, Qt::SortOrder order);
    bool shouldShow();
    /*@}*/

public slots:
            void refresh();

private:
    ClientModel *clientModel;
    QStringList columns;
    std::unique_ptr<BanTablePriv> priv;
    QTimer *timer;
};

#endif // BITCOIN_QT_BANTABLEMODEL_H
