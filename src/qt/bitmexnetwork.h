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
    void readConfig();

    //requests
    void requestGlobalHistory();

    void requestOrderBook();

    void requestBXBT_Index();
    void requestIndices();
private slots:
    void allRequests();
    void replyGlobalHistory();
    void replyOrderBook();
    void replyBXBT_Index();
    void replyIndices();
signals:
    void sigGlobalHistoryData(const LuxgateHistoryData & data);
    void sigOrderBookAsksData(const LuxgateOrderBookData & data);
    void sigOrderBookBidsData(const LuxgateOrderBookData & data);
    void sigBXBT_Index(double dbIndex);
    void sigIndices(const LuxgateIndices & data);
};

#endif // BITMEXNETWORK_H
