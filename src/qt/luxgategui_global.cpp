#include "luxgategui_global.h"

#include <QRegExp>

namespace Luxgate {
    QString priceFormattedText(QString text, bool bBid)
    {
        QString color;
        if(bBid)
            color =  "#267E00";
        else
            color =  "red";

        bool bDouble = true;
        text.toDouble(&bDouble);

        QRegExp rx("[1-9]");
        int  firstNumber = text.indexOf(rx) ;

        if(-1 == firstNumber||
           !text.startsWith("0") ||
           !bDouble)
        {
            return "<font color=\"" + color + "\">" + text + "</font>";
        }

        QString nullPart = "0.";
        for(int i=2; i<firstNumber; i++)
            nullPart +="0";
        QString numberPart = text.mid(firstNumber);


        return nullPart + "<font color=\"" + color + "\">" + numberPart + "</font>";
    }

    std::string StrFromAmount(const CAmount& amount)
    {
        bool sign = amount < 0;
        int64_t n_abs = (sign ? -amount : amount);
        int64_t quotient = n_abs / COIN;
        int64_t remainder = n_abs % COIN;
        return strprintf("%s%d.%08d", sign ? "-" : "", quotient, remainder);
    }

    QString QStrFromAmount(const CAmount& amount)
    {
        return QString::fromStdString(StrFromAmount(amount));
    }
}
