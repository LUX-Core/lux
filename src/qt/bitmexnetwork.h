#ifndef BITMEXNETWORK_H
#define BITMEXNETWORK_H

#include <QString>
#include <QObject>
#include "bitmex_shared.h"

class QNetworkAccessManager;

class BitMexNetwork : public QObject
{
    Q_OBJECT
public:
    BitMexNetwork(QObject *parent=Q_NULLPTR);
private:
    QNetworkAccessManager * nam;
    QString apiKey;
    QString apiPrivate;
    const int apiExpiresInterval {5};

    void requestGlobalHistory();

    void requestOrderBook();
    int depthOrderBook {25};

    void readConfig();
private slots:
    void allRequests();
    void replyGlobalHistory();
    void replyOrderBook();
signals:
    void sigGlobalHistoryData(const LuxgateHistoryData & data);
    void sigOrderBookAsksData(const LuxgateOrderBookData & data);
    void sigOrderBookBidsData(const LuxgateOrderBookData & data);
};

#endif // BITMEXNETWORK_H
