/**
 * @file       denominations_functions.h
 *
 * @brief      Denomination functions for the Zerocoin library.
 *
 * @copyright  Copyright 2017 LUX Developers
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2015-2017 The LUX developers

#include "reverse_iterate.h"
#include "util.h"
#include "libzerocoin/Denominations.h"
#include "primitives/zerocoin.h"
#include <list>
#include <map>
std::vector<CZerocoinMint> SelectMintsFromList(const CAmount nValueTarget, CAmount& nSelectedValue,
                                               int nMaxNumberOfSpends,
                                               bool fMinimizeChange,
                                               int& nCoinsReturned,
                                               const std::list<CZerocoinMint>& listMints,
                                               const std::map<libzerocoin::CoinDenomination, CAmount> mapDenomsHeld,
                                               int& nNeededSpends
                                               );

int calculateChange(
    int nMaxNumberOfSpends,
    bool fMinimizeChange,
    const CAmount nValueTarget,
    const std::map<libzerocoin::CoinDenomination, CAmount>& mapOfDenomsHeld,
    std::map<libzerocoin::CoinDenomination, CAmount>& mapOfDenomsUsed);

void listSpends(const std::vector<CZerocoinMint>& vSelectedMints);
