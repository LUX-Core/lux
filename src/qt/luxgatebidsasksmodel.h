#ifndef LUXGATEBIDSMODEL_H
#define LUXGATEBIDSMODEL_H

#include <QAbstractTableModel>
#include "luxgategui_global.h"
#include "bitmex_shared.h"
#include "luxgate_options.h"
class LuxgateBidsAsksModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LuxgateBidsAsksModel(bool bBids, const Luxgate::Decimals & decimals, QObject *parent = nullptr);

    enum Columns{
        PriceColumn = 0, SizeColumn,
        TotalColumn, NColumns
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

    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    void setOrientation(bool bLeft);
    void setRowsDisplayed (int RowsDisplayed_);
public slots:
    void slotSetDecimals(const Luxgate::Decimals & decimals_);
    void slotUpdateData(const LuxgateOrderBookData & data);
    void slotGroup(bool bGroup, double dbStep = 0.f);
private:
    LuxgateOrderBookData luxgateData;
    LuxgateOrderBookData luxgateDataGroup;
    Luxgate::Decimals decimals;
    bool bLeft;             //true - in left side, false - right
    const bool bBids;       //true - Bids, false - Asks
    bool bGroup{false};     //true - group price, false - dont group
    double dbStepGroup {0.f};
    void groupData();
    int nRowsDisplayed {Luxgate::defaultDisplayRows};
};

#endif // LUXGATEBIDSMODEL_H
