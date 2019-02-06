#ifndef LUXGATEHISTORYMODEL_H
#define LUXGATEHISTORYMODEL_H

#include <QAbstractTableModel>
#include <QVector>
#include "bitmex_shared.h"
#include "luxgate_options.h"

class LuxgateHistoryModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LuxgateHistoryModel(const Luxgate::Decimals & decimals, QObject *parent = nullptr);

    enum Columns{   TickColumn,  PriceColumn,  SizeColumn,
                    DateColumn,  SideColumn,
                    NColumns };
    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void slotSetDecimals(const Luxgate::Decimals & decimals_);
    void slotUpdateData(const LuxgateHistoryData & data);
private:
    Luxgate::Decimals decimals;
    QVector<LuxgateHistoryRow> luxgateData;

};

#endif // LUXGATEHISTORYMODEL_H