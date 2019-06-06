// Copyright (c) 2019 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUXGATEOPENORDERSMODEL_H
#define LUXGATEOPENORDERSMODEL_H

#include "luxgategui_global.h"
#include "luxgateorderview.h"

#include <QAbstractTableModel>

#include <memory>
#include <list>

class LuxgateOpenOrdersModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LuxgateOpenOrdersModel(const Luxgate::Decimals & decimals, QObject *parent = nullptr);

    enum Columns { DateColumn = 0, IdColumn, TypeColumn,
                   PriceColumn, BaseAmountColumn, QuoteTotalColumn,
                   StateColumn, CancelColumn, NColumns };
    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void slotSetDecimals(const Luxgate::Decimals & decimals_);
    void updateRow(const quint64, const int);
    void addRow(const OrderView);
    void deleteRow(const quint64);
    void reset();
signals:
    void rowAdded();
private:
    Luxgate::Decimals decimals;
    struct NewestOrderComparator
    {
        bool operator()(const OrderView& a, const OrderView& b)
        {
            return a.creationTime > b.creationTime;
        }
    };
    std::list<OrderView> openOrders;
};

#endif // LUXGATEOPENORDERSMODEL_H

