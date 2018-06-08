// Copyright (c) 2012-2013 The PPCoin developers                -*- c++ -*-
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_STAKING_H__DUZY
#define BITCOIN_STAKING_H__DUZY

#include "uint256.h"
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

namespace boost { class thread_group; }

struct StakeKernel //!<DuzyDoc>TODO: private
{
    //!<DuzyDoc>TODO: fields
    
protected:
    StakeKernel();
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
    uint64_t nStakeSplitThreshold;
    unsigned int nStakeMinAge;
    unsigned int nHashInterval;

    CAmount nReserveBalance;
    
    std::map<COutPoint, unsigned int> mapStakes;
    std::map<unsigned int, unsigned int> mapHashedBlocks;
    std::map<uint256, uint256> mapProofOfStake;
    std::map<uint256, int64_t> mapRejectedBlocks;

private:

    bool SelectStakeCoins(CWallet *wallet, std::set<std::pair<const CWalletTx*, unsigned int> >& stakecoins, const int64_t targetAmount);
    bool CreateCoinStake(CWallet *wallet, const CKeyStore& keystore, unsigned int nBits, int64_t nSearchInterval, CMutableTransaction& txNew, unsigned int& nTxNewTime);

    bool GenBlockStake(CWallet *wallet, const CReserveKey &key, unsigned int &extra);
    void StakingThread(CWallet *wallet);

public:

    //!<DuzyDoc>: Stake::Pointer() - returns the staking pointer
    static Stake *Pointer();
    
    static uint64_t GetStakeCombineThreshold()
    {
        return 100;
    }

    //!<DuzyDoc>: Stake::ComputeNextModifier - compute the hash modifier for proof-of-stake
    bool ComputeNextModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier);

    //!<DuzyDoc>: Stake::CheckHash - check whether stake kernel meets hash target
    //!<DuzyDoc>:       Sets hashProofOfStake on success return
    bool CheckHash(const CBlockIndex* pindexPrev, unsigned int nBits, const CBlock &blockFrom, const CTransaction &txPrev, const COutPoint &prevout, unsigned int& nTimeTx, uint256& hashProofOfStake);

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

    uint64_t GetSplitThreshold() const;
    void SetSplitThreshold(uint64_t v);

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

#endif // BITCOIN_STAKING_H__DUZY
