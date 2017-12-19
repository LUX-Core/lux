// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include "leveldbwrapper.h"
#include "main.h"
#include "primitives/zerocoin.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

class CCoins;
class uint256;

//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 100;
//! max. -dbcache in (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 4096 : 1024;
//! min. -dbcache in (MiB)
static const int64_t nMinDbCache = 4;

/** CCoinsView backed by the LevelDB coin database (chainstate/) */
class CCoinsViewDB : public CCoinsView
{
protected:
    CLevelDBWrapper db;

public:
    CCoinsViewDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool GetCoins(const uint256& txid, CCoins& coins) const;
    bool HaveCoins(const uint256& txid) const;
    uint256 GetBestBlock() const;
    bool BatchWrite(CCoinsMap& mapCoins, const uint256& hashBlock);
    bool GetStats(CCoinsStats& stats) const;
};

/** Access to the block database (blocks/index/) */
class CBlockTreeDB : public CLevelDBWrapper
{
public:
    CBlockTreeDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CBlockTreeDB(const CBlockTreeDB&);
    void operator=(const CBlockTreeDB&);

public:
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo& fileinfo);
    bool WriteBlockFileInfo(int nFile, const CBlockFileInfo& fileinfo);
    bool ReadLastBlockFile(int& nFile);
    bool WriteLastBlockFile(int nFile);
    bool WriteReindexing(bool fReindex);
    bool ReadReindexing(bool& fReindex);
    bool ReadTxIndex(const uint256& txid, CDiskTxPos& pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >& list);
    bool WriteFlag(const std::string& name, bool fValue);
    bool ReadFlag(const std::string& name, bool& fValue);
    bool WriteInt(const std::string& name, int nValue);
    bool ReadInt(const std::string& name, int& nValue);
    bool LoadBlockIndexGuts();
};

class CZerocoinDB : public CLevelDBWrapper
{
public:
    CZerocoinDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CZerocoinDB(const CZerocoinDB&);
    void operator=(const CZerocoinDB&);

public:
    bool WriteCoinMint(const libzerocoin::PublicCoin& pubCoin, const uint256& txHash);
    bool ReadCoinMint(const CBigNum& bnPubcoin, uint256& txHash);
    bool WriteCoinSpend(const CBigNum& bnSerial, const uint256& txHash);
    bool ReadCoinSpend(const CBigNum& bnSerial, uint256& txHash);
    bool EraseCoinMint(const CBigNum& bnPubcoin);
    bool EraseCoinSpend(const CBigNum& bnSerial);
    bool WriteAccumulatorValue(const uint32_t& nChecksum, const CBigNum& bnValue);
    bool ReadAccumulatorValue(const uint32_t& nChecksum, CBigNum& bnValue);
    bool EraseAccumulatorValue(const uint32_t& nChecksum);
};

#endif // BITCOIN_TXDB_H
