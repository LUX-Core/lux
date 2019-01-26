#ifndef LUXGATEGUI_GLOBAL_H
#define LUXGATEGUI_GLOBAL_H
#include <QString>

struct CurrencyPair
{
    CurrencyPair(QString baseCurrency,QString quoteCurrency):   baseCurrency(baseCurrency),
                                                                quoteCurrency(quoteCurrency)
    {}
    QString baseCurrency;
    QString quoteCurrency;
};

extern CurrencyPair curCurrencyPair;
#endif // LUXGATEGUI_GLOBAL_H
