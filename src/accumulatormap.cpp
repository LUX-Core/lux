// Copyright (c) 2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "accumulatormap.h"
#include "accumulators.h"
#include "main.h"
#include "txdb.h"
#include "libzerocoin/Denominations.h"

using namespace libzerocoin;
using namespace std;

//Construct accumulators for all denominations
AccumulatorMap::AccumulatorMap()
{
    for (auto& denom : zerocoinDenomList) {
        unique_ptr<Accumulator> uptr(new Accumulator(Params().Zerocoin_Params(), denom));
        mapAccumulators.insert(make_pair(denom, std::move(uptr)));
    }
}

//Reset each accumulator to its default state
void AccumulatorMap::Reset()
{
    mapAccumulators.clear();
    for (auto& denom : zerocoinDenomList) {
        unique_ptr<Accumulator> uptr(new Accumulator(Params().Zerocoin_Params(), denom));
        mapAccumulators.insert(make_pair(denom, std::move(uptr)));
    }
}

//Load a checkpoint containing 8 32bit checksums of accumulator values.
bool AccumulatorMap::Load(uint256 nCheckpoint)
{
    for (auto& denom : zerocoinDenomList) {
        uint32_t nChecksum = ParseChecksum(nCheckpoint, denom);

        CBigNum bnValue;
        if (!zerocoinDB->ReadAccumulatorValue(nChecksum, bnValue)) {
            LogPrintf("%s : cannot find checksum %d", __func__, nChecksum);
            return false;
        }

        mapAccumulators.at(denom)->setValue(bnValue);
    }
    return true;
}

//Add a zerocoin to the accumulator of its denomination.
bool AccumulatorMap::Accumulate(PublicCoin pubCoin, bool fSkipValidation)
{
    CoinDenomination denom = pubCoin.getDenomination();
    if (denom == CoinDenomination::ZQ_ERROR)
        return false;

    if (fSkipValidation)
        mapAccumulators.at(denom)->increment(pubCoin.getValue());
    else
        mapAccumulators.at(denom)->accumulate(pubCoin);
    return true;
}

//Get the value of a specific accumulator
CBigNum AccumulatorMap::GetValue(CoinDenomination denom)
{
    if (denom == CoinDenomination::ZQ_ERROR)
        return CBigNum(0);
    return mapAccumulators.at(denom)->getValue();
}

//Calculate a 32bit checksum of each accumulator value. Concatenate checksums into uint256
uint256 AccumulatorMap::GetCheckpoint()
{
    uint256 nCheckpoint;

    //Prevent possible overflows from future changes to the list and forgetting to update this code
    assert(zerocoinDenomList.size() == 8);
    for (auto& denom : zerocoinDenomList) {
        CBigNum bnValue = mapAccumulators.at(denom)->getValue();
        uint32_t nCheckSum = GetChecksum(bnValue);
        nCheckpoint = nCheckpoint << 32 | nCheckSum;
    }

    return nCheckpoint;
}


