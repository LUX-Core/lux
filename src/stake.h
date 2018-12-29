// Copyright (c) 2012-2013 The PPCoin developers                -*- c++ -*-
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_STAKING_H__DUZY
#define BITCOIN_STAKING_H__DUZY

#include "uint256.h"
#include "sync.h"
#include "amount.h"
#include <map>
#include <set>

//!<DuzyDoc>: Class Declarations
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

//!<DuzyDoc>: Stake - singleton class encapsulating PoS feature for Lux.
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
    int64_t nStakeSplitThreshold;
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

    //!<DuzyDoc>: Stake::Pointer() - returns the staking pointer
    static Stake *Pointer();
    
    int64_t GetStakeCombineThreshold() const {
        return 100;
    }

    bool isForbidden(const CScript& scriptPubKey);
    bool getPrevBlock(const CBlock curBlock, CBlock &prevBlock, int &nBlockHeight);
    bool isSpeedAccepted(CBlock prevBlock, CBlock& prev1Block, int& height, int nBlockHeight,const unsigned ind);
    bool isSpeedValid(uint32_t nTime, CBlock prevBlock, CBlockIndex* pindex, int nBlockHeight);

    StakeStatus stakeMiner;

    bool SelectStakeCoins(CWallet *wallet, std::set<std::pair<const CWalletTx*, unsigned int> >& stakecoins, const int64_t targetAmount);

    //!<DuzyDoc>: Stake::ComputeNextModifier - compute the hash modifier for proof-of-stake
    bool ComputeNextModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

    //!<DuzyDoc>: Stake::CheckHash - check whether stake kernel meets hash target
    //!<DuzyDoc>:       Sets hashProofOfStake on success return
    bool CheckHash(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock &blockFrom, const CTransaction &txPrev, const COutPoint &prevout, unsigned int& nTimeTx, uint256& hashProofOfStake);
    bool CheckHashOld(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock &blockFrom, const CTransaction &txPrev, const COutPoint &prevout, unsigned int& nTimeTx, uint256& hashProofOfStake);
    bool CheckHashNew(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock &blockFrom, const CTransaction &txPrev, const COutPoint &prevout, unsigned int& nTimeTx, uint256& hashProofOfStake);

    //!<DuzyDoc>: Stake::CheckProof - check kernel hash target and coinstake signature
    //!<DuzyDoc>:       Sets hashProofOfStake on success return
    bool CheckProof(CBlockIndex* const pindexPrev, const CBlock &block, uint256& hashProofOfStake);

    //!<DuzyDoc>: Stake::GetModifierChecksum - get stake modifier checksum
    unsigned int GetModifierChecksum(const CBlockIndex* pindex);

    //!<DuzyDoc>: Stake::CheckModifierCheckpoints - check stake modifier hard checkpoints
    bool CheckModifierCheckpoints(int nHeight, unsigned int nStakeModifierChecksum);

    //!<DuzyDoc>: Stake::IsActive - check if staking is active.
    bool IsActive() const;
    bool HasStaked() const;

    //!<DuzyDoc>: Stake::IsBlockStaked - check if block is staked.
    bool IsBlockStaked(int nHeight) const;
    bool IsBlockStaked(const CBlock* block) const;

    bool MarkBlockStaked(int nHeight, unsigned int nTime);

    CAmount ReserveBalance(CAmount amount);
    CAmount GetReservedBalance() const;

    int64_t GetSplitThreshold() const;
    void SetSplitThreshold(int64_t); // param unused

    void MarkStake(const COutPoint &out, unsigned int nTime);
    bool IsStaked(const COutPoint &out) const;

    bool HasProof(const uint256 &h) const;
    bool GetProof(const uint256 &h, uint256 &proof) const;
    void SetProof(const uint256 &h, const uint256 &proof);
    
    //!<DuzyDoc>: Stake::ResetInterval - reset stake interval.
    void ResetInterval();
    
    unsigned int GetStakeAge(unsigned int nTime) const;

    //!<DuzyDoc>: Stake::CreateBlockStake - create a new stake.
    bool CreateBlockStake(CWallet *wallet, CBlock *block);

    void GenerateStakes(boost::thread_group &group, CWallet *wallet, int procs);
};

//!<DuzyDoc>: stake - global staking pointer for convenient access of staking kernel.
extern Stake * const stake;

// Get time
int64_t GetWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd);

#endif // BITCOIN_STAKING_H__DUZY
