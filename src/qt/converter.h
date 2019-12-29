#ifndef CONVERTER_H
#define CONVERTER_H

#include <QObject>
#include <QString>

class converter
{

public:
    void addHexConverter(const QString address, QString *convertedAddr, bool *checkedValidity);
    void addAddrConverter(const QString address, QString *convertedAddr, bool *checkedValidity);
    void twoWayConverter(const QString address, QString *convertedAddr, bool *checkedValidityHex, bool *checkedValidityAddr);
};

#endif