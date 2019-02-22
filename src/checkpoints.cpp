// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "checkpoints.h"

#include "chainparams.h"
#include "main.h"
#include "uint256.h"

#include <stdint.h>

static const int nCheckpointSpan = 500;

namespace Checkpoints
{
/**
     * How many times we expect transactions after the last checkpoint to
     * be slower. This number is a compromise, as it can't be accurate for
     * every system. When reindexing from a fast disk with a slow CPU, it
     * can be up to 20, while when downloading from a slow network with a
     * fast multicore CPU, it won't be much higher than 1.
     */
static const double SIGCHECK_VERIFICATION_FACTOR = 5.0;

bool fEnabled = true;

bool CheckBlock(const CCheckpointData& data, int nHeight, const uint256& hash)
{
    if (!fEnabled)
        return true;

    const MapCheckpoints& checkpoints = *data.mapCheckpoints;

    MapCheckpoints::const_iterator i = checkpoints.find(nHeight);
    if (i == checkpoints.end()) return true;
    return hash == i->second;
}

//! Guess how far we are in the verification process at the given block index
double GuessVerificationProgress(const CCheckpointData& data, CBlockIndex *pindex, bool fSigchecks)
{
    if (pindex == NULL)
        return 0.0;

    int64_t nNow = time(NULL);

    double fSigcheckVerificationFactor = fSigchecks ? SIGCHECK_VERIFICATION_FACTOR : 1.0;
    double fWorkBefore = 0.0; // Amount of work done before pindex
    double fWorkAfter = 0.0;  // Amount of work left after pindex (estimated)
    // Work is defined as: 1.0 per transaction before the last checkpoint, and
    // fSigcheckVerificationFactor per transaction after.

    // const CCheckpointData& data = Params().Checkpoints();

    if (pindex->nChainTx <= data.nTransactionsLastCheckpoint) {
        double nCheapBefore = pindex->nChainTx;
        double nCheapAfter = data.nTransactionsLastCheckpoint - pindex->nChainTx;
        double nExpensiveAfter = (nNow - data.nTimeLastCheckpoint) / 86400.0 * data.fTransactionsPerDay;
        fWorkBefore = nCheapBefore;
        fWorkAfter = nCheapAfter + nExpensiveAfter * fSigcheckVerificationFactor;
    } else {
        double nCheapBefore = data.nTransactionsLastCheckpoint;
        double nExpensiveBefore = pindex->nChainTx - data.nTransactionsLastCheckpoint;
        double nExpensiveAfter = (nNow - pindex->GetBlockTime()) / 86400.0 * data.fTransactionsPerDay;
        fWorkBefore = nCheapBefore + nExpensiveBefore * fSigcheckVerificationFactor;
        fWorkAfter = nExpensiveAfter * fSigcheckVerificationFactor;
    }

    return fWorkBefore / (fWorkBefore + fWorkAfter);
}

int GetTotalBlocksEstimate(const CCheckpointData& data)
{
    if (!fEnabled)
        return 0;

    const MapCheckpoints& checkpoints = *data.mapCheckpoints;

    return checkpoints.rbegin()->first;
}

CBlockIndex* GetLastCheckpoint(const CCheckpointData& data)
{
    if (!fEnabled)
        return NULL;

    const MapCheckpoints& checkpoints = *data.mapCheckpoints;

    for (const MapCheckpoints::value_type& i : reverse_iterate(checkpoints)) {
        const uint256& hash = i.second;
        CBlockIndex* pindex = LookupBlockIndex(hash);
        if (pindex) return pindex;
    }
    return NULL;
}

// Automatically select a suitable sync-checkpoint
const CBlockIndex* AutoSelectSyncCheckpoint()
{
    const CBlockIndex *pindexBest = chainActive.Tip();
    const CBlockIndex *pindex = pindexBest;

    if (!pindex) return NULL;

    // Search backward for a block within max span and maturity window
    while (pindex->pprev && pindex->nHeight + nCheckpointSpan > pindexBest->nHeight)
        pindex = pindex->pprev;
    return pindex;
}

// Check against synchronized checkpoint
bool CheckSync(int nHeight)
{
    const CBlockIndex* pindexSync;
    if(nHeight)
        pindexSync = AutoSelectSyncCheckpoint();

    if(nHeight && pindexSync && pindexSync->nHeight)
        return false;
    return true;
}

} // namespace Checkpoints
