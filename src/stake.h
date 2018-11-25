// Copyright (c) 2017 LUX developer
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_STAKING_H
#define BITCOIN_STAKING_H

#include "uint256.h"
#include "sync.h"
#include "amount.h"
#include <map>
#include <set>

class CBlock;
class CBlockIndex;
class CKeyStore;
class COutPoint;
class CReserveKey;
class CTransaction;
class CWallet;
class CWalletTx;

struct CMutableTransaction;

bool CheckCoinStakeTimestamp(uint32_t nTimeBlock);

static const int STAKE_TIMESTAMP_MASK = 15;

namespace boost { class thread_group; }

// Reject all splited stake blocks under 200 LUX
static const int REJECT_INVALID_SPLIT_BLOCK_HEIGHT = 465000;
// Reject stake of small inputs
static const CAmount STAKE_INVALID_SPLIT_MIN_COINS = 90 * COIN;

struct StakeKernel
{
protected:
    StakeKernel();
};

struct StakeStatus {
    CCriticalSection lock;
    double dValueSum;
    double dKernelDiffMax;
    double dKernelDiffSum;
    int64_t nLastCoinStakeSearchInterval;
    uint64_t nWeightSum, nWeightMin, nWeightMax;
    uint64_t nCoinAgeSum;
    uint64_t nBlocksCreated;
    uint64_t nBlocksAccepted;
    uint64_t nKernelsFound;

    void Clear();
    StakeStatus();
};

//Stake - singleton class encapsulating PoS feature for Lux.
class Stake : StakeKernel
{

    Stake();
    ~Stake();

    Stake(const Stake&) = delete;
    void operator=(const Stake&) = delete;

    static Stake kernel;

    int64_t nStakeInterval;
    int64_t nLastStakeTime;
    int64_t nLastSelectTime;
    int64_t nSelectionPeriod;
    uint64_t nStakeSplitThreshold;
    unsigned int nStakeMinAge;
    unsigned int nHashInterval;

    CAmount nReserveBalance;
    
    std::map<COutPoint, unsigned int> mapStakes;
    std::map<unsigned int, unsigned int> mapHashedBlocks;
    std::map<uint256, uint256> mapProofOfStake;
    std::map<uint256, int64_t> mapRejectedBlocks;

private:

    bool CreateCoinStake(CWallet *wallet, const CKeyStore& keystore, unsigned int nBits, int64_t nSearchInterval, CMutableTransaction& txNew, unsigned int& nTxNewTime);

    bool GenBlockStake(CWallet *wallet, unsigned int &extra);
    void StakingThread(CWallet *wallet);

public:

    static Stake *Pointer();
    
    static uint64_t GetStakeCombineThreshold() {
        return 100;
    }

    bool isForbidden(const CScript& scriptPubKey);
    bool CheckTimestamp(int64_t nTimeBlock, int64_t nTimeTx);

    StakeStatus stakeMiner;

    bool SelectStakeCoins(CWallet *wallet, std::set<std::pair<const CWalletTx*, unsigned int> >& stakecoins, const int64_t targetAmount);

    //Stake::ComputeNextModifier - compute the hash modifier for proof-of-stake
    bool ComputeNextModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

    //Stake::CheckHash - check whether stake kernel meets hash target
    //      Sets hashProofOfStake on success return
    bool CheckHash(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock &blockFrom, const CTransaction &txPrev, const COutPoint &prevout, unsigned int& nTimeTx, uint256& hashProofOfStake);
    bool CheckHashOld(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock &blockFrom, const CTransaction &txPrev, const COutPoint &prevout, unsigned int& nTimeTx, uint256& hashProofOfStake);
    bool CheckHashNew(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock &blockFrom, const CTransaction &txPrev, const COutPoint &prevout, unsigned int& nTimeTx, uint256& hashProofOfStake);

    //Stake::CheckProof - check kernel hash target and coinstake signature
    //      Sets hashProofOfStake on success return
    bool CheckProof(CBlockIndex* const pindexPrev, const CBlock &block, uint256& hashProofOfStake);

    //Stake::GetModifierChecksum - get stake modifier checksum
    unsigned int GetModifierChecksum(const CBlockIndex* pindex);

    //Stake::CheckModifierCheckpoints - check stake modifier hard checkpoints
    bool CheckModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum);

    //Stake::IsActive - check if staking is active.
    bool IsActive() const;
    bool HasStaked() const;

    //Stake::IsBlockStaked - check if block is staked.
    bool IsBlockStaked(int nHeight) const;
    bool IsBlockStaked(const CBlock* block) const;

    bool MarkBlockStaked(int nHeight, unsigned int nTime);

    CAmount ReserveBalance(CAmount amount);
    CAmount GetReservedBalance() const;

    uint64_t GetSplitThreshold() const;
    void SetSplitThreshold(uint64_t v);

    void MarkStake(const COutPoint &out, unsigned int nTime);
    bool IsStaked(const COutPoint &out) const;

    bool HasProof(const uint256 &h) const;
    bool GetProof(const uint256 &h, uint256 &proof) const;
    void SetProof(const uint256 &h, const uint256 &proof);
    
    //Stake::ResetInterval - reset stake interval.
    void ResetInterval();
    
    unsigned int GetStakeAge(unsigned int nTime) const;

    //Stake::CreateBlockStake - create a new stake.
    bool CreateBlockStake(CWallet *wallet, CBlock *block);

    void GenerateStakes(boost::thread_group &group, CWallet *wallet, int procs);
};

//stake - global staking pointer for convenient access of staking kernel.
extern Stake * const stake;

// Get time
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd);

#endif // BITCOIN_STAKING_H
