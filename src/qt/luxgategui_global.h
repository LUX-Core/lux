#ifndef LUXGATEGUI_GLOBAL_H
#define LUXGATEGUI_GLOBAL_H
#include <QString>
#include <QDateTime>
#include <boost/filesystem.hpp>

namespace Luxgate {
    struct CurrencyPair {
        CurrencyPair(QString baseCurrency, QString quoteCurrency) : baseCurrency(baseCurrency),
                                                                    quoteCurrency(quoteCurrency) {}

        QString baseCurrency;
        QString quoteCurrency;
    };

    QString priceFormattedText(QString text, bool bBid); //if bBid==true price of bid, else price of ask

    enum CommonRoles {BidAskRole = Qt::UserRole, CopyRowRole, IndividualRole};
}


boost::filesystem::path PathFromQString(const QString & filePath);

QString QStringFromPath(const boost::filesystem::path & filePath);

extern Luxgate::CurrencyPair curCurrencyPair;
#endif // LUXGATEGUI_GLOBAL_H
