#ifndef LUXGATEOPENORDERSMODEL_H
#define LUXGATEOPENORDERSMODEL_H

#include "luxgategui_global.h"

#include "luxgate/luxgate.h"
#include <QAbstractTableModel>

#include <memory>
#include <set>

class COrder;

class LuxgateOpenOrdersModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    explicit LuxgateOpenOrdersModel(const Luxgate::Decimals & decimals, QObject *parent = nullptr);

    enum Columns{   DateColumn = 0, TypeColumn, 
                    PriceColumn,  BaseAmountColumn,
                    QuoteTotalColumn, StateColumn, CancelColumn,  NColumns };
    // Header:
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Basic functionality:
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

public slots:
    void slotSetDecimals(const Luxgate::Decimals & decimals_);
    void updateRow(const OrderId&, COrder::State);
    void addRow(std::shared_ptr<COrder>);
    void deleteRow(const OrderId&);
    void reset();
signals:
    void rowAdded();
private:
    Luxgate::Decimals decimals;
    struct NewestOrderComparator
    {
        bool operator()(const std::shared_ptr<const COrder>& a, const std::shared_ptr<const COrder>& b)
        {
            return *a != *b && a->OrderCreationTime() > b->OrderCreationTime();
        }
    };
    std::set<std::shared_ptr<const COrder>, NewestOrderComparator> openOrders;
    QString stateToString(const COrder::State state) const;
};

#endif // LUXGATEOPENORDERSMODEL_H

