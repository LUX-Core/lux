// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2017-2018 The Luxcore developers

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"

namespace Consensus {

    enum DeploymentPos
    {
        DEPLOYMENT_TESTDUMMY,
        DEPLOYMENT_CSV, // Deployment of BIP68, BIP112, and BIP113.
        DEPLOYMENT_SEGWIT, // Deployment of BIP141 and BIP143
        // NOTE: Also add new deployments to VersionBitsDeploymentInfo in versionbits.cpp
        SMART_CONTRACTS_HARDFORK,
        MAX_VERSION_BITS_DEPLOYMENTS
    };

    /**
     * Struct for each individual consensus rule change using BIP9.
     */
    struct BIP9Deployment {
        /** Bit position to select the particular bit in nVersion. */
        int bit;
        /** Start MedianTime for version bits miner confirmation. Can be a date in the past */
        int64_t nStartTime;
        /** Timeout/expiry MedianTime for the deployment attempt. */
        int64_t nTimeout;
    };

    /**
     * Parameters that influence chain consensus.
     */
    struct Params {
        bool fPowAllowMinDifficultyBlocks;
        bool fPowNoRetargeting;
        BIP9Deployment vDeployments[MAX_VERSION_BITS_DEPLOYMENTS];
        /** Used to check majorities for block version upgrade */
        int nMajorityEnforceBlockUpgrade;
        int nMajorityRejectBlockOutdated;
        int nMajorityWindow;
        /** Proof of work parameters */
        int nLastPOWBlock;
        /**
         * Minimum blocks including miner confirmation of the total of 2016 blocks in a retargetting period,
         * (nPowTargetTimespan / nPowTargetSpacing) which is also used for BIP9 deployments.
         * Examples: 1916 for 95%, 1512 for testchains.
         */
        uint32_t nRuleChangeActivationThreshold;
        uint32_t nMinerConfirmationWindow;
        int64_t nPowTargetSpacing;
        int64_t nPowTargetTimespan;
        uint256 powLimit;
        uint256 hashGenesisBlock;
    };
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
