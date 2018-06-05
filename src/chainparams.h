// Copyright (c) 2009-2010 Satoshi Nakamoto                     -*- c++ -*-
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CHAINPARAMS_H
#define BITCOIN_CHAINPARAMS_H

#include "chainparamsbase.h"
#include "checkpoints.h"
#include "primitives/block.h"
#include "protocol.h"
#include "uint256.h"
#include "consensus/params.h"

#include <vector>

typedef unsigned char MessageStartChars[MESSAGE_START_SIZE];

struct CDNSSeedData {
    std::string name, host;
    bool supportsServiceBitsFiltering;
    CDNSSeedData(const std::string& strName, const std::string& strHost, bool supportsServiceBitsFilteringIn = false) : name(strName), host(strHost), supportsServiceBitsFiltering(supportsServiceBitsFilteringIn) {}
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * LUX system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams
{
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        SECRET_KEY,     // BIP16
        EXT_PUBLIC_KEY, // BIP32
        EXT_SECRET_KEY, // BIP32
        EXT_COIN_TYPE,  // BIP44

        MAX_BASE58_TYPES
    };

    const uint256& HashGenesisBlock() const { return consensus.hashGenesisBlock; }
    const Consensus::Params& GetConsensus() const { return consensus; }
    const MessageStartChars& MessageStart() const { return pchMessageStart; }
    const std::vector<unsigned char>& AlertKey() const { return vAlertPubKey; }
    int GetDefaultPort() const { return nDefaultPort; }
    const uint256& ProofOfWorkLimit() const { return consensus.powLimit; }
    /** Used to check majorities for block version upgrade */
    int EnforceBlockUpgradeMajority() const { return consensus.nMajorityEnforceBlockUpgrade; }
    int RejectBlockOutdatedMajority() const { return consensus.nMajorityRejectBlockOutdated; }
    int MajorityWindow() const { return consensus.nMajorityWindow; }
    int MaxReorganizationDepth() const { return nMaxReorganizationDepth; }

    /** Used if GenerateBitcoins is called with a negative number of threads */
    int DefaultMinerThreads() const { return nMinerThreads; }
    const CBlock& GenesisBlock() const { return genesis; }
    bool RequireRPCPassword() const { return fRequireRPCPassword; }
    /** Make miner wait to have peers to avoid wasting work */
    bool MiningRequiresPeers() const { return fMiningRequiresPeers; }
    /** Headers first syncing is disabled */
    bool HeadersFirstSyncingActive() const { return fHeadersFirstSyncingActive; };
    /** Default value for -checkmempool and -checkblockindex argument */
    bool DefaultConsistencyChecks() const { return fDefaultConsistencyChecks; }
    /** Allow mining of a min-difficulty block */
    bool AllowMinDifficultyBlocks() const { return consensus.fPowAllowMinDifficultyBlocks; }
    /** Skip proof-of-work check: allow mining of any difficulty block */
    bool SkipProofOfWorkCheck() const { return fSkipProofOfWorkCheck; }
    /** Make standard checks */
    bool RequireStandard() const { return fRequireStandard; }
    int64_t TargetTimespan() const { return consensus.nPowTargetTimespan; }
    int64_t TargetSpacing() const { return consensus.nPowTargetSpacing; }
    int64_t Interval() const { return consensus.nPowTargetTimespan / consensus.nPowTargetSpacing; }
    int LAST_POW_BLOCK() const { return consensus.nLastPOWBlock; }
    int COINBASE_MATURITY() const { return nMaturity; }
    int ModifierUpgradeBlock() const { return nModifierUpdateBlock; }
    /** The masternode count that we will allow the see-saw reward payments to be off by */
    int MasternodeCountDrift() const { return nMasternodeCountDrift; }
    /** Make miner stop after a block is found. In RPC, don't return until nGenProcLimit blocks are generated */
    bool MineBlocksOnDemand() const { return fMineBlocksOnDemand; }
    /** In the future use NetworkIDString() for RPC fields */
    bool TestnetToBeDeprecatedFieldRPC() const { return fTestnetToBeDeprecatedFieldRPC; }
    /** Return the BIP70 network string (main, test or regtest) */
    std::string NetworkIDString() const { return strNetworkID; }
    const std::vector<CDNSSeedData>& DNSSeeds() const { return vSeeds; }
    const std::vector<unsigned char>& Base58Prefix(Base58Type type) const { return base58Prefixes[type]; }
    const std::string& Bech32HRP() const { return bech32_hrp; }
    const std::vector<CAddress>& FixedSeeds() const { return vFixedSeeds; }
    virtual const Checkpoints::CCheckpointData& Checkpoints() const = 0;
    int PoolMaxTransactions() const { return nPoolMaxTransactions; }
    std::string SporkKey() const { return strSporkKey; }
    std::string DarksendPoolDummyAddress() const { return strDarksendPoolDummyAddress; }
    int64_t StartMasternodePayments() const { return nStartMasternodePayments; }
    int64_t StakingRoundPeriod() const { return nStakingRoundPeriod; }
    int64_t StakingInterval() const { return nStakingInterval; }
    int64_t StakingMinAge() const { return nStakingMinAge; }
    CBaseChainParams::Network NetworkID() const { return networkID; }
    int FirstSCBlock() const { return nFirstSCBlock; }
    int SwitchPhi2Block() const { return nSwitchPhi2Block; }
    int64_t PruneAfterHeight() const { return nPruneAfterHeight; }
    int FirstSplitRewardBlock() const { return nSplitRewardBlock; }

protected:
    CChainParams() {}

    Consensus::Params consensus;
    MessageStartChars pchMessageStart;
    //! Raw pub key bytes for the broadcast alert signing key.
    std::vector<unsigned char> vAlertPubKey;
    int nDefaultPort;
    int nMaxReorganizationDepth;
    int nMasternodeCountDrift;
    int nMaturity;
    int nModifierUpdateBlock;
    int nMinerThreads;
    std::vector<CDNSSeedData> vSeeds;
    std::vector<unsigned char> base58Prefixes[MAX_BASE58_TYPES];
    std::string bech32_hrp;
    CBaseChainParams::Network networkID;
    std::string strNetworkID;
    CBlock genesis;
    std::vector<CAddress> vFixedSeeds;
    bool fRequireRPCPassword;
    bool fMiningRequiresPeers;

    bool fDefaultConsistencyChecks;
    bool fRequireStandard;
    bool fMineBlocksOnDemand;
    bool fSkipProofOfWorkCheck;
    bool fTestnetToBeDeprecatedFieldRPC;
    bool fHeadersFirstSyncingActive;
    int nPoolMaxTransactions;
    std::string strSporkKey;
    std::string strDarksendPoolDummyAddress;
    int64_t nStartMasternodePayments;
    int64_t nStakingRoundPeriod;
    int64_t nStakingInterval;
    int64_t nStakingMinAge;
    int nFirstSCBlock;
    int nSwitchPhi2Block;
    int nSplitRewardBlock;
    uint64_t nPruneAfterHeight;
};

/**
 * Creates and returns a CChainParams* of the chosen chain.
 * @returns a CChainParams* of the chosen chain.
 * @throws a std::runtime_error if the chain is not supported.
 */
CChainParams* CreateChainParams(CBaseChainParams::Network network);

/**
 * Modifiable parameters interface is used by test cases to adapt the parameters in order
 * to test specific features more easily. Test cases should always restore the previous
 * values after finalization.
 */

class CModifiableParams
{
public:
    //! Published setters to allow changing values in unit test cases
    virtual void setEnforceBlockUpgradeMajority(int anEnforceBlockUpgradeMajority) = 0;
    virtual void setRejectBlockOutdatedMajority(int anRejectBlockOutdatedMajority) = 0;
    virtual void setToCheckBlockUpgradeMajority(int anToCheckBlockUpgradeMajority) = 0;
    virtual void setDefaultConsistencyChecks(bool aDefaultConsistencyChecks) = 0;
    virtual void setAllowMinDifficultyBlocks(bool aAllowMinDifficultyBlocks) = 0;
    virtual void setSkipProofOfWorkCheck(bool aSkipProofOfWorkCheck) = 0;
};


/**
 * Return the currently selected parameters. This won't change after app startup
 * outside of the unit tests.
 */
const CChainParams& Params();

/** Return parameters for the given network. */
CChainParams& Params(CBaseChainParams::Network network);

/** Get modifiable network parameters (UNITTEST only) */
CModifiableParams* ModifiableParams();

/** Sets the params returned by Params() to those for the given network. */
void SelectParams(CBaseChainParams::Network network);

/**
 * Looks for -regtest or -testnet and then calls SelectParams as appropriate.
 * Returns false if an invalid combination is given.
 */
bool SelectParamsFromCommandLine();

// Note: it's deliberate that this returns "false" for regression test mode.
inline bool IsTestNet() { return Params().NetworkID() == CBaseChainParams::TESTNET; }

#endif // BITCOIN_CHAINPARAMS_H
