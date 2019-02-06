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


BitMexNetwork::BitMexNetwork(QObject *parent):
    QObject(parent)
{
    readConfig();
    nam = new QNetworkAccessManager(this);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &BitMexNetwork::allRequests);
    timer->start(5000);
}

void BitMexNetwork::readConfig()
{
    QString configPath =  QStringFromPath(GetDataDir(false)) + "/bitmex.config";
    QFile fileConfig(configPath);
    if(fileConfig.open(QIODevice::ReadOnly)) {
        auto data = fileConfig.readAll();
        QJsonDocument document = QJsonDocument::fromJson(data);
        auto object = document.object();
        apiKey = object.value("apiKey").toString();
        apiPrivate = object.value("apiPrivate").toString();
    }
}

void BitMexNetwork::requestGlobalHistory()
{
    QNetworkRequest request(QUrl("https://testnet.bitmex.com/api/v1/trade?symbol=XBT&count=100&reverse=true"));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    auto reply = nam->get(request);
    connect(reply, &QNetworkReply::finished,
            this, &BitMexNetwork::replyGlobalHistory);
}

void BitMexNetwork::requestOrderBook()
{
    QNetworkRequest request(QUrl("https://testnet.bitmex.com/api/v1/orderBook/L2?symbol=XBT&depth=" + QString::number(depthOrderBook)));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/json");
    auto reply = nam->get(request);
    connect(reply, &QNetworkReply::finished,
            this, &BitMexNetwork::replyOrderBook);
}

void BitMexNetwork::allRequests()
{
    requestGlobalHistory();
    requestOrderBook();
}

void BitMexNetwork::replyGlobalHistory()
{
    auto reply = qobject_cast<QNetworkReply *>(sender());
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