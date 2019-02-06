#ifndef LUXGATEOPTIONS_GLOBAL_H
#define LUXGATEOPTIONS_GLOBAL_H
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
}
#endif // LUXGATEGUI_GLOBAL_H
