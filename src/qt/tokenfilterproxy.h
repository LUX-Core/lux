#ifndef TOKENFILTERPROXY_H
#define TOKENFILTERPROXY_H

#include "amount.h"
#include "tokentransactiontablemodel.h"
#include <QDateTime>
#include <QSortFilterProxyModel>

#include <boost/multiprecision/cpp_int.hpp>
using namespace boost::multiprecision;

class TokenFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit TokenFilterProxy(QObject *parent = 0);

    /** Earliest date that can be represented (far in the past) */
    static const QDateTime MIN_DATE;
    /** Last date that can be represented (far in the future) */
    static const QDateTime MAX_DATE;
    /** Type filter bit field (all types) */
    static const quint32 ALL_TYPES = 0xFFFFFFFF;

    static quint32 TYPE(int type) { return 1<<type; }

    void setDateRange(const QDateTime &from, const QDateTime &to);
    void setAddressPrefix(const QString &addrPrefix);
    /**
      @note Type filter takes a bit field created with TYPE() or ALL_TYPES
     */
    void setTypeFilter(quint32 modes);
    void setMinAmount(const int256_t& minimum);
    void setName(const QString _name);

    /** Set maximum number of rows returned, -1 if unlimited. */
    void setLimit(int limit);

    int rowCount(const QModelIndex &parent = QModelIndex()) const;

protected:
    bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const;
    bool lessThan(const QModelIndex & left, const QModelIndex & right) const;

private:
    QDateTime dateFrom;
    QDateTime dateTo;
    QString addrPrefix;
    QString name;
    quint32 typeFilter;
    int256_t minAmount;
    int limitRows;
    bool showInactive;
};

#endif // TOKENFILTERPROXY_H
