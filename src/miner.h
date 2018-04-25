// Copyright (c) 2009-2010 Satoshi Nakamoto                     -*- c++ -*-
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MINER_H
#define BITCOIN_MINER_H

#include "consensus/params.h"
#include <stdint.h>

class CBlock;
class CBlockHeader;
class CBlockIndex;
class CReserveKey;
class CScript;
class CWallet;

struct CBlockTemplate;

/** Run the miner threads */
void GenerateBitcoins(CWallet* pwallet, int nThreads);
/** Generate a new block, without valid proof-of-work */
CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn, CWallet* pwallet, bool fProofOfStake);
CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey, CWallet* pwallet, bool fProofOfStake);
bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey);
/** Do mining precalculation */
void FormatHashBuffers(CBlock* pblock, char* pmidstate, char* pdata, char* phash1);
/** Base sha256 mining transform */
void SHA256Transform(void* pstate, void* pinput, const void* pinit);
/** Modify the extranonce in a block */
void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce);
/** Check mined block */
void UpdateTime(CBlockHeader* block, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev, bool isProofOfStake);

void BitcoinMiner(CWallet* pwallet, bool fProofOfStake);


void ThreadStakeMiner(CWallet *pwallet);

extern double dHashesPerSec;
extern int64_t nHPSTimerStart;

#endif // BITCOIN_MINER_H
