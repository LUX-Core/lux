// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ADDRESSINDEX_H
#define BITCOIN_ADDRESSINDEX_H

#include "uint256.h"
#include "amount.h"
#include "spentindex.h"

struct CMempoolAddressDelta
{
    int64_t time;
    CAmount amount;
    uint256 prevhash;
    uint32_t prevout;

    CMempoolAddressDelta(int64_t t, CAmount a, uint256 hash, unsigned int out) {
        time = t;
        amount = a;
        prevhash = hash;
        prevout = (uint32_t) out;
    }

    CMempoolAddressDelta(int64_t t, CAmount a) {
        time = t;
        amount = a;
        prevhash.SetNull();
        prevout = 0;
    }
};

struct CMempoolAddressDeltaKey
{
    uint160 hashBytes;
    uint16_t hashType;
    uint16_t spendFlags;
    uint256 txHash;
    uint32_t indexInOut;

    CMempoolAddressDeltaKey(uint16_t addrType, uint160 addrHash, uint256 txh, unsigned int i, uint16_t sf) {
        hashType = addrType;
        hashBytes = addrHash;
        txHash = txh;
        indexInOut = (uint32_t) i;
        spendFlags = sf;
    }

    CMempoolAddressDeltaKey(uint16_t addressType, uint160 addressHash) {
        hashType = addressType;
        hashBytes = addressHash;
        txHash.SetNull();
        indexInOut = 0;
        spendFlags = 0;
    }
};

struct CMempoolAddressDeltaKeyCompare
{
    bool operator()(const CMempoolAddressDeltaKey& a, const CMempoolAddressDeltaKey& b) const {

        if (a.hashType != b.hashType)
            return a.hashType < b.hashType;
        if (a.hashBytes != b.hashBytes)
            return a.hashBytes < b.hashBytes;
        if (a.txHash != b.txHash)
            return a.txHash < b.txHash;
        if (a.spendFlags != b.spendFlags)
            // keep inputs before outputs
            return a.spendFlags > b.spendFlags;
        else
            return a.indexInOut < b.indexInOut;
    }
};

// Type helpers to avoid code lines of 200 chars
typedef std::vector<std::pair<uint160, uint16_t> > AddressTypeVector;
typedef std::vector<std::pair<CAddressIndexKey, CAmount> > AddressIndexVector;
typedef std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > AddressUnspentVector;

typedef std::vector<std::pair<CMempoolAddressDeltaKey, CMempoolAddressDelta> > MempoolAddrDeltaVector;

#endif // BITCOIN_ADDRESSINDEX_H
