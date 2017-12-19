/**
 * @file       Denominations.cpp
 *
 * @brief      Functions for converting to/from Zerocoin Denominations to other values library.
 *
 * @copyright  Copyright 2017 LUX Developers
 * @license    This project is released under the MIT license.
 **/

#include "Denominations.h"
#include "amount.h"

namespace libzerocoin {
// All denomination values should only exist in these routines for consistency.
// For serialization/unserialization enums are converted to int (denoted enumvalue in function name)

CoinDenomination IntToZerocoinDenomination(int64_t amount)
{
    CoinDenomination denomination;
    switch (amount) {
    case 1:		denomination = CoinDenomination::ZQ_ONE; break;
    case 5:	denomination = CoinDenomination::ZQ_FIVE; break;
    case 10:	denomination = CoinDenomination::ZQ_TEN; break;
    case 50:	denomination = CoinDenomination::ZQ_FIFTY; break;
    case 100: denomination = CoinDenomination::ZQ_ONE_HUNDRED; break;
    case 500: denomination = CoinDenomination::ZQ_FIVE_HUNDRED; break;
    case 1000: denomination = CoinDenomination::ZQ_ONE_THOUSAND; break;
    case 5000: denomination = CoinDenomination::ZQ_FIVE_THOUSAND; break;
    default:
        //not a valid denomination
        denomination = CoinDenomination::ZQ_ERROR; break;
    }

    return denomination;
}

int64_t ZerocoinDenominationToInt(const CoinDenomination& denomination)
{
    int64_t Value = 0;
    switch (denomination) {
    case CoinDenomination::ZQ_ONE: Value = 1; break;
    case CoinDenomination::ZQ_FIVE: Value = 5; break;
    case CoinDenomination::ZQ_TEN: Value = 10; break;
    case CoinDenomination::ZQ_FIFTY : Value = 50; break;
    case CoinDenomination::ZQ_ONE_HUNDRED: Value = 100; break;
    case CoinDenomination::ZQ_FIVE_HUNDRED: Value = 500; break;
    case CoinDenomination::ZQ_ONE_THOUSAND: Value = 1000; break;
    case CoinDenomination::ZQ_FIVE_THOUSAND: Value = 5000; break;
    default:
        // Error Case
        Value = 0; break;
    }
    return Value;
}

CoinDenomination AmountToZerocoinDenomination(CAmount amount)
{
    // Check to make sure amount is an exact integer number of COINS
    CAmount residual_amount = amount - COIN * (amount / COIN);
    if (residual_amount == 0) {
        return IntToZerocoinDenomination(amount/COIN);
    } else {
        return CoinDenomination::ZQ_ERROR;
    }
}

// return the highest denomination that is less than or equal to the amount given
// use case: converting Piv to zLux without user worrying about denomination math themselves
CoinDenomination AmountToClosestDenomination(CAmount nAmount, CAmount& nRemaining)
{
    if (nAmount < 1 * COIN)
        return ZQ_ERROR;

    CAmount nConvert = nAmount / COIN;
    CoinDenomination denomination = ZQ_ERROR;
    for (unsigned int i = 0; i < zerocoinDenomList.size(); i++) {
        denomination = zerocoinDenomList[i];

        //exact match
        if (nConvert == denomination) {
            nRemaining = 0;
            return denomination;
        }

        //we are beyond the value, use previous denomination
        if (denomination > nConvert && i) {
            CoinDenomination d = zerocoinDenomList[i - 1];
            nRemaining = nConvert - d;
            return d;
        }
    }
    //last denomination, the highest value possible
    nRemaining = nConvert - denomination;
    return denomination;
}

CAmount ZerocoinDenominationToAmount(const CoinDenomination& denomination)
{
    CAmount nValue = COIN * ZerocoinDenominationToInt(denomination);
    return nValue;
}


CoinDenomination get_denomination(std::string denomAmount) {
    int64_t val = std::stoi(denomAmount);
    return IntToZerocoinDenomination(val);
}


int64_t get_amount(std::string denomAmount) {
    int64_t nAmount = 0;
    CoinDenomination denom = get_denomination(denomAmount);
    if (denom == ZQ_ERROR) {
        // SHOULD WE THROW EXCEPTION or Something?
        nAmount = 0;
    } else {
        nAmount = ZerocoinDenominationToAmount(denom);
    }
    return nAmount;
}

} /* namespace libzerocoin */
