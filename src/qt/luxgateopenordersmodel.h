#ifndef LUXGATEOPENORDERSMODEL_H
#define LUXGATEOPENORDERSMODEL_H

#include "luxgategui_global.h"
#include "luxgate_options.h"
#include "bitmex_shared.h"

#include <QAbstractTableModel>

#include <vector>
#include <memory>

class COrder;

class LuxgateOpenOrdersModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LuxgateOpenOrdersModel(const Luxgate::Decimals & decimals, QObject *parent = nullptr);

    enum Columns{   DateColumn = 0, TypeColumn,
                    PriceColumn,  QtyColumn, RemainQtyColumn,
                    ValueColumn,  StatusColumn, CancelColumn,  NColumns };

    enum UserRoles{OrderIdRole = Luxgate::IndividualRole};
    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void slotSetDecimals(const Luxgate::Decimals & decimals_);
    void slotUpdateData(const LuxgateOpenOrdersData & data);
    //void update();
private:
    Luxgate::Decimals decimals;
    QVector<LuxgateOpenOrdersRow> luxgateData;
    //std::vector<std::shared_ptr<COrder>> openOrders;
signals:
    void sigUpdateButtons();
};

#endif // LUXGATEOPENORDERSMODEL_H