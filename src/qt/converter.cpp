#include "converter.h"
#include "base58.h"
#include "uint256.h"
#include "utilstrencodings.h"

#include <iostream>
#include <QClipboard>
#include <QRegularExpressionValidator>

#define paternAddress "^[a-fA-F0-9]{40,40}$"

void converter::addHexConverter(const QString address, QString* convertedAddr, bool* checkedValidity) {

    if(!address.isEmpty()) {

        bool hasHexAddress = false;
        QRegularExpression regEx(paternAddress);
        hasHexAddress = regEx.match(address).hasMatch();
        if (hasHexAddress) {
            *convertedAddr = address;
            *checkedValidity = hasHexAddress;
        }

        bool hasLuxAddress = false;
        CTxDestination addr = DecodeDestination(address.toStdString());
        hasLuxAddress = IsValidDestination(addr);
        if (hasLuxAddress) {
            CKeyID *keyid = boost::get<CKeyID>(&addr);
            if (keyid) {
                *convertedAddr = QString::fromStdString(HexStr(valtype(keyid->begin(),keyid->end())));
                *checkedValidity = hasLuxAddress;
            }
        }
    }
}

void converter::addAddrConverter(const QString address, QString* convertedAddr, bool* checkedValidity) {

    if(!address.isEmpty()) {

        bool hasHexAddress = false;
        QRegularExpression regEx(paternAddress);
        hasHexAddress = regEx.match(address).hasMatch();
        if (hasHexAddress) {
            uint160 key(ParseHex(address.toStdString()));
            CKeyID keyid(key);
            if(IsValidDestination(CTxDestination(keyid))) {
                *convertedAddr = QString::fromStdString(EncodeDestination(CTxDestination(keyid)));
            }
            *checkedValidity = hasHexAddress;
        }

        bool hasLuxAddress = false;
        CTxDestination addr = DecodeDestination(address.toStdString());
        hasLuxAddress = IsValidDestination(addr);
        if (hasLuxAddress) {
            CKeyID *keyid = boost::get<CKeyID>(&addr);
            if (keyid) {
                *convertedAddr = address;
                *checkedValidity = hasLuxAddress;
            }
        }
    }
}

void converter::twoWayConverter(const QString address, QString *convertedAddr, bool *checkedValidityHex, bool *checkedValidityAddr) {

    if(!address.isEmpty()) {

        bool hasHexAddress = false;
        QRegularExpression regEx(paternAddress);
        hasHexAddress = regEx.match(address).hasMatch();
        if (hasHexAddress) {
            uint160 key(ParseHex(address.toStdString()));
            CKeyID keyid(key);
            if(IsValidDestination(CTxDestination(keyid))) {
                *convertedAddr = QString::fromStdString(EncodeDestination(CTxDestination(keyid)));
            }
            *checkedValidityHex = hasHexAddress;
            *checkedValidityAddr = false;
        }

        bool hasLuxAddress = false;
        CTxDestination addr = DecodeDestination(address.toStdString());
        hasLuxAddress = IsValidDestination(addr);
        if (hasLuxAddress) {
            CKeyID *keyid = boost::get<CKeyID>(&addr);
            if (keyid) {
                *convertedAddr = QString::fromStdString(HexStr(valtype(keyid->begin(),keyid->end())));
                *checkedValidityAddr = hasLuxAddress;
                *checkedValidityHex = false;
            }
        }
    }
}