#ifndef LUXGATEOPENORDERSMODEL_H
#define LUXGATEOPENORDERSMODEL_H

#include <QAbstractTableModel>
#include "optionsmodel.h"

class LuxgateOpenOrdersModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LuxgateOpenOrdersModel(const OptionsModel::Decimals & decimals, QObject *parent = nullptr);

    enum Columns{   DateColumn = 0, TypeColumn, SideColumn,
                    PriceColumn,  BaseAmountColumn,
                    QuoteTotalColumn, CancelColumn,  NColumns };
    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
            void slotSetDecimals(const OptionsModel::Decimals & decimals_);

private:
    OptionsModel::Decimals decimals;
};

#endif // LUXGATEOPENORDERSMODEL_H