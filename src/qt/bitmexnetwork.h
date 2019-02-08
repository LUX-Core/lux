#ifndef BITMEXNETWORK_H
#define BITMEXNETWORK_H

#include <QByteArray>
#include <QObject>
#include "bitmex_shared.h"

class QNetworkAccessManager;

class BitMexNetwork : public QObject
{
    Q_OBJECT
public:
    BitMexNetwork(QObject *parent=Q_NULLPTR);

    //requests
    void requestCancelOrder(const QByteArray & orderId);
    void requestCancelAllOrders();
private:
    QNetworkAccessManager * nam;
    QByteArray apiKey;
    QByteArray apiPrivate;
    const int apiExpiresInterval {5};
    void readConfig();
    QByteArray apiExpires();  //create timestamp to api-expires
    QByteArray apiSignature(const QByteArray & verb, const QByteArray &  path,
            const QByteArray & apiExpires, const QByteArray & data = QByteArray());  //create api-signature

    //requests
    void requestGlobalHistory();
    void requestOrderBook();
    void requestBXBT_Index();
    void requestIndices();
    void requestOpenOrders();
private slots:
    void allRequests();
    void replyGlobalHistory();
    void replyOrderBook();
    void replyBXBT_Index();
    void replyIndices();
    void replyOpenOrders();
    void replyCancelOrder();
    void replyCancelAllOrders();
signals:
    void sigGlobalHistoryData(const LuxgateHistoryData & data);
    void sigOrderBookAsksData(const LuxgateOrderBookData & data);
    void sigOrderBookBidsData(const LuxgateOrderBookData & data);
    void sigOpenOrdersData(const LuxgateOpenOrdersData & data);
    void sigBXBT_Index(double dbIndex);
    void sigIndices(const LuxgateIndices & data);
};

#endif // BITMEXNETWORK_H
