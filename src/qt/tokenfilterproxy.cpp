#include "tokenfilterproxy.h"

#include <cstdlib>

#include <QDateTime>

// Earliest date that can be represented (far in the past)
const QDateTime TokenFilterProxy::MIN_DATE = QDateTime::fromTime_t(0);
// Last date that can be represented (far in the future)
const QDateTime TokenFilterProxy::MAX_DATE = QDateTime::fromTime_t(0xFFFFFFFF);

TokenFilterProxy::TokenFilterProxy(QObject *parent) :
    QSortFilterProxyModel(parent),
    dateFrom(MIN_DATE),
    dateTo(MAX_DATE),
    addrPrefix(),
    name(),
    typeFilter(ALL_TYPES),
    minAmount(0),
    limitRows(-1)
{

}

void TokenFilterProxy::setDateRange(const QDateTime &from, const QDateTime &to)
{
    this->dateFrom = from;
    this->dateTo = to;
    invalidateFilter();
}

void TokenFilterProxy::setAddressPrefix(const QString &_addrPrefix)
{
    this->addrPrefix = _addrPrefix;
    invalidateFilter();
}

void TokenFilterProxy::setTypeFilter(quint32 modes)
{
    this->typeFilter = modes;
    invalidateFilter();
}

void TokenFilterProxy::setMinAmount(const int256_t &minimum)
{
    this->minAmount = minimum;
    invalidateFilter();
}

void TokenFilterProxy::setName(const QString _name)
{
    this->name = _name;
    invalidateFilter();
}

void TokenFilterProxy::setLimit(int limit)
{
    this->limitRows = limit;
}

int TokenFilterProxy::rowCount(const QModelIndex &parent) const
{
    if(limitRows != -1)
    {
        return std::min(QSortFilterProxyModel::rowCount(parent), limitRows);
    }
    else
    {
        return QSortFilterProxyModel::rowCount(parent);
    }
}

bool TokenFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    int type = index.data(TokenTransactionTableModel::TypeRole).toInt();
    QDateTime datetime = index.data(TokenTransactionTableModel::DateRole).toDateTime();
    QString address = index.data(TokenTransactionTableModel::AddressRole).toString();
    int256_t amount(index.data(TokenTransactionTableModel::AmountRole).toString().toStdString());
    amount = amount < 0 ? - amount : amount;
    QString tokenName = index.data(TokenTransactionTableModel::NameRole).toString();

    if(!(TYPE(type) & typeFilter))
        return false;
    if(datetime < dateFrom || datetime > dateTo)
        return false;
    if (!address.contains(addrPrefix, Qt::CaseInsensitive))
        return false;
    if(amount < minAmount)
        return false;
    if(!name.isEmpty() && name != tokenName)
        return false;

    return true;
}
