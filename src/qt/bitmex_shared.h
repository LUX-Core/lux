#ifndef BITMEX_SHARED_H
#define BITMEX_SHARED_H
#include <QVector>
#include <QDateTime>

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
    double dbSize;
    double dbPrice;
};
typedef QVector<LuxgateOrderBookRow> LuxgateOrderBookData;
#endif // LUXGATEGUI_GLOBAL_H
