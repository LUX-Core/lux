// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"

#include "addrman.h"
#include "alert.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "checkqueue.h"
#include "init.h"
#include "kernel.h"
#include "masternode.h"
//#include "masternode-budget.h"
//#include "masternode-payments.h"
//#include "masternodeman.h"
#include "merkleblock.h"
#include "net.h"
//#include "obfuscation.h"
#include "pow.h"
#include "spork.h"
#include "instantx.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "util.h"
#include "utilmoneystr.h"

#include <sstream>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

using namespace boost;
using namespace std;

#if defined(NDEBUG)
#error "LUX cannot be compiled without assertions."
#endif

/**
 * Global state
 */

CCriticalSection cs_main;

BlockMap mapBlockIndex;
map<uint256, uint256> mapProofOfStake;
set<pair<COutPoint, unsigned int> > setStakeSeen;
map<unsigned int, unsigned int> mapHashedBlocks;
CChain chainActive;
CBlockIndex* pindexBestHeader = NULL;
int64_t nTimeBestReceived = 0;
CWaitableCriticalSection csBestBlock;
CConditionVariable cvBlockChange;
int nScriptCheckThreads = 0;
bool fImporting = false;
bool fReindex = false;
bool fTxIndex = true;
bool fIsBareMultisigStd = true;
bool fCheckBlockIndex = false;
unsigned int nCoinCacheSize = 5000;
bool fAlerts = DEFAULT_ALERTS;

unsigned int nStakeMinAge = 36 * 60 * 60;
int64_t nReserveBalance = 0;

uint256 bnProofOfStakeLimit = (~uint256(0) >> 20);
uint256 bnProofOfStakeLimitV2 = (~uint256(0) >> 34);

/** Fees smaller than this (in duffs) are considered zero fee (for relaying and mining)
 * We are ~100 times smaller then bitcoin now (2015-06-23), set minRelayTxFee only 10 times higher
 * so it's still 10 times lower comparing to bitcoin.
 */
CFeeRate minRelayTxFee = CFeeRate(10000);

CTxMemPool mempool(::minRelayTxFee);

struct COrphanTx {
    CTransaction tx;
    NodeId fromPeer;
};
map<uint256, COrphanTx> mapOrphanTransactions;
map<uint256, set<uint256> > mapOrphanTransactionsByPrev;
map<uint256, int64_t> mapRejectedBlocks;


void EraseOrphansFor(NodeId peer);

static void CheckBlockIndex();

/** Constant stuff for coinbase transactions we create: */
CScript COINBASE_FLAGS;

const string strMessageMagic = "Lux Signed Message:\n";

// Internal stuff
namespace
{
struct CBlockIndexWorkComparator {
    bool operator()(CBlockIndex* pa, CBlockIndex* pb) const
    {
        // First sort by most total work, ...
        if (pa->nChainWork > pb->nChainWork) return false;
        if (pa->nChainWork < pb->nChainWork) return true;

        // ... then by earliest time received, ...
        if (pa->nSequenceId < pb->nSequenceId) return false;
        if (pa->nSequenceId > pb->nSequenceId) return true;

        // Use pointer address as tie breaker (should only happen with blocks
        // loaded from disk, as those all have id 0).
        if (pa < pb) return false;
        if (pa > pb) return true;

        // Identical blocks.
        return false;
    }
};

CBlockIndex* pindexBestInvalid;

/**
     * The set of all CBlockIndex entries with BLOCK_VALID_TRANSACTIONS (for itself and all ancestors) and
     * as good as our current tip or better. Entries may be failed, though.
     */
set<CBlockIndex*, CBlockIndexWorkComparator> setBlockIndexCandidates;
/** Number of nodes with fSyncStarted. */
int nSyncStarted = 0;
/** All pairs A->B, where A (or one if its ancestors) misses transactions, but B has transactions. */
multimap<CBlockIndex*, CBlockIndex*> mapBlocksUnlinked;

CCriticalSection cs_LastBlockFile;
std::vector<CBlockFileInfo> vinfoBlockFile;
int nLastBlockFile = 0;

/**
     * Every received block is assigned a unique and increasing identifier, so we
     * know which one to give priority in case of a fork.
     */
CCriticalSection cs_nBlockSequenceId;
/** Blocks loaded from disk are assigned id 0, so start the counter at 1. */
uint32_t nBlockSequenceId = 1;

/**
     * Sources of received blocks, to be able to send them reject messages or ban
     * them, if processing happens afterwards. Protected by cs_main.
     */
map<uint256, NodeId> mapBlockSource;

/** Blocks that are in flight, and that are in the queue to be downloaded. Protected by cs_main. */
struct QueuedBlock {
    uint256 hash;
    CBlockIndex* pindex;        //! Optional.
    int64_t nTime;              //! Time of "getdata" request in microseconds.
    int nValidatedQueuedBefore; //! Number of blocks queued with validated headers (globally) at the time this one is requested.
    bool fValidatedHeaders;     //! Whether this block has validated headers at the time of request.
};
map<uint256, pair<NodeId, list<QueuedBlock>::iterator> > mapBlocksInFlight;

/** Number of blocks in flight with validated headers. */
int nQueuedValidatedHeaders = 0;

/** Number of preferable block download peers. */
int nPreferredDownload = 0;

/** Dirty block index entries. */
set<CBlockIndex*> setDirtyBlockIndex;

/** Dirty block file entries. */
set<int> setDirtyFileInfo;
} // anon namespace

//////////////////////////////////////////////////////////////////////////////
//
// dispatching functions
//

// These functions dispatch to one or all registered wallets

namespace
{
struct CMainSignals {
    /** Notifies listeners of updated transaction data (transaction, and optionally the block it is found in. */
    boost::signals2::signal<void(const CTransaction&, const CBlock*)> SyncTransaction;
    /** Notifies listeners of an erased transaction (currently disabled, requires transaction replacement). */
    boost::signals2::signal<void(const uint256&)> EraseTransaction;
    /** Notifies listeners of an updated transaction without new data (for now: a coinbase potentially becoming visible). */
    boost::signals2::signal<void(const uint256&)> UpdatedTransaction;
    /** Notifies listeners of a new active block chain. */
    boost::signals2::signal<void(const CBlockLocator&)> SetBestChain;
    /** Notifies listeners about an inventory item being seen on the network. */
    boost::signals2::signal<void(const uint256&)> Inventory;
    /** Tells listeners to broadcast their data. */
    boost::signals2::signal<void()> Broadcast;
    /** Notifies listeners of a block validation result */
    boost::signals2::signal<void(const CBlock&, const CValidationState&)> BlockChecked;
} g_signals;

} // anon namespace

void RegisterValidationInterface(CValidationInterface* pwalletIn)
{
    g_signals.SyncTransaction.connect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, _1, _2));
    g_signals.EraseTransaction.connect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, _1));
    g_signals.UpdatedTransaction.connect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, _1));
    g_signals.SetBestChain.connect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.Inventory.connect(boost::bind(&CValidationInterface::Inventory, pwalletIn, _1));
    g_signals.Broadcast.connect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn));
    g_signals.BlockChecked.connect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
}

void UnregisterValidationInterface(CValidationInterface* pwalletIn)
{
    g_signals.BlockChecked.disconnect(boost::bind(&CValidationInterface::BlockChecked, pwalletIn, _1, _2));
    g_signals.Broadcast.disconnect(boost::bind(&CValidationInterface::ResendWalletTransactions, pwalletIn));
    g_signals.Inventory.disconnect(boost::bind(&CValidationInterface::Inventory, pwalletIn, _1));
    g_signals.SetBestChain.disconnect(boost::bind(&CValidationInterface::SetBestChain, pwalletIn, _1));
    g_signals.UpdatedTransaction.disconnect(boost::bind(&CValidationInterface::UpdatedTransaction, pwalletIn, _1));
    g_signals.EraseTransaction.disconnect(boost::bind(&CValidationInterface::EraseFromWallet, pwalletIn, _1));
    g_signals.SyncTransaction.disconnect(boost::bind(&CValidationInterface::SyncTransaction, pwalletIn, _1, _2));
}

void UnregisterAllValidationInterfaces()
{
    g_signals.BlockChecked.disconnect_all_slots();
    g_signals.Broadcast.disconnect_all_slots();
    g_signals.Inventory.disconnect_all_slots();
    g_signals.SetBestChain.disconnect_all_slots();
    g_signals.UpdatedTransaction.disconnect_all_slots();
    g_signals.EraseTransaction.disconnect_all_slots();
    g_signals.SyncTransaction.disconnect_all_slots();
}

void SyncWithWallets(const CTransaction& tx, const CBlock* pblock)
{
    g_signals.SyncTransaction(tx, pblock);
}

//////////////////////////////////////////////////////////////////////////////
//
// Registration of network node signals.
//

namespace
{
struct CBlockReject {
    unsigned char chRejectCode;
    string strRejectReason;
    uint256 hashBlock;
};

/**
 * Maintain validation-specific state about nodes, protected by cs_main, instead
 * by CNode's own locks. This simplifies asynchronous operation, where
 * processing of incoming data is done after the ProcessMessage call returns,
 * and we're no longer holding the node's locks.
 */
struct CNodeState {
    //! The peer's address
    CService address;
    //! Whether we have a fully established connection.
    bool fCurrentlyConnected;
    //! Accumulated misbehaviour score for this peer.
    int nMisbehavior;
    //! Whether this peer should be disconnected and banned (unless whitelisted).
    bool fShouldBan;
    //! String name of this peer (debugging/logging purposes).
    std::string name;
    //! List of asynchronously-determined block rejections to notify this peer about.
    std::vector<CBlockReject> rejects;
    //! The best known block we know this peer has announced.
    CBlockIndex* pindexBestKnownBlock;
    //! The hash of the last unknown block this peer has announced.
    uint256 hashLastUnknownBlock;
    //! The last full block we both have.
    CBlockIndex* pindexLastCommonBlock;
    //! Whether we've started headers synchronization with this peer.
    bool fSyncStarted;
    //! Since when we're stalling block download progress (in microseconds), or 0.
    int64_t nStallingSince;
    list<QueuedBlock> vBlocksInFlight;
    int nBlocksInFlight;
    //! Whether we consider this a preferred download peer.
    bool fPreferredDownload;

    CNodeState()
    {
        fCurrentlyConnected = false;
        nMisbehavior = 0;
        fShouldBan = false;
        pindexBestKnownBlock = NULL;
        hashLastUnknownBlock = uint256(0);
        pindexLastCommonBlock = NULL;
        fSyncStarted = false;
        nStallingSince = 0;
        nBlocksInFlight = 0;
        fPreferredDownload = false;
    }
};

/** Map maintaining per-node state. Requires cs_main. */
map<NodeId, CNodeState> mapNodeState;

// Requires cs_main.
CNodeState* State(NodeId pnode)
{
    map<NodeId, CNodeState>::iterator it = mapNodeState.find(pnode);
    if (it == mapNodeState.end())
        return NULL;
    return &it->second;
}

int GetHeight()
{
    while (true) {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            MilliSleep(50);
            continue;
        }
        return chainActive.Height();
    }
}

void UpdatePreferredDownload(CNode* node, CNodeState* state)
{
    nPreferredDownload -= state->fPreferredDownload;

    // Whether this node should be marked as a preferred download node.
    state->fPreferredDownload = (!node->fInbound || node->fWhitelisted) && !node->fOneShot && !node->fClient;

    nPreferredDownload += state->fPreferredDownload;
}

void InitializeNode(NodeId nodeid, const CNode* pnode)
{
    LOCK(cs_main);
    CNodeState& state = mapNodeState.insert(std::make_pair(nodeid, CNodeState())).first->second;
    state.name = pnode->addrName;
    state.address = pnode->addr;
}

void FinalizeNode(NodeId nodeid)
{
    LOCK(cs_main);
    CNodeState* state = State(nodeid);

    if (state->fSyncStarted)
        nSyncStarted--;

    if (state->nMisbehavior == 0 && state->fCurrentlyConnected) {
        AddressCurrentlyConnected(state->address);
    }

    BOOST_FOREACH (const QueuedBlock& entry, state->vBlocksInFlight)
        mapBlocksInFlight.erase(entry.hash);
    EraseOrphansFor(nodeid);
    nPreferredDownload -= state->fPreferredDownload;

    mapNodeState.erase(nodeid);
}

// Requires cs_main.
void MarkBlockAsReceived(const uint256& hash)
{
    map<uint256, pair<NodeId, list<QueuedBlock>::iterator> >::iterator itInFlight = mapBlocksInFlight.find(hash);
    if (itInFlight != mapBlocksInFlight.end()) {
        CNodeState* state = State(itInFlight->second.first);
        nQueuedValidatedHeaders -= itInFlight->second.second->fValidatedHeaders;
        state->vBlocksInFlight.erase(itInFlight->second.second);
        state->nBlocksInFlight--;
        state->nStallingSince = 0;
        mapBlocksInFlight.erase(itInFlight);
    }
}

// Requires cs_main.
void MarkBlockAsInFlight(NodeId nodeid, const uint256& hash, CBlockIndex* pindex = NULL)
{
    CNodeState* state = State(nodeid);
    assert(state != NULL);

    // Make sure it's not listed somewhere already.
    MarkBlockAsReceived(hash);

    QueuedBlock newentry = {hash, pindex, GetTimeMicros(), nQueuedValidatedHeaders, pindex != NULL};
    nQueuedValidatedHeaders += newentry.fValidatedHeaders;
    list<QueuedBlock>::iterator it = state->vBlocksInFlight.insert(state->vBlocksInFlight.end(), newentry);
    state->nBlocksInFlight++;
    mapBlocksInFlight[hash] = std::make_pair(nodeid, it);
}

/** Check whether the last unknown block a peer advertized is not yet known. */
void ProcessBlockAvailability(NodeId nodeid)
{
    CNodeState* state = State(nodeid);
    assert(state != NULL);

    if (state->hashLastUnknownBlock != 0) {
        BlockMap::iterator itOld = mapBlockIndex.find(state->hashLastUnknownBlock);
        if (itOld != mapBlockIndex.end() && itOld->second->nChainWork > 0) {
            if (state->pindexBestKnownBlock == NULL || itOld->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
                state->pindexBestKnownBlock = itOld->second;
            state->hashLastUnknownBlock = uint256(0);
        }
    }
}

/** Update tracking information about which blocks a peer is assumed to have. */
void UpdateBlockAvailability(NodeId nodeid, const uint256& hash)
{
    CNodeState* state = State(nodeid);
    assert(state != NULL);

    ProcessBlockAvailability(nodeid);

    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end() && it->second->nChainWork > 0) {
        // An actually better block was announced.
        if (state->pindexBestKnownBlock == NULL || it->second->nChainWork >= state->pindexBestKnownBlock->nChainWork)
            state->pindexBestKnownBlock = it->second;
    } else {
        // An unknown block was announced; just assume that the latest one is the best one.
        state->hashLastUnknownBlock = hash;
    }
}

/** Find the last common ancestor two blocks have.
 *  Both pa and pb must be non-NULL. */
CBlockIndex* LastCommonAncestor(CBlockIndex* pa, CBlockIndex* pb)
{
    if (pa->nHeight > pb->nHeight) {
        pa = pa->GetAncestor(pb->nHeight);
    } else if (pb->nHeight > pa->nHeight) {
        pb = pb->GetAncestor(pa->nHeight);
    }

    while (pa != pb && pa && pb) {
        pa = pa->pprev;
        pb = pb->pprev;
    }

    // Eventually all chain branches meet at the genesis block.
    assert(pa == pb);
    return pa;
}

/** Update pindexLastCommonBlock and add not-in-flight missing successors to vBlocks, until it has
 *  at most count entries. */
void FindNextBlocksToDownload(NodeId nodeid, unsigned int count, std::vector<CBlockIndex*>& vBlocks, NodeId& nodeStaller)
{
    if (count == 0)
        return;

    vBlocks.reserve(vBlocks.size() + count);
    CNodeState* state = State(nodeid);
    assert(state != NULL);

    // Make sure pindexBestKnownBlock is up to date, we'll need it.
    ProcessBlockAvailability(nodeid);

    if (state->pindexBestKnownBlock == NULL || state->pindexBestKnownBlock->nChainWork < chainActive.Tip()->nChainWork) {
        // This peer has nothing interesting.
        return;
    }

    if (state->pindexLastCommonBlock == NULL) {
        // Bootstrap quickly by guessing a parent of our best tip is the forking point.
        // Guessing wrong in either direction is not a problem.
        state->pindexLastCommonBlock = chainActive[std::min(state->pindexBestKnownBlock->nHeight, chainActive.Height())];
    }

    // If the peer reorganized, our previous pindexLastCommonBlock may not be an ancestor
    // of their current tip anymore. Go back enough to fix that.
    state->pindexLastCommonBlock = LastCommonAncestor(state->pindexLastCommonBlock, state->pindexBestKnownBlock);
    if (state->pindexLastCommonBlock == state->pindexBestKnownBlock)
        return;

    std::vector<CBlockIndex*> vToFetch;
    CBlockIndex* pindexWalk = state->pindexLastCommonBlock;
    // Never fetch further than the best block we know the peer has, or more than BLOCK_DOWNLOAD_WINDOW + 1 beyond the last
    // linked block we have in common with this peer. The +1 is so we can detect stalling, namely if we would be able to
    // download that next block if the window were 1 larger.
    int nWindowEnd = state->pindexLastCommonBlock->nHeight + BLOCK_DOWNLOAD_WINDOW;
    int nMaxHeight = std::min<int>(state->pindexBestKnownBlock->nHeight, nWindowEnd + 1);
    NodeId waitingfor = -1;
    while (pindexWalk->nHeight < nMaxHeight) {
        // Read up to 128 (or more, if more blocks than that are needed) successors of pindexWalk (towards
        // pindexBestKnownBlock) into vToFetch. We fetch 128, because CBlockIndex::GetAncestor may be as expensive
        // as iterating over ~100 CBlockIndex* entries anyway.
        int nToFetch = std::min(nMaxHeight - pindexWalk->nHeight, std::max<int>(count - vBlocks.size(), 128));
        vToFetch.resize(nToFetch);
        pindexWalk = state->pindexBestKnownBlock->GetAncestor(pindexWalk->nHeight + nToFetch);
        vToFetch[nToFetch - 1] = pindexWalk;
        for (unsigned int i = nToFetch - 1; i > 0; i--) {
            vToFetch[i - 1] = vToFetch[i]->pprev;
        }

        // Iterate over those blocks in vToFetch (in forward direction), adding the ones that
        // are not yet downloaded and not in flight to vBlocks. In the mean time, update
        // pindexLastCommonBlock as long as all ancestors are already downloaded.
        BOOST_FOREACH (CBlockIndex* pindex, vToFetch) {
            if (!pindex->IsValid(BLOCK_VALID_TREE)) {
                // We consider the chain that this peer is on invalid.
                return;
            }
            if (pindex->nStatus & BLOCK_HAVE_DATA) {
                if (pindex->nChainTx)
                    state->pindexLastCommonBlock = pindex;
            } else if (mapBlocksInFlight.count(pindex->GetBlockHash()) == 0) {
                // The block is not already downloaded, and not yet in flight.
                if (pindex->nHeight > nWindowEnd) {
                    // We reached the end of the window.
                    if (vBlocks.size() == 0 && waitingfor != nodeid) {
                        // We aren't able to fetch anything, but we would be if the download window was one larger.
                        nodeStaller = waitingfor;
                    }
                    return;
                }
                vBlocks.push_back(pindex);
                if (vBlocks.size() == count) {
                    return;
                }
            } else if (waitingfor == -1) {
                // This is the first already-in-flight block.
                waitingfor = mapBlocksInFlight[pindex->GetBlockHash()].first;
            }
        }
    }
}

} // anon namespace

bool GetNodeStateStats(NodeId nodeid, CNodeStateStats& stats)
{
    LOCK(cs_main);
    CNodeState* state = State(nodeid);
    if (state == NULL)
        return false;
    stats.nMisbehavior = state->nMisbehavior;
    stats.nSyncHeight = state->pindexBestKnownBlock ? state->pindexBestKnownBlock->nHeight : -1;
    stats.nCommonHeight = state->pindexLastCommonBlock ? state->pindexLastCommonBlock->nHeight : -1;
    BOOST_FOREACH (const QueuedBlock& queue, state->vBlocksInFlight) {
        if (queue.pindex)
            stats.vHeightInFlight.push_back(queue.pindex->nHeight);
    }
    return true;
}

void RegisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.GetHeight.connect(&GetHeight);
    nodeSignals.ProcessMessages.connect(&ProcessMessages);
    nodeSignals.SendMessages.connect(&SendMessages);
    nodeSignals.InitializeNode.connect(&InitializeNode);
    nodeSignals.FinalizeNode.connect(&FinalizeNode);
}

void UnregisterNodeSignals(CNodeSignals& nodeSignals)
{
    nodeSignals.GetHeight.disconnect(&GetHeight);
    nodeSignals.ProcessMessages.disconnect(&ProcessMessages);
    nodeSignals.SendMessages.disconnect(&SendMessages);
    nodeSignals.InitializeNode.disconnect(&InitializeNode);
    nodeSignals.FinalizeNode.disconnect(&FinalizeNode);
}

CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator)
{
    // Find the first block the caller has in the main chain
    BOOST_FOREACH (const uint256& hash, locator.vHave) {
        BlockMap::iterator mi = mapBlockIndex.find(hash);
        if (mi != mapBlockIndex.end()) {
            CBlockIndex* pindex = (*mi).second;
            if (chain.Contains(pindex))
                return pindex;
        }
    }
    return chain.Genesis();
}

CCoinsViewCache* pcoinsTip = NULL;
CBlockTreeDB* pblocktree = NULL;

//////////////////////////////////////////////////////////////////////////////
//
// mapOrphanTransactions
//

bool AddOrphanTx(const CTransaction& tx, NodeId peer)
{
    uint256 hash = tx.GetHash();
    if (mapOrphanTransactions.count(hash))
        return false;

    // Ignore big transactions, to avoid a
    // send-big-orphans memory exhaustion attack. If a peer has a legitimate
    // large transaction with a missing parent then we assume
    // it will rebroadcast it later, after the parent transaction(s)
    // have been mined or received.
    // 10,000 orphans, each of which is at most 5,000 bytes big is
    // at most 500 megabytes of orphans:
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    if (sz > 5000) {
        LogPrint("mempool", "ignoring large orphan tx (size: %u, hash: %s)\n", sz, hash.ToString());
        return false;
    }

    mapOrphanTransactions[hash].tx = tx;
    mapOrphanTransactions[hash].fromPeer = peer;
    BOOST_FOREACH (const CTxIn& txin, tx.vin)
        mapOrphanTransactionsByPrev[txin.prevout.hash].insert(hash);

    LogPrint("mempool", "stored orphan tx %s (mapsz %u prevsz %u)\n", hash.ToString(),
        mapOrphanTransactions.size(), mapOrphanTransactionsByPrev.size());
    return true;
}

void static EraseOrphanTx(uint256 hash)
{
    map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.find(hash);
    if (it == mapOrphanTransactions.end())
        return;
    BOOST_FOREACH (const CTxIn& txin, it->second.tx.vin) {
        map<uint256, set<uint256> >::iterator itPrev = mapOrphanTransactionsByPrev.find(txin.prevout.hash);
        if (itPrev == mapOrphanTransactionsByPrev.end())
            continue;
        itPrev->second.erase(hash);
        if (itPrev->second.empty())
            mapOrphanTransactionsByPrev.erase(itPrev);
    }
    mapOrphanTransactions.erase(it);
}

void EraseOrphansFor(NodeId peer)
{
    int nErased = 0;
    map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
    while (iter != mapOrphanTransactions.end()) {
        map<uint256, COrphanTx>::iterator maybeErase = iter++; // increment to avoid iterator becoming invalid
        if (maybeErase->second.fromPeer == peer) {
            EraseOrphanTx(maybeErase->second.tx.GetHash());
            ++nErased;
        }
    }
    if (nErased > 0) LogPrint("mempool", "Erased %d orphan tx from peer %d\n", nErased, peer);
}


unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans)
{
    unsigned int nEvicted = 0;
    while (mapOrphanTransactions.size() > nMaxOrphans) {
        // Evict a random orphan:
        uint256 randomhash = GetRandHash();
        map<uint256, COrphanTx>::iterator it = mapOrphanTransactions.lower_bound(randomhash);
        if (it == mapOrphanTransactions.end())
            it = mapOrphanTransactions.begin();
        EraseOrphanTx(it->first);
        ++nEvicted;
    }
    return nEvicted;
}

bool IsStandardTx(const CTransaction& tx, string& reason)
{
    AssertLockHeld(cs_main);
    if (tx.nVersion > CTransaction::CURRENT_VERSION || tx.nVersion < 1) {
        reason = "version";
        return false;
    }

    // Treat non-final transactions as non-standard to prevent a specific type
    // of double-spend attack, as well as DoS attacks. (if the transaction
    // can't be mined, the attacker isn't expending resources broadcasting it)
    // Basically we don't want to propagate transactions that can't be included in
    // the next block.
    //
    // However, IsFinalTx() is confusing... Without arguments, it uses
    // chainActive.Height() to evaluate nLockTime; when a block is accepted, chainActive.Height()
    // is set to the value of nHeight in the block. However, when IsFinalTx()
    // is called within CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a transaction can
    // be part of the *next* block, we need to call IsFinalTx() with one more
    // than chainActive.Height().
    //
    // Timestamps on the other hand don't get any special treatment, because we
    // can't know what timestamp the next block will have, and there aren't
    // timestamp applications where it matters.
    if (!IsFinalTx(tx, chainActive.Height() + 1)) {
        reason = "non-final";
        return false;
    }

    // Extremely large transactions with lots of inputs can cost the network
    // almost as much to process as they cost the sender in fees, because
    // computing signature hashes is O(ninputs*txsize). Limiting transactions
    // to MAX_STANDARD_TX_SIZE mitigates CPU exhaustion attacks.
    unsigned int sz = tx.GetSerializeSize(SER_NETWORK, CTransaction::CURRENT_VERSION);
    if (sz >= MAX_STANDARD_TX_SIZE) {
        reason = "tx-size";
        return false;
    }

    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        // Biggest 'standard' txin is a 15-of-15 P2SH multisig with compressed
        // keys. (remember the 520 byte limit on redeemScript size) That works
        // out to a (15*(33+1))+3=513 byte redeemScript, 513+1+15*(73+1)+3=1627
        // bytes of scriptSig, which we round off to 1650 bytes for some minor
        // future-proofing. That's also enough to spend a 20-of-20
        // CHECKMULTISIG scriptPubKey, though such a scriptPubKey is not
        // considered standard)
        if (txin.scriptSig.size() > 1650) {
            reason = "scriptsig-size";
            return false;
        }
        if (!txin.scriptSig.IsPushOnly()) {
            reason = "scriptsig-not-pushonly";
            return false;
        }
    }

    unsigned int nDataOut = 0;
    txnouttype whichType;
    BOOST_FOREACH (const CTxOut& txout, tx.vout) {
        if (!::IsStandard(txout.scriptPubKey, whichType)) {
            reason = "scriptpubkey";
            return false;
        }

        if (whichType == TX_NULL_DATA)
            nDataOut++;
        else if ((whichType == TX_MULTISIG) && (!fIsBareMultisigStd)) {
            reason = "bare-multisig";
            return false;
        } else if (txout.nValue == 0) {
            reason = "dust";
            return false;
        }
    }

    // only one OP_RETURN txout is permitted
    if (nDataOut > 1) {
        reason = "multi-op-return";
        return false;
    }

    return true;
}

bool IsFinalTx(const CTransaction& tx, int nBlockHeight, int64_t nBlockTime)
{
    AssertLockHeld(cs_main);
    // Time based nLockTime implemented in 0.1.6
    if (tx.nLockTime == 0)
        return true;
    if (nBlockHeight == 0)
        nBlockHeight = chainActive.Height();
    if (nBlockTime == 0)
        nBlockTime = GetAdjustedTime();
    if ((int64_t)tx.nLockTime < ((int64_t)tx.nLockTime < LOCKTIME_THRESHOLD ? (int64_t)nBlockHeight : nBlockTime))
        return true;
    BOOST_FOREACH (const CTxIn& txin, tx.vin)
        if (!txin.IsFinal())
            return false;
    return true;
}

/**
 * Check transaction inputs to mitigate two
 * potential denial-of-service attacks:
 *
 * 1. scriptSigs with extra data stuffed into them,
 *    not consumed by scriptPubKey (or P2SH script)
 * 2. P2SH scripts with a crazy number of expensive
 *    CHECKSIG/CHECKMULTISIG operations
 */
bool AreInputsStandard(const CTransaction& tx, const CCoinsViewCache& mapInputs)
{
    if (tx.IsCoinBase())
        return true; // Coinbases don't use vin normally

    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxOut& prev = mapInputs.GetOutputFor(tx.vin[i]);

        vector<vector<unsigned char> > vSolutions;
        txnouttype whichType;
        // get the scriptPubKey corresponding to this input:
        const CScript& prevScript = prev.scriptPubKey;
        if (!Solver(prevScript, whichType, vSolutions))
            return false;
        int nArgsExpected = ScriptSigArgsExpected(whichType, vSolutions);
        if (nArgsExpected < 0)
            return false;

        // Transactions with extra stuff in their scriptSigs are
        // non-standard. Note that this EvalScript() call will
        // be quick, because if there are any operations
        // beside "push data" in the scriptSig
        // IsStandard() will have already returned false
        // and this method isn't called.
        vector<vector<unsigned char> > stack;
        if (!EvalScript(stack, tx.vin[i].scriptSig, false, BaseSignatureChecker()))
            return false;

        if (whichType == TX_SCRIPTHASH) {
            if (stack.empty())
                return false;
            CScript subscript(stack.back().begin(), stack.back().end());
            vector<vector<unsigned char> > vSolutions2;
            txnouttype whichType2;
            if (Solver(subscript, whichType2, vSolutions2)) {
                int tmpExpected = ScriptSigArgsExpected(whichType2, vSolutions2);
                if (tmpExpected < 0)
                    return false;
                nArgsExpected += tmpExpected;
            } else {
                // Any other Script with less than 15 sigops OK:
                unsigned int sigops = subscript.GetSigOpCount(true);
                // ... extra data left on the stack after execution is OK, too:
                return (sigops <= MAX_P2SH_SIGOPS);
            }
        }

        if (stack.size() != (unsigned int)nArgsExpected)
            return false;
    }

    return true;
}

unsigned int GetLegacySigOpCount(const CTransaction& tx)
{
    unsigned int nSigOps = 0;
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        nSigOps += txin.scriptSig.GetSigOpCount(false);
    }
    BOOST_FOREACH (const CTxOut& txout, tx.vout) {
        nSigOps += txout.scriptPubKey.GetSigOpCount(false);
    }
    return nSigOps;
}

unsigned int GetP2SHSigOpCount(const CTransaction& tx, const CCoinsViewCache& inputs)
{
    if (tx.IsCoinBase())
        return 0;

    unsigned int nSigOps = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxOut& prevout = inputs.GetOutputFor(tx.vin[i]);
        if (prevout.scriptPubKey.IsPayToScriptHash())
            nSigOps += prevout.scriptPubKey.GetSigOpCount(tx.vin[i].scriptSig);
    }
    return nSigOps;
}

int GetInputAge(CTxIn& vin)
{
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(mempool.cs);
        CCoinsViewMemPool viewMempool(pcoinsTip, mempool);
        view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

        const CCoins* coins = view.AccessCoins(vin.prevout.hash);

        if (coins) {
            if (coins->nHeight < 0) return 0;
            return (chainActive.Tip()->nHeight + 1) - coins->nHeight;
        } else
            return -1;
    }
}

//int GetInputAgeIX(uint256 nTXHash, CTxIn& vin)
//{
//    int sigs = 0;
//    int nResult = GetInputAge(vin);
//    if (nResult < 0) nResult = 0;
//
//    if (nResult < 6) {
//        std::map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(nTXHash);
//        if (i != mapTxLocks.end()) {
//            sigs = (*i).second.CountSignatures();
//        }
//        if (sigs >= SWIFTTX_SIGNATURES_REQUIRED) {
//            return nSwiftTXDepth + nResult;
//        }
//    }
//
//    return -1;
//}

int GetIXConfirmations(uint256 nTXHash)
{
    int sigs = 0;

    std::map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(nTXHash);
    if (i != mapTxLocks.end()) {
        sigs = (*i).second.CountSignatures();
    }
    if (sigs >= INSTANTX_SIGNATURES_REQUIRED) {
        return nInstantXDepth;
    }

    return 0;
}

// ppcoin: total coin age spent in transaction, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches.
bool GetCoinAge(const CTransaction& tx, const unsigned int nTxTime, uint64_t& nCoinAge)
{
    uint256 bnCentSecond = 0; // coin age in the unit of cent-seconds
    nCoinAge = 0;

    CBlockIndex* pindex = NULL;
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        // First try finding the previous transaction in database
        CTransaction txPrev;
        uint256 hashBlockPrev;
        if (!GetTransaction(txin.prevout.hash, txPrev, hashBlockPrev, true)) {
            LogPrintf("GetCoinAge: failed to find vin transaction \n");
            continue; // previous transaction not in main chain
        }

        BlockMap::iterator it = mapBlockIndex.find(hashBlockPrev);
        if (it != mapBlockIndex.end())
            pindex = it->second;
        else {
            LogPrintf("GetCoinAge() failed to find block index \n");
            continue;
        }

        // Read block header
        CBlockHeader prevblock = pindex->GetBlockHeader();

        if (prevblock.nTime + nStakeMinAge > nTxTime)
            continue; // only count coins meeting min age requirement

        if (nTxTime < prevblock.nTime) {
            LogPrintf("GetCoinAge: Timestamp Violation: txtime less than txPrev.nTime");
            return false; // Transaction timestamp violation
        }

        int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
        bnCentSecond += uint256(nValueIn) * (nTxTime - prevblock.nTime);
    }

    uint256 bnCoinDay = bnCentSecond / COIN / (24 * 60 * 60);
    LogPrintf("coin age bnCoinDay=%s\n", bnCoinDay.ToString().c_str());
    nCoinAge = bnCoinDay.GetCompact();
    return true;
}

bool CheckTransaction(const CTransaction& tx, CValidationState& state)
{
    // Basic checks that don't depend on any context
    if (tx.vin.empty())
        return state.DoS(10, error("CheckTransaction() : vin empty"),
            REJECT_INVALID, "bad-txns-vin-empty");
    if (tx.vout.empty())
        return state.DoS(10, error("CheckTransaction() : vout empty"),
            REJECT_INVALID, "bad-txns-vout-empty");
    // Size limits
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_SIZE)
        return state.DoS(100, error("CheckTransaction() : size limits failed"),
            REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    CAmount nValueOut = 0;
    BOOST_FOREACH (const CTxOut& txout, tx.vout) {
        if (txout.IsEmpty() && !tx.IsCoinBase() && !tx.IsCoinStake())
            return state.DoS(100, error("CheckTransaction(): txout empty for user transaction"));

        if (txout.nValue < 0)
            return state.DoS(100, error("CheckTransaction() : txout.nValue negative"),
                REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, error("CheckTransaction() : txout.nValue too high"),
                REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, error("CheckTransaction() : txout total out of range"),
                REJECT_INVALID, "bad-txns-txouttotal-toolarge");
    }

    // Check for duplicate inputs
    set<COutPoint> vInOutPoints;
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        if (vInOutPoints.count(txin.prevout))
            return state.DoS(100, error("CheckTransaction() : duplicate inputs"),
                REJECT_INVALID, "bad-txns-inputs-duplicate");
        vInOutPoints.insert(txin.prevout);
    }

    if (tx.IsCoinBase()) {
        if (/*tx.vin[0].scriptSig.size() < 2 || */ tx.vin[0].scriptSig.size() > 150)
            return state.DoS(100, error("CheckTransaction() : coinbase script size=%d", tx.vin[0].scriptSig.size()),
                REJECT_INVALID, "bad-cb-length");
    } else {
        BOOST_FOREACH (const CTxIn& txin, tx.vin)
            if (txin.prevout.IsNull())
                return state.DoS(10, error("CheckTransaction() : prevout is null"),
                    REJECT_INVALID, "bad-txns-prevout-null");
    }

    return true;
}

bool CheckFinalTx(const CTransaction& tx, int flags)
{
    AssertLockHeld(cs_main);

    // By convention a negative value for flags indicates that the
    // current network-enforced consensus rules should be used. In
    // a future soft-fork scenario that would mean checking which
    // rules would be enforced for the next block and setting the
    // appropriate flags. At the present time no soft-forks are
    // scheduled, so no flags are set.
    flags = std::max(flags, 0);

    // CheckFinalTx() uses chainActive.Height()+1 to evaluate
    // nLockTime because when IsFinalTx() is called within
    // CBlock::AcceptBlock(), the height of the block *being*
    // evaluated is what is used. Thus if we want to know if a
    // transaction can be part of the *next* block, we need to call
    // IsFinalTx() with one more than chainActive.Height().
    const int nBlockHeight = chainActive.Height() + 1;

    // BIP113 will require that time-locked transactions have nLockTime set to
    // less than the median time of the previous block they're contained in.
    // When the next block is created its previous block will be the current
    // chain tip, so we use that to calculate the median time passed to
    // IsFinalTx() if LOCKTIME_MEDIAN_TIME_PAST is set.
    const int64_t nBlockTime = (flags & LOCKTIME_MEDIAN_TIME_PAST) ? chainActive.Tip()->GetMedianTimePast() : GetAdjustedTime();

    return IsFinalTx(tx, nBlockHeight, nBlockTime);
}

CAmount GetMinRelayFee(const CTransaction& tx, unsigned int nBytes, bool fAllowFree)
{
    {
        LOCK(mempool.cs);
        uint256 hash = tx.GetHash();
        double dPriorityDelta = 0;
        CAmount nFeeDelta = 0;
        mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
        if (dPriorityDelta > 0 || nFeeDelta > 0)
            return 0;
    }

    CAmount nMinFee = ::minRelayTxFee.GetFee(nBytes);

    if (fAllowFree) {
        // There is a free transaction area in blocks created by most miners,
        // * If we are relaying we allow transactions up to DEFAULT_BLOCK_PRIORITY_SIZE - 1000
        //   to be considered to fall into this category. We don't want to encourage sending
        //   multiple transactions instead of one big transaction to avoid fees.
        if (nBytes < (DEFAULT_BLOCK_PRIORITY_SIZE - 1000))
            nMinFee = 0;
    }

    if (!MoneyRange(nMinFee))
        nMinFee = MAX_MONEY;
    return nMinFee;
}


bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState& state, const CTransaction& tx, bool fLimitFree, bool* pfMissingInputs, bool fRejectInsaneFee, bool ignoreFees)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!CheckTransaction(tx, state))
        return error("AcceptToMemoryPool: : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return state.DoS(100, error("AcceptToMemoryPool: : coinbase as individual tx"),
            REJECT_INVALID, "coinbase");

    //Coinstake is also only valid in a block, not as a loose transaction
    if (tx.IsCoinStake())
        return state.DoS(100, error("AcceptToMemoryPool: coinstake as individual tx"),
            REJECT_INVALID, "coinstake");

    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    string reason;
    if (Params().RequireStandard() && !IsStandardTx(tx, reason))
        return state.DoS(0,
            error("AcceptToMemoryPool : nonstandard transaction: %s", reason),
            REJECT_NONSTANDARD, reason);

    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
        return false;

    // ----------- swiftTX transaction scanning -----------

    BOOST_FOREACH (const CTxIn& in, tx.vin) {
        if (mapLockedInputs.count(in.prevout)) {
            if (mapLockedInputs[in.prevout] != tx.GetHash()) {
                return state.DoS(0,
                    error("AcceptToMemoryPool : conflicts with existing transaction lock: %s", reason),
                    REJECT_INVALID, "tx-lock-conflict");
            }
        }
    }

    // Check for conflicts with in-memory transactions
    {
        LOCK(pool.cs); // protect pool.mapNextTx
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            COutPoint outpoint = tx.vin[i].prevout;
            if (pool.mapNextTx.count(outpoint)) {
                // Disable replacement feature for now
                return false;
            }
        }
    }


    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        CAmount nValueIn = 0;
        {
            LOCK(pool.cs);
            CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
            view.SetBackend(viewMemPool);

            // do we already have it?
            if (view.HaveCoins(hash))
                return false;

            // do all inputs exist?
            // Note that this does not check for the presence of actual outputs (see the next check for that),
            // only helps filling in pfMissingInputs (to determine missing vs spent).
            BOOST_FOREACH (const CTxIn txin, tx.vin) {
                if (!view.HaveCoins(txin.prevout.hash)) {
                    if (pfMissingInputs)
                        *pfMissingInputs = true;
                    return false;
                }
            }

            // are the actual inputs available?
            if (!view.HaveInputs(tx))
                return state.Invalid(error("AcceptToMemoryPool : inputs already spent"),
                    REJECT_DUPLICATE, "bad-txns-inputs-spent");

            // Bring the best block into scope
            view.GetBestBlock();

            nValueIn = view.GetValueIn(tx);

            // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
            view.SetBackend(dummy);
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (Params().RequireStandard() && !AreInputsStandard(tx, view))
            return error("AcceptToMemoryPool: : nonstandard transaction input");

        // Check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. Since the coinbase transaction
        // itself can contain sigops MAX_TX_SIGOPS is less than
        // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
        // merely non-standard transaction.
        unsigned int nSigOps = GetLegacySigOpCount(tx);
        nSigOps += GetP2SHSigOpCount(tx, view);
        if (nSigOps > MAX_TX_SIGOPS)
            return state.DoS(0,
                error("AcceptToMemoryPool : too many sigops %s, %d > %d",
                    hash.ToString(), nSigOps, MAX_TX_SIGOPS),
                REJECT_NONSTANDARD, "bad-txns-too-many-sigops");

        CAmount nValueOut = tx.GetValueOut();
        CAmount nFees = nValueIn - nValueOut;
        double dPriority = view.GetPriority(tx, chainActive.Height());

        CTxMemPoolEntry entry(tx, nFees, GetTime(), dPriority, chainActive.Height());
        unsigned int nSize = entry.GetTxSize();

        // Don't accept it if it can't get into a block
        // but prioritise dstx and don't check fees for it
        if (mapLuxsendBroadcastTxes.count(hash)) {
            mempool.PrioritiseTransaction(hash, hash.ToString(), 1000, 0.1 * COIN);
        } else if (!ignoreFees) {
            CAmount txMinFee = GetMinRelayFee(tx, nSize, true);
            if (fLimitFree && nFees < txMinFee)
                return state.DoS(0, error("AcceptToMemoryPool : not enough fees %s, %d < %d",
                                        hash.ToString(), nFees, txMinFee),
                    REJECT_INSUFFICIENTFEE, "insufficient fee");

            // Require that free transactions have sufficient priority to be mined in the next block.
            if (GetBoolArg("-relaypriority", true) && nFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(view.GetPriority(tx, chainActive.Height() + 1))) {
                return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "insufficient priority");
            }

            // Continuously rate-limit free (really, very-low-fee) transactions
            // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
            // be annoying or make others' transactions take longer to confirm.
            if (fLimitFree && nFees < ::minRelayTxFee.GetFee(nSize)) {
                static CCriticalSection csFreeLimiter;
                static double dFreeCount;
                static int64_t nLastTime;
                int64_t nNow = GetTime();

                LOCK(csFreeLimiter);

                // Use an exponentially decaying ~10-minute window:
                dFreeCount *= pow(1.0 - 1.0 / 600.0, (double)(nNow - nLastTime));
                nLastTime = nNow;
                // -limitfreerelay unit is thousand-bytes-per-minute
                // At default rate it would take over a month to fill 1GB
                if (dFreeCount >= GetArg("-limitfreerelay", 15) * 10 * 1000)
                    return state.DoS(0, error("AcceptToMemoryPool : free transaction rejected by rate limiter"),
                        REJECT_INSUFFICIENTFEE, "rate limited free transaction");
                LogPrint("mempool", "Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount + nSize);
                dFreeCount += nSize;
            }
        }

        if (fRejectInsaneFee && nFees > ::minRelayTxFee.GetFee(nSize) * 10000)
            return error("AcceptToMemoryPool: : insane fees %s, %d > %d",
                hash.ToString(),
                nFees, ::minRelayTxFee.GetFee(nSize) * 10000);

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!CheckInputs(tx, state, view, true, STANDARD_SCRIPT_VERIFY_FLAGS, true)) {
            return error("AcceptToMemoryPool: : ConnectInputs failed %s", hash.ToString());
        }

        // Check again against just the consensus-critical mandatory script
        // verification flags, in case of bugs in the standard flags that cause
        // transactions to pass as valid when they're actually invalid. For
        // instance the STRICTENC flag was incorrectly allowing certain
        // CHECKSIG NOT scripts to pass, even though they were invalid.
        //
        // There is a similar check in CreateNewBlock() to prevent creating
        // invalid blocks, however allowing such transactions into the mempool
        // can be exploited as a DoS attack.
        if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true)) {
            return error("AcceptToMemoryPool: : BUG! PLEASE REPORT THIS! ConnectInputs failed against MANDATORY but not STANDARD flags %s", hash.ToString());
        }

        // Store transaction in memory
        pool.addUnchecked(hash, entry);
    }

    SyncWithWallets(tx, NULL);

    return true;
}

bool AcceptableInputs(CTxMemPool& pool, CValidationState& state, const CTransaction& tx, bool fLimitFree, bool* pfMissingInputs, bool fRejectInsaneFee, bool isDSTX)
{
    AssertLockHeld(cs_main);
    if (pfMissingInputs)
        *pfMissingInputs = false;

    if (!CheckTransaction(tx, state))
        return error("AcceptableInputs: : CheckTransaction failed");

    // Coinbase is only valid in a block, not as a loose transaction
    if (tx.IsCoinBase())
        return state.DoS(100, error("AcceptableInputs: : coinbase as individual tx"),
            REJECT_INVALID, "coinbase");

    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    string reason;
    // for any real tx this will be checked on AcceptToMemoryPool anyway
    //    if (Params().RequireStandard() && !IsStandardTx(tx, reason))
    //        return state.DoS(0,
    //                         error("AcceptableInputs : nonstandard transaction: %s", reason),
    //                         REJECT_NONSTANDARD, reason);

    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
        return false;

    // ----------- swiftTX transaction scanning -----------

    BOOST_FOREACH (const CTxIn& in, tx.vin) {
        if (mapLockedInputs.count(in.prevout)) {
            if (mapLockedInputs[in.prevout] != tx.GetHash()) {
                return state.DoS(0,
                    error("AcceptableInputs : conflicts with existing transaction lock: %s", reason),
                    REJECT_INVALID, "tx-lock-conflict");
            }
        }
    }

    // Check for conflicts with in-memory transactions
    {
        LOCK(pool.cs); // protect pool.mapNextTx
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            COutPoint outpoint = tx.vin[i].prevout;
            if (pool.mapNextTx.count(outpoint)) {
                // Disable replacement feature for now
                return false;
            }
        }
    }


    {
        CCoinsView dummy;
        CCoinsViewCache view(&dummy);

        CAmount nValueIn = 0;
        {
            LOCK(pool.cs);
            CCoinsViewMemPool viewMemPool(pcoinsTip, pool);
            view.SetBackend(viewMemPool);

            // do we already have it?
            if (view.HaveCoins(hash))
                return false;

            // do all inputs exist?
            // Note that this does not check for the presence of actual outputs (see the next check for that),
            // only helps filling in pfMissingInputs (to determine missing vs spent).
            BOOST_FOREACH (const CTxIn txin, tx.vin) {
                if (!view.HaveCoins(txin.prevout.hash)) {
                    if (pfMissingInputs)
                        *pfMissingInputs = true;
                    return false;
                }
            }

            // are the actual inputs available?
            if (!view.HaveInputs(tx))
                return state.Invalid(error("AcceptableInputs : inputs already spent"),
                    REJECT_DUPLICATE, "bad-txns-inputs-spent");

            // Bring the best block into scope
            view.GetBestBlock();

            nValueIn = view.GetValueIn(tx);

            // we have all inputs cached now, so switch back to dummy, so we don't need to keep lock on mempool
            view.SetBackend(dummy);
        }

        // Check for non-standard pay-to-script-hash in inputs
        // for any real tx this will be checked on AcceptToMemoryPool anyway
        //        if (Params().RequireStandard() && !AreInputsStandard(tx, view))
        //            return error("AcceptableInputs: : nonstandard transaction input");

        // Check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. Since the coinbase transaction
        // itself can contain sigops MAX_TX_SIGOPS is less than
        // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
        // merely non-standard transaction.
        unsigned int nSigOps = GetLegacySigOpCount(tx);
        nSigOps += GetP2SHSigOpCount(tx, view);
        if (nSigOps > MAX_TX_SIGOPS)
            return state.DoS(0,
                error("AcceptableInputs : too many sigops %s, %d > %d",
                    hash.ToString(), nSigOps, MAX_TX_SIGOPS),
                REJECT_NONSTANDARD, "bad-txns-too-many-sigops");

        CAmount nValueOut = tx.GetValueOut();
        CAmount nFees = nValueIn - nValueOut;
        double dPriority = view.GetPriority(tx, chainActive.Height());

        CTxMemPoolEntry entry(tx, nFees, GetTime(), dPriority, chainActive.Height());
        unsigned int nSize = entry.GetTxSize();

        // Don't accept it if it can't get into a block
        // but prioritise dstx and don't check fees for it
        if (isDSTX) {
            mempool.PrioritiseTransaction(hash, hash.ToString(), 1000, 0.1 * COIN);
        } else { // same as !ignoreFees for AcceptToMemoryPool
            CAmount txMinFee = GetMinRelayFee(tx, nSize, true);
            if (fLimitFree && nFees < txMinFee)
                return state.DoS(0, error("AcceptableInputs : not enough fees %s, %d < %d",
                                        hash.ToString(), nFees, txMinFee),
                    REJECT_INSUFFICIENTFEE, "insufficient fee");

            // Require that free transactions have sufficient priority to be mined in the next block.
            if (GetBoolArg("-relaypriority", true) && nFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(view.GetPriority(tx, chainActive.Height() + 1))) {
                return state.DoS(0, false, REJECT_INSUFFICIENTFEE, "insufficient priority");
            }

            // Continuously rate-limit free (really, very-low-fee) transactions
            // This mitigates 'penny-flooding' -- sending thousands of free transactions just to
            // be annoying or make others' transactions take longer to confirm.
            if (fLimitFree && nFees < ::minRelayTxFee.GetFee(nSize)) {
                static CCriticalSection csFreeLimiter;
                static double dFreeCount;
                static int64_t nLastTime;
                int64_t nNow = GetTime();

                LOCK(csFreeLimiter);

                // Use an exponentially decaying ~10-minute window:
                dFreeCount *= pow(1.0 - 1.0 / 600.0, (double)(nNow - nLastTime));
                nLastTime = nNow;
                // -limitfreerelay unit is thousand-bytes-per-minute
                // At default rate it would take over a month to fill 1GB
                if (dFreeCount >= GetArg("-limitfreerelay", 15) * 10 * 1000)
                    return state.DoS(0, error("AcceptableInputs : free transaction rejected by rate limiter"),
                        REJECT_INSUFFICIENTFEE, "rate limited free transaction");
                LogPrint("mempool", "Rate limit dFreeCount: %g => %g\n", dFreeCount, dFreeCount + nSize);
                dFreeCount += nSize;
            }
        }

        if (fRejectInsaneFee && nFees > ::minRelayTxFee.GetFee(nSize) * 10000)
            return error("AcceptableInputs: : insane fees %s, %d > %d",
                hash.ToString(),
                nFees, ::minRelayTxFee.GetFee(nSize) * 10000);

        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        if (!CheckInputs(tx, state, view, false, STANDARD_SCRIPT_VERIFY_FLAGS, true)) {
            return error("AcceptableInputs: : ConnectInputs failed %s", hash.ToString());
        }

        // Check again against just the consensus-critical mandatory script
        // verification flags, in case of bugs in the standard flags that cause
        // transactions to pass as valid when they're actually invalid. For
        // instance the STRICTENC flag was incorrectly allowing certain
        // CHECKSIG NOT scripts to pass, even though they were invalid.
        //
        // There is a similar check in CreateNewBlock() to prevent creating
        // invalid blocks, however allowing such transactions into the mempool
        // can be exploited as a DoS attack.
        // for any real tx this will be checked on AcceptToMemoryPool anyway
        //        if (!CheckInputs(tx, state, view, false, MANDATORY_SCRIPT_VERIFY_FLAGS, true))
        //        {
        //            return error("AcceptableInputs: : BUG! PLEASE REPORT THIS! ConnectInputs failed against MANDATORY but not STANDARD flags %s", hash.ToString());
        //        }

        // Store transaction in memory
        // pool.addUnchecked(hash, entry);
    }

    // SyncWithWallets(tx, NULL);

    return true;
}

/** Return transaction in tx, and if it was found inside a block, its hash is placed in hashBlock */
bool GetTransaction(const uint256& hash, CTransaction& txOut, uint256& hashBlock, bool fAllowSlow)
{
    CBlockIndex* pindexSlow = NULL;
    {
        LOCK(cs_main);
        {
            if (mempool.lookup(hash, txOut)) {
                return true;
            }
        }

        if (fTxIndex) {
            CDiskTxPos postx;
            if (pblocktree->ReadTxIndex(hash, postx)) {
                CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
                if (file.IsNull())
                    return error("%s: OpenBlockFile failed", __func__);
                CBlockHeader header;
                try {
                    file >> header;
                    fseek(file.Get(), postx.nTxOffset, SEEK_CUR);
                    file >> txOut;
                } catch (std::exception& e) {
                    return error("%s : Deserialize or I/O error - %s", __func__, e.what());
                }
                hashBlock = header.GetHash();
                if (txOut.GetHash() != hash)
                    return error("%s : txid mismatch", __func__);
                return true;
            }
        }

        if (fAllowSlow) { // use coin database to locate block that contains transaction, and scan it
            int nHeight = -1;
            {
                CCoinsViewCache& view = *pcoinsTip;
                const CCoins* coins = view.AccessCoins(hash);
                if (coins)
                    nHeight = coins->nHeight;
            }
            if (nHeight > 0)
                pindexSlow = chainActive[nHeight];
        }
    }

    if (pindexSlow) {
        CBlock block;
        if (ReadBlockFromDisk(block, pindexSlow)) {
            BOOST_FOREACH (const CTransaction& tx, block.vtx) {
                if (tx.GetHash() == hash) {
                    txOut = tx;
                    hashBlock = pindexSlow->GetBlockHash();
                    return true;
                }
            }
        }
    }

    return false;
}


//////////////////////////////////////////////////////////////////////////////
//
// CBlock and CBlockIndex
//

bool WriteBlockToDisk(CBlock& block, CDiskBlockPos& pos)
{
    // Open history file to append
    CAutoFile fileout(OpenBlockFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("WriteBlockToDisk : OpenBlockFile failed");

    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(block);
    fileout << FLATDATA(Params().MessageStart()) << nSize;

    // Write block
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("WriteBlockToDisk : ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << block;

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos)
{
    block.SetNull();

    // Open history file to read
    CAutoFile filein(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("ReadBlockFromDisk : OpenBlockFile failed");

    // Read block
    try {
        filein >> block;
    } catch (std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    // Check the header
    if (block.IsProofOfWork()) {
        if (!CheckProofOfWork(block.GetHash(), block.nBits))
            return error("ReadBlockFromDisk : Errors in block header");
    }

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex)
{
    if (!ReadBlockFromDisk(block, pindex->GetBlockPos()))
        return false;
    if (block.GetHash() != pindex->GetBlockHash()) {
        LogPrintf("%s : block=%s index=%s\n", __func__, block.GetHash().ToString().c_str(), pindex->GetBlockHash().ToString().c_str());
        return error("ReadBlockFromDisk(CBlock&, CBlockIndex*) : GetHash() doesn't match index");
    }
    return true;
}


double ConvertBitsToDouble(unsigned int nBits)
{
    int nShift = (nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(nBits & 0x00ffffff);

    while (nShift < 29) {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29) {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

uint256 GetProofOfStakeLimit(int nHeight)
{
    if (IsProtocolV2(nHeight))
        return bnProofOfStakeLimitV2;
    else
        return bnProofOfStakeLimit;
}

CAmount GetBlockValue(int nHeight) {
    int64_t nSubsidy = 1 * COIN;

    if (Params().NetworkID() == CBaseChainParams::TESTNET) {
        if (nHeight < 200 && nHeight > 0)
            return 250000 * COIN;
    }

    if (nHeight == 1) {
        nSubsidy = 3000000 * COIN;
    } else if (nHeight < 500) {
        nSubsidy = 1 * COIN;
    } else if (nHeight == 501) {
        nSubsidy = 1000 * COIN;
    } else if (nHeight < 1000000) {
        nSubsidy = 10 * COIN;
    } else if (nHeight < 1001000) {
        nSubsidy = 30 * COIN;
    } else if (nHeight < 5000000) {
        nSubsidy = 10 * COIN;
    } else if (nHeight < 6000000) {
        nSubsidy = 10 * COIN;
    } else {
        nSubsidy = 1 * COIN;
    }
    return nSubsidy + nHeight + 1;
}

CAmount GetProofOfStakeReward(int64_t nCoinAge, int64_t nFees, int nHeight) {
    int64_t nSubsidy = STATIC_POS_REWARD;

    // First 100,000 blocks double stake for masternode ready
    if(pindexBestHeader->nHeight < 100000) {
        nSubsidy = 2 * COIN;
    } else {
        nSubsidy = 1 * COIN;
    }

//    LogPrint("creation", "GetProofOfStakeReward(): create=%s nCoinAge=%d nHeight=%d\n", FormatMoney(nSubsidy), nCoinAge, nHeight);

    return nSubsidy + nFees;
}


bool IsBlockValueValid(const CBlock& block, int64_t nExpectedValue)
{
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (pindexPrev == NULL) return true;

    int nHeight = 0;
    if (pindexPrev->GetBlockHash() == block.hashPrevBlock) {
        nHeight = pindexPrev->nHeight + 1;
    } else { //out of order
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi != mapBlockIndex.end() && (*mi).second)
            nHeight = (*mi).second->nHeight + 1;
    }

    if (nHeight == 0) {
        LogPrintf("IsBlockValueValid() : WARNING: Couldn't find previous block");
    }

//    if (!masternodeSync.IsSynced()) { //there is no budget data to use to check anything
//        //super blocks will always be on these blocks, max 100 per budgeting
//        if (nHeight % GetBudgetPaymentCycleBlocks() < 100) {
//            return true;
//        } else {
//            if (block.vtx[0].GetValueOut() > nExpectedValue) return false;
//        }
//    } else { // we're synced and have data so check the budget schedule
//
//        //are these blocks even enabled
//        if (!IsSporkActive(SPORK_13_ENABLE_SUPERBLOCKS)) {
            return block.vtx[0].GetValueOut() <= nExpectedValue;
//        }
//
//        if (budget.IsBudgetPaymentBlock(nHeight)) {
//            //the value of the block is evaluated in CheckBlock
//            return true;
//        } else {
//            if (block.vtx[0].GetValueOut() > nExpectedValue) return false;
//        }
//    }

    return true;
}


int64_t GetMasternodePayment(int nHeight, int64_t blockValue, int nMasternodeCount)
{
    int64_t ret = blockValue * 0.4; //40% for masternodes

    return ret;
}

bool IsInitialBlockDownload()
{
    LOCK(cs_main);
    if (fImporting || fReindex || chainActive.Height() < Checkpoints::GetTotalBlocksEstimate())
        return true;
    static bool lockIBDState = false;
    if (lockIBDState)
        return false;
    bool state = (chainActive.Height() < pindexBestHeader->nHeight - 24 * 6 ||
                  pindexBestHeader->GetBlockTime() < GetTime() - 8 * 60 * 60); // ~144 blocks behind -> 2 x fork detection time
    if (!state)
        lockIBDState = true;
    return state;
}

bool fLargeWorkForkFound = false;
bool fLargeWorkInvalidChainFound = false;
CBlockIndex *pindexBestForkTip = NULL, *pindexBestForkBase = NULL;

void CheckForkWarningConditions()
{
    AssertLockHeld(cs_main);
    // Before we get past initial download, we cannot reliably alert about forks
    // (we assume we don't get stuck on a fork before the last checkpoint)
    if (IsInitialBlockDownload())
        return;

    // If our best fork is no longer within 72 blocks (+/- 3 hours if no one mines it)
    // of our head, drop it
    if (pindexBestForkTip && chainActive.Height() - pindexBestForkTip->nHeight >= 72)
        pindexBestForkTip = NULL;

    if (pindexBestForkTip || (pindexBestInvalid && pindexBestInvalid->nChainWork > chainActive.Tip()->nChainWork + (GetBlockProof(*chainActive.Tip()) * 6))) {
        if (!fLargeWorkForkFound && pindexBestForkBase) {
            if (pindexBestForkBase->phashBlock) {
                std::string warning = std::string("'Warning: Large-work fork detected, forking after block ") +
                                      pindexBestForkBase->phashBlock->ToString() + std::string("'");
                CAlert::Notify(warning, true);
            }
        }
        if (pindexBestForkTip && pindexBestForkBase) {
            if (pindexBestForkBase->phashBlock) {
                LogPrintf("CheckForkWarningConditions: Warning: Large valid fork found\n  forking the chain at height %d (%s)\n  lasting to height %d (%s).\nChain state database corruption likely.\n",
                    pindexBestForkBase->nHeight, pindexBestForkBase->phashBlock->ToString(),
                    pindexBestForkTip->nHeight, pindexBestForkTip->phashBlock->ToString());
                fLargeWorkForkFound = true;
            }
        } else {
            LogPrintf("CheckForkWarningConditions: Warning: Found invalid chain at least ~6 blocks longer than our best chain.\nChain state database corruption likely.\n");
            fLargeWorkInvalidChainFound = true;
        }
    } else {
        fLargeWorkForkFound = false;
        fLargeWorkInvalidChainFound = false;
    }
}

void CheckForkWarningConditionsOnNewFork(CBlockIndex* pindexNewForkTip)
{
    AssertLockHeld(cs_main);
    // If we are on a fork that is sufficiently large, set a warning flag
    CBlockIndex* pfork = pindexNewForkTip;
    CBlockIndex* plonger = chainActive.Tip();
    while (pfork && pfork != plonger) {
        while (plonger && plonger->nHeight > pfork->nHeight)
            plonger = plonger->pprev;
        if (pfork == plonger)
            break;
        pfork = pfork->pprev;
    }

    // We define a condition which we should warn the user about as a fork of at least 7 blocks
    // who's tip is within 72 blocks (+/- 3 hours if no one mines it) of ours
    // or a chain that is entirely longer than ours and invalid (note that this should be detected by both)
    // We use 7 blocks rather arbitrarily as it represents just under 10% of sustained network
    // hash rate operating on the fork.
    // We define it this way because it allows us to only store the highest fork tip (+ base) which meets
    // the 7-block condition and from this always have the most-likely-to-cause-warning fork
    if (pfork && (!pindexBestForkTip || (pindexBestForkTip && pindexNewForkTip->nHeight > pindexBestForkTip->nHeight)) &&
        pindexNewForkTip->nChainWork - pfork->nChainWork > (GetBlockProof(*pfork) * 7) &&
        chainActive.Height() - pindexNewForkTip->nHeight < 72) {
        pindexBestForkTip = pindexNewForkTip;
        pindexBestForkBase = pfork;
    }

    CheckForkWarningConditions();
}

// Requires cs_main.
void Misbehaving(NodeId pnode, int howmuch)
{
    if (howmuch == 0)
        return;

    CNodeState* state = State(pnode);
    if (state == NULL)
        return;

    state->nMisbehavior += howmuch;
    int banscore = GetArg("-banscore", 100);
    if (state->nMisbehavior >= banscore && state->nMisbehavior - howmuch < banscore) {
        LogPrintf("Misbehaving: %s (%d -> %d) BAN THRESHOLD EXCEEDED\n", state->name, state->nMisbehavior - howmuch, state->nMisbehavior);
        state->fShouldBan = true;
    } else
        LogPrintf("Misbehaving: %s (%d -> %d)\n", state->name, state->nMisbehavior - howmuch, state->nMisbehavior);
}

void static InvalidChainFound(CBlockIndex* pindexNew)
{
    if (!pindexBestInvalid || pindexNew->nChainWork > pindexBestInvalid->nChainWork)
        pindexBestInvalid = pindexNew;

    LogPrintf("InvalidChainFound: invalid block=%s  height=%d  log2_work=%.8g  date=%s\n",
        pindexNew->GetBlockHash().ToString(), pindexNew->nHeight,
        log(pindexNew->nChainWork.getdouble()) / log(2.0), DateTimeStrFormat("%Y-%m-%d %H:%M:%S",
                                                               pindexNew->GetBlockTime()));
    LogPrintf("InvalidChainFound:  current best=%s  height=%d  log2_work=%.8g  date=%s\n",
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(), log(chainActive.Tip()->nChainWork.getdouble()) / log(2.0),
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()));
    CheckForkWarningConditions();
}

void static InvalidBlockFound(CBlockIndex* pindex, const CValidationState& state)
{
    int nDoS = 0;
    if (state.IsInvalid(nDoS)) {
        std::map<uint256, NodeId>::iterator it = mapBlockSource.find(pindex->GetBlockHash());
        if (it != mapBlockSource.end() && State(it->second)) {
            CBlockReject reject = {state.GetRejectCode(), state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), pindex->GetBlockHash()};
            State(it->second)->rejects.push_back(reject);
            if (nDoS > 0)
                Misbehaving(it->second, nDoS);
        }
    }
    if (!state.CorruptionPossible()) {
        pindex->nStatus |= BLOCK_FAILED_VALID;
        setDirtyBlockIndex.insert(pindex);
        setBlockIndexCandidates.erase(pindex);
        InvalidChainFound(pindex);
    }
}

void UpdateCoins(const CTransaction& tx, CValidationState& state, CCoinsViewCache& inputs, CTxUndo& txundo, int nHeight)
{
    // mark inputs spent
    if (!tx.IsCoinBase()) {
        txundo.vprevout.reserve(tx.vin.size());
        BOOST_FOREACH (const CTxIn& txin, tx.vin) {
            txundo.vprevout.push_back(CTxInUndo());
            bool ret = inputs.ModifyCoins(txin.prevout.hash)->Spend(txin.prevout, txundo.vprevout.back());
            assert(ret);
        }
    }

    // add outputs
    inputs.ModifyCoins(tx.GetHash())->FromTx(tx, nHeight);
}

bool CScriptCheck::operator()()
{
    const CScript& scriptSig = ptxTo->vin[nIn].scriptSig;
    if (!VerifyScript(scriptSig, scriptPubKey, nFlags, CachingTransactionSignatureChecker(ptxTo, nIn, cacheStore), &error)) {
        return ::error("CScriptCheck(): %s:%d VerifySignature failed: %s", ptxTo->GetHash().ToString(), nIn, ScriptErrorString(error));
    }
    return true;
}

bool CheckInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, bool fScriptChecks, unsigned int flags, bool cacheStore, std::vector<CScriptCheck>* pvChecks)
{
    if (!tx.IsCoinBase()) {
        if (pvChecks)
            pvChecks->reserve(tx.vin.size());

        // This doesn't trigger the DoS code on purpose; if it did, it would make it easier
        // for an attacker to attempt to split the network.
        if (!inputs.HaveInputs(tx))
            return state.Invalid(error("CheckInputs() : %s inputs unavailable", tx.GetHash().ToString()));

        // While checking, GetBestBlock() refers to the parent block.
        // This is also true for mempool checks.
        CBlockIndex* pindexPrev = mapBlockIndex.find(inputs.GetBestBlock())->second;
        int nSpendHeight = pindexPrev->nHeight + 1;
        CAmount nValueIn = 0;
        CAmount nFees = 0;
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            const COutPoint& prevout = tx.vin[i].prevout;
            const CCoins* coins = inputs.AccessCoins(prevout.hash);
            assert(coins);

            // If prev is coinbase, check that it's matured
            if (coins->IsCoinBase() || coins->IsCoinStake()) {
                if (nSpendHeight - coins->nHeight < Params().COINBASE_MATURITY())
                    return state.Invalid(
                        error("CheckInputs() : tried to spend coinbase at depth %d, coinstake=%d", nSpendHeight - coins->nHeight, coins->IsCoinStake()),
                        REJECT_INVALID, "bad-txns-premature-spend-of-coinbase");
            }

            // Check for negative or overflow input values
            nValueIn += coins->vout[prevout.n].nValue;
            if (!MoneyRange(coins->vout[prevout.n].nValue) || !MoneyRange(nValueIn))
                return state.DoS(100, error("CheckInputs() : txin values out of range"),
                    REJECT_INVALID, "bad-txns-inputvalues-outofrange");
        }

        if (!tx.IsCoinStake()) {
            if (nValueIn < tx.GetValueOut())
                return state.DoS(100, error("CheckInputs() : %s value in (%s) < value out (%s)",
                                          tx.GetHash().ToString(), FormatMoney(nValueIn), FormatMoney(tx.GetValueOut())),
                    REJECT_INVALID, "bad-txns-in-belowout");

            // Tally transaction fees
            CAmount nTxFee = nValueIn - tx.GetValueOut();
            if (nTxFee < 0)
                return state.DoS(100, error("CheckInputs() : %s nTxFee < 0", tx.GetHash().ToString()),
                    REJECT_INVALID, "bad-txns-fee-negative");
            nFees += nTxFee;
            if (!MoneyRange(nFees))
                return state.DoS(100, error("CheckInputs() : nFees out of range"),
                    REJECT_INVALID, "bad-txns-fee-outofrange");
        }
        // The first loop above does all the inexpensive checks.
        // Only if ALL inputs pass do we perform expensive ECDSA signature checks.
        // Helps prevent CPU exhaustion attacks.

        // Skip ECDSA signature verification when connecting blocks
        // before the last block chain checkpoint. This is safe because block merkle hashes are
        // still computed and checked, and any change will be caught at the next checkpoint.
        if (fScriptChecks) {
            for (unsigned int i = 0; i < tx.vin.size(); i++) {
                const COutPoint& prevout = tx.vin[i].prevout;
                const CCoins* coins = inputs.AccessCoins(prevout.hash);
                assert(coins);

                // Verify signature
                CScriptCheck check(*coins, tx, i, flags, cacheStore);
                if (pvChecks) {
                    pvChecks->push_back(CScriptCheck());
                    check.swap(pvChecks->back());
                } else if (!check()) {
                    if (flags & STANDARD_NOT_MANDATORY_VERIFY_FLAGS) {
                        // Check whether the failure was caused by a
                        // non-mandatory script verification check, such as
                        // non-standard DER encodings or non-null dummy
                        // arguments; if so, don't trigger DoS protection to
                        // avoid splitting the network between upgraded and
                        // non-upgraded nodes.
                        CScriptCheck check(*coins, tx, i,
                            flags & ~STANDARD_NOT_MANDATORY_VERIFY_FLAGS, cacheStore);
                        if (check())
                            return state.Invalid(false, REJECT_NONSTANDARD, strprintf("non-mandatory-script-verify-flag (%s)", ScriptErrorString(check.GetScriptError())));
                    }
                    // Failures of other flags indicate a transaction that is
                    // invalid in new blocks, e.g. a invalid P2SH. We DoS ban
                    // such nodes as they are not following the protocol. That
                    // said during an upgrade careful thought should be taken
                    // as to the correct behavior - we may want to continue
                    // peering with non-upgraded nodes even after a soft-fork
                    // super-majority vote has passed.
                    return state.DoS(100, false, REJECT_INVALID, strprintf("mandatory-script-verify-flag-failed (%s)", ScriptErrorString(check.GetScriptError())));
                }
            }
        }
    }

    return true;
}

bool DisconnectBlock(CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, bool* pfClean)
{
    assert(pindex->GetBlockHash() == view.GetBestBlock());

    if (pfClean)
        *pfClean = false;

    bool fClean = true;

    CBlockUndo blockUndo;
    CDiskBlockPos pos = pindex->GetUndoPos();
    if (pos.IsNull())
        return error("DisconnectBlock() : no undo data available");
    if (!blockUndo.ReadFromDisk(pos, pindex->pprev->GetBlockHash()))
        return error("DisconnectBlock() : failure reading undo data");

    if (blockUndo.vtxundo.size() + 1 != block.vtx.size())
        return error("DisconnectBlock() : block and undo data inconsistent");

    // undo transactions in reverse order
    for (int i = block.vtx.size() - 1; i >= 0; i--) {
        const CTransaction& tx = block.vtx[i];
        uint256 hash = tx.GetHash();

        // Check that all outputs are available and match the outputs in the block itself
        // exactly. Note that transactions with only provably unspendable outputs won't
        // have outputs available even in the block itself, so we handle that case
        // specially with outsEmpty.
        {
            CCoins outsEmpty;
            CCoinsModifier outs = view.ModifyCoins(hash);
            outs->ClearUnspendable();

            CCoins outsBlock(tx, pindex->nHeight);
            // The CCoins serialization does not serialize negative numbers.
            // No network rules currently depend on the version here, so an inconsistency is harmless
            // but it must be corrected before txout nversion ever influences a network rule.
            if (outsBlock.nVersion < 0)
                outs->nVersion = outsBlock.nVersion;
            if (*outs != outsBlock)
                fClean = fClean && error("DisconnectBlock() : added transaction mismatch? database corrupted");

            // remove outputs
            outs->Clear();
        }

        // restore inputs
        if (i > 0) { // not coinbases
            const CTxUndo& txundo = blockUndo.vtxundo[i - 1];
            if (txundo.vprevout.size() != tx.vin.size())
                return error("DisconnectBlock() : transaction and undo data inconsistent - txundo.vprevout.siz=%d tx.vin.siz=%d", txundo.vprevout.size(), tx.vin.size());
            for (unsigned int j = tx.vin.size(); j-- > 0;) {
                const COutPoint& out = tx.vin[j].prevout;
                const CTxInUndo& undo = txundo.vprevout[j];
                CCoinsModifier coins = view.ModifyCoins(out.hash);
                if (undo.nHeight != 0) {
                    // undo data contains height: this is the last output of the prevout tx being spent
                    if (!coins->IsPruned())
                        fClean = fClean && error("DisconnectBlock() : undo data overwriting existing transaction");
                    coins->Clear();
                    coins->fCoinBase = undo.fCoinBase;
                    coins->nHeight = undo.nHeight;
                    coins->nVersion = undo.nVersion;
                } else {
                    if (coins->IsPruned())
                        fClean = fClean && error("DisconnectBlock() : undo data adding output to missing transaction");
                }
                if (coins->IsAvailable(out.n))
                    fClean = fClean && error("DisconnectBlock() : undo data overwriting existing output");
                if (coins->vout.size() < out.n + 1)
                    coins->vout.resize(out.n + 1);
                coins->vout[out.n] = undo.txout;
            }
        }
    }

    // move best block pointer to prevout block
    view.SetBestBlock(pindex->pprev->GetBlockHash());

    if (pfClean) {
        *pfClean = fClean;
        return true;
    } else {
        return fClean;
    }
}

void static FlushBlockFile(bool fFinalize = false)
{
    LOCK(cs_LastBlockFile);

    CDiskBlockPos posOld(nLastBlockFile, 0);

    FILE* fileOld = OpenBlockFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }

    fileOld = OpenUndoFile(posOld);
    if (fileOld) {
        if (fFinalize)
            TruncateFile(fileOld, vinfoBlockFile[nLastBlockFile].nUndoSize);
        FileCommit(fileOld);
        fclose(fileOld);
    }
}

bool FindUndoPos(CValidationState& state, int nFile, CDiskBlockPos& pos, unsigned int nAddSize);

static CCheckQueue<CScriptCheck> scriptcheckqueue(128);

void ThreadScriptCheck()
{
    RenameThread("lux-scriptch");
    scriptcheckqueue.Thread();
}

static int64_t nTimeVerify = 0;
static int64_t nTimeConnect = 0;
static int64_t nTimeIndex = 0;
static int64_t nTimeCallbacks = 0;
static int64_t nTimeTotal = 0;

bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, bool fJustCheck)
{
    AssertLockHeld(cs_main);
    // Check it again in case a previous version let a bad block in
    if (!CheckBlock(block, state, !fJustCheck, !fJustCheck))
        return false;

    // verify that the view's current state corresponds to the previous block
    uint256 hashPrevBlock = pindex->pprev == NULL ? uint256(0) : pindex->pprev->GetBlockHash();
    if (hashPrevBlock != view.GetBestBlock())
        LogPrintf("%s: hashPrev=%s view=%s\n", __func__, hashPrevBlock.ToString().c_str(), view.GetBestBlock().ToString().c_str());
    assert(hashPrevBlock == view.GetBestBlock());

    // Special case for the genesis block, skipping connection of its transactions
    // (its coinbase is unspendable)
    if (block.GetHash() == Params().HashGenesisBlock()) {
        view.SetBestBlock(pindex->GetBlockHash());
        return true;
    }

//    if (pindex->nHeight <= Params().LAST_POW_BLOCK() && block.IsProofOfStake())
//        return state.DoS(100, error("ConnectBlock() : PoS period not active"),
//            REJECT_INVALID, "PoS-early");

    if (pindex->nHeight > Params().LAST_POW_BLOCK() && block.IsProofOfWork())
        return state.DoS(100, error("ConnectBlock() : PoW period ended"),
            REJECT_INVALID, "PoW-ended");

    bool fScriptChecks = pindex->nHeight >= Checkpoints::GetTotalBlocksEstimate();

    // Do not allow blocks that contain transactions which 'overwrite' older transactions,
    // unless those are already completely spent.
    // If such overwrites are allowed, coinbases and transactions depending upon those
    // can be duplicated to remove the ability to spend the first instance -- even after
    // being sent to another address.
    // See BIP30 and http://r6.ca/blog/20120206T005236Z.html for more information.
    // This logic is not necessary for memory pool transactions, as AcceptToMemoryPool
    // already refuses previously-known transaction ids entirely.
    // This rule was originally applied all blocks whose timestamp was after March 15, 2012, 0:00 UTC.
    // Now that the whole chain is irreversibly beyond that time it is applied to all blocks except the
    // two in the chain that violate it. This prevents exploiting the issue against nodes in their
    // initial block download.
    bool fEnforceBIP30 = (!pindex->phashBlock) || // Enforce on CreateNewBlock invocations which don't have a hash.
                         !((pindex->nHeight == 91842 && pindex->GetBlockHash() == uint256("0x00000000000a4d0a398161ffc163c503763b1f4360639393e0e4c8e300e0caec")) ||
                             (pindex->nHeight == 91880 && pindex->GetBlockHash() == uint256("0x00000000000743f190a18c5577a3c2d2a1f610ae9601ac046a38084ccb7cd721")));
    if (fEnforceBIP30) {
        BOOST_FOREACH (const CTransaction& tx, block.vtx) {
            const CCoins* coins = view.AccessCoins(tx.GetHash());
            if (coins && !coins->IsPruned())
                return state.DoS(100, error("ConnectBlock() : tried to overwrite transaction"),
                    REJECT_INVALID, "bad-txns-BIP30");
        }
    }

    // BIP16 didn't become active until Apr 1 2012
    int64_t nBIP16SwitchTime = 1333238400;
    bool fStrictPayToScriptHash = (pindex->GetBlockTime() >= nBIP16SwitchTime);

    unsigned int flags = fStrictPayToScriptHash ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE;

    // Start enforcing the DERSIG (BIP66) rules, for block.nVersion=3 blocks, when 75% of the network has upgraded:
    if (block.nVersion >= 3 && CBlockIndex::IsSuperMajority(3, pindex->pprev, Params().EnforceBlockUpgradeMajority())) {
        flags |= SCRIPT_VERIFY_DERSIG;
    }

    CBlockUndo blockundo;

    CCheckQueueControl<CScriptCheck> control(fScriptChecks && nScriptCheckThreads ? &scriptcheckqueue : NULL);

    int64_t nTimeStart = GetTimeMicros();
    int64_t nFees = 0;
    int nInputs = 0;
    unsigned int nSigOps = 0;
    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos> > vPos;
    vPos.reserve(block.vtx.size());
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    int64_t nValueOut = 0;
    int64_t nValueIn = 0;
    int64_t nStakeReward = 0;
    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        const CTransaction& tx = block.vtx[i];

        nInputs += tx.vin.size();
        nSigOps += GetLegacySigOpCount(tx);
        if (nSigOps > MAX_BLOCK_SIGOPS)
            return state.DoS(100, error("ConnectBlock() : too many sigops"),
                REJECT_INVALID, "bad-blk-sigops");

        if (tx.IsCoinBase()) {
            nValueOut += tx.GetValueOut();
        } else {
            if (!view.HaveInputs(tx))
                return state.DoS(100, error("ConnectBlock() : inputs missing/spent"),
                    REJECT_INVALID, "bad-txns-inputs-missingorspent");

            if (fStrictPayToScriptHash) {
                // Add in sigops done by pay-to-script-hash inputs;
                // this is to prevent a "rogue miner" from creating
                // an incredibly-expensive-to-validate block.
                nSigOps += GetP2SHSigOpCount(tx, view);
                if (nSigOps > MAX_BLOCK_SIGOPS)
                    return state.DoS(100, error("ConnectBlock() : too many sigops"),
                        REJECT_INVALID, "bad-blk-sigops");
            }

            int64_t nTxValueIn = view.GetValueIn(tx);
            int64_t nTxValueOut = tx.GetValueOut();
            nValueIn += nTxValueIn;
            nValueOut += nTxValueOut;
            if (!tx.IsCoinStake())
                nFees += nTxValueIn - nTxValueOut;
            if (tx.IsCoinStake())
                nStakeReward = nTxValueOut - nTxValueIn;

            std::vector<CScriptCheck> vChecks;
            if (!CheckInputs(tx, state, view, fScriptChecks, flags, false, nScriptCheckThreads ? &vChecks : NULL))
                return false;
            control.Add(vChecks);
        }

        CTxUndo undoDummy;
        if (i > 0) {
            blockundo.vtxundo.push_back(CTxUndo());
        }
        UpdateCoins(tx, state, view, i == 0 ? undoDummy : blockundo.vtxundo.back(), pindex->nHeight);

        vPos.push_back(std::make_pair(tx.GetHash(), pos));
        pos.nTxOffset += ::GetSerializeSize(tx, SER_DISK, CLIENT_VERSION);
    }

    // ppcoin: track money supply and mint amount info
    pindex->nMint = nValueOut - nValueIn + nFees;
    pindex->nMoneySupply = (pindex->pprev ? pindex->pprev->nMoneySupply : 0) + nValueOut - nValueIn;

    if (!pblocktree->WriteBlockIndex(CDiskBlockIndex(pindex)))
        return error("Connect() : WriteBlockIndex for pindex failed");

    int64_t nTime1 = GetTimeMicros();
    nTimeConnect += nTime1 - nTimeStart;
    LogPrint("bench", "      - Connect %u transactions: %.2fms (%.3fms/tx, %.3fms/txin) [%.2fs]\n", (unsigned)block.vtx.size(), 0.001 * (nTime1 - nTimeStart), 0.001 * (nTime1 - nTimeStart) / block.vtx.size(), nInputs <= 1 ? 0 : 0.001 * (nTime1 - nTimeStart) / (nInputs - 1), nTimeConnect * 0.000001);

    if (block.IsProofOfWork()) {
        if (/*!IsInitialBlockDownload() && */!IsBlockValueValid(block, GetBlockValue(pindex->pprev->nHeight))) {
            return state.DoS(100,
                             error("ConnectBlock() : %d reward pays too much  (actual=%d vs limit=%d) fee: %d",
                                   pindex->nHeight, block.vtx[0].GetValueOut(), GetBlockValue(pindex->pprev->nHeight), nFees),
                             REJECT_INVALID, "bad-cb-amount");
        }
    }

    if(block.IsProofOfStake()) {
        uint64_t nCoinAge = 1;
        if(!GetCoinAge(block.vtx[1], block.vtx[1].nTime, nCoinAge))
           return error("ConnectBlock() : %s unable to get coin age for coinstake", block.vtx[1].GetHash().ToString());

        int64_t nCalculatedStakeReward = GetProofOfStakeReward(pindex->nHeight, nCoinAge, nFees);
        if (nStakeReward > nCalculatedStakeReward)
            return error("ConnectBlock() : %d coinstake pays too much(actual=%d vs calculated=%d) fee: %d",pindex->nHeight, nStakeReward, nCalculatedStakeReward, nFees);

    }

    if (!control.Wait())
        return state.DoS(100, false);
    int64_t nTime2 = GetTimeMicros();
    nTimeVerify += nTime2 - nTimeStart;
    LogPrint("bench", "    - Verify %u txins: %.2fms (%.3fms/txin) [%.2fs]\n", nInputs - 1, 0.001 * (nTime2 - nTimeStart), nInputs <= 1 ? 0 : 0.001 * (nTime2 - nTimeStart) / (nInputs - 1), nTimeVerify * 0.000001);

    if (fJustCheck)
        return true;

    // Write undo information to disk
    if (pindex->GetUndoPos().IsNull() || !pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
        if (pindex->GetUndoPos().IsNull()) {
            CDiskBlockPos pos;
            if (!FindUndoPos(state, pindex->nFile, pos, ::GetSerializeSize(blockundo, SER_DISK, CLIENT_VERSION) + 40))
                return error("ConnectBlock() : FindUndoPos failed");
            if (!blockundo.WriteToDisk(pos, pindex->pprev->GetBlockHash()))
                return state.Abort("Failed to write undo data");

            // update nUndoPos in block index
            pindex->nUndoPos = pos.nPos;
            pindex->nStatus |= BLOCK_HAVE_UNDO;
        }

        pindex->RaiseValidity(BLOCK_VALID_SCRIPTS);
        setDirtyBlockIndex.insert(pindex);
    }

    if (fTxIndex)
        if (!pblocktree->WriteTxIndex(vPos))
            return state.Abort("Failed to write transaction index");

    // add this block to the view's block chain
    view.SetBestBlock(pindex->GetBlockHash());

    int64_t nTime3 = GetTimeMicros();
    nTimeIndex += nTime3 - nTime2;
    LogPrint("bench", "    - Index writing: %.2fms [%.2fs]\n", 0.001 * (nTime3 - nTime2), nTimeIndex * 0.000001);

    // Watch for changes to the previous coinbase transaction.
    static uint256 hashPrevBestCoinBase;
    g_signals.UpdatedTransaction(hashPrevBestCoinBase);
    hashPrevBestCoinBase = block.vtx[0].GetHash();

    int64_t nTime4 = GetTimeMicros();
    nTimeCallbacks += nTime4 - nTime3;
    LogPrint("bench", "    - Callbacks: %.2fms [%.2fs]\n", 0.001 * (nTime4 - nTime3), nTimeCallbacks * 0.000001);

    return true;
}

enum FlushStateMode {
    FLUSH_STATE_IF_NEEDED,
    FLUSH_STATE_PERIODIC,
    FLUSH_STATE_ALWAYS
};

/**
 * Update the on-disk chain state.
 * The caches and indexes are flushed if either they're too large, forceWrite is set, or
 * fast is not set and it's been a while since the last write.
 */
bool static FlushStateToDisk(CValidationState& state, FlushStateMode mode)
{
    LOCK(cs_main);
    static int64_t nLastWrite = 0;
    try {
        if ((mode == FLUSH_STATE_ALWAYS) ||
            ((mode == FLUSH_STATE_PERIODIC || mode == FLUSH_STATE_IF_NEEDED) && pcoinsTip->GetCacheSize() > nCoinCacheSize) ||
            (mode == FLUSH_STATE_PERIODIC && GetTimeMicros() > nLastWrite + DATABASE_WRITE_INTERVAL * 1000000)) {
            // Typical CCoins structures on disk are around 100 bytes in size.
            // Pushing a new one to the database can cause it to be written
            // twice (once in the log, and once in the tables). Th
