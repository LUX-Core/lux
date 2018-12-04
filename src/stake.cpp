// Copyright (c) 2017 LUX developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>
#include <boost/lexical_cast.hpp>

#include "db.h"
#include "stake.h"
#include "init.h"
#include "chainparams.h"
#include "policy/policy.h"
#include "main.h"
#include "miner.h"
#include "wallet.h"
#include "masternode.h"
#include "utilmoneystr.h"
#include "script/sign.h"
#include "script/interpreter.h"
#include "timedata.h"
#include <boost/thread.hpp>
#include <atomic>

#if defined(DEBUG_DUMP_STAKING_INFO)
#  include "DEBUG_DUMP_STAKING_INFO.hpp"
#endif

#ifndef DEBUG_DUMP_STAKING_INFO_CheckHash
#  define DEBUG_DUMP_STAKING_INFO_CheckHash() (void)0
#endif
#ifndef DEBUG_DUMP_MULTIFIER
#  define DEBUG_DUMP_MULTIFIER() (void)0
#endif

using namespace std;

// MODIFIER_INTERVAL: time to elapse before new modifier is computed
static const unsigned int MODIFIER_INTERVAL = 10 * 60;
static const unsigned int MODIFIER_INTERVAL_TESTNET = 60;

bool CheckCoinStakeTimestamp(uint32_t nTimeBlock) { return (nTimeBlock & STAKE_TIMESTAMP_MASK) == 0; }

// MODIFIER_INTERVAL_RATIO:
// ratio of group interval length between the last group and the first group
static const int MODIFIER_INTERVAL_RATIO = 3;

static const int LAST_MULTIPLIED_BLOCK = 180 * 1000; // 180K

static const bool ENABLE_ADVANCED_STAKING = true;

static const int ADVANCED_STAKING_HEIGHT = 225000;

static const int nLuxProtocolSwitchHeight = 520000;

static const int nLuxProtocolSwitchHeightTestnet = 95000; // nHeight = 89032 (1:54 am Tuesday, 20 November 2018)

static std::atomic<bool> nStakingInterrupped;

Stake* const stake = Stake::Pointer();

StakeKernel::StakeKernel() {
    //!<DuzyDoc>TODO: kernel initialization
}

Stake::Stake()
    : StakeKernel()
    , nStakeInterval(0)
    , nLastStakeTime(GetAdjustedTime())
    , nLastSelectTime(GetTime())
    , nSelectionPeriod(0)
    , nStakeSplitThreshold(GetStakeCombineThreshold())
    , nStakeMinAge(0)
    , nHashInterval(0)
    , nReserveBalance(0)
    , mapStakes()
    , mapHashedBlocks()
    , mapProofOfStake()
    , mapRejectedBlocks()
{
}

Stake::~Stake() {}

StakeStatus::StakeStatus() {
    Clear();
    nBlocksCreated = nBlocksAccepted = nKernelsFound = 0;
    dKernelDiffMax = 0;
}

void StakeStatus::Clear() {
    nWeightSum = nWeightMin = nWeightMax = 0;
    dValueSum = 0;
    nCoinAgeSum = 0;
    dKernelDiffSum = 0;
    nLastCoinStakeSearchInterval = 0;
}

// Modifier interval: time to elapse before new modifier is computed
// Set to 3-hour for production network and 20-minute for test network
static inline unsigned int GetInterval() {
    return IsTestNet() ? MODIFIER_INTERVAL_TESTNET : MODIFIER_INTERVAL;
}

// Get time
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd) {
    // Stake hash weight starts from 0 at the min age
    // this change increases active coins participating the hash and helps
    // to secure the network when proof-of-stake difficulty is low
    return min(nIntervalEnd - nIntervalBeginning - Params().StakingMinAge(), (int64_t)0);
}

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime) {
    if (!pindex)
        return error("%s: null pindex", __func__);

    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;

    if (!pindex->GeneratedStakeModifier())
        return error("%s: no generation at genesis block", __func__);

    nStakeModifier = pindex->nStakeModifier;
    nModifierTime = pindex->GetBlockTime();
    return true;
}

// Get selection interval (nSection in seconds)
static int64_t GetSelectionInterval(int nSection) {
    assert(nSection >= 0 && nSection < 64);
    return (GetInterval() * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1))));
}

// Get stake modifier selection time (in seconds)
static int64_t GetSelectionTime() {
    int64_t nSelectionTime = 0;
    for (int nSection = 0; nSection < 64; nSection++) {
        nSelectionTime += GetSelectionInterval(nSection);
    }
    return nSelectionTime;
}

// Finds a block from the candidate blocks in vSortedCandidates, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionTime.
static bool FindModifierBlockFromCandidates(vector <pair<int64_t, uint256>>& vSortedCandidates, map<uint256, const CBlockIndex*>& mSelectedBlocks, int64_t nSelectionTime, uint64_t nStakeModifierPrev,
                                            const CBlockIndex*& pindexSelected) {
    uint256 hashBest = 0;

    pindexSelected = nullptr;

    for (auto& item : vSortedCandidates) {
        if (!mapBlockIndex.count(item.second))
            return error("%s: invalid candidate block %s", __func__, item.second.GetHex());

        const CBlockIndex* pindex = mapBlockIndex[item.second];
        if (pindex->IsProofOfStake() && pindex->hashProofOfStake == 0) {
            return error("%s: zero stake (block %s)", __func__, item.second.GetHex());
        }

        if (pindexSelected && pindex->GetBlockTime() > nSelectionTime) break;
        if (mSelectedBlocks.count(pindex->GetBlockHash()) > 0) continue;

        // compute the selection hash by hashing an input that is unique to that block
        CDataStream ss(SER_GETHASH, 0);
        ss << uint256(pindex->IsProofOfStake() ? pindex->hashProofOfStake : pindex->GetBlockHash())
           << nStakeModifierPrev;

        uint256 hashSelection(Hash(ss.begin(), ss.end()));

        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        if (pindex->IsProofOfStake())
            hashSelection >>= 32;

        if (pindexSelected == nullptr || hashSelection < hashBest) {
            pindexSelected = pindex;
            hashBest = hashSelection;
        }
    }

#   if defined(DEBUG_DUMP_STAKING_INFO) && false
    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("%s: selected block %d %s %s\n", __func__, 
                  pindexSelected->nHeight,
                  pindexSelected->GetBlockHash().GetHex(),
                  hashBest.ToString());
#   endif

    if (pindexSelected) {
        // add the selected block from candidates to selected list
        mSelectedBlocks.insert(make_pair(pindexSelected->GetBlockHash(), pindexSelected));
    }
    return pindexSelected != nullptr;
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool Stake::ComputeNextModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier) {
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;

    if (!pindexPrev) {
        fGeneratedStakeModifier = true;
        return true; // genesis block's modifier is 0
    }

    if (pindexPrev->nHeight == 0) {
        //Give a stake modifier to the first block
        fGeneratedStakeModifier = true;
        nStakeModifier = uint64_t("stakemodifier");
        return true;
    }

    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("%s: unable to get last modifier", __func__);

#   if defined(DEBUG_DUMP_STAKING_INFO) && false
    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("%s: last modifier=%d time=%d\n", __func__, nStakeModifier, nModifierTime); // DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nModifierTime)
#   endif

    auto const nPrevRounds = pindexPrev->GetBlockTime() / GetInterval();
    if (nModifierTime / GetInterval() >= nPrevRounds)
        return true;

    // Sort candidate blocks by timestamp
    vector< pair<int64_t, uint256> > vSortedCandidates;
    vSortedCandidates.reserve(64 * GetInterval() / GetTargetSpacing(pindexPrev->nHeight));

    int64_t nSelectionTime = nPrevRounds * GetInterval() - GetSelectionTime();
    const CBlockIndex* pindex = pindexPrev;
    while (pindex && pindex->GetBlockTime() >= nSelectionTime) {
        if (pindex->IsProofOfStake() && pindex->hashProofOfStake == 0) {
            return error("%s: zero stake block %s", __func__, pindex->GetBlockHash().GetHex());
        }
        vSortedCandidates.push_back(make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }

#   if defined(DEBUG_DUMP_STAKING_INFO) && false
    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
#   endif

    //reverse(vSortedCandidates.begin(), vSortedCandidates.end());
    sort(vSortedCandidates.begin(), vSortedCandidates.end());

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    map<uint256, const CBlockIndex*> mSelectedBlocks;
    for (int nRound = 0; nRound < min(64, int(vSortedCandidates.size())); nRound++) {
        // add an interval section to the current selection round
        nSelectionTime += GetSelectionInterval(nRound);

        // select a block from the candidates of current round
        if (!FindModifierBlockFromCandidates(vSortedCandidates, mSelectedBlocks, nSelectionTime, nStakeModifier, pindex))
            return error("%s: unable to select block at round %d", nRound);

        // write the entropy bit of the selected block
        nStakeModifierNew |= (uint64_t(pindex->GetStakeEntropyBit()) << nRound);

#       if defined(DEBUG_DUMP_STAKING_INFO) && false
        if (fDebug || GetBoolArg("-printstakemodifier", false)) //DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nSelectionTime)
            LogPrintf("%s: selected round %d stop=%s height=%d bit=%d\n", __func__,
                      nRound, nSelectionTime, pindex->nHeight, pindex->GetStakeEntropyBit());
#       endif
    }

#   if defined(DEBUG_DUMP_STAKING_INFO) && false
    // Print selection map for visualization of the selected blocks
    if (fDebug || GetBoolArg("-printstakemodifier", false)) {
        string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate) {
            // '=' indicates proof-of-stake blocks not selected
            if (pindex->IsProofOfStake())
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        for (auto &item : mSelectedBlocks) {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1, item.second->IsProofOfStake() ? "S" : "W");
        }
        LogPrintf("%s: selection height [%d, %d] map %s\n", __func__, nHeightFirstCandidate, pindexPrev->nHeight, strSelectionMap.c_str());
    }
    if (fDebug || GetBoolArg("-printstakemodifier", false)) {
        LogPrintf("%s: new modifier=%d time=%s\n", __func__, nStakeModifierNew, pindexPrev->GetBlockTime()); // DateTimeStrFormat("%Y-%m-%d %H:%M:%S", pindexPrev->GetBlockTime())
    }
#   endif

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}

#if 0
// The stake modifier used to hash for a stake kernel is chosen as the stake
// modifier about a selection interval later than the coin generating the kernel
bool GetKernelStakeModifier(uint256 hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake)
{
    nStakeModifier = 0;
    if (!mapBlockIndex.count(hashBlockFrom))
        return error("GetKernelStakeModifier() : block not indexed");

    const CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    nStakeModifierHeight = pindexFrom->nHeight;
    nStakeModifierTime = pindexFrom->GetBlockTime();

    const int64_t nSelectionTime = GetSelectionTime();
    const CBlockIndex* pindex = pindexFrom;
    CBlockIndex* pindexNext = chainActive[pindexFrom->nHeight + 1];

    // loop to find the stake modifier later by a selection time
    while (nStakeModifierTime < pindexFrom->GetBlockTime() + nSelectionTime) {
        if (!pindexNext) {
            // Should never happen
            return error("Null pindexNext\n");
        }

        pindex = pindexNext;
        pindexNext = chainActive[pindexNext->nHeight + 1];
        if (pindex->GeneratedStakeModifier()) {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
        }
    }
    nStakeModifier = pindex->nStakeModifier;
    return true;
}
#endif

bool MultiplyStakeTarget(uint256& bnTarget, int nModifierHeight, int64_t nModifierTime, int64_t nWeight) {
    typedef std::pair<uint32_t, const char*> mult;

    static std::map<int, mult> stakeTargetMultipliers;
    if (stakeTargetMultipliers.empty()) {
        std::multimap<int, mult> mm = boost::assign::map_list_of
        #include "multipliers.hpp"
            ;
        for (auto i = mm.begin(); i != mm.end();) {
            auto p = stakeTargetMultipliers.emplace(i->first, i->second).first;
            for (auto e = mm.upper_bound(i->first); i != e && i != mm.end(); ++i) {
                if (i->second.first < p->second.first) continue;
                if (i->second.first > p->second.first) p->second = i->second;
                else if (uint256(i->second.second) > uint256(p->second.second)) p->second = i->second;
            }
        }
    }

    if (stakeTargetMultipliers.count(nModifierHeight)) {
        const mult& m = stakeTargetMultipliers[nModifierHeight];
        bnTarget = bnTarget * m.first + uint256(m.second);
        return true;
    }
    return false;
}

// LUX Stake modifier used to hash the stake kernel which is chosen as the stake
// modifier (nStakeMinAge - nSlectionTime). So at least, it selects the period later than the stake produces since from the nStakeMinAge
static bool GetLuxStakeKernel(unsigned int nTimeTx, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake) {
    unsigned int nStakeMinAge = Params().StakingMinAge();
    const CBlockIndex* pindex = pindexBestHeader;
    nStakeModifierHeight = pindex->nHeight;
    nStakeModifierTime = pindex->GetBlockTime();
    int64_t nSelectionTime = GetSelectionTime();

    if (fDebug) {
        if (fPrintProofOfStake) {
            LogPrintf("%s : stake modifier time: %s interval: %d, time: %s\n", __func__,
                      DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nStakeModifierTime).c_str(),
                      nSelectionTime, DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nTimeTx).c_str());
        }
    }

    // Loop to find the stake modifier earlier
    if (nStakeModifierTime + nStakeMinAge - nSelectionTime <= nTimeTx) {
        // Best block is more than (nStakeMinAge - nSlectionTime). Older than kernel timestamp
        if (fDebug) {
            if (fPrintProofOfStake)
                return error("GetKernelStakeModifier() : best block %s at height %d too old for stake",
                             pindex->GetBlockHash().ToString().c_str(), pindex->nHeight);
            else
                return false;
        }
    }

    // TODO: Test before use
    while (nStakeModifierTime + nStakeMinAge - nSelectionTime > nTimeTx) {
        if (!pindex->pprev) {
            return error("GetKernelStakeModifier() : reached genesis block");
        }

        pindex = pindex->pprev;
        if (pindex->GeneratedStakeModifier()) {
            nStakeModifierHeight = pindex->nHeight;
            nStakeModifierTime = pindex->GetBlockTime();
        }
    }

    nStakeModifier = pindex->nStakeModifier;
    return true;
}

// Get the StakeModifier specified by our protocol for the checkhash function.
// Note: Separated GetKernelStakeModifier & GetLuxStakeKernel for convenient in case we need any other fork later on
static bool GetKernelStakeModifier(unsigned int nTimeTx, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake) {
        return GetLuxStakeKernel(nTimeTx, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, fPrintProofOfStake);
}

//instead of looping outside and reinitializing variables many times, we will give a nTimeTx and also search interval so that we can do all the hashing here
bool Stake::CheckHashOld(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock& blockFrom, const CTransaction& txPrev, const COutPoint& prevout, unsigned int& nTimeTx, uint256& hashProofOfStake) {
    if (pindexPrev == nullptr)
        return false;

    unsigned int nTimeBlockFrom = blockFrom.GetBlockTime();

    if (nTimeTx < txPrev.nTime) {  // Transaction timestamp violation
        return false; //error("%s: nTime violation (nTime=%d, nTimeTx=%d)", __func__, txPrev.nTime, nTimeTx);
    }

    if (GetStakeAge(nTimeBlockFrom) > nTimeTx) // Min age requirement
        return false; //error("%s: min age violation (nBlockTime=%d, nTimeTx=%d)", __func__, nTimeBlockFrom, nTimeTx);

    if (nHashInterval < Params().StakingInterval()) {
        nHashInterval = Params().StakingInterval();
    }
    if (nSelectionPeriod < Params().StakingRoundPeriod()) {
        nSelectionPeriod = Params().StakingRoundPeriod();
    }
    if (nStakeMinAge < Params().StakingMinAge()) {
        nStakeMinAge = Params().StakingMinAge();
    }

    // Base target difficulty
    uint256 bnTarget;
    bnTarget.SetCompact(nBits);

    // Weighted target
    int64_t nValueIn = txPrev.vout[prevout.n].nValue;
    uint256 bnWeight = uint256(nValueIn);
    bnTarget *= bnWeight; // comment out this will cause 'ERROR: CheckWork: invalid proof-of-stake at block 1144'

    uint64_t nStakeModifier = pindexPrev->nStakeModifier;
    int nStakeModifierHeight = pindexPrev->nHeight;
    int64_t nStakeModifierTime = pindexPrev->nTime;

    // Calculate hash
    CDataStream ss(SER_GETHASH, 0);
    ss << nStakeModifier << nTimeBlockFrom << txPrev.nTime << prevout.hash << prevout.n << nTimeTx;
    if (ENABLE_ADVANCED_STAKING && (GetBoolArg("-regtest", false) || nStakeModifierHeight >= ADVANCED_STAKING_HEIGHT)) {
        ss << nHashInterval << nSelectionPeriod << nStakeMinAge << nStakeSplitThreshold
           << bnWeight << nStakeModifierTime;
    }
    hashProofOfStake = Hash(ss.begin(), ss.end());

    if (fDebug) {
#       if 0
        LogPrintf("%s: using modifier 0x%016x at height=%d timestamp=%s for block from timestamp=%s\n", __func__,
                  nStakeModifier, nStakeModifierHeight,
                  DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nStakeModifierTime).c_str(),
                  DateTimeStrFormat("%Y-%m-%d %H:%M:%S", blockFrom.GetBlockTime()).c_str());
        LogPrintf("%s: check modifier=0x%016x nTimeBlockFrom=%u nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s\n", __func__,
                  nStakeModifier,
                  blockFrom.GetBlockTime(), txPrev.nTime, prevout.n, nTimeTx,
                  hashProofOfStake.ToString());
#       endif
        DEBUG_DUMP_STAKING_INFO_CheckHash();
    }

    if (Params().NetworkID() == CBaseChainParams::MAIN && hashProofOfStake > bnTarget && nStakeModifierHeight < 174453 && nStakeModifierHeight <= LAST_MULTIPLIED_BLOCK) {
        DEBUG_DUMP_MULTIFIER();
        if (!MultiplyStakeTarget(bnTarget, nStakeModifierHeight, nStakeModifierTime, nValueIn)) {
#           if 1
            return false;
#           else
            return error("%s: cant adjust stake target %s, %d, %d", __func__, bnTarget.GetHex(), nStakeModifierHeight, nStakeModifierTime);
#           endif
        }
    }

    // Now check if proof-of-stake hash meets target protocol
    return !(hashProofOfStake > bnTarget);
}

// New CheckHash function
bool Stake::CheckHashNew(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock& blockFrom, const CTransaction& txPrev, const COutPoint& prevout, unsigned int& nTimeTx, uint256& hashProofOfStake) {
    if (pindexPrev == nullptr)
        return false;

    unsigned int nTimeBlockFrom = blockFrom.GetBlockTime();

    // Transaction timestamp violation
    if (nTimeTx < txPrev.nTime) {
        return false;
    }

    // Min age requirement
    if (GetStakeAge(nTimeBlockFrom) > nTimeTx)
        return false;

    if (nHashInterval < Params().StakingInterval()) {
        nHashInterval = Params().StakingInterval();
    }
    if (nSelectionPeriod < Params().StakingRoundPeriod()) {
        nSelectionPeriod = Params().StakingRoundPeriod();
    }
    if (nStakeMinAge < Params().StakingMinAge()) {
        nStakeMinAge = Params().StakingMinAge();
    }

    // Base target difficulty
    uint256 bnTarget;
    bnTarget.SetCompact(nBits);

    // Weighted target
    int64_t nValueIn = txPrev.vout[prevout.n].nValue;
    uint256 bnWeight = uint256(nValueIn);

    int64_t nTimeWeight = min((int64_t)nTimeTx - txPrev.nTime, Params().StakingMinAge());
    if(nTimeWeight) {
        bnWeight = uint256(nValueIn) * nTimeWeight / COIN / (24 * 60 * 60);
    }
    bnTarget *= bnWeight;

    uint64_t nStakeModifier = pindexPrev->nStakeModifier;
    int nStakeModifierHeight = pindexPrev->nHeight;
    int64_t nStakeModifierTime = pindexPrev->nTime;

    // Calculate hash
    CDataStream ss(SER_GETHASH, 0);

    if (!GetKernelStakeModifier(nTimeBlockFrom, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, true))
        return false;

    ss << nStakeModifier;

    ss << nTimeBlockFrom << txPrev.nTime << prevout.hash << prevout.n << nTimeTx;

    hashProofOfStake = Hash(ss.begin(), ss.end());

    if (fDebug) {
        LogPrintf("%s: using modifier 0x%016x at height=%d timestamp=%s for block from timestamp=%s\n", __func__,
                  nStakeModifier, nStakeModifierHeight,
                  DateTimeStrFormat("%Y-%m-%d %H:%M:%S", nStakeModifierTime).c_str(),
                  DateTimeStrFormat("%Y-%m-%d %H:%M:%S", blockFrom.GetBlockTime()).c_str());
        LogPrintf("%s: check modifier=0x%016x nTimeBlockFrom=%u nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s\n", __func__,
                  nStakeModifier,
                  blockFrom.GetBlockTime(), txPrev.nTime, prevout.n, nTimeTx,
                  hashProofOfStake.ToString());
        DEBUG_DUMP_STAKING_INFO_CheckHash();
    }

    if (Params().NetworkID() == CBaseChainParams::MAIN && hashProofOfStake > bnTarget && nStakeModifierHeight < 174453 && nStakeModifierHeight <= LAST_MULTIPLIED_BLOCK) {
        DEBUG_DUMP_MULTIFIER();
        if (!MultiplyStakeTarget(bnTarget, nStakeModifierHeight, nStakeModifierTime, nValueIn)) {
            return false;
        }
    }

    // Now check if proof-of-stake hash meets target protocol
    return !(hashProofOfStake > bnWeight * bnTarget);
}

// Check our Proof of Stake hash meets target protocol
bool Stake::CheckHash(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock& blockFrom, const CTransaction& txPrev, const COutPoint& prevout, unsigned int& nTimeTx, uint256& hashProofOfStake) {

    int nBlockHeight = pindexPrev ? pindexPrev->nHeight + 1 : chainActive.Height() + 1;
    if (IsTestNet()) {
        return (nBlockHeight < nLuxProtocolSwitchHeightTestnet) ? CheckHashOld(pindexPrev, nBits, blockFrom, txPrev, prevout, nTimeTx, hashProofOfStake) : CheckHashNew(pindexPrev, nBits, blockFrom, txPrev, prevout, nTimeTx, hashProofOfStake);
    }

    return (nBlockHeight < nLuxProtocolSwitchHeight) ? CheckHashOld(pindexPrev, nBits, blockFrom, txPrev, prevout, nTimeTx, hashProofOfStake) : CheckHashNew(pindexPrev, nBits, blockFrom, txPrev, prevout, nTimeTx, hashProofOfStake);
}

bool Stake::isForbidden(const CScript& scriptPubKey)
{
    CTxDestination dest; uint160 hash;
    if (ExtractDestination(scriptPubKey, dest)) {
        hash = GetHashForDestination(dest);
        // see Hex converter
        if (hash.ToStringReverseEndian() == "70d3f1e0dd2d7c267066670d2d302f6506cf0146") return true;
    }
    return false;
}

// Check kernel hash target and coinstake signature
bool Stake::CheckProof(CBlockIndex* const pindexPrev, const CBlock &block, uint256& hashProofOfStake)
{
    int nBlockHeight = pindexPrev ? pindexPrev->nHeight + 1 : chainActive.Height() + 1;

    // Reject all blocks from older forks
    if (nBlockHeight > SNAPSHOT_BLOCK && block.nTime < SNAPSHOT_VALID_TIME)
        return error("%s: Invalid block %d, time too old (%x) for %s", __func__, nBlockHeight, block.nTime, block.GetHash().GetHex());

    if (block.vtx.size() < 2)
        return error("%s: called on non-coinstake %s", __func__, block.ToString());

    const CTransaction& tx = block.vtx[1];
    if (!tx.IsCoinStake())
        return error("%s: called on non-coinstake %s", __func__, tx.ToString());

    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTxIn& txin = tx.vin[0];

    // First try finding the previous transaction in database
    uint256 prevBlockHash;
    CTransaction txPrev;
    const Consensus::Params& consensusparams = Params().GetConsensus();
    if (!GetTransaction(txin.prevout.hash, txPrev, consensusparams, prevBlockHash, true))
        return error("%s: read txPrev failed", __func__);

    // Verify amount and split
    const CAmount& amount = txPrev.vout[txin.prevout.n].nValue;
    if (nBlockHeight >= REJECT_INVALID_SPLIT_BLOCK_HEIGHT) {
        if (tx.vout.size() > 3 && amount < (CAmount)(GetStakeCombineThreshold() * COIN * 2))
            return error("%s: Invalid stake block format", __func__);
        if (amount < STAKE_INVALID_SPLIT_MIN_COINS)
            return error("%s: Invalid stake block", __func__);
        if (isForbidden(txPrev.vout[txin.prevout.n].scriptPubKey))
            return error("%s: Refused stake block", __func__);
    }

    // Verify signature and script
    ScriptError err = SCRIPT_ERR_OK;
    bool isVerified = VerifyScript(txin.scriptSig, txPrev.vout[txin.prevout.n].scriptPubKey,
            tx.wit.vtxinwit.size() > 0 ? &tx.wit.vtxinwit[0].scriptWitness : NULL, STANDARD_SCRIPT_VERIFY_FLAGS,
            TransactionSignatureChecker(&tx, 0, amount), &err);
    if (!isVerified)
        return error("%s: VerifySignature failed on %s coinstake, err %d", __func__, FormatMoney(amount), (int)err);

    CBlockIndex* pindex = LookupBlockIndex(prevBlockHash);
    if (!pindex)
        return error("%s: read block failed", __func__);

    // Read block header
    CBlock prevBlock;
    if (!ReadBlockFromDisk(prevBlock, pindex->GetBlockPos(), pindex->nHeight, consensusparams))
        return error("%s: failed to find block", __func__);

    unsigned int nTime = block.nTime;

    return CheckHash(pindexPrev, block.nBits, prevBlock, txPrev, txin.prevout, nTime, hashProofOfStake);
}

// Get stake modifier checksum
unsigned int Stake::GetModifierChecksum(const CBlockIndex* pindex) {
    assert(pindex->pprev || pindex->GetBlockHash() == Params().HashGenesisBlock());
    // Hash previous checksum with flags, hashProofOfStake and nStakeModifier
    CDataStream ss(SER_GETHASH, 0);
    if (pindex->pprev)
        ss << pindex->pprev->nStakeModifierChecksum;
    ss << pindex->nFlags << pindex->hashProofOfStake << pindex->nStakeModifier;
    uint256 hashChecksum = Hash(ss.begin(), ss.end());
    hashChecksum >>= (256 - 32);
    return hashChecksum.Get64();
}

// Check stake modifier hard checkpoints
bool Stake::CheckModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum) {
    if (!IsTestNet()) return true; // Testnet has no checkpoints

    // Hard checkpoints of stake modifiers to ensure they are deterministic
    static std::map<int, unsigned int> mapStakeModifierCheckpoints = boost::assign::map_list_of(0, 0xfd11f4e7u)
        (1,    0x6bf176c7u) (10,   0xc533c5deu) (20,   0x722262a0u) (30,   0xdd0abd8au) (90,   0x0308db9cu)
        (409,  0xa9e2be4au) (410,  0xc23d7dd1u) (1141, 0xe81ae310u) (1144, 0x6543da9du) (1154, 0x8d110f11u)
        (2914, 0x4fc1bc8du) (2916, 0x838297bau) (2915, 0xf5c77be4u) (2991, 0x6873f1efu) (3000, 0xffc1801fu)
        (3001, 0x4b76d1f9u) (3035, 0x5cd70041u) (3036, 0xc689f15au) (3040, 0x19e1fa9eu) (3046, 0xa53146c5u)
        (3277, 0x992f3f6cu) (3278, 0x9db692d0u) (3288, 0x96fb270du) (3438, 0x2ea722b2u) (4003, 0xdf3987e9u)
        (4012, 0x205080bcu) (4025, 0x19ebed80u) (4034, 0xd02dd7ecu) (4045, 0x4b753d54u) (4053, 0xe7265e2au)
        (10004,  0x09b6b5e1u) (10016,  0x05be852du) (10023,  0x4dcc3f34u) (10036,  0x5c16bf7du) (10049,  0x3b542151u)
        (19998,  0x52052da4u) (20338,  0x3174b362u) (20547,  0x5e94b5acu) (20555,  0x5d77d04au) (33742,  0x998c4a1bu)
        (127733, 0x92aa36acu) (129248, 0x680b9ce2u) (130092, 0x202cab1fu) (130775, 0x09694eb8u) (130780, 0x02e5287fu)
        (131465, 0x515203adu) (132137, 0xb14a3a42u) (132136, 0x81b5ef99u) (135072, 0xea90da6au) (139756, 0xb3d7fb47u)
        (141584, 0xeee6259fu) (143866, 0xcd2ed8ddu) (151181, 0xc2377de7u) (151659, 0x2bb1e741u) (151660, 0xade7324du)
        ;
    if (mapStakeModifierCheckpoints.count(nHeight)) {
        return nStakeModifierChecksum == mapStakeModifierCheckpoints[nHeight];
    }
    return true;
}

unsigned int Stake::GetStakeAge(unsigned int nTime) const {
    if (nStakeMinAge < Params().StakingMinAge()) {
        auto that = const_cast<Stake*>(this);
        that->nStakeMinAge = Params().StakingMinAge();
    }
    return nStakeMinAge + nTime;
}

void Stake::ResetInterval() {
    nStakeInterval = 0;
}

bool Stake::HasStaked() const {
    return nLastStakeTime > 0 && nStakeInterval > 0;
}

bool Stake::MarkBlockStaked(int nHeight, unsigned int nTime) {
    bool result = false;
    auto it = mapHashedBlocks.find(nHeight);
    if (it == mapHashedBlocks.end()) {
        auto res = mapHashedBlocks.emplace((unsigned int) nHeight, nTime);
        result = res.second && res.first != mapHashedBlocks.end();
    }
    return result;
}

bool Stake::IsBlockStaked(int nHeight) const {
    bool result = false;
    auto it = mapHashedBlocks.find(nHeight);
    if (it != mapHashedBlocks.end()) {
        if (nHashInterval < (unsigned int) Params().StakingInterval()) {
            auto that = const_cast<Stake*>(this);
            that->nHashInterval = (unsigned int) Params().StakingInterval();
        }
        if (GetTime() - it->second < max(nHashInterval, (unsigned int) 1)) {
            result = true;
        }
    }
    return result;
}

bool Stake::IsBlockStaked(const CBlock* block) const {
    bool result = false;
    if (block->IsProofOfStake()) {
        result = mapStakes.find(block->vtx[1].vin[0].prevout) != mapStakes.end();
    }
    return result;
}

CAmount Stake::ReserveBalance(CAmount amount) {
    auto prev = nReserveBalance;
    nReserveBalance = amount;
    return prev;
}

CAmount Stake::GetReservedBalance() const {
    return nReserveBalance;
}

uint64_t Stake::GetSplitThreshold() const {
    return GetStakeCombineThreshold();
}

void Stake::SetSplitThreshold(uint64_t v) {
    nStakeSplitThreshold = GetStakeCombineThreshold();
}

void Stake::MarkStake(const COutPoint& out, unsigned int nTime) {
    mapStakes[out] = nTime;
}

bool Stake::IsStaked(const COutPoint& out) const {
    return mapStakes.find(out) != mapStakes.end();
}

bool Stake::HasProof(const uint256& h) const {
    return mapProofOfStake.find(h) != mapProofOfStake.end();
}

bool Stake::GetProof(const uint256& h, uint256& proof) const {
    bool result = false;
    auto it = mapProofOfStake.find(h);
    if (it != mapProofOfStake.end()) {
        proof = it->second;
        result = true;
    }
    return result;
}

void Stake::SetProof(const uint256& h, const uint256& proof) {
    mapProofOfStake.emplace(h, proof);
}

bool Stake::IsActive() const {
    bool nStaking = false;
    auto tip = chainActive.Tip();
    if (GetBoolArg("-staking", DEFAULT_STAKE) == false)
        return false;
    if (mapHashedBlocks.count(tip->nHeight))
        nStaking = true;
    else if (mapHashedBlocks.count(tip->nHeight - 1) && HasStaked())
        nStaking = true;
    return nStaking;
}

bool Stake::SelectStakeCoins(CWallet* wallet, std::set <std::pair<const CWalletTx*, unsigned int>>& stakecoins, const int64_t targetAmount) {
    auto const nTime = GetTime();
    if (nSelectionPeriod < Params().StakingRoundPeriod()) {
        nSelectionPeriod = Params().StakingRoundPeriod();
    }
    if (nTime - nLastSelectTime < nSelectionPeriod) {
        return false;
    }

    int64_t selectedAmount = 0;
    bool hasMinInputSize = chainActive.Height() >= REJECT_INVALID_SPLIT_BLOCK_HEIGHT;
    vector<COutput> coins;
    wallet->AvailableCoins(coins, true);
    stakecoins.clear();
    for (auto const& out : coins) {
        CAmount nValue = out.tx->vout[out.i].nValue;
        bool addToSet = true;
        // make sure not to outrun target amount
        if (selectedAmount >= targetAmount)
            break;

        // do not add small inputs to stake set
        if (hasMinInputSize && nValue < STAKE_INVALID_SPLIT_MIN_COINS)
            addToSet = false;

        // do not add locked coins to stake set
        if (wallet->IsLockedCoin(out.tx->GetHash(), out.i))
            addToSet = false;

        // check for min age
        auto const nAge = stake->GetStakeAge(out.tx->GetTxTime());
        if (nTime < nAge) continue;

        // check that it is matured
        if (out.nDepth < (out.tx->IsCoinStake() ? Params().COINBASE_MATURITY() : 10))
            continue;

        // add to our stake set
        if (addToSet)
            stakecoins.insert(make_pair(out.tx, out.i));
        selectedAmount += nValue;
    }
    if (!stakecoins.empty()) {
        if (hasMinInputSize && selectedAmount < STAKE_INVALID_SPLIT_MIN_COINS)
            stakecoins.clear();
        nLastSelectTime = nTime;
        return true;
    }
    return false;
}

// Same as GetDifficulty, but takes bits as an argument
double GetBlockDifficulty(unsigned int nBits) {
    int nShift = (nBits >> 24) & 0xff;
    double dDiff = (double)0x0000ffff / (double)(nBits & 0x00ffffff);
    while (nShift < 29) { dDiff *= 256.0;nShift++; }
    while (nShift > 29) { dDiff /= 256.0;nShift--; }
    return dDiff;
}

bool Stake::CreateCoinStake(CWallet* wallet, const CKeyStore& keystore, unsigned int nBits, int64_t nSearchInterval, CMutableTransaction& txNew, unsigned int& nTxNewTime) {
    txNew.vin.clear();
    txNew.vout.clear();

    double dStakeValueSum = 0.0;
    double dStakeDiffSum = 0.0;
    double dStakeDiffMax = 0.0;
    int64_t nStakeWeightMin = MAX_MONEY;
    int64_t nStakeWeightMax = 0;
    int64_t nStakeWeightSum = 0;
    int64_t nCoinWeight = 0;
    uint64_t nStakeCoinAgeSum = 0;

    // Mark coin stake transaction
    CScript scriptEmpty;
    scriptEmpty.clear();
    txNew.vout.push_back(CTxOut(0, scriptEmpty));

    // Choose coins to use
    int64_t nBalance = wallet->GetBalance();

    if (mapArgs.count("-reservebalance") && !ParseMoney(mapArgs["-reservebalance"], nReserveBalance)) {
        return error("%s: invalid reserve balance amount", __func__);
    }

    if (nBalance <= nReserveBalance) {
        return false;
    }

    // presstab HyperStake - Initialize as static and don't update the set on every run of
    // CreateCoinStake() in order to lighten resource use
    static std::set< pair<const CWalletTx*, unsigned int> > stakeCoins;
    if (!SelectStakeCoins(wallet, stakeCoins, nBalance - nReserveBalance)) {
        return false;
    }

    int64_t nCredit = 0;
    uint256 bnCentSecond = 0; // coin age in the unit of cent-seconds
    CScript scriptPubKeyKernel;
    vector<const CWalletTx*> vCoins;

    //prevent staking a time that won't be accepted
    if (GetAdjustedTime() <= chainActive.Tip()->nTime)
        MilliSleep(10000);

    const CBlockIndex* pIndex0 = chainActive.Tip();
    for (std::pair<const CWalletTx*, unsigned int> pcoin : stakeCoins) {
        //make sure that enough time has elapsed between
        CBlockIndex* pindex = LookupBlockIndex(pcoin.first->hashBlock);
        if (!pindex) {
            if (fDebug)
                LogPrintf("%s: failed to find block index \n", __func__);
            continue;
        }

        //this is genesis block, which supposedly should not be stake block, so skip it
        if (pindex->pprev == nullptr)
            continue;

        // Read block header
        CBlockHeader block = pindex->GetBlockHeader();

        bool fKernelFound = false;
        uint256 hashProofOfStake = 0;
        COutPoint prevoutStake = COutPoint(pcoin.first->GetHash(), pcoin.second);
        nTxNewTime = GetAdjustedTime();

        nCoinWeight = pcoin.first->vout[pcoin.second].nValue;
        uint256 bnStakeTarget;
        bnStakeTarget.SetCompact(nBits);
        bnStakeTarget *= nCoinWeight;
        nStakeWeightSum += nCoinWeight;
        nStakeWeightMin = std::min(nStakeWeightMin, nCoinWeight);
        nStakeWeightMax = std::max(nStakeWeightMax, nCoinWeight);
        double dStakeKernelDiff = GetBlockDifficulty(nBits);
        dStakeDiffSum += dStakeKernelDiff;
        dStakeDiffMax = std::max(dStakeDiffMax, dStakeKernelDiff);
//        uint64_t coinAge = 0;
//        if (GetCoinAge(*pcoin.first, pcoin.first->nTime, coinAge)) {
//            nStakeCoinAgeSum += coinAge;
//        }

        //iterates each utxo inside of CheckStakeKernelHash()
        if (CheckHash(pindex->pprev, bnStakeTarget.GetCompact(), block, *pcoin.first, prevoutStake, nTxNewTime, hashProofOfStake)) {
            //Double check that this will pass time requirements
            if (nTxNewTime <= chainActive.Tip()->GetMedianTimePast()) {
                LogPrintf("%s: stake found, but it is too far in the past \n", __func__);
                continue;
            }

            vector<valtype> vSolutions;
            txnouttype whichType;
            CScript scriptPubKeyOut;
            scriptPubKeyKernel = pcoin.first->vout[pcoin.second].scriptPubKey;
            if (!Solver(scriptPubKeyKernel, whichType, vSolutions)) {
                LogPrintf("%s: failed to parse kernel\n", __func__);
                break;
            }

            if (fDebug && GetBoolArg("-printcoinstake", false))
                LogPrintf("%s: parsed kernel type=%d\n", __func__, whichType);

            if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH) {
                if (fDebug && GetBoolArg("-printcoinstake", false))
                    LogPrintf("%s: no support for kernel type=%d\n", __func__, whichType);
                break; // only support pay to public key and pay to address
            } else if (whichType == TX_PUBKEYHASH) { // pay to address type
                //convert to pay to public key type
                CKey key;
                CKeyID keyID = CKeyID(uint160(vSolutions[0]));
                if (!pwalletMain->GetKey(keyID, key))
                    return false;

                scriptPubKeyOut << key.GetPubKey() << OP_CHECKSIG;
            } else {
                scriptPubKeyOut = scriptPubKeyKernel;
            }

            auto nValueIn = pcoin.first->vout[pcoin.second].nValue;
            txNew.vin.push_back(CTxIn(pcoin.first->GetHash(), pcoin.second));
            bnCentSecond += uint256(nValueIn) * (nTxNewTime - pIndex0->nTime);
            nCredit += nValueIn;
            vCoins.push_back(pcoin.first);
            txNew.vout.push_back(CTxOut(0, scriptPubKeyOut));

            //presstab HyperStake - calculate the total size of our new output including the stake reward so that we can use it to decide whether to split the stake outputs
            const CBlockIndex* pIndex0 = chainActive.Tip();
            uint64_t nTotalSize = pcoin.first->vout[pcoin.second].nValue + GetProofOfStakeReward(0, 0, pIndex0->nHeight);

            //presstab HyperStake - if MultiSend is set to send in coinstake we will add our outputs here (values asigned further down)
            if (nTotalSize / 2 > (uint64_t)(GetStakeCombineThreshold() * COIN))
                txNew.vout.push_back(CTxOut(0, scriptPubKeyOut)); //split stake

            fKernelFound = true;

            LOCK(stakeMiner.lock);
            stakeMiner.nKernelsFound++;
            stakeMiner.dKernelDiffMax = 0;
            stakeMiner.dKernelDiffSum = dStakeDiffSum;

            break;
        }
        if (fKernelFound) break; // if kernel is found stop searching
    }
    if (nCredit == 0 || nCredit > nBalance - nReserveBalance) {
        return false;
    }

    // Calculate reward
    uint256 bnCoinDay = bnCentSecond / COIN / (24 * 60 * 60);
    uint64_t nReward = GetProofOfStakeReward(bnCoinDay.GetCompact(), 0, pIndex0->nHeight);
    nCredit += nReward;

    int64_t nMinFee = 0;
    while (true) {
        // Set output amount
        if (txNew.vout.size() == 3) {
            txNew.vout[1].nValue = ((nCredit - nMinFee) / 2 / CENT) * CENT;
            txNew.vout[2].nValue = nCredit - nMinFee - txNew.vout[1].nValue;
        } else {
            txNew.vout[1].nValue = nCredit - nMinFee;
        }

        // Limit size
        unsigned int nBytes = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);
        if (nBytes >= DEFAULT_BLOCK_MAX_SIZE / 5) {
            return error("%s: exceeded coinstake size limit", __func__);
        }

        // Check enough fee is paid
        CAmount nFeeNeeded = wallet->GetMinimumFee(nBytes, nTxConfirmTarget, mempool);
        if (nMinFee < nFeeNeeded) {
            nMinFee = nFeeNeeded;
            continue; // try signing again
        } else {
            if (fDebug) LogPrintf("%s: fee for stake %d (%s, %s)\n", __func__,
                        nMinFee, FormatMoney(nMinFee), FormatMoney(nFeeNeeded));
            break;
        }
    }

    int numout = 0;
    CScript payeeScript;
    bool hasMasternodePayment = SelectMasternodePayee(payeeScript);
	CAmount blockValue = nCredit;
    CAmount masternodePayment = GetMasternodePosReward(chainActive.Height() + 1, nReward);

    if (hasMasternodePayment) {
        numout = txNew.vout.size();
        txNew.vout.resize(numout + 1);
        txNew.vout[numout].scriptPubKey = payeeScript;
        txNew.vout[numout].nValue = masternodePayment;

        blockValue -= masternodePayment;

        CTxDestination txDest;
        ExtractDestination(payeeScript, txDest);
        if (fDebug) LogPrintf("%s: Masternode payment to %s (pos)\n", __func__, EncodeDestination(txDest));
        // Set output amount
        if (txNew.vout.size() == 4) { // 2 stake outputs, stake was split, plus a masternode payment
            txNew.vout[1].nValue = (blockValue / 2 / CENT) * CENT;
            txNew.vout[2].nValue = blockValue - txNew.vout[1].nValue;
        } else if (txNew.vout.size() == 3) { // only 1 stake output, was not split, plus a masternode payment
            txNew.vout[1].nValue = blockValue;
        }
    } else {
        if (txNew.vout.size() == 3) { // 2 stake outputs, stake was split, no masternode payment
            txNew.vout[1].nValue = (blockValue / 2 / CENT) * CENT;
            txNew.vout[2].nValue = blockValue - txNew.vout[1].nValue;
        } else if (txNew.vout.size() == 2) { // only 1 stake output, was not split, no masternode payment
            txNew.vout[1].nValue = blockValue;
        }
    }

    // Check vout values.
    unsigned i = 0;
    for (auto& vout : txNew.vout) {
        if (vout.nValue < 0 || vout.nValue > MAX_MONEY) {
            return error("%s: bad nValue (vout[%d].nValue = %d)", __func__, i, vout.nValue);
        }
        dStakeValueSum += vout.nValue /(double)COIN;
        i += 1;
    }

#if 1
    // Sign
    for (const CWalletTx* pcoin : vCoins) {
        if (!SignSignature(keystore, *pcoin, txNew, 0, SIGHASH_ALL))
            return error("%s: failed to sign coinstake", __func__);
    }
#else
    const CScript& pubKey = txNew.vout[1].scriptPubKey;
    SignatureData sigdata;
    bool isSigned = ProduceSignature(
            MutableTransactionSignatureCreator(&keystore, &txNew, 0, nCredit - nReward, SIGHASH_SINGLE),
            pubKey, sigdata);
    if (!isSigned)
        return error("%s: failed to sign %s stake", __func__, FormatMoney(nCredit - nReward));
    UpdateTransaction(txNew, 0, sigdata);
    //const CTxIn& txin = txNew.vin[0];
    //LogPrintf("%s: sign %s %s\n", __func__, FormatMoney(nCredit - nReward), txin.ToString());
#endif

    // Successfully generated coinstake, reset select timestamp to 
    // start next round as soon as possible.
    nLastSelectTime = 0;

    LOCK(stakeMiner.lock);
    stakeMiner.nWeightSum = nStakeWeightSum;
    stakeMiner.dValueSum = dStakeValueSum;
    stakeMiner.nWeightMin = nStakeWeightMin;
    stakeMiner.nWeightMax = nStakeWeightMax;
    stakeMiner.nCoinAgeSum = nStakeCoinAgeSum;
    stakeMiner.dKernelDiffMax = std::max(stakeMiner.dKernelDiffMax, dStakeDiffMax);
    stakeMiner.dKernelDiffSum = dStakeDiffSum;
    stakeMiner.nLastCoinStakeSearchInterval = txNew.nTime;
    return true;
}

bool Stake::CreateBlockStake(CWallet* wallet, CBlock* block) {
    bool result = false;
    int64_t nTime = GetAdjustedTime();
    CBlockIndex* tip = chainActive.Tip();
    block->nTime = nTime;
    block->nBits = GetNextWorkRequired(tip, block, Params().GetConsensus(), true);
    if (nTime >= nLastStakeTime) {
        CMutableTransaction tx;
        unsigned int txTime = 0;
        if (CreateCoinStake(wallet, *wallet, block->nBits, nTime - nLastStakeTime, tx, txTime)) {
            block->nTime = txTime;
            CMutableTransaction buftx = CMutableTransaction(block->vtx[0]);
            buftx.vout[0].SetEmpty();
            block->vtx[0] = CTransaction(buftx);
            block->vtx[1] = CTransaction(tx);
            result = true;
        }
        nStakeInterval = nTime - nLastStakeTime;
        nLastStakeTime = nTime;
    }
    return result;
}

bool Stake::GenBlockStake(CWallet* wallet, unsigned int& extra) {
    CBlockIndex* tip = nullptr;
    {
        LOCK(cs_main);
        tip = chainActive.Tip();
    }

    std::unique_ptr <CBlockTemplate> blocktemplate(BlockAssembler(Params()).CreateNewStake(true, true));
    if (!blocktemplate) {
        return false; // No stake available.
    }

    CBlock* const block = &blocktemplate->block;
    if (!block->IsProofOfStake()) {
        return error("%s: Created non-staked block:\n%s", __func__, block->ToString());
    }

    IncrementExtraNonce(block, tip, extra);

    if (!block->SignBlock(*wallet)) {
        return error("%s: Cant sign new block.", __func__);
    }

    {
        LOCK(stakeMiner.lock);
        stakeMiner.nBlocksCreated++;
    }

    SetThreadPriority(THREAD_PRIORITY_NORMAL);

    bool usePhi2;
    {
        LOCK(cs_main);
        CBlockIndex* pindexPrev = LookupBlockIndex(block->hashPrevBlock);
        usePhi2 = pindexPrev ? pindexPrev->nHeight + 1 >= Params().SwitchPhi2Block() : false;
    }

    bool result = true;
    uint256 proof1, proof2;
    auto hash = block->GetHash(usePhi2);
    auto good = CheckProof(tip, *block, proof1);

#if defined(DEBUG_DUMP_STAKE_CHECK) && defined(DEBUG_DUMP_STAKING_INFO)
    DEBUG_DUMP_STAKE_CHECK();
#endif
    if (good) {
#if defined(DEBUG_DUMP_STAKE_FOUND) && defined(DEBUG_DUMP_STAKING_INFO)
        DEBUG_DUMP_STAKE_FOUND();
#endif
        LogPrintf("%s: found stake %s, block %s\n%s\n", __func__,
                  proof1.GetHex(), hash.GetHex(), block->ToString());
        ProcessBlockFound(block, *wallet);
    } else if (!GetProof(hash, proof2)) {
        SetProof(hash, proof1);
    } else if (proof1 != proof2) {
        result = error("%s: diverged stake %s, %s (block %s)", __func__,
                       proof1.GetHex(), proof2.GetHex(), hash.GetHex());
    }

    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    return result;
}

void Stake::StakingThread(CWallet* wallet) {
    LogPrintf("staking thread started!\n");

    SetThreadPriority(THREAD_PRIORITY_LOWEST);

    try {
        int nHeight = 0;
        (void) nHeight;
        unsigned int extra = 0;
        while (!nStakingInterrupped && !ShutdownRequested()) {
            std::size_t nNodes = 0;
            bool nCanStake = !IsInitialBlockDownload();

            boost::this_thread::interruption_point();

            if (nCanStake) {
                LOCK(cs_vNodes); // Lock nodes for security.
                if ((nNodes = vNodes.size()) == 0) {
                    nCanStake = false;
                }
            }

            const CBlockIndex* tip = nullptr;
            if (nCanStake) {
                while (wallet->IsLocked() || nReserveBalance >= wallet->GetBalance()) {
                    if (!nStakingInterrupped && !ShutdownRequested()) {
                        nStakeInterval = 0;
                        MilliSleep(3000);
                        continue;
                    } else {
                        nCanStake = false;
                        break;
                    }
                }

                if (nCanStake) {
                    LOCK(cs_main);
                    tip = chainActive.Tip();
                    nHeight = tip->nHeight;
                    if (/*tip->nHeight < Params().LAST_POW_BLOCK() ||*/ IsBlockStaked(tip->nHeight)) {
                        nCanStake = false;
                    }
                }
            } else {
                // Give a break to avoid interrupting other jobs too much (e.g. syncing)
                MilliSleep(3000);
                continue;
            }

#if defined(DEBUG_DUMP_STAKING_THREAD) && defined(DEBUG_DUMP_STAKING_INFO)
            DEBUG_DUMP_STAKING_THREAD();
#endif
            boost::this_thread::interruption_point();

            if (nCanStake && GenBlockStake(wallet, extra)) {
                MilliSleep(1500);
            } else {
                MilliSleep(1000);
            }
        }
    } catch (std::exception& e) {
        LogPrintf("%s: exception: %s\n", __func__, e.what());
    } catch (...) {
        LogPrintf("%s: exception\n", __func__);
    }

    LogPrintf("%s: done!\n", __func__);
}

void Stake::GenerateStakes(boost::thread_group& group, CWallet* wallet, int procs) {
    if (GetBoolArg("-staking", DEFAULT_STAKE) == false) {
        LogPrintf("staking is disabled\n");
        return;
    }
#if 0
    static std::unique_ptr<boost::thread_group> StakingThreads(new boost::thread_group);
    if (procs == 0) {
        nStakingInterrupped = true;
        StakingThreads->interrupt_all();
        StakingThreads->join_all();
        nStakingInterrupped = false;
        return;
    }

    for (int i = 0; i < procs; ++i) {
        StakingThreads->create_thread(boost::bind(&Stake::StakingThread, this, wallet));
    }
#else
    nStakingInterrupped = procs == 0;
    for (int i = 0; i < procs; ++i) {
        group.create_thread(boost::bind(&Stake::StakingThread, this, wallet));
    }
#endif
}

Stake* Stake::Pointer() { return &kernel; }

Stake  Stake::kernel;
