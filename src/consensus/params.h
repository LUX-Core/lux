// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2017-2018 The Luxcore developers

#ifndef BITCOIN_CONSENSUS_PARAMS_H
#define BITCOIN_CONSENSUS_PARAMS_H

#include "uint256.h"

namespace Consensus {
    /**
     * Parameters that influence chain consensus.
     */
    struct Params {
        uint256 hashGenesisBlock;
        int nSubsidyHalvingInterval;
        /** Used to check majorities for block version upgrade */
        int nEnforceBlockUpgradeMajority;
        int nRejectBlockOutdatedMajority;
        int nToCheckBlockUpgradeMajority;
        /** Proof of work parameters */
        int nLastPOWBlock;
        int64_t nTargetSpacing;
        int64_t nTargetTimespan;
        /** Proof of stake parameters */
        int64_t nStakingRoundPeriod;
        int64_t nStakingInterval;
        int64_t nStakingMinAge;
    };
} // namespace Consensus

#endif // BITCOIN_CONSENSUS_PARAMS_H
