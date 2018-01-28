// Copyright (c) 2012-2013 The PPCoin developers                -*- c++ -*-
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_KERNEL_H
#define BITCOIN_KERNEL_H

#include "main.h"

// Compute the hash modifier for proof-of-stake
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

// Check whether stake kernel meets hash target
// Sets hashProofOfStake on success return
bool CheckStakeKernelHash(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock &blockFrom, const CTransaction &txPrev, const COutPoint &prevout, unsigned int& nTimeTx, uint256& hashProofOfStake);

// Check kernel hash target and coinstake signature
// Sets hashProofOfStake on success return
bool CheckProofOfStake(CBlockIndex* const pindexPrev, const CBlock &block, uint256& hashProofOfStake);

// Get stake modifier checksum
unsigned int GetStakeModifierChecksum(const CBlockIndex* pindex);

// Check stake modifier hard checkpoints
bool CheckStakeModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum);

#endif // BITCOIN_KERNEL_H
