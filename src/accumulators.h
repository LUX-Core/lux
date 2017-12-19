// Copyright (c) 2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUX_ACCUMULATORS_H
#define LUX_ACCUMULATORS_H

#include "libzerocoin/Accumulator.h"
#include "libzerocoin/Denominations.h"
#include "libzerocoin/Coin.h"
#include "primitives/zerocoin.h"
#include "uint256.h"

bool GenerateAccumulatorWitness(const libzerocoin::PublicCoin &coin, libzerocoin::Accumulator& accumulator, libzerocoin::AccumulatorWitness& witness, int nSecurityLevel, int& nMintsAdded, std::string& strError);
bool GetAccumulatorValueFromDB(uint256 nCheckpoint, libzerocoin::CoinDenomination denom, CBigNum& bnAccValue);
bool GetAccumulatorValueFromChecksum(uint32_t nChecksum, bool fMemoryOnly, CBigNum& bnAccValue);
void AddAccumulatorChecksum(const uint32_t nChecksum, const CBigNum &bnValue, bool fMemoryOnly);
bool CalculateAccumulatorCheckpoint(int nHeight, uint256& nCheckpoint);
bool LoadAccumulatorValuesFromDB(const uint256 nCheckpoint);
bool EraseAccumulatorValues(const uint256& nCheckpointErase, const uint256& nCheckpointPrevious);
uint32_t ParseChecksum(uint256 nChecksum, libzerocoin::CoinDenomination denomination);
uint32_t GetChecksum(const CBigNum &bnValue);
bool InvalidCheckpointRange(int nHeight);

#endif //LUX_ACCUMULATORS_H
