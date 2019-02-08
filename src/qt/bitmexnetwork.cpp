#include "bitmexnetwork.h"

#include "util.h"
#include "luxgategui_global.h"

#include <QNetworkAccessManager>
#include <QTimer>
#include <QByteArray>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QFile>
#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QDebug>
#include <QBuffer>


BitMexNetwork::BitMexNetwork(QObject *parent):
    QObject(parent)
{
    readConfig();
    nam = new QNetworkAccessManager(this);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &BitMexNetwork::allRequests);
    timer->start(15000);

    allRequests();
}

//create timestamp to api-expires
QByteArray BitMexNetwork::apiExpires()
{
    QByteArray res;
    res.append(QString::number(QDateTime::currentSecsSinceEpoch() + 30));
    return res;
}

//create api-signature
QByteArray BitMexNetwork::apiSignature(const QByteArray & verb, const QByteArray & path,
                                const QByteArray & apiExpires,  const QByteArray & data)
{
    QByteArray message = verb + path + apiExpires + data;
    return QMessageAuthenticationCode::hash(message, apiPrivate, QCryptographicHash::Sha256).toHex();
}

void BitMexNetwork::requestIndices()
{
    QNetworkRequest request(QUrl("https://testnet.bitmex.com/api/v1/instrument/indices"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    auto reply = nam->get(request);
    connect(reply, &QNetworkReply::finished,
            this, &BitMexNetwork::replyIndices);
}

void BitMexNetwork::requestCancelOrder(const QByteArray & orderId)
{
    QNetworkRequest request(QUrl("https://testnet.bitmex.com/api/v1/order"));
    request.setRawHeader("HTTP", "DELETE");

    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    auto apiExp = apiExpires();
    request.setRawHeader("api-expires", apiExp);
    request.setRawHeader("api-key", apiKey);

    QBuffer *buff = new QBuffer;
    QByteArray requestData = "orderID=" + orderId;
    const char * dataRequest =  requestData.data();
    buff->setData(requestData);
    buff->open(QIODevice::ReadOnly);

    request.setRawHeader("api-signature", apiSignature("DELETE", "/api/v1/order", apiExp, requestData));
    auto reply = nam->sendCustomRequest(request, "DELETE", buff);
    buff->setParent(reply);
    connect(reply, &QNetworkReply::finished,
            this, &BitMexNetwork::replyCancelOrder);
}

void BitMexNetwork::requestCancelAllOrders()
{
    QNetworkRequest request(QUrl("https://testnet.bitmex.com/api/v1/order/all"));

    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    request.setRawHeader("Content-Type", "application/x-www-form-urlencoded");
    auto apiExp = apiExpires();
    request.setRawHeader("api-expires", apiExp);
    request.setRawHeader("api-key", apiKey);

    request.setRawHeader("api-signature", apiSignature("DELETE", "/api/v1/order/all", apiExp));
    auto reply = nam->deleteResource(request);
    connect(reply, &QNetworkReply::finished,
            this, &BitMexNetwork::replyCancelAllOrders);
}

void BitMexNetwork::replyCancelAllOrders()
{
    auto reply = qobject_cast<QNetworkReply *>(sender());
    auto baData = reply->readAll();
    const char * data = baData.data();
    int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(200 != httpCode)
        return;
    requestOpenOrders();
}

void BitMexNetwork::replyCancelOrder()
{
    auto reply = qobject_cast<QNetworkReply *>(sender());
    auto baData = reply->readAll();
    const char * data = baData.data();
    int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(200 != httpCode)
        return;
    requestOpenOrders();
}

void BitMexNetwork::replyIndices()
{
    auto reply = qobject_cast<QNetworkReply *>(sender());
    if(200 != reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
        return;
    QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    auto array = document.array();
    LuxgateIndices data;
    for(int i=0; i<array.size(); i++)
    {
        auto obj = array[i].toObject();
        if(".BXBT" == obj.value("symbol").toString())
        {
            data.dbLastPrice = obj.value("lastPrice").toDouble();
            data.dbMarkPrice = obj.value("markPrice").toDouble();
            data.lastTickDirection = obj.value("lastTickDirection").toString();
            break;
        }
    }
    emit sigIndices(data);
}

void BitMexNetwork::readConfig()
{
    QString configPath =  QStringFromPath(GetDataDir(false)) + "/bitmex.config";
    QFile fileConfig(configPath);
    if(fileConfig.open(QIODevice::ReadOnly)) {
        auto data = fileConfig.readAll();
        QJsonDocument document = QJsonDocument::fromJson(data);
        auto object = document.object();
        apiKey.clear();
        apiKey.append(object.value("apiKey").toString());
        apiPrivate.clear();
        apiPrivate.append(object.value("apiPrivate").toString());
    }
}

void BitMexNetwork::requestGlobalHistory()
{
    QNetworkRequest request(QUrl("https://testnet.bitmex.com/api/v1/trade?symbol=XBT&count=500&reverse=true"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    auto reply = nam->get(request);
    connect(reply, &QNetworkReply::finished,
            this, &BitMexNetwork::replyGlobalHistory);
}

void BitMexNetwork::requestBXBT_Index()
{
    QNetworkRequest request(QUrl("https://testnet.bitmex.com/api/v1/trade?symbol=.BXBT&count=1&columns=price&reverse=true"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    auto reply = nam->get(request);
    connect(reply, &QNetworkReply::finished,
            this, &BitMexNetwork::replyBXBT_Index);
}

void BitMexNetwork::requestOrderBook()
{
    QNetworkRequest request(QUrl("https://testnet.bitmex.com/api/v1/orderBook/L2?symbol=XBT&depth=1000"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    auto reply = nam->get(request);
    connect(reply, &QNetworkReply::finished,
            this, &BitMexNetwork::replyOrderBook);
}

void BitMexNetwork::requestOpenOrders()
{
    QNetworkRequest request(QUrl("https://testnet.bitmex.com/api/v1/order?symbol=XBTUSD&filter=%7B%22open%22%3A%20true%7D&count=100&reverse=true"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    auto apiExp = apiExpires();
    request.setRawHeader("api-expires", apiExp);
    request.setRawHeader("api-key", apiKey);
    request.setRawHeader("api-signature", apiSignature("GET","/api/v1/order?symbol=XBTUSD&filter=%7B%22open%22%3A%20true%7D&count=100&reverse=true", apiExp));
    auto reply = nam->get(request);
    connect(reply, &QNetworkReply::finished,
            this, &BitMexNetwork::replyOpenOrders);
}

void BitMexNetwork::replyOpenOrders()
{
    auto reply = qobject_cast<QNetworkReply *>(sender());
    int httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    auto baData = reply->readAll();
    const char * data = baData.data();
    if(200 != httpCode)
        return;
    QJsonDocument document = QJsonDocument::fromJson(baData);
    auto array = document.array();
    LuxgateOpenOrdersData luxgateData;
    for(int i=0; i<array.size(); i++)
    {
        LuxgateOpenOrdersRow row;
        row.dateTimeOpen = QDateTime::fromString(array[i].toObject().value("transactTime").toString(),
                                             "yyyy-MM-ddTHH:mm:ss.zzzZ");
        row.dbPrice = array[i].toObject().value("price").toDouble();
        row.dbQty = array[i].toObject().value("orderQty").toDouble();
        row.dbValue = row.dbQty/row.dbPrice;
        row.type = array[i].toObject().value("ordType").toString();
        row.bBuy = ("Buy" == array[i].toObject().value("side").toString());
        row.dbLeavesQty = array[i].toObject().value("leavesQty").toDouble();
        row.status = array[i].toObject().value("ordStatus").toString();
        row.orderId.clear();
        row.orderId.append(array[i].toObject().value("orderID").toString());
        luxgateData.append(row);
    }
    emit sigOpenOrdersData(luxgateData);
}

void BitMexNetwork::allRequests()
{
    requestGlobalHistory();
    requestOrderBook();
    requestBXBT_Index();
    requestIndices();
    requestOpenOrders();
}

void BitMexNetwork::replyBXBT_Index()
{
    auto reply = qobject_cast<QNetworkReply *>(sender());
    if(200 != reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
        return;
    QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    auto array = document.array();
    double dbIndex = array[0].toObject().value("price").toDouble();
    emit sigBXBT_Index(dbIndex);
}

void BitMexNetwork::replyGlobalHistory()
{
    auto reply = qobject_cast<QNetworkReply *>(sender());
    if(200 != reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
        return;
    QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    auto array = document.array();
    LuxgateHistoryData data;
    for(int i=0; i<array.size(); i++)
    {
        LuxgateHistoryRow row;
        row.dateTime = QDateTime::fromString(array[i].toObject().value("timestamp").toString(),
                                             "yyyy-MM-ddTHH:mm:ss.zzzZ");
        row.bBuy = ("Buy" == array[i].toObject().value("side").toString());
        row.dbPrice = array[i].toObject().value("price").toDouble();
        row.dbSize = array[i].toObject().value("size").toDouble();
        if("PlusTick" == array[i].toObject().value("tickDirection").toString())
            row.arrayDirection = 1;
        else if("MinusTick" == array[i].toObject().value("tickDirection").toString())
            row.arrayDirection = -1;
        else
            row.arrayDirection = 0;
        data.append(row);
    }
    emit sigGlobalHistoryData(data);
}

void BitMexNetwork::replyOrderBook()
{
    auto reply = qobject_cast<QNetworkReply *>(sender());
    if(200 != reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
        return;
    QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
    auto array = document.array();
    QVector<LuxgateOrderBookRow> dataBids;
    QVector<LuxgateOrderBookRow> dataAsks;
    for(int i=0; i<array.size(); i++)
    {
        LuxgateOrderBookRow row;
        row.dbPrice = array[i].toObject().value("price").toDouble();
        row.dbSize = array[i].toObject().value("size").toDouble();
        if ("Buy" == array[i].toObject().value("side").toString())
            dataBids.append(row);
        else
            dataAsks.prepend(row);
    }
    emit sigOrderBookAsksData(dataAsks);
    emit sigOrderBookBidsData(dataBids);
}