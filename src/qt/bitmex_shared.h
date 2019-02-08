#ifndef BITMEX_SHARED_H
#define BITMEX_SHARED_H
#include <QVector>
#include <QDateTime>
#include <QString>
#include <QByteArray>

struct LuxgateHistoryRow
{
    QDateTime dateTime;
    bool bBuy;
    double dbPrice;
    double dbSize;
    int arrayDirection; //-1 - down, 0 - no array, 1- up array
};
typedef QVector<LuxgateHistoryRow> LuxgateHistoryData;

struct LuxgateOrderBookRow
{
    double dbSize {0.f};
    double dbPrice {0.f};
};
typedef QVector<LuxgateOrderBookRow> LuxgateOrderBookData;

struct LuxgateIndices
{
    double dbLastPrice {0.f};
    double dbMarkPrice {0.f};
    QString lastTickDirection;
};

struct LuxgateOpenOrdersRow
{
    double dbQty {0.f};         //quote currency
    double dbLeavesQty {0.f};   //quote leaves currency
    double dbValue {0.f};       //base currency
    double dbPrice {0.f};       //base/quote
    QString type;
    QString status;
    QByteArray orderId;
    bool bBuy;
    QDateTime dateTimeOpen;
};
typedef QVector<LuxgateOpenOrdersRow> LuxgateOpenOrdersData;
#endif // LUXGATEGUI_GLOBAL_H
