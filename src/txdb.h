// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TXDB_H
#define BITCOIN_TXDB_H

#include "leveldbwrapper.h"
#include "main.h"
#include "addressindex.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

class CBlockFileInfo;
class CBlockIndex;
struct CDiskTxPos;
class CCoins;
class uint256;

//! -dbcache default (MiB)
static const int64_t nDefaultDbCache = 450;
//! max. -dbcache in (MiB)
static const int64_t nMaxDbCache = sizeof(void*) > 4 ? 16384 : 1024;
//! min. -dbcache in (MiB)
static const int64_t nMinDbCache = 4;
//! Max memory allocated to block tree DB specific cache, if no -txindex (MiB)
static const int64_t nMaxBlockDBCache = 2;
//! Max memory allocated to block tree DB specific cache, if -txindex (MiB)
static const int64_t nMaxBlockDBAndTxIndexCache = 1024;
//! Max memory allocated to coin DB specific cache (MiB)
static const int64_t nMaxCoinsDBCache = 8;

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

    //! Attempt to update from an older database format. Returns whether an error occurred.
    bool Upgrade();
    size_t EstimateSize() const;
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
    bool WriteBatchSync(const std::vector<std::pair<int, const CBlockFileInfo*> >& fileInfo, int nLastFile, const std::vector<const CBlockIndex*>& blockinfo);
    bool WriteBlockIndex(const CDiskBlockIndex& blockindex);
    bool ReadBlockFileInfo(int nFile, CBlockFileInfo& fileinfo);
    bool WriteBlockFileInfo(int nFile, const CBlockFileInfo& fileinfo);
    bool ReadLastBlockFile(int& nFile);
    bool WriteLastBlockFile(int nFile);
    bool WriteReindexing(bool fReindex);
    bool ReadReindexing(bool& fReindex);
    bool EraseTxIndex(const uint256& txid);
    bool ReadTxIndex(const uint256& txid, CDiskTxPos& pos);
    bool WriteTxIndex(const std::vector<std::pair<uint256, CDiskTxPos> >& list);
    bool ReadSpentIndex(CSpentIndexKey &key, CSpentIndexValue &value);
    bool UpdateSpentIndex(const std::vector<std::pair<CSpentIndexKey, CSpentIndexValue> >&vect);
    bool UpdateAddressUnspentIndex(const AddressUnspentVector &vect);
    bool ReadAddressUnspentIndex(uint160 addrHash, uint16_t addrType, AddressUnspentVector &vect);
    bool EraseAddressUnspentIndex(const AddressUnspentVector &vect);
    bool FindTxEntriesInUnspentIndex(uint256 txid, AddressUnspentVector &addressUnspent);
    bool WriteAddressIndex(const AddressIndexVector &vect);
    bool EraseAddressIndex(const AddressIndexVector &vect);
    bool FindTxEntriesInAddressIndex(uint256 txid, AddressIndexVector &addressIndex);
    bool ReadAddressIndex(uint160 addrHash, uint16_t addrType, AddressIndexVector &addressIndex, int start = 0, int end = 0);
    bool WriteFlag(const std::string& name, bool fValue);
    bool ReadFlag(const std::string& name, bool& fValue);
    bool LoadBlockIndexGuts();

    ////////////////////////////////////////////////////////////////////////////// // lux
    bool WriteHeightIndex(const CHeightTxIndexKey &heightIndex, const std::vector<uint256>& hash);

    /**
     * Iterates through blocks by height, starting from low.
     *
     * @param low start iterating from this block height
     * @param high end iterating at this block height (ignored if <= 0)
     * @param minconf stop iterating of the block height does not have enough confirmations (ignored if <= 0)
     * @param blocksOfHashes transaction hashes in blocks iterated are collected into this vector.
     * @param addresses filter out a block unless it matches one of the addresses in this set.
     *
     * @return the height of the latest block iterated. 0 if no block is iterated.
     */
    int ReadHeightIndex(int low, int high, int minconf,
                        std::vector<std::vector<uint256>> &blocksOfHashes,
                        std::set<dev::h160> const &addresses);
    bool EraseHeightIndex(const unsigned int &height);
    bool WipeHeightIndex();

    //////////////////////////////////////////////////////////////////////////////
};

#endif // BITCOIN_TXDB_H
