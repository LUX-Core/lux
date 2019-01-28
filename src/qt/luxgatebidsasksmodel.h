#ifndef LUXGATEBIDSMODEL_H
#define LUXGATEBIDSMODEL_H

#include <QAbstractTableModel>
#include "luxgategui_global.h"
class LuxgateBidsAsksModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LuxgateBidsAsksModel(bool bBids, const Luxgate::Decimals & decimals, QObject *parent = nullptr);

    enum Columns{
        PriceColumn = 0, BaseColumn,
        QuoteColumn, NColumns
    };

    //convert value from Columns to columnNum
    int columnNum(Columns col) const;
    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setOrientation(bool bLeft);

public slots:
    void slotSetDecimals(const Luxgate::Decimals & decimals_);

private:
    Luxgate::Decimals decimals;
    bool bLeft;             //true - in left side, false - right
    const bool bBids;       //true - Bids, false - Asks
};

#endif // LUXGATEBIDSMODEL_H
