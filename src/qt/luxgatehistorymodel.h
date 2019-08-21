// Copyright (c) 2019 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUXGATEHISTORYMODEL_H
#define LUXGATEHISTORYMODEL_H

#include "luxgategui_global.h"
#include "luxgateorderview.h"

#include <QAbstractTableModel>

class LuxgateHistoryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LuxgateHistoryModel(const Luxgate::Decimals &decimals, QObject *parent = nullptr);

    enum Columns { CompleteDateColumn = 0, IdColumn, TypeColumn,
                   PriceColumn, BaseAmountColumn, QuoteTotalColumn,
                   StateColumn, NColumns };
    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void slotSetDecimals(const Luxgate::Decimals & decimals_);
    void updateTable();

private:
    Luxgate::Decimals decimals;
    std::list<OrderView> historyOrders;
};

#endif // LUXGATEHISTORYMODEL_H

