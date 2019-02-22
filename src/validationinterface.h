// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATIONINTERFACE_H
#define BITCOIN_VALIDATIONINTERFACE_H

#include <boost/signals2/signal.hpp>
#include <boost/shared_ptr.hpp>

class CBlock;

struct CBlockLocator;

class CBlockIndex;

class CReserveScript;

class CTransaction;

class CValidationInterface;

class CValidationState;

class uint256;

// These functions dispatch to one or all registered wallets

/** Register a wallet to receive updates from core */
void RegisterValidationInterface(CValidationInterface *pwalletIn);

/** Unregister a wallet from core */
void UnregisterValidationInterface(CValidationInterface *pwalletIn);

/** Unregister all wallets from core */
void UnregisterAllValidationInterfaces();

/** Push an updated transaction to all registered wallets */
void SyncWithWallets(const CTransaction &tx, const CBlock *pblock);

class CValidationInterface {
protected:
    virtual void UpdatedBlockTip(const CBlockIndex *pindex) {}

    virtual void SyncTransaction(const CTransaction &tx, const CBlock *pblock) {}

    virtual void NotifyTransactionLock(const CTransaction &tx) {}

    virtual void SetBestChain(const CBlockLocator &locator) {}

    virtual bool UpdatedTransaction(const uint256 &hash) { return false; }

// XX42    virtual void ResendWalletTransactions(int64_t nBestBlockTime) {}
    virtual void ResendWalletTransactions() {}

    virtual void BlockChecked(const CBlock &, const CValidationState &) {}

    virtual void GetScriptForMining(boost::shared_ptr <CReserveScript> &) {};

    virtual void ResetRequestCount(const uint256 &hash) {};

    friend void::RegisterValidationInterface(CValidationInterface *);

    friend void::UnregisterValidationInterface(CValidationInterface *);

    friend void::UnregisterAllValidationInterfaces();
};

struct CMainSignals {
    /** Notifies listeners of updated block chain tip */
    boost::signals2::signal<void(const CBlockIndex *)> UpdatedBlockTip;
    /** Notifies listeners of updated transaction data (transaction, and optionally the block it is found in. */
    boost::signals2::signal<void(const CTransaction &, const CBlock *)> SyncTransaction;
    /** Notifies listeners of an updated transaction lock without new data. */
    boost::signals2::signal<void(const CTransaction &)> NotifyTransactionLock;
    /** Notifies listeners of an updated transaction without new data (for now: a coinbase potentially becoming visible). */
    boost::signals2::signal<bool(const uint256 &)> UpdatedTransaction;
    /** Notifies listeners of a new active block chain. */
    boost::signals2::signal<void(const CBlockLocator &)> SetBestChain;
    /** Tells listeners to broadcast their data. */
    boost::signals2::signal<void(int64_t nBestBlockTime)> Broadcast;
    /** Notifies listeners of a block validation result */
    boost::signals2::signal<void(const CBlock &, const CValidationState &)> BlockChecked;
    /** Notifies listeners that a key for mining is required (coinbase) */
    boost::signals2::signal<void(boost::shared_ptr < CReserveScript > &)> ScriptForMining;
    /** Notifies listeners that a block has been successfully mined */
    boost::signals2::signal<void(const uint256 &)> BlockFound;
};

CMainSignals &GetMainSignals();

#endif // BITCOIN_VALIDATIONINTERFACE_H
