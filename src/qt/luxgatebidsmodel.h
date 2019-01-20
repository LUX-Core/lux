#ifndef LUXGATEBIDSMODEL_H
#define LUXGATEBIDSMODEL_H

#include <QAbstractTableModel>
#include "optionsmodel.h"

class LuxgateBidsModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LuxgateBidsModel(const OptionsModel::Decimals & decimals, QObject *parent = nullptr);

    enum Columns{
        PriceColumn = 0, BaseColumn,
        QuoteColumn, NColumns
    };

    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    // Add data:
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    // Remove data:
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

public slots:
    void slotSetDecimals(const OptionsModel::Decimals & decimals_);

private:
    OptionsModel::Decimals decimals;

};

#endif // LUXGATEBIDSMODEL_H
