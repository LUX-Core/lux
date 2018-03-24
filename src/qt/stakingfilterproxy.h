#ifndef STAKINGFILTERPROXY_H
#define STAKINGFILTERPROXY_H

#include <QSortFilterProxyModel>

class StakingFilterProxy : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit StakingFilterProxy(QObject *parent = 0);
};

#endif // STAKINGFILTERPROXY_H
