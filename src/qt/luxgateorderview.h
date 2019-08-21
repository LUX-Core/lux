// Copyright (c) 2019 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef QT_LUXGATEORDERVIEW_H
#define QT_LUXGATEORDERVIEW_H

#include "luxgate/order.h"

#include <QMetaType>
#include <QString>

QString stateToString(const COrder::State state);

class OrderView
{
public:
    OrderView() {}
    OrderView(const std::shared_ptr<const COrder> order);
    OrderView(const std::shared_ptr<COrder> order);
    ~OrderView() {}
    std::string log() const { return sId.toStdString(); }
    qint64 creationTime;
    qint64 completionTime;
    OrderId id;
    bool isBuy;
    QString sCreationTime;
    QString sCompletionTime;
    QString sType;
    QString sPrice;
    QString sAmount;
    QString sTotal;
    QString sState;
    QString sId;
};
Q_DECLARE_METATYPE(OrderView);

#endif // QT_LUXGATEORDERVIEW_H

