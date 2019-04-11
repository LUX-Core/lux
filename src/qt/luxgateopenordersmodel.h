#ifndef LUXGATEOPENORDERSMODEL_H
#define LUXGATEOPENORDERSMODEL_H

#include "luxgategui_global.h"

#include "luxgate/luxgate.h"
#include <QAbstractTableModel>

#include <memory>
#include <list>

class COrder;

QString stateToString(const COrder::State state);

class OrderView
{
public:
  OrderView() {}
  OrderView(const std::shared_ptr<const COrder> order);
  ~OrderView() {}
  std::string log() const { return sId.toStdString(); }
  qint64 creationTime;
  OrderId id;
  bool isBuy;
  QString sCreationTime;
  QString sType;
  QString sPrice;
  QString sAmount;
  QString sTotal;
  QString sState;
  QString sId;
};
Q_DECLARE_METATYPE(OrderView);

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

