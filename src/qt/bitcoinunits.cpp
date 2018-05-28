// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoinunits.h"
#include "chainparams.h"
#include "primitives/transaction.h"

#include <QSettings>
#include <QStringList>

BitcoinUnits::BitcoinUnits(QObject* parent) : QAbstractListModel(parent),
                                              unitlist(availableUnits())
{
}

QList<BitcoinUnits::Unit> BitcoinUnits::availableUnits()
{
    QList<BitcoinUnits::Unit> unitlist;
    unitlist.append(LUX);
    unitlist.append(mLUX);
    unitlist.append(uLUX);
    return unitlist;
}

bool BitcoinUnits::valid(int unit)
{
    switch (unit) {
    case LUX:
    case mLUX:
    case uLUX:
        return true;
    default:
        return false;
    }
}

QString BitcoinUnits::id(int unit)
{
    switch (unit) {
    case LUX:
        return QString("lux");
    case mLUX:
        return QString("mlux");
    case uLUX:
        return QString::fromUtf8("ulux");
    default:
        return QString("???");
    }
}

QString BitcoinUnits::name(int unit)
{
    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        switch (unit) {
        case LUX:
            return QString("LUX");
        case mLUX:
            return QString("mLUX");
        case uLUX:
            return QString::fromUtf8("μLUX");
        default:
            return QString("???");
        }
    } else {
        switch (unit) {
        case LUX:
            return QString("tLUX");
        case mLUX:
            return QString("mtLUX");
        case uLUX:
            return QString::fromUtf8("μtLUX");
        default:
            return QString("???");
        }
    }
}

QString BitcoinUnits::description(int unit)
{
    if (Params().NetworkID() == CBaseChainParams::MAIN) {
        switch (unit) {
        case LUX:
            return QString("LUX");
        case mLUX:
            return QString("Milli-LUX (1 / 1" THIN_SP_UTF8 "000)");
        case uLUX:
            return QString("Micro-LUX (1 / 1" THIN_SP_UTF8 "000" THIN_SP_UTF8 "000)");
        default:
            return QString("???");
        }
    } else {
        switch (unit) {
        case LUX:
            return QString("TestLUXs");
        case mLUX:
            return QString("Milli-TestLUX (1 / 1" THIN_SP_UTF8 "000)");
        case uLUX:
            return QString("Micro-TestLUX (1 / 1" THIN_SP_UTF8 "000" THIN_SP_UTF8 "000)");
        default:
            return QString("???");
        }
    }
}

qint64 BitcoinUnits::factor(int unit)
{
    switch (unit) {
    case LUX:
        return 100000000;
    case mLUX:
        return 100000;
    case uLUX:
        return 100;
    default:
        return 100000000;
    }
}

int256_t BitcoinUnits::tokenFactor(int decimalUnits)
{
    if(decimalUnits == 0)
        return 0;
    int256_t factor = 1;
    for(int i = 0; i < decimalUnits; i++){
        factor *= 10;
    }
    return factor;
}
int BitcoinUnits::decimals(int unit)
{
    switch(unit)
    {
    case LUX: return 8;
    case mLUX: return 5;
    case uLUX: return 2;
    default: return 0;
    }
}

QString BitcoinUnits::format(int unit, const CAmount& nIn, bool fPlus, SeparatorStyle separators)
{
    // Note: not using straight sprintf here because we do NOT want
    // localized number formatting.
    if (!valid(unit))
        return QString(); // Refuse to format invalid unit
    qint64 n = (qint64)nIn;
    qint64 coin = factor(unit);
    int num_decimals = decimals(unit);
    qint64 n_abs = (n > 0 ? n : -n);
    qint64 quotient = n_abs / coin;
    qint64 remainder = n_abs % coin;
    QString quotient_str = QString::number(quotient);
    QString remainder_str = QString::number(remainder).rightJustified(num_decimals, '0');

    // Use SI-style thin space separators as these are locale independent and can't be
    // confused with the decimal marker.
    QChar thin_sp(THIN_SP_CP);
    int q_size = quotient_str.size();
    if (separators == separatorAlways || (separators == separatorStandard && q_size > 4))
        for (int i = 3; i < q_size; i += 3)
            quotient_str.insert(q_size - i, thin_sp);

    if (n < 0)
        quotient_str.insert(0, '-');
    else if (fPlus && n > 0)
        quotient_str.insert(0, '+');

    if (num_decimals <= 0)
        return quotient_str;

    return quotient_str + QString(".") + remainder_str;
}


// TODO: Review all remaining calls to BitcoinUnits::formatWithUnit to
// TODO: determine whether the output is used in a plain text context
// TODO: or an HTML context (and replace with
// TODO: BtcoinUnits::formatHtmlWithUnit in the latter case). Hopefully
// TODO: there aren't instances where the result could be used in
// TODO: either context.

// NOTE: Using formatWithUnit in an HTML context risks wrapping
// quantities at the thousands separator. More subtly, it also results
// in a standard space rather than a thin space, due to a bug in Qt's
// XML whitespace canonicalisation
//
// Please take care to use formatHtmlWithUnit instead, when
// appropriate.

QString BitcoinUnits::formatWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    return format(unit, amount, plussign, separators) + QString(" ") + name(unit);
}

QString BitcoinUnits::formatHtmlWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    QString str(formatWithUnit(unit, amount, plussign, separators));
    str.replace(QChar(THIN_SP_CP), QString(THIN_SP_HTML));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}

QString BitcoinUnits::floorWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    QSettings settings;
    int digits = settings.value("digits").toInt();

    QString result = format(unit, amount, plussign, separators);
    if (decimals(unit) > digits) result.chop(decimals(unit) - digits);

    return result + QString(" ") + name(unit);
}

QString BitcoinUnits::floorHtmlWithUnit(int unit, const CAmount& amount, bool plussign, SeparatorStyle separators)
{
    QString str(floorWithUnit(unit, amount, plussign, separators));
    str.replace(QChar(THIN_SP_CP), QString(THIN_SP_HTML));
    return QString("<span style='white-space: nowrap;'>%1</span>").arg(str);
}

bool BitcoinUnits::parse(int unit, const QString& value, CAmount* val_out)
{
    if (!valid(unit) || value.isEmpty())
        return false; // Refuse to parse invalid unit or empty string
    int num_decimals = decimals(unit);

    // Ignore spaces and thin spaces when parsing
    QStringList parts = removeSpaces(value).split(".");

    if (parts.size() > 2) {
        return false; // More than one dot
    }
    QString whole = parts[0];
    QString decimals;

    if (parts.size() > 1) {
        decimals = parts[1];
    }
    if (decimals.size() > num_decimals) {
        return false; // Exceeds max precision
    }
    bool ok = false;
    QString str = whole + decimals.leftJustified(num_decimals, '0');

    if (str.size() > 18) {
        return false; // Longer numbers will exceed 63 bits
    }
    CAmount retvalue(str.toLongLong(&ok));
    if(val_out)
    {
        *val_out = retvalue;
    }
    return ok;
}

bool BitcoinUnits::parseToken(int decimal_units, const QString &value, int256_t *val_out)
{
    if(value.isEmpty())
        return false; // Refuse to parse empty string

    // Ignore spaces and thin spaces when parsing
    QStringList parts = removeSpaces(value).split(".");

    if(parts.size() > 2)
    {
        return false; // More than one dot
    }
    QString whole = parts[0];
    QString decimals;

    if(parts.size() > 1)
    {
        decimals = parts[1];
    }
    if(decimals.size() > decimal_units)
    {
        return false; // Exceeds max precision
    }
    bool ok = false;
    QString str = whole + decimals.leftJustified(decimal_units, '0');

    //remove leading zeros from str
    while (str.startsWith('0'))
    {
        str.remove(0,1);
    }

    if(str.size() > 77)
    {
        return false; // Longer numbers will exceed 256 bits
    }

    int256_t retvalue;
    try
    {
        retvalue = int256_t(str.toStdString());
        ok = true;
    }
    catch(...)
    {
        ok = false;
    }

    if(val_out)
    {
        *val_out = retvalue;
    }
    return ok;
}

QString BitcoinUnits::formatToken(int decimal_units, const int256_t& nIn, bool fPlus, SeparatorStyle separators)
{
    int256_t n = nIn;
    int256_t n_abs = (n > 0 ? n : -n);
    int256_t quotient;
    int256_t remainder;
    QString quotient_str;
    QString remainder_str;
    int256_t coin = tokenFactor(decimal_units);
    if(coin != 0)
    {
        quotient = n_abs / coin;
        remainder = n_abs % coin;
        remainder_str = QString::fromStdString(remainder.str()).rightJustified(decimal_units, '0');
    }
    else
    {
        quotient = n_abs;
    }
    quotient_str = QString::fromStdString(quotient.str());

    // Use SI-style thin space separators as these are locale independent and can't be
    // confused with the decimal marker.
    QChar thin_sp(THIN_SP_CP);
    int q_size = quotient_str.size();
    if (separators == separatorAlways || (separators == separatorStandard && q_size > 4))
        for (int i = 3; i < q_size; i += 3)
            quotient_str.insert(q_size - i, thin_sp);

    if (n < 0)
        quotient_str.insert(0, '-');
    else if (fPlus && n > 0)
        quotient_str.insert(0, '+');
    if(remainder_str.size())
    {
        return quotient_str + QString(".") + remainder_str;
    }
    return quotient_str;
}

QString BitcoinUnits::formatTokenWithUnit(const QString unit, int decimals, const int256_t &amount, bool plussign, BitcoinUnits::SeparatorStyle separators)
{
    return formatToken(decimals, amount, plussign, separators) + " " + unit;
}

QString BitcoinUnits::getAmountColumnTitle(int unit)
{
    QString amountTitle = QObject::tr("Amount");
    if (BitcoinUnits::valid(unit)) {
        amountTitle += " (" + BitcoinUnits::name(unit) + ")";
    }
    return amountTitle;
}

int BitcoinUnits::rowCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);
    return unitlist.size();
}

QVariant BitcoinUnits::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    if (row >= 0 && row < unitlist.size()) {
        Unit unit = unitlist.at(row);
        switch (role) {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return QVariant(name(unit));
        case Qt::ToolTipRole:
            return QVariant(description(unit));
        case UnitRole:
            return QVariant(static_cast<int>(unit));
        }
    }
    return QVariant();
}

CAmount BitcoinUnits::maxMoney()
{
    return MAX_MONEY;
}
