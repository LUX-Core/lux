#ifndef LUXGATEGUI_GLOBAL_H
#define LUXGATEGUI_GLOBAL_H
#include <QString>


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

    enum CommonRoles {BidAskRole = Qt::UserRole, IndividualRole};
}
extern Luxgate::CurrencyPair curCurrencyPair;
#endif // LUXGATEGUI_GLOBAL_H
