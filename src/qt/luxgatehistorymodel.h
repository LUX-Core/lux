#ifndef LUXGATEHISTORYMODEL_H
#define LUXGATEHISTORYMODEL_H

#include <QAbstractTableModel>
#include "luxgategui_global.h"

class LuxgateHistoryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LuxgateHistoryModel(const Luxgate::Decimals & decimals, QObject *parent = nullptr);

    enum Columns{   PriceColumn,  BaseAmountColumn,
        QuoteTotalColumn, DateColumn,  NColumns };
    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void slotSetDecimals(const Luxgate::Decimals & decimals_);

private:
    Luxgate::Decimals decimals;
};

#endif // LUXGATEHISTORYMODEL_H