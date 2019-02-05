#ifndef LUXGATEGUI_GLOBAL_H
#define LUXGATEGUI_GLOBAL_H
#include <QString>
#include <QDateTime>
#include <boost/filesystem.hpp>

namespace Luxgate {
    struct Decimals {
        Decimals(int price, int base, int quote) : price(price),
                                                   base(base),
                                                   quote(quote) {}
        Decimals(): price(8),
                    base(8),
                    quote(8) {}

        Decimals(const Decimals &other) {
            price = other.price;
            base = other.base;
            quote = other.quote;
        }

        Decimals &operator=(const Decimals &other) {
            price = other.price;
            base = other.base;
            quote = other.quote;
            return *this;
        }

        int price;
        int base;
        int quote;
    };

    struct CurrencyPair {
        CurrencyPair(QString baseCurrency, QString quoteCurrency) : baseCurrency(baseCurrency),
                                                                    quoteCurrency(quoteCurrency) {}

        QString baseCurrency;
        QString quoteCurrency;
    };

    QString priceFormattedText(QString text, bool bBid); //if bBid==true price of bid, else price of ask

    enum CommonRoles {BidAskRole = Qt::UserRole, CopyRowRole, IndividualRole};
}


struct LuxgateHistoryRow
{
    QDateTime dateTime;
    bool bBuy;
    double dbPrice;
    double dbSize;
    int arrayDirection; //-1 - down, 0 - no array, 1- up array
};
typedef QVector<LuxgateHistoryRow> LuxgateHistoryData;

boost::filesystem::path PathFromQString(const QString & filePath);

QString QStringFromPath(const boost::filesystem::path & filePath);

extern Luxgate::CurrencyPair curCurrencyPair;
#endif // LUXGATEGUI_GLOBAL_H
