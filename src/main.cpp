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
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
#include "init.h"
#include "stake.h"
#include "masternode.h"
#include "merkleblock.h"
#include "net.h"
#include "policy/fees.h"
#include "policy/policy.h"
#include "pow.h"
#include "primitives/block.h"
#include "primitives/transaction.h"
#include "spork.h"
#include "instantx.h"
#include "script/script.h"
#include "script/sigcache.h"
#include "script/standard.h"
#include "timedata.h"
#include "txdb.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"
#include "versionbits.h"
#include "script/interpreter.h"

#include "univalue/univalue.h"
#include <atomic>
#include <sstream>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/unordered_map.hpp>

#if defined(DEBUG_DUMP_STAKING_INFO)
#  include "DEBUG_DUMP_STAKING_INFO.hpp"
#endif

using namespace boost;
using namespace std;

#if defined(NDEBUG)
#error "LUX cannot be compiled without assertions."
#endif

#ifndef DEBUG_DUMP_STAKING_INFO_AddToBlockIndex
#  define DEBUG_DUMP_STAKING_INFO_AddToBlockIndex() (void)0
#endif

#define MAX_DATA_FLUSH_RETRY 10

const int LAST_HEIGHT_FEE_BLOCK = 180000;

static const bool ENABLE_POS_REWARD_CHANGED = true;

static const int POS_REWARD_CHANGED_BLOCK = 300000;

/**
 * Global LuxState
 */

////////////////////////////// lux
#include <iostream>
#include <bitset>
#include "pubkey.h"

std::unique_ptr<LuxState> globalState;
std::shared_ptr<dev::eth::SealEngineFace> globalSealEngine;
bool fRecordLogOpcodes = false;
bool fIsVMlogFile = false;
bool fGettingValuesDGP = false;


/** The maximum allowed size for a serialized block, in bytes (only for buffer size limits) */
unsigned int dgpMaxBlockSerSize = 8000000;
/** The maximum allowed weight for a block, see BIP 141 (network rule) */
unsigned int dgpMaxBlockWeight = 8000000;
/** The maximum allowed size for a block excluding witness data, in bytes (network rule) */
unsigned int dgpMaxBlockBaseSize = 2000000;

unsigned int dgpMaxBlockSize = 2000000; // qtum

///** The maximum allowed number of signature check operations in a block (network rule) */
int64_t dgpMaxBlockSigOps = 80000;

unsigned int dgpMaxProtoMsgLength = 8000000;

unsigned int dgpMaxTxSigOps = 16000;

void updateBlockSizeParams(unsigned int newBlockSize){
    unsigned int newSizeForParams=WITNESS_SCALE_FACTOR*newBlockSize;
    dgpMaxBlockSerSize=newSizeForParams;
    dgpMaxBlockWeight=newSizeForParams;
    dgpMaxBlockBaseSize=newBlockSize;
    dgpMaxBlockSigOps=(int64_t)(newSizeForParams/100);
    dgpMaxTxSigOps = (unsigned int)(dgpMaxBlockSigOps/5);
    dgpMaxProtoMsgLength=newSizeForParams;
}


//////////////////////////////

CCriticalSection cs_main;

BlockMap mapBlockIndex;
CChain chainActive;
CBlockIndex* pindexBestHeader = NULL;
int64_t nTimeBestReceived = 0;
CWaitableCriticalSection csBestBlock;
CConditionVariable cvBlockChange;
int nScriptCheckThreads = 0;
std::atomic_bool fImporting(false);
std::atomic_bool fReindex(false);
bool fLogEvents = false;
bool fTxIndex = true;
bool fIsBareMultisigStd = true;
bool fRequireStandard = true;
bool fCheckBlockIndex = false;
unsigned int nCoinCacheSize = 5000;
unsigned int nBytesPerSigOp = DEFAULT_BYTES_PER_SIGOP;
bool fAlerts = DEFAULT_ALERTS;

bool fHavePruned = false;
bool fPruneMode = false;
uint64_t nPruneTarget = 0;

uint256 bnProofOfStakeLimit = (~uint256(0) >> 20);
uint256 bnProofOfStakeLimitV2 = (~uint256(0) >> 34);

/** Fees smaller than this (in duffs) are considered zero fee (for relaying and mining)
 * We are ~100 times smaller then bitcoin now (2015-06-23), set minRelayTxFee only 10 times higher
 * so it's still 10 times lower comparing to bitcoin.
 */
CFeeRate minRelayTxFee = CFeeRate(10000);
FeeFilterRounder filterRounder(::minRelayTxFee);

CTxMemPool mempool(::minRelayTxFee);

struct COrphanTx {
    CTransaction tx;
    NodeId fromPeer;
    int64_t nTimeExpire;
};
map<uint256, COrphanTx> mapOrphanTransactions;
map<uint256, set<uint256> > mapOrphanTransactionsByPrev;

void EraseOrphansFor(NodeId peer);

static void CheckBlockIndex(const Consensus::Params& consensusParams);

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

/** Global flag to indicate we should check to see if there are
     *  block/undo files that should be deleted.  Set on startup
     *  or if we allocate more file space when we're in prune mode
     */
    bool fCheckForPruning = false;

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

// int g_outbound_peers_with_protect_from_disconnect = 0;

/** Dirty block index entries. */
set<CBlockIndex*> setDirtyBlockIndex;

/** Dirty block file entries. */
set<int> setDirtyFileInfo;
} // anon namespace

//////////////////////////////////////////////////////////////////////////////
//
// dispatching functions
//

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
    //! Whether this peer can give us witnesses
    bool fHaveWitness;

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
        fHaveWitness = false;
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
    LOCK(cs_main);
    return chainActive.Height();
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
// returns false, still setting pit, if the block was already in flight from the same peer
// pit will only be valid as long as the same cs_main lock is being held
void MarkBlockAsInFlight(NodeId nodeid, const uint256& hash, const Consensus::Params& consensusParams, CBlockIndex* pindex = NULL)
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
        // pindexLastCommonBlock as long as all ancestors are already downloaded, or if it's
        // already part of our chain (and therefore don't need it even if pruned).
        BOOST_FOREACH (CBlockIndex* pindex, vToFetch) {
            if (!pindex->IsValid(BLOCK_VALID_TREE)) {
                // We consider the chain that this peer is on invalid.
                return;
            }
            if (pindex->nStatus & BLOCK_HAVE_DATA || chainActive.Contains(pindex)) {
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
StorageResults *pstorageresult = NULL;

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
    unsigned int sz = GetTransactionCost(tx);
    if (sz >= MAX_STANDARD_TX_COST) {
        LogPrint("mempool", "ignoring large orphan tx (size: %u, hash: %s)\n", sz, hash.ToString());
        return false;
    }

    mapOrphanTransactions.emplace(hash, COrphanTx{tx, peer, GetTime() + ORPHAN_TX_EXPIRE_TIME});
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

    static int64_t nNextSweep;
    int64_t nNow = GetTime();
    if (nNextSweep <= nNow) {
        // Sweep out expired orphan pool entries:
        int nErased = 0;
        int64_t nMinExpTime = nNow + ORPHAN_TX_EXPIRE_TIME - ORPHAN_TX_EXPIRE_INTERVAL;
        map<uint256, COrphanTx>::iterator iter = mapOrphanTransactions.begin();
        while (iter != mapOrphanTransactions.end())
        {
            map<uint256, COrphanTx>::iterator maybeErase = iter++;
            if (maybeErase->second.nTimeExpire <= nNow) {
                EraseOrphanTx(maybeErase->second.tx.GetHash());
                ++nErased;
            } else {
                nMinExpTime = std::min(maybeErase->second.nTimeExpire, nMinExpTime);
            }
        }
        // Sweep again 5 minutes after the next entry that expires in order to batch the linear scan.
        nNextSweep = nMinExpTime + ORPHAN_TX_EXPIRE_INTERVAL;
        if (nErased > 0) LogPrint("mempool", "Erased %d orphan tx due to expiration\n", nErased);
    }

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

int GetIXConfirmations(uint256 nTXHash)
{
    int sigs = 0;
    std::map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(nTXHash);
    if (i != mapTxLocks.end()) {
        sigs = (*i).second.CountSignatures();
    }
    if (sigs >= INSTANTX_SIGNATURES_REQUIRED) {
        return nInstanTXDepth;
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

    const Consensus::Params& consensusParams = Params().GetConsensus();

    CBlockIndex* pindex = NULL;
    BOOST_FOREACH (const CTxIn& txin, tx.vin) {
        // First try finding the previous transaction in database
        CTransaction txPrev;
        uint256 hashBlockPrev;
        if (!GetTransaction(txin.prevout.hash, txPrev, consensusParams, hashBlockPrev, true)) {
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

        if (stake->GetStakeAge(prevblock.nTime) > nTxTime)
            continue; // only count coins meeting min age requirement

        if (nTxTime < prevblock.nTime) {
            LogPrintf("GetCoinAge: Timestamp Violation: txtime less than txPrev.nTime");
            return false; // Transaction timestamp violation
        }

        int64_t nValueIn = txPrev.vout[txin.prevout.n].nValue;
        bnCentSecond += uint256(nValueIn) * (nTxTime - prevblock.nTime);
    }

    uint256 bnCoinDay = bnCentSecond / COIN / (24 * 60 * 60);
    LogPrintf("coin age bnCoinDay=%s\n", bnCoinDay.GetHex());
    nCoinAge = bnCoinDay.GetCompact();
    return true;
}

int64_t GetTransactionSigOpCost(const CTransaction& tx, const CCoinsViewCache& inputs, int flags)
{
    int64_t nSigOps = GetLegacySigOpCount(tx) * WITNESS_SCALE_FACTOR;

    if (tx.IsCoinBase())
        return nSigOps;

    if (flags & SCRIPT_VERIFY_P2SH) {
        nSigOps += GetP2SHSigOpCount(tx, inputs) * WITNESS_SCALE_FACTOR;
    }

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut &prevout = inputs.GetOutputFor(tx.vin[i]);
        nSigOps += CountWitnessSigOps(tx.vin[i].scriptSig, prevout.scriptPubKey, i < tx.wit.vtxinwit.size() ? &tx.wit.vtxinwit[i].scriptWitness : NULL, flags);
    }
    return nSigOps;
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

    // Size limits (this doesn't take the witness into account, as that hasn't been checked for malleability)
    if (::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) > MAX_BLOCK_BASE_SIZE)
        return state.DoS(100, error("CheckTransaction() : size limits failed"),
            REJECT_INVALID, "bad-txns-oversize");

    // Check for negative or overflow output values
    unsigned i = 0;
    CAmount nValueOut = 0;
    for (const CTxOut& txout : tx.vout) {
        //Removed tx out empty in function CheckTransaction , CHECKSEQUENCEVERIFY doesn't have non-empty tx out
        //if (txout.IsEmpty() && !tx.IsCoinBase() && !tx.IsCoinStake())
        //    return state.DoS(100, error("CheckTransaction(): txout empty for user transaction"));
        if (txout.nValue < 0)
            return state.DoS(100, error("%s: tx.vout[%d].nValue negative (%s, empty=%s, coinstake=%s)", __func__, i,
                                        txout.ToString(), (txout.IsEmpty()?"yes":"no"), (tx.IsCoinStake()?"yes":"no")),
                REJECT_INVALID, "bad-txns-vout-negative");
        if (txout.nValue > MAX_MONEY)
            return state.DoS(100, error("CheckTransaction() : txout.nValue too high"),
                REJECT_INVALID, "bad-txns-vout-toolarge");
        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut))
            return state.DoS(100, error("CheckTransaction() : txout total out of range"),
                REJECT_INVALID, "bad-txns-txouttotal-toolarge");

        /////////////////////////////////////////////////////////// // lux
        if (txout.scriptPubKey.HasOpCall() || txout.scriptPubKey.HasOpCreate()) {
            std::vector<valtype> vSolutions;
            txnouttype whichType;
            if (!Solver(txout.scriptPubKey, whichType, vSolutions, true)) {
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-contract-nonstandard");
            }
        }
        ///////////////////////////////////////////////////////////

        i += 1;
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

    CAmount nMinFee = ::minRelayTxFee.GetFee((size_t)nBytes);

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

    const CChainParams& chainparams = Params();
    // Reject transactions with witness before segregated witness activates (override with -prematurewitness)
    bool witnessEnabled = IsWitnessEnabled(chainActive.Tip(), chainparams.GetConsensus());
    if (!GetBoolArg("-prematurewitness", false) && tx.HasWitness() && !witnessEnabled) {
        return state.DoS(0, false, REJECT_NONSTANDARD, "no-witness-yet", true);
    }

    // Rather not work on nonstandard transactions (unless -testnet/-regtest)
    string reason;
    if (fRequireStandard && !IsStandardTx(tx, reason, witnessEnabled))
        return state.DoS(0,
            error("AcceptToMemoryPool : nonstandard transaction: %s", reason),
            REJECT_NONSTANDARD, reason);

    // is it already in the memory pool?
    uint256 hash = tx.GetHash();
    if (pool.exists(hash))
        return false;

    // ----------- INSTANTX transaction scanning -----------

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
        LockPoints lp;
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
#                   if defined(DEBUG_DUMP_STAKING_INFO)&&defined(DEBUG_DUMP_AcceptToMemoryPool)
                    DEBUG_DUMP_AcceptToMemoryPool();
#                   endif
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

            // Only accept BIP68 sequence locked transactions that can be mined in the next
            // block; we don't want our mempool filled up with transactions that can't
            // be mined yet.
            // Must keep pool.cs for this unless we change CheckSequenceLocks to take a
            // CoinsViewCache instead of create its own
            if (!CheckSequenceLocks(tx, STANDARD_LOCKTIME_VERIFY_FLAGS, &lp))
                return state.DoS(0, false, REJECT_NONSTANDARD, "non-BIP68-final");
        }

        // Check for non-standard pay-to-script-hash in inputs
        if (fRequireStandard && !AreInputsStandard(tx, view))
            return error("AcceptToMemoryPool: : nonstandard transaction input");

        // Check for non-standard witness in P2WSH
        if (tx.HasWitness() && fRequireStandard && !IsWitnessStandard(tx, view))
            return state.DoS(0, false, REJECT_NONSTANDARD, "bad-witness-nonstandard", true);

        int64_t nSigOpsCost = GetTransactionSigOpCost(tx, view, STANDARD_SCRIPT_VERIFY_FLAGS);

        bool fSpendsCoinbase = false;
        BOOST_FOREACH(const CTxIn &txin, tx.vin) {
            const CCoins *coins = view.AccessCoins(txin.prevout.hash);
            if (coins->IsCoinBase()) {
                fSpendsCoinbase = true;
                break;
            }
        }

        CAmount inChainInputValue;
        CAmount nValueOut = tx.GetValueOut();
        CAmount nFees = nValueIn - nValueOut;

        dev::u256 txMinGasPrice = 0;

        //////////////////////////////////////////////////////////// // lux
        if(chainActive.Height() >= chainparams.FirstSCBlock() && tx.HasCreateOrCall()) {

            if(!CheckSenderScript(view, tx)){
                return state.DoS(1, false, REJECT_INVALID, "bad-txns-invalid-sender-script");
            }

            LuxDGP luxDGP(globalState.get(), fGettingValuesDGP);
            uint64_t minGasPrice = luxDGP.getMinGasPrice(chainActive.Tip()->nHeight + 1);
            uint64_t blockGasLimit = luxDGP.getBlockGasLimit(chainActive.Tip()->nHeight + 1);
            size_t count = 0;
            for(const CTxOut& o : tx.vout)
                count += o.scriptPubKey.HasOpCreate() || o.scriptPubKey.HasOpCall() ? 1 : 0;
            LuxTxConverter converter(tx, NULL);
            ExtractLuxTX resultConverter;
            if(!converter.extractionLuxTransactions(resultConverter)){
                return state.DoS(100, error("AcceptToMempool(): Contract transaction of the wrong format"), REJECT_INVALID, "bad-tx-bad-contract-format");
            }
            std::vector<LuxTransaction> luxTransactions = resultConverter.first;
            std::vector<EthTransactionParams> luxETP = resultConverter.second;

            dev::u256 sumGas = dev::u256(0);
            dev::u256 gasAllTxs = dev::u256(0);
            for(LuxTransaction luxTransaction : luxTransactions){
                sumGas += luxTransaction.gas() * luxTransaction.gasPrice();

                if(sumGas > dev::u256(INT64_MAX)) {
                    return state.DoS(100, error("AcceptToMempool(): Transaction's gas stipend overflows"), REJECT_INVALID, "bad-tx-gas-stipend-overflow");
                }

                if(sumGas > dev::u256(nFees)) {
                    return state.DoS(100, error("AcceptToMempool(): Transaction fee does not cover the gas stipend"), REJECT_INVALID, "bad-txns-fee-notenough");
                }

                if(txMinGasPrice != 0) {
                    txMinGasPrice = std::min(txMinGasPrice, luxTransaction.gasPrice());
                } else {
                    txMinGasPrice = luxTransaction.gasPrice();
                }
                VersionVM v = luxTransaction.getVersion();
                if(v.format!=0)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution uses unknown version format"), REJECT_INVALID, "bad-tx-version-format");
                if(v.rootVM != 1)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution uses unknown root VM"), REJECT_INVALID, "bad-tx-version-rootvm");
                if(v.vmVersion != 0)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution uses unknown VM version"), REJECT_INVALID, "bad-tx-version-vmversion");
                if(v.flagOptions != 0)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution uses unknown flag options"), REJECT_INVALID, "bad-tx-version-flags");

                //check gas limit is not less than minimum mempool gas limit
                if(luxTransaction.gas() < GetArg("-minmempoolgaslimit", MEMPOOL_MIN_GAS_LIMIT))
                    return state.DoS(100, error("AcceptToMempool(): Contract execution has lower gas limit than allowed to accept into mempool"), REJECT_INVALID, "bad-tx-too-little-mempool-gas");

                //check gas limit is not less than minimum gas limit (unless it is a no-exec tx)
                if(luxTransaction.gas() < MINIMUM_GAS_LIMIT && v.rootVM != 0)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution has lower gas limit than allowed"), REJECT_INVALID, "bad-tx-too-little-gas");

                if(luxTransaction.gas() > UINT32_MAX)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution can not specify greater gas limit than can fit in 32-bits"), REJECT_INVALID, "bad-tx-too-much-gas");

                gasAllTxs += luxTransaction.gas();
                if(gasAllTxs > dev::u256(blockGasLimit))
                    return state.DoS(1, false, REJECT_INVALID, "bad-txns-gas-exceeds-blockgaslimit");

                //don't allow less than DGP set minimum gas price to prevent MPoS greedy mining/spammers
                if(v.rootVM!=0 && (uint64_t)luxTransaction.gasPrice() < minGasPrice)
                return state.DoS(100, error("AcceptToMempool(): Contract execution has lower gas price than allowed"), REJECT_INVALID, "bad-tx-low-gas-price");
            }

            if(!CheckMinGasPrice(luxETP, minGasPrice))
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-small-gasprice");

            if(count > luxTransactions.size())
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-incorrect-format");
        }
        ////////////////////////////////////////////////////////////

        double dPriority = view.GetPriority(tx, chainActive.Height(), inChainInputValue);
        CTxMemPoolEntry entry(MakeTransactionRef(tx), nFees, GetTime(), dPriority, chainActive.Height(), inChainInputValue, fSpendsCoinbase, nSigOpsCost,  lp, pool.HasNoInputsOf(tx),CAmount(txMinGasPrice));

        // Check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. Since the coinbase transaction
        // itself can contain sigops MAX_STANDARD_TX_SIGOPS_COST is less than
        // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
        // merely non-standard transaction.
        unsigned int nSize = entry.GetTxSize();
        /*unsigned int nSigOps = GetLegacySigOpCount(tx);
        nSigOps += GetP2SHSigOpCount(tx, view);*/
        if ((nSigOpsCost > MAX_STANDARD_TX_SIGOPS_COST) || (nBytesPerSigOp && nSigOpsCost > nSize * WITNESS_SCALE_FACTOR / nBytesPerSigOp))
            return state.DoS(0, error("AcceptToMemoryPool : too many sigops %s, %d > %d", hash.ToString(), nSigOpsCost, MAX_STANDARD_TX_SIGOPS_COST),
                             REJECT_NONSTANDARD, "bad-txns-too-many-sigops");

        // Don't accept it if it can't get into a block
        // but prioritise dstx and don't check fees for it
        if (!ignoreFees) {
            CAmount txMinFee = GetMinRelayFee(tx, nSize, true);
            if (fLimitFree && nFees < txMinFee)
                return state.DoS(0, error("AcceptToMemoryPool : not enough fees %s, %d < %d",
                                        hash.ToString(), nFees, txMinFee),
                    REJECT_INSUFFICIENTFEE, "insufficient fee");

            // Require that free transactions have sufficient priority to be mined in the next block.
            if (GetBoolArg("-relaypriority", true) && nFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(entry.GetPriority(chainActive.Height() + 1))) {
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

//        if (fRejectInsaneFee && nFees > ::minRelayTxFee.GetFee(nSize) * 10000)
//            return error("AcceptToMemoryPool: : insane fees %s, %d > %d",
//                hash.ToString(),
//                nFees, ::minRelayTxFee.GetFee(nSize) * 10000);
        unsigned int scriptVerifyFlags = STANDARD_SCRIPT_VERIFY_FLAGS;
        // Check against previous transactions
        // This is done last to help prevent CPU exhaustion denial-of-service attacks.
        PrecomputedTransactionData txdata(tx);
        if (!CheckInputs(tx, state, view, true, STANDARD_SCRIPT_VERIFY_FLAGS, true, txdata)) {
            // SCRIPT_VERIFY_CLEANSTACK requires SCRIPT_VERIFY_WITNESS, so we
            // need to turn both off, and compare against just turning off CLEANSTACK
            // to see if the failure is specifically due to witness validation.
            CValidationState stateDummy; // Want reported failures to be from first CheckInputs
            if (!tx.HasWitness() && CheckInputs(tx, stateDummy, view, true, scriptVerifyFlags & ~(SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_CLEANSTACK), true, txdata) &&
            !CheckInputs(tx, stateDummy, view, true, scriptVerifyFlags & ~SCRIPT_VERIFY_CLEANSTACK, true, txdata)) {
                // Only the witness is missing, so the transaction itself may be fine.
                state.SetCorruptionPossible();
            }
            return error("AcceptToMemoryPool: : CheckInputs failed %s", hash.ToString());
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
        if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true, txdata)) {
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

    // ----------- INSTANTX transaction scanning -----------

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
        LockPoints lp;
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

            // Only accept BIP68 sequence locked transactions that can be mined in the next
            // block; we don't want our mempool filled up with transactions that can't
            // be mined yet.
            // Must keep pool.cs for this unless we change CheckSequenceLocks to take a
            // CoinsViewCache instead of create its own
            if (!CheckSequenceLocks(tx, STANDARD_LOCKTIME_VERIFY_FLAGS, &lp))
                return state.DoS(0, false, REJECT_NONSTANDARD, "non-BIP68-final");
        }

        int64_t nSigOpsCost = GetTransactionSigOpCost(tx, view, STANDARD_SCRIPT_VERIFY_FLAGS);

        bool fSpendsCoinbase = false;
        BOOST_FOREACH(const CTxIn &txin, tx.vin) {
            const CCoins *coins = view.AccessCoins(txin.prevout.hash);
            if (coins->IsCoinBase()) {
                fSpendsCoinbase = true;
                break;
            }
        }

        CAmount inChainInputValue;
        CAmount nValueOut = tx.GetValueOut();
        CAmount nFees = nValueIn - nValueOut;

        dev::u256 txMinGasPrice = 0;

        //////////////////////////////////////////////////////////// // lux
        if(chainActive.Height() >= Params().FirstSCBlock() && tx.HasCreateOrCall()) {

            if(!CheckSenderScript(view, tx)){
                return state.DoS(1, false, REJECT_INVALID, "bad-txns-invalid-sender-script");
            }

            LuxDGP luxDGP(globalState.get(), fGettingValuesDGP);
            uint64_t minGasPrice = luxDGP.getMinGasPrice(chainActive.Tip()->nHeight + 1);
            uint64_t blockGasLimit = luxDGP.getBlockGasLimit(chainActive.Tip()->nHeight + 1);
            size_t count = 0;
            for(const CTxOut& o : tx.vout)
                count += o.scriptPubKey.HasOpCreate() || o.scriptPubKey.HasOpCall() ? 1 : 0;
            LuxTxConverter converter(tx, NULL);
            ExtractLuxTX resultConverter;
            if(!converter.extractionLuxTransactions(resultConverter)){
                return state.DoS(100, error("AcceptToMempool(): Contract transaction of the wrong format"), REJECT_INVALID, "bad-tx-bad-contract-format");
            }
            std::vector<LuxTransaction> luxTransactions = resultConverter.first;
            std::vector<EthTransactionParams> luxETP = resultConverter.second;

            dev::u256 sumGas = dev::u256(0);
            dev::u256 gasAllTxs = dev::u256(0);
            for(LuxTransaction luxTransaction : luxTransactions){
                sumGas += luxTransaction.gas() * luxTransaction.gasPrice();

                if(sumGas > dev::u256(INT64_MAX)) {
                    return state.DoS(100, error("AcceptToMempool(): Transaction's gas stipend overflows"), REJECT_INVALID, "bad-tx-gas-stipend-overflow");
                }

                if(sumGas > dev::u256(nFees)) {
                    return state.DoS(100, error("AcceptToMempool(): Transaction fee does not cover the gas stipend"), REJECT_INVALID, "bad-txns-fee-notenough");
                }

                if(txMinGasPrice != 0) {
                    txMinGasPrice = std::min(txMinGasPrice, luxTransaction.gasPrice());
                } else {
                    txMinGasPrice = luxTransaction.gasPrice();
                }
                VersionVM v = luxTransaction.getVersion();
                if(v.format!=0)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution uses unknown version format"), REJECT_INVALID, "bad-tx-version-format");
                if(v.rootVM != 1)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution uses unknown root VM"), REJECT_INVALID, "bad-tx-version-rootvm");
                if(v.vmVersion != 0)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution uses unknown VM version"), REJECT_INVALID, "bad-tx-version-vmversion");
                if(v.flagOptions != 0)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution uses unknown flag options"), REJECT_INVALID, "bad-tx-version-flags");

                //check gas limit is not less than minimum mempool gas limit
                if(luxTransaction.gas() < GetArg("-minmempoolgaslimit", MEMPOOL_MIN_GAS_LIMIT))
                    return state.DoS(100, error("AcceptToMempool(): Contract execution has lower gas limit than allowed to accept into mempool"), REJECT_INVALID, "bad-tx-too-little-mempool-gas");

                //check gas limit is not less than minimum gas limit (unless it is a no-exec tx)
                if(luxTransaction.gas() < MINIMUM_GAS_LIMIT && v.rootVM != 0)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution has lower gas limit than allowed"), REJECT_INVALID, "bad-tx-too-little-gas");

                if(luxTransaction.gas() > UINT32_MAX)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution can not specify greater gas limit than can fit in 32-bits"), REJECT_INVALID, "bad-tx-too-much-gas");

                gasAllTxs += luxTransaction.gas();
                if(gasAllTxs > dev::u256(blockGasLimit))
                    return state.DoS(1, false, REJECT_INVALID, "bad-txns-gas-exceeds-blockgaslimit");

                //don't allow less than DGP set minimum gas price to prevent MPoS greedy mining/spammers
                if(v.rootVM!=0 && (uint64_t)luxTransaction.gasPrice() < minGasPrice)
                    return state.DoS(100, error("AcceptToMempool(): Contract execution has lower gas price than allowed"), REJECT_INVALID, "bad-tx-low-gas-price");
            }

            if(!CheckMinGasPrice(luxETP, minGasPrice))
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-small-gasprice");

            if(count > luxTransactions.size())
                return state.DoS(100, false, REJECT_INVALID, "bad-txns-incorrect-format");
        }
        ////////////////////////////////////////////////////////////

        double dPriority = view.GetPriority(tx, chainActive.Height(), inChainInputValue);
        CTxMemPoolEntry entry(MakeTransactionRef(tx), nFees, GetTime(), dPriority, chainActive.Height(), inChainInputValue, fSpendsCoinbase, nSigOpsCost,  lp, pool.HasNoInputsOf(tx), CAmount(txMinGasPrice));

        // Check for non-standard pay-to-script-hash in inputs
        // for any real tx this will be checked on AcceptToMemoryPool anyway
        //        if (Params().RequireStandard() && !AreInputsStandard(tx, view))
        //            return error("AcceptableInputs: : nonstandard transaction input");

        // Check that the transaction doesn't have an excessive number of
        // sigops, making it impossible to mine. Since the coinbase transaction
        // itself can contain sigops MAX_STANDARD_TX_SIGOPS_COST is less than
        // MAX_BLOCK_SIGOPS; we still consider this an invalid rather than
        // merely non-standard transaction.
        unsigned int nSize = entry.GetTxSize();
        /*unsigned int nSigOps = GetLegacySigOpCount(tx);
        nSigOps += GetP2SHSigOpCount(tx, view);*/
        if ((nSigOpsCost > MAX_STANDARD_TX_SIGOPS_COST) || (nBytesPerSigOp && nSigOpsCost > nSize * WITNESS_SCALE_FACTOR / nBytesPerSigOp))
            return state.DoS(0,
                error("AcceptableInputs : too many sigops %s, %d > %d",
                    hash.ToString(), nSigOpsCost, MAX_STANDARD_TX_SIGOPS_COST),
                REJECT_NONSTANDARD, "bad-txns-too-many-sigops");

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
            if (GetBoolArg("-relaypriority", true) && nFees < ::minRelayTxFee.GetFee(nSize) && !AllowFree(entry.GetPriority(chainActive.Height() + 1))) {
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
        unsigned int scriptVerifyFlags = STANDARD_SCRIPT_VERIFY_FLAGS;
        PrecomputedTransactionData txdata(tx);
        if (!CheckInputs(tx, state, view, false, STANDARD_SCRIPT_VERIFY_FLAGS, true, txdata)) {
            // SCRIPT_VERIFY_CLEANSTACK requires SCRIPT_VERIFY_WITNESS, so we
            // need to turn both off, and compare against just turning off CLEANSTACK
            // to see if the failure is specifically due to witness validation.
            CValidationState stateDummy; // Want reported failures to be from first CheckInputs
            if (!tx.HasWitness() && CheckInputs(tx, stateDummy, view, true, scriptVerifyFlags & ~(SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_CLEANSTACK), true, txdata) &&
                !CheckInputs(tx, stateDummy, view, true, scriptVerifyFlags & ~SCRIPT_VERIFY_CLEANSTACK, true, txdata)) {
                // Only the witness is missing, so the transaction itself may be fine.
                state.SetCorruptionPossible();
            }
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
bool GetTransaction(const uint256& hash, CTransaction& txOut, const Consensus::Params& consensusParams, uint256& hashBlock, bool fAllowSlow)
{
    CBlockIndex* pindexSlow = NULL;

    LOCK(cs_main);

    CTransactionRef ptx = mempool.get(hash);
    if (ptx)
    {
        txOut = *ptx.get();
        return true;
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
            } catch (const std::exception& e) {
                return error("%s: Deserialize or I/O error - %s", __func__, e.what());
            }

            bool usePhi2 = false;
            BlockMap::iterator mi = mapBlockIndex.find(header.hashPrevBlock);
            if (mi != mapBlockIndex.end()) {
                usePhi2 = mi->second->nHeight + 1 >= Params().SwitchPhi2Block();
            }

            hashBlock = header.GetHash(usePhi2);
            if (txOut.GetHash() != hash)
                return error("%s: txid mismatch", __func__);
            return true;
        }

        return false;
    }

    if (fAllowSlow) { // use coin database to locate block that contains transaction, and scan it
        int nHeight = -1;
        {
            const CCoinsViewCache& view = *pcoinsTip;
            const CCoins* coins = view.AccessCoins(hash);
            if (coins)
                nHeight = coins->nHeight;
        }
        if (nHeight > 0)
            pindexSlow = chainActive[nHeight];
    }

    if (pindexSlow) {
        CBlock block;
        if (ReadBlockFromDisk(block, pindexSlow, consensusParams)) {
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

bool WriteBlockToDisk(const CBlock& block, CDiskBlockPos& pos)
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

bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos, int nHeight, const Consensus::Params& consensusParams)
{
    block.SetNull();

    // Open history file to read
    CAutoFile filein(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("ReadBlockFromDisk : OpenBlockFile failed");

    // Read block
    try {
        filein >> block;
    } catch (const std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    // Check the header
    if (block.IsProofOfWork()) {
        if (!CheckProofOfWork(block.GetHash(nHeight >= Params().SwitchPhi2Block()), block.nBits, consensusParams))
            return error("ReadBlockFromDisk : Errors in block header");
    } else {
        // TODO: CheckProofOfStake(block, ...)
    }

    return true;
}

bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& consensusParams) {
    if (!ReadBlockFromDisk(block, pindex->GetBlockPos(), pindex->nHeight, consensusParams))
        return false;
    //both phi1612 and phi2 hashes do not match indexed hash
    if (block.GetHash() != pindex->GetBlockHash() && block.GetHash(true) != pindex->GetBlockHash()) {
        LogPrintf("%s : block=%s index=%s\n", __func__, block.GetHash().GetHex(), pindex->GetBlockHash().GetHex());
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

CAmount GetProofOfWorkReward(int64_t nFees, int nHeight)
{
    if (Params().NetworkID() == CBaseChainParams::TESTNET) {
        if (nHeight < 200) return 250000 * COIN;
    }

    CAmount nSubsidy = 1 * COIN;
    if (nHeight < 1) {
        nSubsidy = 1 * COIN;
    } else if (nHeight == 1) {
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

    if (nHeight < LAST_HEIGHT_FEE_BLOCK) {
        nFees = nHeight;
    }
    return nSubsidy + nFees;
}

CAmount GetProofOfStakeReward(int64_t nCoinAge, int64_t nFees, int nHeight)
{
    CAmount nSubsidy = STATIC_POS_REWARD;

    // First 100,000 blocks double stake for masternode ready
    if (nHeight < 100000) {
        nSubsidy = 2 * COIN;
    } else {
        nSubsidy = 1 * COIN;
    }

    return nSubsidy + nFees;
}

CAmount GetMasternodePayment(int nHeight, int64_t blockValue, int nMasternodeCount)
{
    int64_t ret = blockValue * 0.4;
    if (ENABLE_POS_REWARD_CHANGED && (mapArgs.count("-testnet") || nHeight >= POS_REWARD_CHANGED_BLOCK)) {
        ret = blockValue * 0.4/2; //20% for masternode
    } else {
        ret = blockValue * 0.4; //40% for masternode
    }
    return ret;
}

bool IsInitialBlockDownload()
{
    const CChainParams& chainParams = Params();
    LOCK(cs_main);
    if (fImporting || fReindex || chainActive.Height() < Checkpoints::GetTotalBlocksEstimate(chainParams.Checkpoints()))
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

// Compute at which vout of the block's coinbase transaction the witness
// commitment occurs, or -1 if not found.
static int GetWitnessCommitmentIndex(const CBlock& block)
{
    int commitpos = -1;
    if (!block.vtx.empty()) {
        for (size_t o = 0; o < block.vtx[0].vout.size(); o++) {
            if (block.vtx[0].vout[o].scriptPubKey.size() >= 38 && block.vtx[0].vout[o].scriptPubKey[0] == OP_RETURN && block.vtx[0].vout[o].scriptPubKey[1] == 0x24 && block.vtx[0].vout[o].scriptPubKey[2] == 0xaa && block.vtx[0].vout[o].scriptPubKey[3] == 0x21 && block.vtx[0].vout[o].scriptPubKey[4] == 0xa9 && block.vtx[0].vout[o].scriptPubKey[5] == 0xed) {
                commitpos = o;
            }
        }
    }
    return commitpos;
}

void static InvalidBlockFound(CBlockIndex* pindex, const CValidationState& state)
{
    int nDoS = 0;
    if (state.IsInvalid(nDoS)) {
        std::map<uint256, NodeId>::iterator it = mapBlockSource.find(pindex->GetBlockHash());
        if (it != mapBlockSource.end() && State(it->second)) {
            assert (state.GetRejectCode() < REJECT_INTERNAL); // Blocks are never rejected with internal reject codes
            CBlockReject reject = {static_cast<unsigned char>(state.GetRejectCode()), state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), pindex->GetBlockHash()};
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
    const CScriptWitness *witness = (nIn < ptxTo->wit.vtxinwit.size()) ? &ptxTo->wit.vtxinwit[nIn].scriptWitness : nullptr;
    if (!VerifyScript(scriptSig, scriptPubKey, witness, nFlags, CachingTransactionSignatureChecker(ptxTo, nIn, amount, cacheStore, *txdata), &error)) {
        return ::error("CScriptCheck(): %s:%d VerifySignature failed: %s", ptxTo->GetHash().ToString(), nIn, ScriptErrorString(error));
    }
    return true;
}

bool CheckInputs(const CTransaction& tx, CValidationState& state, const CCoinsViewCache& inputs, bool fScriptChecks, unsigned int flags, bool cacheStore, PrecomputedTransactionData& txdata, std::vector<CScriptCheck>* pvChecks)
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

        BlockMap::iterator mi = mapBlockIndex.find(inputs.GetBestBlock());
        if (mi == mapBlockIndex.end())
        {
            LogPrintf("%s (line %d): Cannot find best block\n", __FUNCTION__, __LINE__);
            return state.Invalid(error("Cannot find best block"));
        }

        CBlockIndex* pindexPrev = mi->second;
        if (!pindexPrev)
        {
            LogPrintf("%s (line %d): Cannot find the parent of current best block\n", __FUNCTION__, __LINE__);
            return state.Invalid(error("Cannot find the parent of current best block"));
        }

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
                CScriptCheck check(*coins, tx, i, flags, cacheStore, &txdata);
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
                        CScriptCheck check2(*coins, tx, i,
                            flags & ~STANDARD_NOT_MANDATORY_VERIFY_FLAGS, cacheStore, &txdata);
                        if (check2())
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
//#if 0
      if (pindex->nHeight >= Params().FirstSCBlock()) {
        globalState->setRoot(uintToh256(pindex->pprev->hashStateRoot)); // lux
        globalState->setRootUTXO(uintToh256(pindex->pprev->hashUTXORoot)); // lux

        if (pfClean == NULL && fLogEvents) {
            pstorageresult->deleteResults(block.vtx);
            //pblocktree->EraseHeightIndex(pindex->nHeight);
        }
   }
//#endif
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

static bool IsBlockValueValid(const CBlock& block, int64_t nExpectedValue)
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
        LogPrintf("%s: WARNING: Couldn't find previous block", __func__);
    }

    return block.vtx[0].GetValueOut() <= nExpectedValue;
}

// Protected by cs_main
VersionBitsCache versionbitscache;

int32_t ComputeBlockVersion(const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    LOCK(cs_main);
    int32_t nVersion = VERSIONBITS_TOP_BITS;

    for (int i = 0; i < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; i++) {
        ThresholdState state = VersionBitsState(pindexPrev, params, (Consensus::DeploymentPos)i, versionbitscache);
        if ((Consensus::DeploymentPos)i == Consensus::SMART_CONTRACTS_HARDFORK) {
            int nextHeight = pindexPrev != nullptr ? pindexPrev->nHeight + 1 : 1;
            if (nextHeight >= Params().FirstSCBlock()) {
                nVersion |= VersionBitsMask(params, (Consensus::DeploymentPos)i);
            }
        } else {
            if (state == THRESHOLD_LOCKED_IN || state == THRESHOLD_STARTED) {
                nVersion |= VersionBitsMask(params, (Consensus::DeploymentPos)i);
            }
        }
    }

    return nVersion;
}

/**
 * Threshold condition checker that triggers when unknown versionbits are seen on the network.
 */
class WarningBitsConditionChecker : public AbstractThresholdConditionChecker
{
private:
    int bit;

public:
    WarningBitsConditionChecker(int bitIn) : bit(bitIn) {}

    int64_t BeginTime(const Consensus::Params& params) const { return 0; }
    int64_t EndTime(const Consensus::Params& params) const { return std::numeric_limits<int64_t>::max(); }
    int Period(const Consensus::Params& params) const { return params.nMinerConfirmationWindow; }
    int Threshold(const Consensus::Params& params) const { return params.nRuleChangeActivationThreshold; }

    bool Condition(const CBlockIndex* pindex, const Consensus::Params& params) const
    {
        return ((pindex->nVersion & VERSIONBITS_TOP_MASK) == VERSIONBITS_TOP_BITS) &&
               ((pindex->nVersion >> bit) & 1) != 0 &&
               ((ComputeBlockVersion(pindex->pprev, params) >> bit) & 1) == 0;
    }
};

ThresholdState VersionBitsTipState(const Consensus::Params& params, Consensus::DeploymentPos pos)
{
    LOCK(cs_main);
    return VersionBitsState(chainActive.Tip(), params, pos, versionbitscache);
}

// Protected by cs_main
static ThresholdConditionCache warningcache[VERSIONBITS_NUM_BITS];

static int64_t nTimeVerify = 0;
static int64_t nTimeConnect = 0;
static int64_t nTimeIndex = 0;
static int64_t nTimeCallbacks = 0;
static int64_t nTimeTotal = 0;

bool ConnectBlock(const CBlock& block, CValidationState& state, CBlockIndex* pindex, CCoinsViewCache& view, const CChainParams& chainparams, bool fJustCheck)
{
    AssertLockHeld(cs_main);

    ///////////////////////////////////////////////// // lux
//#if 0
    LuxDGP luxDGP(globalState.get(), fGettingValuesDGP);
    globalSealEngine->setLuxSchedule(luxDGP.getGasSchedule(pindex->nHeight + 1));
//#endif
    uint64_t minGasPrice = 0;//luxDGP.getMinGasPrice(pindex->nHeight + 1);
    uint64_t blockGasLimit = DEFAULT_BLOCK_GAS_LIMIT_DGP;//luxDGP.getBlockGasLimit(pindex->nHeight + 1);

#if 0
    uint32_t sizeBlockDGP = 0;//luxDGP.getBlockSize(pindex->nHeight + 1);
    dgpMaxBlockSize = sizeBlockDGP ? sizeBlockDGP : dgpMaxBlockSize;
    updateBlockSizeParams(dgpMaxBlockSize);
#endif
    std::vector<CTxOut> checkVouts;
    uint64_t countCumulativeGasUsed = 0;

    CBlock checkBlock(block.GetBlockHeader());
    /////////////////////////////////////////////////

    // Check it again in case a previous version let a bad block in
    if (!CheckBlock(block, state, chainparams.GetConsensus(), !fJustCheck, !fJustCheck))
        return false;

    // verify that the view's current state corresponds to the previous block
    uint256 hashPrevBlock;
    if (pindex->pprev) {
        hashPrevBlock = pindex->pprev->GetBlockHash();
    }
    if (hashPrevBlock != view.GetBestBlock()) {
        LogPrintf("%s: block=%s,%d prev=%s best=%s\n", __func__, pindex->GetBlockHash().GetHex(), pindex->nHeight, view.GetBestBlock().GetHex(), hashPrevBlock.GetHex());
        return error("%s: previous block not best", __func__);
    }

    assert(hashPrevBlock == view.GetBestBlock());

    // Special case for the genesis block, skipping connection of its transactions
    // (its coinbase is unspendable)
    //Genesis block's hash cannot be calculated using PHI2, so no need to check PHI2 block hash
    if (block.GetHash() == chainparams.GetConsensus().hashGenesisBlock) {
        view.SetBestBlock(pindex->GetBlockHash());
        return true;
    }

//    if (pindex->nHeight <= Params().LAST_POW_BLOCK() && block.IsProofOfStake())
//        return state.DoS(100, error("%s: PoS period not active", __func__),
//            REJECT_INVALID, "PoS-early");

    if (pindex->nHeight > Params().LAST_POW_BLOCK() && block.IsProofOfWork())
        return state.DoS(100, error("%s: PoW period ended", __func__),
            REJECT_INVALID, "PoW-ended");

    bool fScriptChecks = pindex->nHeight >= Checkpoints::GetTotalBlocksEstimate(chainparams.Checkpoints());

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
                return state.DoS(100, error("%s: tried to overwrite transaction", __func__),
                    REJECT_INVALID, "bad-txns-BIP30");
        }
    }

    // Start enforcing BIP68 (sequence locks) and BIP112 (CHECKSEQUENCEVERIFY) using versionbits logic.
    int nLockTimeFlags = 0;
    if (VersionBitsState(pindex->pprev, chainparams.GetConsensus(), Consensus::DEPLOYMENT_CSV, versionbitscache) == THRESHOLD_ACTIVE) {
        nLockTimeFlags |= LOCKTIME_VERIFY_SEQUENCE;
    }

    // BIP16 didn't become active until Apr 1 2012
    int64_t nBIP16SwitchTime = 1333238400;
    bool fStrictPayToScriptHash = (pindex->GetBlockTime() >= nBIP16SwitchTime);

    unsigned int flags = fStrictPayToScriptHash ? SCRIPT_VERIFY_P2SH : SCRIPT_VERIFY_NONE;

    // Start enforcing the DERSIG (BIP66) rules, for block.nVersion=3 blocks, when 75% of the network has upgraded:
    if (block.nVersion >= 3 && CBlockIndex::IsSuperMajority(3, pindex->pprev, chainparams.EnforceBlockUpgradeMajority(), chainparams.GetConsensus())) {
        flags |= SCRIPT_VERIFY_DERSIG;
    }

    // Start enforcing WITNESS rules using versionbits logic.
    if (IsWitnessEnabled(pindex->pprev, chainparams.GetConsensus())) {
        flags |= SCRIPT_VERIFY_WITNESS;
    }

    CBlockUndo blockundo;

    CCheckQueueControl<CScriptCheck> control(fScriptChecks && nScriptCheckThreads ? &scriptcheckqueue : NULL);

    ///////////////////////////////////////////////////////// // lux
    std::map<dev::Address, std::pair<CHeightTxIndexKey, std::vector<uint256>>> heightIndexes;
    /////////////////////////////////////////////////////////

    int64_t nTimeStart = GetTimeMicros();
    CAmount nFees = 0;
    int nInputs = 0;
    int64_t nSigOpsCost = 0;
    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos> > vPos;
    vPos.reserve(block.vtx.size());
    blockundo.vtxundo.reserve(block.vtx.size() - 1);
    int64_t nValueOut = 0;
    int64_t nValueIn = 0;
    int64_t nStakeReward = 0;
    std::vector<PrecomputedTransactionData> txdata;
    txdata.reserve(block.vtx.size()); // Required so that pointers to individual PrecomputedTransactionData don't get invalidated
    uint64_t blockGasUsed = 0;
    CAmount gasRefunds=0;

    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        const CTransaction& tx = block.vtx[i];

        nInputs += tx.vin.size();

        // GetTransactionSigOpCost counts 3 types of sigops:
        // * legacy (always)
        // * p2sh (when P2SH enabled in flags and excludes coinbase)
        // * witness (when witness enabled in flags and excludes coinbase)
        nSigOpsCost += GetTransactionSigOpCost(tx, view, flags);
        if (nSigOpsCost > MAX_BLOCK_SIGOPS_COST)
            return state.DoS(100, error("%s: too many sigops", __func__),
                REJECT_INVALID, "bad-blk-sigops");

        txdata.emplace_back(tx);

        if (!tx.IsCoinBase()) {
            if (!view.HaveInputs(tx))
                return state.DoS(100, error("%s: inputs missing/spent", __func__),
                    REJECT_INVALID, "bad-txns-inputs-missingorspent");

            if (fStrictPayToScriptHash) {
                // Add in sigops done by pay-to-script-hash inputs;
                // this is to prevent a "rogue miner" from creating
                // an incredibly-expensive-to-validate block.
                nSigOpsCost += GetP2SHSigOpCount(tx, view);
                if (nSigOpsCost > MAX_BLOCK_SIGOPS)
                    return state.DoS(100, error("%s: too many sigops", __func__),
                        REJECT_INVALID, "bad-blk-sigops");
            }

            nValueIn += view.GetValueIn(tx);
            if (tx.IsCoinStake()) {
                nStakeReward = view.GetValueIn(tx) - tx.GetValueOut();
            } else {
                nFees += view.GetValueIn(tx) - tx.GetValueOut();
            }

            std::vector<CScriptCheck> vChecks;
            if (!CheckInputs(tx, state, view, fScriptChecks, flags, false, txdata[i], nScriptCheckThreads ? &vChecks : NULL))
                return false;
            control.Add(vChecks);
        } else {
            nValueOut += tx.GetValueOut();
        }

///////////////////////////////////////////////////////////////////////////////////////// lux
        if (pindex->nHeight >= Params().FirstSCBlock()) {
            bool hasOpSpend = tx.HasOpSpend();

            if(!hasOpSpend) {
                checkBlock.vtx.push_back(block.vtx[i]);
            }
            if(tx.HasCreateOrCall() && !hasOpSpend) {
                if(!CheckSenderScript(view, tx)) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-txns-invalid-sender-script");
                }

                LuxTxConverter convert(tx, &view, &block.vtx);

                ExtractLuxTX resultConvertLuxTX;
                if(!convert.extractionLuxTransactions(resultConvertLuxTX)) {
                    return state.DoS(100, error("ConnectBlock(): Contract transaction of the wrong format"), REJECT_INVALID, "bad-tx-bad-contract-format");
                }
                if(!CheckMinGasPrice(resultConvertLuxTX.second, minGasPrice))
                    return state.DoS(100, error("ConnectBlock(): Contract execution has lower gas price than allowed"), REJECT_INVALID, "bad-tx-low-gas-price");

                dev::u256 gasAllTxs = dev::u256(0);
                ByteCodeExec exec(block, resultConvertLuxTX.first, blockGasLimit);
                //validate VM version and other ETH params before execution
                //Reject anything unknown (could be changed later by DGP)
                //TODO evaluate if this should be relaxed for soft-fork purposes
                bool nonZeroVersion = false;
                dev::u256 sumGas = dev::u256(0);
                CAmount nTxFee = view.GetValueIn(tx) - tx.GetValueOut();
                for(LuxTransaction& ltx : resultConvertLuxTX.first) {
                    sumGas += ltx.gas() * ltx.gasPrice();

                    if(sumGas > dev::u256(INT64_MAX)) {
                        return state.DoS(100, error("ConnectBlock(): Transaction's gas stipend overflows"), REJECT_INVALID, "bad-tx-gas-stipend-overflow");
                    }

                    if(sumGas > dev::u256(nTxFee)) {
                        return state.DoS(100, error("ConnectBlock(): Transaction fee does not cover the gas stipend"), REJECT_INVALID, "bad-txns-fee-notenough");
                    }

                    VersionVM v = ltx.getVersion();
                    if(v.format != 0)
                        return state.DoS(100, error("ConnectBlock(): Contract execution uses unknown version format"), REJECT_INVALID, "bad-tx-version-format");
                    if(v.rootVM != 0) {
                        nonZeroVersion=true;
                    } else {
                        if(nonZeroVersion) {
                            //If an output is version 0, then do not allow any other versions in the same tx
                            return state.DoS(100, error("ConnectBlock(): Contract tx has mixed version 0 and non-0 VM executions"), REJECT_INVALID, "bad-tx-mixed-zero-versions");
                        }
                    }
                    if(!(v.rootVM == 0 || v.rootVM == 1))
                        return state.DoS(100, error("ConnectBlock(): Contract execution uses unknown root VM"), REJECT_INVALID, "bad-tx-version-rootvm");
                    if(v.vmVersion != 0)
                        return state.DoS(100, error("ConnectBlock(): Contract execution uses unknown VM version"), REJECT_INVALID, "bad-tx-version-vmversion");
                    if(v.flagOptions != 0)
                        return state.DoS(100, error("ConnectBlock(): Contract execution uses unknown flag options"), REJECT_INVALID, "bad-tx-version-flags");

                    //check gas limit is not less than minimum gas limit (unless it is a no-exec tx)
                    if(ltx.gas() < MINIMUM_GAS_LIMIT && v.rootVM != 0)
                        return state.DoS(100, error("ConnectBlock(): Contract execution has lower gas limit than allowed"), REJECT_INVALID, "bad-tx-too-little-gas");

                    if(ltx.gas() > UINT32_MAX)
                        return state.DoS(100, error("ConnectBlock(): Contract execution can not specify greater gas limit than can fit in 32-bits"), REJECT_INVALID, "bad-tx-too-much-gas");

                    gasAllTxs += ltx.gas();
                    if(gasAllTxs > dev::u256(blockGasLimit))
                        return state.DoS(1, false, REJECT_INVALID, "bad-txns-gas-exceeds-blockgaslimit");

                    //don't allow less than DGP set minimum gas price to prevent MPoS greedy mining/spammers
                    if(v.rootVM != 0 && (uint64_t)ltx.gasPrice() < minGasPrice)
                        return state.DoS(100, error("ConnectBlock(): Contract execution has lower gas price than allowed"), REJECT_INVALID, "bad-tx-low-gas-price");
                }

                if(!nonZeroVersion){
                    //if tx is 0 version, then the tx must already have been added by a previous contract execution
                    if(!tx.HasOpSpend()){
                        return state.DoS(100, error("ConnectBlock(): Version 0 contract executions are not allowed unless created by the AAL "), REJECT_INVALID, "bad-tx-improper-version-0");
                    }
                }

                if(!exec.performByteCode()){
                    return state.DoS(100, error("ConnectBlock(): Unknown error during contract execution"), REJECT_INVALID, "bad-tx-unknown-error");
                }

                std::vector<ResultExecute> resultExec(exec.getResult());
                ByteCodeExecResult bcer;
                if(!exec.processingResults(bcer)){
                    return state.DoS(100, error("ConnectBlock(): Error processing VM execution results"), REJECT_INVALID, "bad-vm-exec-processing");
                }

                countCumulativeGasUsed += bcer.usedGas;
                std::vector<TransactionReceiptInfo> tri;
                if (fLogEvents && !fJustCheck)
                {
                    for(size_t k = 0; k < resultConvertLuxTX.first.size(); k ++){
                        dev::Address key = resultExec[k].execRes.newAddress;
                        if(!heightIndexes.count(key)){
                            heightIndexes[key].first = CHeightTxIndexKey(pindex->nHeight, resultExec[k].execRes.newAddress);
                        }
                        heightIndexes[key].second.push_back(tx.GetHash());
                        tri.push_back(TransactionReceiptInfo{block.GetHash(pindex->nHeight >= Params().SwitchPhi2Block()), uint32_t(pindex->nHeight), tx.GetHash(), uint32_t(i), resultConvertLuxTX.first[k].from(), resultConvertLuxTX.first[k].to(),
                                                             countCumulativeGasUsed, uint64_t(resultExec[k].execRes.gasUsed), resultExec[k].execRes.newAddress, resultExec[k].txRec.log(), resultExec[k].execRes.excepted});
                    }

                    pstorageresult->addResult(uintToh256(tx.GetHash()), tri);
                }

                blockGasUsed += bcer.usedGas;
                if(blockGasUsed > blockGasLimit){
                    return state.DoS(1000, error("ConnectBlock(): Block exceeds gas limit"), REJECT_INVALID, "bad-blk-gaslimit");
                }
                for(CTxOut refundVout : bcer.refundOutputs){
                    gasRefunds += refundVout.nValue;
                }
                checkVouts.insert(checkVouts.end(), bcer.refundOutputs.begin(), bcer.refundOutputs.end());
                for(CTransaction& t : bcer.valueTransfers){
                    checkBlock.vtx.push_back(std::move(t));
                }
                if(fRecordLogOpcodes && !fJustCheck){
                    writeVMlog(resultExec, tx, block);
                }

                for(ResultExecute& re: resultExec){
                    if(re.execRes.newAddress != dev::Address() && !fJustCheck)
                        dev::g_logPost(std::string("Address : " + re.execRes.newAddress.hex()), NULL);
                }
            }
        }
/////////////////////////////////////////////////////////////////////////////////////////

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
        return error("%s: WriteBlockIndex failed\n", __func__, pindex->ToString());

    int64_t nTime1 = GetTimeMicros();
    nTimeConnect += nTime1 - nTimeStart;
    LogPrint("bench", "      - Connect %u transactions: %.2fms (%.3fms/tx, %.3fms/txin) [%.2fs]\n", (unsigned)block.vtx.size(), 0.001 * (nTime1 - nTimeStart), 0.001 * (nTime1 - nTimeStart) / block.vtx.size(), nInputs <= 1 ? 0 : 0.001 * (nTime1 - nTimeStart) / (nInputs - 1), nTimeConnect * 0.000001);

    if (block.IsProofOfWork()) {
        auto nReward = GetProofOfWorkReward(nFees, pindex->nHeight/*pindex->pprev->nHeight*/);
        if (!IsInitialBlockDownload() && !IsBlockValueValid(block, nReward)) {
            return state.DoS(100, error("%s: reward pays too much (actual=%d vs limit=%d) (nHeight=%d, nFees=%d)", __func__,
                                        block.vtx[0].GetValueOut(), nReward, pindex->nHeight, nFees),
                             REJECT_INVALID, "bad-cb-amount");
        }
    }

    if (block.IsProofOfStake()) {
        uint64_t nCoinAge;
        const CTransaction &tx = block.vtx[1];
        if (!GetCoinAge(tx, tx.nTime, nCoinAge))
            return error("%s: %s unable to get coin age for coinstake", __func__, tx.GetHash().GetHex());

        int64_t nCalculatedStakeReward = GetProofOfStakeReward(nCoinAge, nFees, pindex->nHeight);
        if (nStakeReward > nCalculatedStakeReward)
            return error("%s: coinstake pays too much(actual=%d vs calculated=%d)", __func__, nStakeReward, nCalculatedStakeReward);
    }

    if (!control.Wait())
        return state.DoS(100, false);

    int64_t nTime2 = GetTimeMicros();
    nTimeVerify += nTime2 - nTimeStart;
    LogPrint("bench", "    - Verify %u txins: %.2fms (%.3fms/txin) [%.2fs]\n", nInputs - 1, 0.001 * (nTime2 - nTimeStart), nInputs <= 1 ? 0 : 0.001 * (nTime2 - nTimeStart) / (nInputs - 1), nTimeVerify * 0.000001);

    ////////////////////////////////////////////////////////////////// // lux
    if (pindex->nHeight >= Params().FirstSCBlock()) {
        checkBlock.hashMerkleRoot = checkBlock.BuildMerkleTree();
        checkBlock.hashStateRoot = h256Touint(globalState->rootHash());
        checkBlock.hashUTXORoot = h256Touint(globalState->rootHashUTXO());

        bool usePhi2 = pindex->nHeight >= Params().SwitchPhi2Block();
        //If this error happens, it probably means that something with AAL created transactions didn't match up to what is expected
        if ((checkBlock.GetHash(usePhi2) != block.GetHash(usePhi2)) && !fJustCheck) {
            LogPrintf("Actual block data does not match block expected by AAL\n");
            //Something went wrong with AAL, compare different elements and determine what the problem is
            if (checkBlock.hashMerkleRoot != block.hashMerkleRoot) {
                //there is a mismatched tx, so go through and determine which txs
                if (block.vtx.size() > checkBlock.vtx.size()) {
                    LogPrintf("Unexpected AAL transactions in block. Actual txs: %i, expected txs: %i\n",
                              block.vtx.size(), checkBlock.vtx.size());
                    for (size_t i = 0; i < block.vtx.size(); i++) {
                        if (i > checkBlock.vtx.size()) {
                            LogPrintf("Unexpected transaction: %s\n", block.vtx[i].ToString());
                        } else {
                            if (block.vtx[i].GetHash() != block.vtx[i].GetHash()) {
                                LogPrintf("Mismatched transaction at entry %i\n", i);
                                LogPrintf("Actual: %s\n", block.vtx[i].ToString());
                                LogPrintf("Expected: %s\n", checkBlock.vtx[i].ToString());
                            }
                        }
                    }
                } else if (block.vtx.size() < checkBlock.vtx.size()) {
                    LogPrintf("Actual block is missing AAL transactions. Actual txs: %i, expected txs: %i\n",
                              block.vtx.size(), checkBlock.vtx.size());
                    for (size_t i = 0; i < checkBlock.vtx.size(); i++) {
                        if (i > block.vtx.size()) {
                            LogPrintf("Missing transaction: %s\n", checkBlock.vtx[i].ToString());
                        } else {
                            if (block.vtx[i].GetHash() != block.vtx[i].GetHash()) {
                                LogPrintf("Mismatched transaction at entry %i\n", i);
                                LogPrintf("Actual: %s\n", block.vtx[i].ToString());
                                LogPrintf("Expected: %s\n", checkBlock.vtx[i].ToString());
                            }
                        }
                    }
                } else {
                    //count is correct, but a tx is wrong
                    for (size_t i = 0; i < checkBlock.vtx.size(); i++) {
                        if (block.vtx[i].GetHash() != block.vtx[i].GetHash()) {
                            LogPrintf("Mismatched transaction at entry %i\n", i);
                            LogPrintf("Actual: %s\n", block.vtx[i].ToString());
                            LogPrintf("Expected: %s\n", checkBlock.vtx[i].ToString());
                        }
                    }
                }
            }

            if (checkBlock.hashUTXORoot != block.hashUTXORoot) {
                LogPrintf("Actual block data does not match hashUTXORoot expected by AAL block\n");
            }
            if (checkBlock.hashStateRoot != block.hashStateRoot) {
                LogPrintf("Actual block data does not match hashStateRoot expected by AAL block\n");
            }

            return state.DoS(100,
                             error("ConnectBlock(): Incorrect AAL transactions or hashes (hashStateRoot, hashUTXORoot)"),
                             REJECT_INVALID, "incorrect-transactions-or-hashes-block");
        }
    }

    if (fJustCheck) {
        if (pindex->nHeight >= Params().FirstSCBlock()) {
            dev::h256 prevHashStateRoot(dev::sha3(dev::rlp("")));
            dev::h256 prevHashUTXORoot(dev::sha3(dev::rlp("")));
            if (pindex->pprev->hashStateRoot != uint256() && pindex->pprev->hashUTXORoot != uint256()) {
                prevHashStateRoot = uintToh256(pindex->pprev->hashStateRoot);
                prevHashUTXORoot = uintToh256(pindex->pprev->hashUTXORoot);
            }
            globalState->setRoot(prevHashStateRoot);
            globalState->setRootUTXO(prevHashUTXORoot);
        }
        return true;
    }
//////////////////////////////////////////////////////////////////

    // Write undo information to disk
    if (pindex->GetUndoPos().IsNull() || !pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
        if (pindex->GetUndoPos().IsNull()) {
            CDiskBlockPos pos;
            if (!FindUndoPos(state, pindex->nFile, pos, ::GetSerializeSize(blockundo, SER_DISK, CLIENT_VERSION) + 40))
                return error("%s: FindUndoPos failed", __func__);
            if (!blockundo.WriteToDisk(pos, pindex->pprev->GetBlockHash()))
                return state.Error("Failed to write undo data");

            // update nUndoPos in block index
            pindex->nUndoPos = pos.nPos;
            pindex->nStatus |= BLOCK_HAVE_UNDO;
        }

        pindex->RaiseValidity(BLOCK_VALID_SCRIPTS);
        setDirtyBlockIndex.insert(pindex);
    }

    if (fTxIndex)
        if (!pblocktree->WriteTxIndex(vPos))
            return state.Error("Failed to write transaction index");

    // add this block to the view's block chain
    view.SetBestBlock(pindex->GetBlockHash());

    int64_t nTime3 = GetTimeMicros();
    nTimeIndex += nTime3 - nTime2;
    LogPrint("bench", "    - Index writing: %.2fms [%.2fs]\n", 0.001 * (nTime3 - nTime2), nTimeIndex * 0.000001);

    // Watch for changes to the previous coinbase transaction.
    static uint256 hashPrevBestCoinBase;
    GetMainSignals().UpdatedTransaction(hashPrevBestCoinBase);
    hashPrevBestCoinBase = block.vtx[0].GetHash();

    int64_t nTime4 = GetTimeMicros();
    nTimeCallbacks += nTime4 - nTime3;
    LogPrint("bench", "    - Callbacks: %.2fms [%.2fs]\n", 0.001 * (nTime4 - nTime3), nTimeCallbacks * 0.000001);

    return true;
}

enum FlushStateMode {
    FLUSH_STATE_NONE,
    FLUSH_STATE_IF_NEEDED,
    FLUSH_STATE_PERIODIC,
    FLUSH_STATE_ALWAYS
};


static bool FlushStateToDisk(CValidationState &state, FlushStateMode mode, int nManualPruneHeight=0);
static void FindFilesToPruneManual(std::set<int>& setFilesToPrune, int nManualPruneHeight);

/**
 * Update the on-disk chain state.
 * The caches and indexes are flushed if either they're too large, forceWrite is set, or
 * fast is not set and it's been a while since the last write.
 */
static bool FlushStateToDisk(CValidationState& state, FlushStateMode mode, int nManualPruneHeight)
{
    LOCK2(cs_main, cs_LastBlockFile);
    static int64_t nLastWrite = 0;
    int retries = MAX_DATA_FLUSH_RETRY;
    string strErr = "";

    while (retries > 0)
    {
        bool isExceptionOccured = false;
        std::set<int> setFilesToPrune;
        bool fFlushForPrune = false;
        try
        {
            if (fPruneMode && (fCheckForPruning || nManualPruneHeight > 0) && !fReindex)
            {
                if (nManualPruneHeight > 0) {
                    FindFilesToPruneManual(setFilesToPrune, nManualPruneHeight);
                } else {
                    FindFilesToPrune(setFilesToPrune, Params().PruneAfterHeight());
                    fCheckForPruning = false;
                }

                if (!setFilesToPrune.empty()) {
                    fFlushForPrune = true;
                    if (!fHavePruned) {
                        pblocktree->WriteFlag("prunedblockfiles", true);
                        fHavePruned = true;
                    }
                }
            }
		    if ((mode == FLUSH_STATE_ALWAYS) ||
		        ((mode == FLUSH_STATE_PERIODIC || mode == FLUSH_STATE_IF_NEEDED) && pcoinsTip->GetCacheSize() > nCoinCacheSize) ||
		        (mode == FLUSH_STATE_PERIODIC && GetTimeMicros() > nLastWrite + DATABASE_WRITE_INTERVAL * 1000000)) {
		        // Typical CCoins structures on disk are around 100 bytes in size.
		        // Pushing a new one to the database can cause it to be written
		        // twice (once in the log, and once in the tables). This is already
		        // an overestimation, as most will delete an existing entry or
		        // overwrite one. Still, use a conservative safety factor of 2.
		        if (!CheckDiskSpace(100 * 2 * 2 * pcoinsTip->GetCacheSize()))
		            return state.Error("out of disk space");
		        // First make sure all block and undo data is flushed to disk.
		        FlushBlockFile();
		        // Then update all block file information (which may refer to block and undo files).
		        bool fileschanged = false;
		        for (set<int>::iterator it = setDirtyFileInfo.begin(); it != setDirtyFileInfo.end();) {
		            if (!pblocktree->WriteBlockFileInfo(*it, vinfoBlockFile[*it])) {
		                return state.Error("Failed to write to block index");
		            }
		            fileschanged = true;
		            setDirtyFileInfo.erase(it++);
		        }
		        if (fileschanged && !pblocktree->WriteLastBlockFile(nLastBlockFile)) {
		            return state.Error("Failed to write to block index");
		        }
		        for (set<CBlockIndex*>::iterator it = setDirtyBlockIndex.begin(); it != setDirtyBlockIndex.end();) {
		            if (!pblocktree->WriteBlockIndex(CDiskBlockIndex(*it))) {
		                return state.Error("Failed to write to block index");
		            }
		            setDirtyBlockIndex.erase(it++);
		        }
		        pblocktree->Sync();
		        // Finally flush the chainstate (which may refer to block index entries).
		        if (!pcoinsTip->Flush())
		            return state.Error("Failed to write to coin database");

                // Finally remove any pruned files
                if (fFlushForPrune) {
                    UnlinkPrunedFiles(setFilesToPrune);
                    fCheckForPruning = false;
                }

		        // Update best block in wallet (so we can detect restored wallets).
		        if (mode != FLUSH_STATE_IF_NEEDED) {
		            GetMainSignals().SetBestChain(chainActive.GetLocator());
		        }
		        nLastWrite = GetTimeMicros();
		    }
		} catch (const std::runtime_error& e) {
		    isExceptionOccured = true;

		    strErr = strprintf("System error while flushing: %s", e.what());
		    LogPrintf("%s\n", strErr);
		}

		if (!isExceptionOccured)
		{
			return true;
		}
		retries--;
    }

    return state.Error(strErr);
}

void FlushStateToDisk()
{
    CValidationState state;
    FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
}

void PruneAndFlush() {
    CValidationState state;
    fCheckForPruning = true;
    FlushStateToDisk(state, FLUSH_STATE_NONE);
}

/** Update chainActive and related internal data structures. */
void static UpdateTip(CBlockIndex* pindexNew, const CChainParams& chainParams)
{
    chainActive.SetTip(pindexNew);

    // New best block
    nTimeBestReceived = GetTime();
    mempool.AddTransactionsUpdated(1);

    LogPrintf("UpdateTip: new best=%s  height=%d  log2_work=%.8g  tx=%lu  date=%s progress=%f  cache=%u\n",
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(), log(chainActive.Tip()->nChainWork.getdouble()) / log(2.0), (unsigned long)chainActive.Tip()->nChainTx,
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
        Checkpoints::GuessVerificationProgress(chainParams.Checkpoints(), chainActive.Tip()), (unsigned int)pcoinsTip->GetCacheSize());

    cvBlockChange.notify_all();

    // Check the version of the last 100 blocks to see if we need to upgrade:
    static bool fWarned = false;
    std::vector<std::string> warningMessages;
    if (!IsInitialBlockDownload() && !fWarned) {
        int nUpgraded = 0;
        const CBlockIndex* pindex = chainActive.Tip();
        for (int bit = 0; bit < VERSIONBITS_NUM_BITS; bit++) {
            WarningBitsConditionChecker checker(bit);
            ThresholdState state = checker.GetStateFor(pindex, chainParams.GetConsensus(), warningcache[bit]);
            if (state == THRESHOLD_ACTIVE || state == THRESHOLD_LOCKED_IN) {
                if (state == THRESHOLD_ACTIVE) {
                    strMiscWarning = strprintf(_("Warning: unknown new rules activated (versionbit %i)"), bit);
                    if (!fWarned) {
                        CAlert::Notify(strMiscWarning, true);
                        fWarned = true;
                    }
                } else {
                    warningMessages.push_back(strprintf("unknown new rules are about to activate (versionbit %i)", bit));
                }
            }
        }
        // Check the version of the last 100 blocks to see if we need to upgrade:
        for (int i = 0; i < 100 && pindex != NULL; i++)
        {
            int32_t nExpectedVersion = ComputeBlockVersion(pindex->pprev, chainParams.GetConsensus());
            if (pindex->nVersion > VERSIONBITS_LAST_OLD_BLOCK_VERSION && (pindex->nVersion & ~nExpectedVersion) != 0)
                ++nUpgraded;
            pindex = pindex->pprev;
        }
        if (nUpgraded > 0)
            warningMessages.push_back(strprintf("%d of last 100 blocks have unexpected version", nUpgraded));
        if (nUpgraded > 100/2)
        {
            // strMiscWarning is read by GetWarnings(), called by Qt and the JSON-RPC code to warn the user:
            strMiscWarning = _("Warning: This version is obsolete, upgrade required!");
            CAlert::Notify(strMiscWarning, true);
            fWarned = true;
        }
    }
}

/** Disconnect chainActive's tip. */
static bool DisconnectTip(CValidationState& state, const CChainParams& chainparams)
{
    CBlockIndex* pindexDelete = chainActive.Tip();
    assert(pindexDelete);
    mempool.check(pcoinsTip);
    // Read block from disk.
    CBlock block;
    if (!ReadBlockFromDisk(block, pindexDelete, chainparams.GetConsensus()))
        return state.Error("Failed to read block");
    // Apply the block atomically to the chain state.
    int64_t nStart = GetTimeMicros();
    {
        CCoinsViewCache view(pcoinsTip);
        if (!DisconnectBlock(block, state, pindexDelete, view))
            return error("DisconnectTip() : DisconnectBlock %s failed", pindexDelete->GetBlockHash().ToString());
        assert(view.Flush());
    }
    LogPrint("bench", "- Disconnect block: %.2fms\n", (GetTimeMicros() - nStart) * 0.001);
    // Write the chain state to disk, if necessary.
    if (!FlushStateToDisk(state, FLUSH_STATE_ALWAYS))
        return false;
    // Resurrect mempool transactions from the disconnected block.
    std::vector<uint256> vHashUpdate;
    BOOST_FOREACH (const CTransaction& tx, block.vtx) {
        // ignore validation errors in resurrected transactions
        CValidationState stateDummy;
        if (tx.IsCoinBase() || tx.IsCoinStake() || !AcceptToMemoryPool(mempool, stateDummy, tx, false, NULL)) {
            mempool.removeRecursive(tx, MemPoolRemovalReason::REORG);
        } else if (mempool.exists(tx.GetHash())) {
            vHashUpdate.push_back(tx.GetHash());
        }
    }
        // AcceptToMemoryPool/addUnchecked all assume that new mempool entries have
        // no in-mempool children, which is generally not true when adding
        // previously-confirmed transactions back to the mempool.
        // UpdateTransactionsFromBlock finds descendants of any transactions in this
        // block that were added back and cleans up the mempool state.
    mempool.UpdateTransactionsFromBlock(vHashUpdate);
    mempool.check(pcoinsTip);
    // Update chainActive and related variables.
    UpdateTip(pindexDelete->pprev, chainparams);
    // Let wallets know transactions went from 1-confirmed to
    // 0-confirmed or conflicted:
    BOOST_FOREACH (const CTransaction& tx, block.vtx) {
        SyncWithWallets(tx, NULL);
    }
    return true;
}

static int64_t nTimeReadFromDisk = 0;
static int64_t nTimeConnectTotal = 0;
static int64_t nTimeFlush = 0;
static int64_t nTimeChainState = 0;
static int64_t nTimePostConnect = 0;

/**
 * Connect a new block to chainActive. pblock is either NULL or a pointer to a CBlock
 * corresponding to pindexNew, to bypass loading it again from disk.
 */
bool static ConnectTip(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexNew, const CBlock* pblock)
{
    assert(pindexNew->pprev == chainActive.Tip());
    mempool.check(pcoinsTip);
    CCoinsViewCache view(pcoinsTip);

    // Read block from disk.
    int64_t nTime1 = GetTimeMicros();
    CBlock block;
    if (!pblock) {
        if (!ReadBlockFromDisk(block, pindexNew, chainparams.GetConsensus()))
            return state.Error("Failed to read block");
        pblock = &block;
    }
    // Apply the block atomically to the chain state.
    int64_t nTime2 = GetTimeMicros();
    nTimeReadFromDisk += nTime2 - nTime1;
    int64_t nTime3;
    LogPrint("bench", "  - Load block from disk: %.2fms [%.2fs]\n", (nTime2 - nTime1) * 0.001, nTimeReadFromDisk * 0.000001);
    {
        CInv inv(MSG_BLOCK, pindexNew->GetBlockHash());


        dev::h256 oldHashStateRoot(globalState->rootHash()); // lux
        dev::h256 oldHashUTXORoot(globalState->rootHashUTXO()); // lux

        bool rv = ConnectBlock(*pblock, state, pindexNew, view, chainparams);
        GetMainSignals().BlockChecked(*pblock, state);
        if (!rv) {
            if (state.IsInvalid())
                InvalidBlockFound(pindexNew, state);

            globalState->setRoot(oldHashStateRoot); // lux
            globalState->setRootUTXO(oldHashUTXORoot); // lux

            return error("ConnectTip() : ConnectBlock %s failed", pindexNew->GetBlockHash().ToString());
        }
        mapBlockSource.erase(inv.hash);
        nTime3 = GetTimeMicros();
        nTimeConnectTotal += nTime3 - nTime2;
        LogPrint("bench", "  - Connect total: %.2fms [%.2fs]\n", (nTime3 - nTime2) * 0.001, nTimeConnectTotal * 0.000001);
        assert(view.Flush());
    }
    int64_t nTime4 = GetTimeMicros();
    nTimeFlush += nTime4 - nTime3;
    LogPrint("bench", "  - Flush: %.2fms [%.2fs]\n", (nTime4 - nTime3) * 0.001, nTimeFlush * 0.000001);

    // Write the chain state to disk, if necessary. Always write to disk if this is the first of a new file.
    FlushStateMode flushMode = FLUSH_STATE_IF_NEEDED;
    if (pindexNew->pprev && (pindexNew->GetBlockPos().nFile != pindexNew->pprev->GetBlockPos().nFile))
        flushMode = FLUSH_STATE_ALWAYS;
    if (!FlushStateToDisk(state, flushMode))
        return false;
    int64_t nTime5 = GetTimeMicros();
    nTimeChainState += nTime5 - nTime4;
    LogPrint("bench", "  - Writing chainstate: %.2fms [%.2fs]\n", (nTime5 - nTime4) * 0.001, nTimeChainState * 0.000001);

    // Remove conflicting transactions from the mempool.
    list<CTransaction> txConflicted;
    mempool.removeForBlock(pblock->vtx, pindexNew->nHeight/*, txConflicted, !IsInitialBlockDownload()*/);
    mempool.check(pcoinsTip);
    // Update chainActive & related variables.
    UpdateTip(pindexNew, chainparams);
    // Tell wallet about transactions that went from mempool
    // to conflicted:
    BOOST_FOREACH (const CTransaction& tx, txConflicted) {
        SyncWithWallets(tx, NULL);
    }
    // ... and about transactions that got confirmed:
    BOOST_FOREACH (const CTransaction& tx, pblock->vtx) {
        SyncWithWallets(tx, pblock);
    }

    int64_t nTime6 = GetTimeMicros();
    nTimePostConnect += nTime6 - nTime5;
    nTimeTotal += nTime6 - nTime1;
    LogPrint("bench", "  - Connect postprocess: %.2fms [%.2fs]\n", (nTime6 - nTime5) * 0.001, nTimePostConnect * 0.000001);
    LogPrint("bench", "- Connect block: %.2fms [%.2fs]\n", (nTime6 - nTime1) * 0.001, nTimeTotal * 0.000001);
    return true;
}

bool DisconnectBlocksAndReprocess(int blocks)
{
    LOCK(cs_main);

    const CChainParams& chainParams = Params();

    CValidationState state;

    LogPrintf("DisconnectBlocksAndReprocess: Got command to replay %d blocks\n", blocks);
    for (int i = 0; i <= blocks; i++)
        DisconnectTip(state, chainParams);

    return true;
}

/*
    DisconnectBlockAndInputs

    Remove conflicting blocks for successful InstanTX transaction locks
    This should be very rare (Probably will never happen)
*/
// ***TODO*** clean up here
//bool DisconnectBlockAndInputs(CValidationState& state, CTransaction txLock)
//{
//    // All modifications to the coin state will be done in this cache.
//    // Only when all have succeeded, we push it to pcoinsTip.
//    //    CCoinsViewCache view(*pcoinsTip, true);
//
//    const CChainParams& chainParams = Params();
//
//    CBlockIndex* BlockReading = chainActive.Tip();
//    CBlockIndex* pindexNew = NULL;
//
//    bool foundConflictingTx = false;
//
//    //remove anything conflicting in the memory pool
//    list<CTransaction> txConflicted;
//    mempool.removeConflicts(txLock, txConflicted);
//
//
//    // List of what to disconnect (typically nothing)
//    vector<CBlockIndex*> vDisconnect;
//
//    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0 && !foundConflictingTx && i < 6; i++) {
//        vDisconnect.push_back(BlockReading);
//        pindexNew = BlockReading->pprev; //new best block
//
//        CBlock block;
//        if (!ReadBlockFromDisk(block, BlockReading, chainParams.GetConsensus()))
//            return state.Error(_("Failed to read block"));
//
//        // Queue memory transactions to resurrect.
//        // We only do this for blocks after the last checkpoint (reorganisation before that
//        // point should only happen with -reindex/-loadblock, or a misbehaving peer.
//        BOOST_FOREACH (const CTransaction& tx, block.vtx) {
//            if (!tx.IsCoinBase()) {
//                BOOST_FOREACH (const CTxIn& in1, txLock.vin) {
//                    BOOST_FOREACH (const CTxIn& in2, tx.vin) {
//                        if (in1.prevout == in2.prevout) foundConflictingTx = true;
//                    }
//                }
//            }
//        }
//
//        if (BlockReading->pprev == NULL) {
//            assert(BlockReading);
//            break;
//        }
//        BlockReading = BlockReading->pprev;
//    }
//
//    if (!foundConflictingTx) {
//        LogPrintf("DisconnectBlockAndInputs: Can't find a conflicting transaction to inputs\n");
//        return false;
//    }
//
//    if (vDisconnect.size() > 0) {
//        LogPrintf("REORGANIZE: Disconnect Conflicting Blocks %lli blocks; %s..\n", vDisconnect.size(), pindexNew->GetBlockHash().ToString());
//        BOOST_FOREACH (CBlockIndex* pindex, vDisconnect) {
//            LogPrintf(" -- disconnect %s\n", pindex->GetBlockHash().ToString());
//            DisconnectTip(state, chainParams);
//        }
//    }
//
//    return true;
//}


/**
 * Return the tip of the chain with the most work in it, that isn't
 * known to be invalid (it's however far from certain to be valid).
 */
static CBlockIndex* FindMostWorkChain()
{
    do {
        CBlockIndex* pindexNew = NULL;

        // Find the best candidate header.
        {
            std::set<CBlockIndex*, CBlockIndexWorkComparator>::reverse_iterator it = setBlockIndexCandidates.rbegin();
            if (it == setBlockIndexCandidates.rend())
                return NULL;
            pindexNew = *it;
        }

        // Check whether all blocks on the path between the currently active chain and the candidate are valid.
        // Just going until the active chain is an optimization, as we know all blocks in it are valid already.
        CBlockIndex* pindexTest = pindexNew;
        bool fInvalidAncestor = false;
        while (pindexTest && !chainActive.Contains(pindexTest)) {
            assert(pindexTest->nChainTx || pindexTest->nHeight == 0);

            // Pruned nodes may have entries in setBlockIndexCandidates for
            // which block files have been deleted.  Remove those as candidates
            // for the most work chain if we come across them; we can't switch
            // to a chain unless we have all the non-active-chain parent blocks.
            bool fFailedChain = pindexTest->nStatus & BLOCK_FAILED_MASK;
            bool fMissingData = !(pindexTest->nStatus & BLOCK_HAVE_DATA);
            if (fFailedChain || fMissingData) {
                // Candidate chain is not usable (either invalid or missing data)
                if (fFailedChain && (pindexBestInvalid == NULL || pindexNew->nChainWork > pindexBestInvalid->nChainWork))
                    pindexBestInvalid = pindexNew;
                CBlockIndex* pindexFailed = pindexNew;
                // Remove the entire chain from the set.
                while (pindexTest != pindexFailed) {
                    if (fFailedChain) {
                        pindexFailed->nStatus |= BLOCK_FAILED_CHILD;
                    } else if (fMissingData) {
                        // If we're missing data, then add back to mapBlocksUnlinked,
                        // so that if the block arrives in the future we can try adding
                        // to setBlockIndexCandidates again.
                        mapBlocksUnlinked.insert(std::make_pair(pindexFailed->pprev, pindexFailed));
                    }
                    setBlockIndexCandidates.erase(pindexFailed);
                    pindexFailed = pindexFailed->pprev;
                }
                setBlockIndexCandidates.erase(pindexTest);
                fInvalidAncestor = true;
                break;
            }
            pindexTest = pindexTest->pprev;
        }
        if (!fInvalidAncestor)
            return pindexNew;
    } while (true);
}

/** Delete all entries in setBlockIndexCandidates that are worse than the current tip. */
static void PruneBlockIndexCandidates()
{
    // Note that we can't delete the current block itself, as we may need to return to it later in case a
    // reorganization to a better block fails.
    std::set<CBlockIndex*, CBlockIndexWorkComparator>::iterator it = setBlockIndexCandidates.begin();
    while (it != setBlockIndexCandidates.end() && setBlockIndexCandidates.value_comp()(*it, chainActive.Tip())) {
        setBlockIndexCandidates.erase(it++);
    }
    // Either the current tip or a successor of it we're working towards is left in setBlockIndexCandidates.
    assert(!setBlockIndexCandidates.empty());
}

/**
 * Try to make some progress towards making pindexMostWork the active block.
 * pblock is either NULL or a pointer to a CBlock corresponding to pindexMostWork.
 */
static bool ActivateBestChainStep(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindexMostWork, const CBlock* pblock)
{
    AssertLockHeld(cs_main);
    bool fInvalidFound = false;
    const CBlockIndex* pindexOldTip = chainActive.Tip();
    const CBlockIndex* pindexFork = chainActive.FindFork(pindexMostWork);

    // Disconnect active blocks which are no longer in the best chain.
    while (chainActive.Tip() && chainActive.Tip() != pindexFork) {
        if (!DisconnectTip(state, chainparams))
            return false;
    }

    // Build list of new blocks to connect.
    std::vector<CBlockIndex*> vpindexToConnect;
    bool fContinue = true;
    int nHeight = pindexFork ? pindexFork->nHeight : -1;
    while (fContinue && nHeight != pindexMostWork->nHeight) {
        // Don't iterate the entire list of potential improvements toward the best tip, as we likely only need
        // a few blocks along the way.
        int nTargetHeight = std::min(nHeight + 32, pindexMostWork->nHeight);
        vpindexToConnect.clear();
        vpindexToConnect.reserve(nTargetHeight - nHeight);
        CBlockIndex* pindexIter = pindexMostWork->GetAncestor(nTargetHeight);
        while (pindexIter && pindexIter->nHeight != nHeight) {
            vpindexToConnect.push_back(pindexIter);
            pindexIter = pindexIter->pprev;
        }
        nHeight = nTargetHeight;

        // Connect new blocks.
        BOOST_REVERSE_FOREACH (CBlockIndex* pindexConnect, vpindexToConnect) {
            if (!ConnectTip(state, chainparams, pindexConnect, pindexConnect == pindexMostWork ? pblock : NULL)) {
                if (state.IsInvalid()) {
                    // The block violates a consensus rule.
                    if (!state.CorruptionPossible())
                        InvalidChainFound(vpindexToConnect.back());
                    state = CValidationState();
                    fInvalidFound = true;
                    fContinue = false;
                    break;
                } else {
                    // A system error occurred (disk space, database error, ...).
                    return false;
                }
            } else {
                PruneBlockIndexCandidates();
                if (!pindexOldTip || chainActive.Tip()->nChainWork > pindexOldTip->nChainWork) {
                    // We're in a better position than we were. Return temporarily to release the lock.
                    fContinue = false;
                    break;
                }
            }
        }
    }

    // Callbacks/notifications for a new best chain.
    if (fInvalidFound)
        CheckForkWarningConditionsOnNewFork(vpindexToConnect.back());
    else
        CheckForkWarningConditions();

    return true;
}

/**
 * Make the best chain active, in multiple steps. The result is either failure
 * or an activated best chain. pblock is either NULL or a pointer to a block
 * that is already loaded (to avoid loading it again from disk).
 */
bool ActivateBestChain(CValidationState& state, const CChainParams& chainparams, const CBlock* pblock)
{
    CBlockIndex* pindexNewTip = NULL;
    CBlockIndex* pindexMostWork = NULL;
    do {
        boost::this_thread::interruption_point();

        bool fInitialDownload;
        bool usePhi2 = false;
        while (true) {
            TRY_LOCK(cs_main, lockMain);
            if (!lockMain) {
                MilliSleep(50);
                continue;
            }

            pindexMostWork = FindMostWorkChain();

            // Whether we have anything to do at all.
            if (pindexMostWork == NULL || pindexMostWork == chainActive.Tip())
                return true;

            //Active chain's tip will be updated after ActivateBestChainStep, when block is added, so add 1 to active height
            usePhi2 = chainActive.Height() + 1 >= Params().SwitchPhi2Block();

            if (!ActivateBestChainStep(state, chainparams, pindexMostWork, pblock && pblock->GetHash(usePhi2) == pindexMostWork->GetBlockHash() ? pblock : NULL))
                return false;

            pindexNewTip = chainActive.Tip();
            fInitialDownload = IsInitialBlockDownload();
            break;
        }
        // When we reach this point, we switched to a new tip (stored in pindexNewTip).

        // Notifications/callbacks that can run without cs_main
        if (!fInitialDownload) {
            uint256 hashNewTip = pindexNewTip->GetBlockHash();
            // Relay inventory, but don't relay old inventory during initial block download.
            int nBlockEstimate = Checkpoints::GetTotalBlocksEstimate(chainparams.Checkpoints());
            if (nLocalServices & NODE_NETWORK) {
                LOCK(cs_vNodes);
                BOOST_FOREACH (CNode* pnode, vNodes)
                    if (chainActive.Height() > (pnode->nStartingHeight != -1 ? pnode->nStartingHeight - 2000 : nBlockEstimate))
                        pnode->PushInventory(CInv(MSG_BLOCK, hashNewTip));
            }
            // Notify external listeners about the new tip.
            uiInterface.NotifyBlockTip(hashNewTip);
        }
    } while (pindexMostWork != chainActive.Tip());

    CheckBlockIndex(chainparams.GetConsensus());

    // Write changes periodically to disk, after relay.
    if (!FlushStateToDisk(state, FLUSH_STATE_PERIODIC)) {
        return false;
    }

    return true;
}

bool InvalidateBlock(CValidationState& state, const CChainParams& chainparams, CBlockIndex* pindex)
{
    AssertLockHeld(cs_main);

    // Mark the block itself as invalid.
    pindex->nStatus |= BLOCK_FAILED_VALID;
    setDirtyBlockIndex.insert(pindex);
    setBlockIndexCandidates.erase(pindex);

    while (chainActive.Contains(pindex)) {
        CBlockIndex* pindexWalk = chainActive.Tip();
        pindexWalk->nStatus |= BLOCK_FAILED_CHILD;
        setDirtyBlockIndex.insert(pindexWalk);
        setBlockIndexCandidates.erase(pindexWalk);
        // ActivateBestChain considers blocks already in chainActive
        // unconditionally valid already, so force disconnect away from it.
        if (!DisconnectTip(state, chainparams)) {
            return false;
        }
    }

    // The resulting new best tip may not be in setBlockIndexCandidates anymore, so
    // add them again.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && !setBlockIndexCandidates.value_comp()(it->second, chainActive.Tip())) {
            setBlockIndexCandidates.insert(it->second);
        }
        it++;
    }

    InvalidChainFound(pindex);
    return true;
}

bool ReconsiderBlock(CValidationState& state, CBlockIndex* pindex)
{
    AssertLockHeld(cs_main);

    int nHeight = pindex->nHeight;

    // Remove the invalidity flag from this block and all its descendants.
    BlockMap::iterator it = mapBlockIndex.begin();
    while (it != mapBlockIndex.end()) {
        if (!it->second->IsValid() && it->second->GetAncestor(nHeight) == pindex) {
            it->second->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(it->second);
            if (it->second->IsValid(BLOCK_VALID_TRANSACTIONS) && it->second->nChainTx && setBlockIndexCandidates.value_comp()(chainActive.Tip(), it->second)) {
                setBlockIndexCandidates.insert(it->second);
            }
            if (it->second == pindexBestInvalid) {
                // Reset invalid block marker if it was pointing to one of those.
                pindexBestInvalid = NULL;
            }
        }
        it++;
    }

    // Remove the invalidity flag from all ancestors too.
    while (pindex != NULL) {
        if (pindex->nStatus & BLOCK_FAILED_MASK) {
            pindex->nStatus &= ~BLOCK_FAILED_MASK;
            setDirtyBlockIndex.insert(pindex);
        }
        pindex = pindex->pprev;
    }
    return true;
}

CBlockIndex* AddToBlockIndex(const CBlock& block)
{
    BlockMap::iterator prev_block_it = mapBlockIndex.find(block.hashPrevBlock);
    bool usePhi2 = false;
    if (prev_block_it != mapBlockIndex.end()) {
        usePhi2 = prev_block_it->second->nHeight + 1 >= Params().SwitchPhi2Block();
    }

    // Check for duplicate
    uint256 hash = block.GetHash(usePhi2);
    BlockMap::iterator it = mapBlockIndex.find(hash);
    if (it != mapBlockIndex.end())
        return it->second;

    // Construct new block index object
    CBlockIndex* pindexNew = new CBlockIndex(block);
    assert(pindexNew);

    // We assign the sequence id to blocks only when the full data is available,
    // to avoid miners withholding blocks but broadcasting headers, to get a
    // competitive advantage.
    pindexNew->nSequenceId = 0;
    BlockMap::iterator mi = mapBlockIndex.emplace(hash, pindexNew).first;

    //mark as PoS seen
    if (pindexNew->IsProofOfStake())
        stake->MarkStake(pindexNew->prevoutStake, pindexNew->nStakeTime);

    pindexNew->phashBlock = &((*mi).first);
    BlockMap::iterator miPrev = mapBlockIndex.find(block.hashPrevBlock);
    if (miPrev != mapBlockIndex.end()) {
        pindexNew->pprev = (*miPrev).second;
        pindexNew->nHeight = pindexNew->pprev->nHeight + 1;
        pindexNew->BuildSkip();

        //update previous block pointer
        pindexNew->pprev->pnext = pindexNew;

        // ppcoin: compute chain trust score
        pindexNew->bnChainTrust = (pindexNew->pprev ? pindexNew->pprev->bnChainTrust : 0) + pindexNew->GetBlockTrust();

        // ppcoin: compute stake entropy bit for stake modifier
        if (!pindexNew->SetStakeEntropyBit(pindexNew->GetStakeEntropyBit()))
            LogPrintf("%s: SetStakeEntropyBit() failed \n", __func__);

        // ppcoin: record proof-of-stake hash value
        if (pindexNew->IsProofOfStake()) {
            if (!stake->GetProof(hash, pindexNew->hashProofOfStake)) {
                LogPrintf("%s: zero stake (%s)\n", __func__, hash.GetHex());
            }
        }

        // ppcoin: compute stake modifier
        uint64_t nStakeModifier = 0;
        bool fGeneratedStakeModifier = false;
        if (!stake->ComputeNextModifier(pindexNew->pprev, nStakeModifier, fGeneratedStakeModifier))
            LogPrintf("%s: ComputeNextModifier() failed \n", __func__);

        pindexNew->SetStakeModifier(nStakeModifier, fGeneratedStakeModifier);
        pindexNew->nStakeModifierChecksum = stake->GetModifierChecksum(pindexNew);
        if (!stake->CheckModifierCheckpoints(pindexNew->nHeight, pindexNew->nStakeModifierChecksum))
            LogPrintf("%s: Rejected by stake modifier checkpoint height=%d, modifier=%s \n", __func__, pindexNew->nHeight, boost::lexical_cast<std::string>(nStakeModifier));

        DEBUG_DUMP_STAKING_INFO_AddToBlockIndex();
    }
  //  pindexNew->nTimeMax = (pindexNew->pprev ? std::max(pindexNew->pprev->nTimeMax, pindexNew->nTime) : pindexNew->nTime);
    pindexNew->nChainWork = (pindexNew->pprev ? pindexNew->pprev->nChainWork : 0) + GetBlockProof(*pindexNew);
    pindexNew->RaiseValidity(BLOCK_VALID_TREE);
    if (pindexBestHeader == NULL || pindexBestHeader->nChainWork < pindexNew->nChainWork)
        pindexBestHeader = pindexNew;

    //update previous block pointer
    if (pindexNew->nHeight)
        pindexNew->pprev->pnext = pindexNew;

    setDirtyBlockIndex.insert(pindexNew);

    return pindexNew;
}

/** Mark a block as having its data received and checked (up to BLOCK_VALID_TRANSACTIONS). */
bool ReceivedBlockTransactions(const CBlock& block, CValidationState& state, CBlockIndex* pindexNew, const CDiskBlockPos& pos)
{
    if (block.IsProofOfStake())
        pindexNew->SetProofOfStake();
    pindexNew->nTx = block.vtx.size();
    pindexNew->nChainTx = 0;
    pindexNew->nFile = pos.nFile;
    pindexNew->nDataPos = pos.nPos;
    pindexNew->nUndoPos = 0;
    pindexNew->nStatus |= BLOCK_HAVE_DATA;
    if (IsWitnessEnabled(pindexNew->pprev, Params().GetConsensus())) {
        pindexNew->nStatus |= BLOCK_OPT_WITNESS;
    }
    pindexNew->RaiseValidity(BLOCK_VALID_TRANSACTIONS);
    setDirtyBlockIndex.insert(pindexNew);

    if (pindexNew->pprev == NULL || pindexNew->pprev->nChainTx) {
        // If pindexNew is the genesis block or all parents are BLOCK_VALID_TRANSACTIONS.
        deque<CBlockIndex*> queue;
        queue.push_back(pindexNew);

        // Recursively process any descendant blocks that now may be eligible to be connected.
        while (!queue.empty()) {
            CBlockIndex* pindex = queue.front();
            queue.pop_front();
            pindex->nChainTx = (pindex->pprev ? pindex->pprev->nChainTx : 0) + pindex->nTx;
            {
                LOCK(cs_nBlockSequenceId);
                pindex->nSequenceId = nBlockSequenceId++;
            }
            if (chainActive.Tip() == NULL || !setBlockIndexCandidates.value_comp()(pindex, chainActive.Tip())) {
                setBlockIndexCandidates.insert(pindex);
            }
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex);
            while (range.first != range.second) {
                std::multimap<CBlockIndex*, CBlockIndex*>::iterator it = range.first;
                queue.push_back(it->second);
                range.first++;
                mapBlocksUnlinked.erase(it);
            }
        }
    } else {
        if (pindexNew->pprev && pindexNew->pprev->IsValid(BLOCK_VALID_TREE)) {
            mapBlocksUnlinked.insert(std::make_pair(pindexNew->pprev, pindexNew));
        }
    }

    return true;
}

bool FindBlockPos(CValidationState& state, CDiskBlockPos& pos, unsigned int nAddSize, unsigned int nHeight, uint64_t nTime, bool fKnown = false)
{
    LOCK(cs_LastBlockFile);

    unsigned int nFile = fKnown ? pos.nFile : nLastBlockFile;
    if (vinfoBlockFile.size() <= nFile) {
        vinfoBlockFile.resize(nFile + 1);
    }

    if (!fKnown) {
        while (vinfoBlockFile[nFile].nSize + nAddSize >= MAX_BLOCKFILE_SIZE) {
            LogPrintf("Leaving block file %i: %s\n", nFile, vinfoBlockFile[nFile].ToString());
            FlushBlockFile(true);
            nFile++;
            if (vinfoBlockFile.size() <= nFile) {
                vinfoBlockFile.resize(nFile + 1);
            }
        }
        pos.nFile = nFile;
        pos.nPos = vinfoBlockFile[nFile].nSize;
    }

    nLastBlockFile = nFile;
    vinfoBlockFile[nFile].AddBlock(nHeight, nTime);
    if (fKnown)
        vinfoBlockFile[nFile].nSize = std::max(pos.nPos + nAddSize, vinfoBlockFile[nFile].nSize);
    else
        vinfoBlockFile[nFile].nSize += nAddSize;

    if (!fKnown) {
        unsigned int nOldChunks = (pos.nPos + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        unsigned int nNewChunks = (vinfoBlockFile[nFile].nSize + BLOCKFILE_CHUNK_SIZE - 1) / BLOCKFILE_CHUNK_SIZE;
        if (nNewChunks > nOldChunks) {
            if (fPruneMode)
                fCheckForPruning = true;
            if (CheckDiskSpace(nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos)) {
                FILE* file = OpenBlockFile(pos);
                if (file) {
                    LogPrintf("Pre-allocating up to position 0x%x in blk%05u.dat\n", nNewChunks * BLOCKFILE_CHUNK_SIZE, pos.nFile);
                    AllocateFileRange(file, pos.nPos, nNewChunks * BLOCKFILE_CHUNK_SIZE - pos.nPos);
                    fclose(file);
                }
            } else
                return state.Error("out of disk space");
        }
    }

    setDirtyFileInfo.insert(nFile);
    return true;
}

bool FindUndoPos(CValidationState& state, int nFile, CDiskBlockPos& pos, unsigned int nAddSize)
{
    pos.nFile = nFile;

    LOCK(cs_LastBlockFile);

    unsigned int nNewSize;
    pos.nPos = vinfoBlockFile[nFile].nUndoSize;
    nNewSize = vinfoBlockFile[nFile].nUndoSize += nAddSize;
    setDirtyFileInfo.insert(nFile);

    unsigned int nOldChunks = (pos.nPos + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    unsigned int nNewChunks = (nNewSize + UNDOFILE_CHUNK_SIZE - 1) / UNDOFILE_CHUNK_SIZE;
    if (nNewChunks > nOldChunks) {
        if (fPruneMode)
            fCheckForPruning = true;
        if (CheckDiskSpace(nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos)) {
            FILE* file = OpenUndoFile(pos);
            if (file) {
                LogPrintf("Pre-allocating up to position 0x%x in rev%05u.dat\n", nNewChunks * UNDOFILE_CHUNK_SIZE, pos.nFile);
                AllocateFileRange(file, pos.nPos, nNewChunks * UNDOFILE_CHUNK_SIZE - pos.nPos);
                fclose(file);
            }
        } else
            return state.Error("out of disk space");
    }

    return true;
}

bool CheckBlockHeader(const CBlockHeader& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW) {
    // Get prev block index
    bool usePhi2 = false;
    BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
    if (mi != mapBlockIndex.end()) {
        usePhi2 = mi->second->nHeight + 1 >= Params().SwitchPhi2Block();
    }

    // Check proof of work matches claimed amount
    if (fCheckPOW && !CheckProofOfWork(block.GetHash(usePhi2), block.nBits, consensusParams))
        return state.DoS(50, error("%s: proof of work failed", __func__),
            REJECT_INVALID, "high-hash");
    return true;
}

bool CheckBlock(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW, bool fCheckMerkleRoot, bool fCheckSig)
{
    const char * const s = block.IsProofOfStake() ? "pos" : "pow";

    // These are checks that are independent of context.

    // Check that the header is valid (particularly PoW). This is mostly
    // redundant with the call in AcceptBlockHeader.
    if (fCheckPOW && !CheckBlockHeader(block, state, consensusParams, fCheckPOW && block.IsProofOfWork()))
        return state.DoS(100, error("%s: invalid (%s) block header", __func__, s),
            REJECT_INVALID, "bad-header", true);

    // 3 minute future drift for PoS
    auto const nBlockTimeLimit = GetAdjustedTime() + (block.IsProofOfStake() ? 180 : 7200);

    LogPrint("debug", "%s: block=%s (%s %d %d)\n", __func__, block.GetHash().GetHex(), s,
             block.GetBlockTime(), nBlockTimeLimit);

    // Check block time, reject far future blocks.
    if (block.GetBlockTime() > nBlockTimeLimit)
        return state.Invalid(error("%s: block timestamp too far in the future", __func__),
            REJECT_INVALID, "time-too-new");

    // Check the merkle root.
    if (fCheckMerkleRoot) {
        bool mutated;
        uint256 hashMerkleRoot2 = block.BuildMerkleTree(&mutated);
        if (block.hashMerkleRoot != hashMerkleRoot2)
            return state.DoS(100, error("%s: invalid merkle root", __func__),
                REJECT_INVALID, "bad-txnmrklroot", true);

        // Check for merkle tree malleability (CVE-2012-2459): repeating sequences
        // of transactions in a block without affecting the merkle root of a block,
        // while still invalidating it.
        if (mutated)
            return state.DoS(100, error("%s: duplicate transaction", __func__),
                REJECT_INVALID, "bad-txns-duplicate", true);
    }

    // All potential-corruption validation must be done before we do any
    // transaction validation, as otherwise we may mark the header as invalid
    // because we receive the wrong transactions for it.
    // Note that witness malleability is checked in ContextualCheckBlock, so no
    // checks that use witness data may be performed here.

    // Size limits
    if (block.vtx.empty() || block.vtx.size() > MAX_BLOCK_BASE_SIZE || ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION) > MAX_BLOCK_BASE_SIZE)
        return state.DoS(100, error("%s: size limits failed", __func__),
            REJECT_INVALID, "bad-blk-length");

    // First transaction must be coinbase, the rest must not be
    if (block.vtx.empty() || !block.vtx[0].IsCoinBase())
        return state.DoS(100, error("%s: first tx is not coinbase", __func__),
            REJECT_INVALID, "bad-cb-missing");

    for (unsigned int i = 1; i < block.vtx.size(); i++)
        if (block.vtx[i].IsCoinBase())
            return state.DoS(100, error("%s: more than one coinbase", __func__),
                REJECT_INVALID, "bad-cb-multiple");

    if (block.IsProofOfStake()) {
        // Coinbase output should be empty if proof-of-stake block
        if (block.vtx[0].vout.size() != 1 || !block.vtx[0].vout[0].IsEmpty())
            return state.DoS(100, error("%s: coinbase output not empty for proof-of-stake block", __func__));

        // Second transaction must be coinstake, the rest must not be
        if (block.vtx.empty() || !block.vtx[1].IsCoinStake())
            return state.DoS(100, error("%s: second tx is not coinstake", __func__));
        for (unsigned int i = 2; i < block.vtx.size(); i++)
            if (block.vtx[i].IsCoinStake())
                return state.DoS(100, error("%s: more than one coinstake", __func__));
    }

    LogPrint("debug", "%s: checking transactions, block %s (%s)\n", __func__, block.GetHash().GetHex(), s);

    // -------------------------------------------

    // Check transactions
    unsigned int nTx = 0;
    BOOST_FOREACH (const CTransaction& tx, block.vtx) {
        if (!CheckTransaction(tx, state)) {
            LogPrint("debug", "%s: invalid transaction %s", __func__, tx.ToString());
            return error("%s: CheckTransaction failed (nTx=%d, reason: %s)", __func__, nTx, state.GetRejectReason());
        }
        ++nTx;
    }

    unsigned int nSigOps = 0;
    BOOST_FOREACH (const CTransaction& tx, block.vtx) {
        nSigOps += GetLegacySigOpCount(tx);
    }
    if (nSigOps * WITNESS_SCALE_FACTOR > MAX_BLOCK_SIGOPS_COST)
        return state.DoS(100, error("%s: out-of-bounds SigOpCount", __func__),
            REJECT_INVALID, "bad-blk-sigops", true);

    return true;
}

bool CheckWork(const CBlock &block, CBlockIndex* const pindexPrev)
{
    const CChainParams& chainParams = Params();
    const Consensus::Params& consensusParams = chainParams.GetConsensus();
    if (pindexPrev == NULL)
        return error("%s: null pindexPrev for block %s", __func__, block.GetHash().GetHex());

    unsigned int nBitsRequired = GetNextWorkRequired(pindexPrev, &block, consensusParams, block.IsProofOfStake());

//    if (block.IsProofOfWork() && (pindexPrev->nHeight + 1 <= 68589)) {
//        double n1 = ConvertBitsToDouble(block.nBits);
//        double n2 = ConvertBitsToDouble(nBitsRequired);
//
//        if (abs(n1 - n2) > n1 * 0.5)
//            return error("%s : incorrect proof of work (DGW pre-fork) - %f %f %f at %d", __func__, abs(n1 - n2), n1, n2, pindexPrev->nHeight + 1);
//
//        return true;
//    }

    if (block.IsProofOfWork() && pindexPrev->nHeight + 1 > chainParams.LAST_POW_BLOCK())
        return error("%s: reject proof-of-work at height %d", __func__, pindexPrev->nHeight + 1);

    if (block.nBits != nBitsRequired)
        return error("%s: incorrect proof of work at %d", __func__, pindexPrev->nHeight + 1);

    if (block.IsProofOfStake()) {
        uint256 hashProofOfStake, proof;
        uint256 hash = block.GetHash(pindexPrev->nHeight + 1 >= chainParams.SwitchPhi2Block());
        if (!stake->CheckProof(pindexPrev, block, hashProofOfStake)) {
            return error("%s: invalid proof-of-stake (block %s)\n", __func__, hash.GetHex());
        }
        if (stake->GetProof(hash, proof)) {
            if (proof != hashProofOfStake)
                return error("%s: diverged stake %s, %s (block %s)\n", __func__,
                             hashProofOfStake.GetHex(), proof.GetHex(), hash.GetHex());
        } else {
            stake->SetProof(hash, hashProofOfStake);
        }
    }
    return true;
}

bool ContextualCheckBlockHeader(const CBlockHeader& block, CValidationState& state, const Consensus::Params& consensusParams, CBlockIndex* const pindexPrev)
{
    const CChainParams& chainParams = Params();
    uint256 hash;
    if (pindexPrev) {
        hash = block.GetHash(pindexPrev->nHeight + 1 >= Params().SwitchPhi2Block());
    } else {
        hash = block.GetHash();
    }

    if (hash == Params().HashGenesisBlock())
        return true;

    assert(pindexPrev);

    int nHeight = pindexPrev->nHeight + 1;

    if (chainActive.Height() - nHeight >= Params().FirstSCBlock() && !(block.nVersion & (1 << 30)))
        return state.DoS(1, error("%s: smart contracts hardfork is not active yet. (height %d)", __func__, nHeight));

    //If this is a reorg, check that it is not too depp
    if (chainActive.Height() - nHeight >= Params().MaxReorganizationDepth())
        return state.DoS(1, error("%s: forked chain older than max reorganization depth (height %d)", __func__, nHeight));

//    // Check timestamp against prev
//    if (block.GetBlockTime() <= pindexPrev->GetMedianTimePast()) {
//        LogPrintf("Block time = %d , GetMedianTimePast = %d \n", block.GetBlockTime(), pindexPrev->GetMedianTimePast());
//        return state.Invalid(error("%s : block's timestamp is too early", __func__),
//            REJECT_INVALID, "time-too-old");
//    }

    // Check that the block chain matches the known block chain up to a checkpoint
    if (!Checkpoints::CheckBlock(chainParams.Checkpoints(), nHeight, hash))
        return state.DoS(100, error("%s : rejected by checkpoint lock-in at %d", __func__, nHeight),
            REJECT_CHECKPOINT, "checkpoint mismatch");

    // Don't accept any forks from the main chain prior to last checkpoint
    CBlockIndex* pcheckpoint = Checkpoints::GetLastCheckpoint(chainParams.Checkpoints());
    if (pcheckpoint && nHeight < pcheckpoint->nHeight)
        return state.DoS(0, error("%s : forked chain older than last checkpoint (height %d)", __func__, nHeight));

//    // Reject block.nVersion=1 blocks when 95% (75% on testnet) of the network has upgraded:
//    if (block.nVersion < 2 &&
//        CBlockIndex::IsSuperMajority(2, pindexPrev, )) {
//        return state.Invalid(error("%s : rejected nVersion=1 block", __func__),
//            REJECT_OBSOLETE, "bad-version");
//    }
//
//    // Reject block.nVersion=2 blocks when 95% (75% on testnet) of the network has upgraded:
//    if (block.nVersion < 3 && CBlockIndex::IsSuperMajority(3, pindexPrev, consensusParams.nMajorityRejectBlockOutdated, consensusParams)) {
//        return state.Invalid(error("%s : rejected nVersion=2 block", __func__),
//            REJECT_OBSOLETE, "bad-version");
//    }

    return true;
}

bool ContextualCheckBlock(const CBlock& block, CValidationState& state, CBlockIndex* const pindexPrev)
{
    const int nHeight = pindexPrev == NULL ? 0 : pindexPrev->nHeight + 1;
    const Consensus::Params& consensusParams = Params().GetConsensus();

    // Check that all transactions are finalized
    BOOST_FOREACH (const CTransaction& tx, block.vtx)
        if (!IsFinalTx(tx, nHeight, block.GetBlockTime())) {
            return state.DoS(10, error("%s : contains a non-final transaction", __func__), REJECT_INVALID, "bad-txns-nonfinal");
        }

    // Enforce block.nVersion=2 rule that the coinbase starts with serialized block height
    // if 750 of the last 1,000 blocks are version 2 or greater (51/100 if testnet):
    if (block.nVersion >= 2 &&
        CBlockIndex::IsSuperMajority(2, pindexPrev, consensusParams.nMajorityEnforceBlockUpgrade, consensusParams)) {
        CScript expect = CScript() << nHeight;
        if (block.vtx[0].vin[0].scriptSig.size() < expect.size() ||
            !std::equal(expect.begin(), expect.end(), block.vtx[0].vin[0].scriptSig.begin())) {
            return state.DoS(100, error("%s : block height mismatch in coinbase", __func__), REJECT_INVALID, "bad-cb-height");
        }
    }

    // Validation for witness commitments.
    // * We compute the witness hash (which is the hash including witnesses) of all the block's transactions, except the
    //   coinbase (where 0x0000....0000 is used instead).
    // * The coinbase scriptWitness is a stack of a single 32-byte vector, containing a witness nonce (unconstrained).
    // * We build a merkle tree with all those witness hashes as leaves (similar to the hashMerkleRoot in the block header).
    // * There must be at least one output whose scriptPubKey is a single 36-byte push, the first 4 bytes of which are
    //   {0xaa, 0x21, 0xa9, 0xed}, and the following 32 bytes are SHA256^2(witness root, witness nonce). In case there are
    //   multiple, the last one is used.
    bool fHaveWitness = false;
    if (IsWitnessEnabled(pindexPrev, consensusParams)) {
        int commitpos = GetWitnessCommitmentIndex(block);
        if (commitpos != -1) {
            bool malleated = false;
            uint256 hashWitness = BlockWitnessMerkleRoot(block, &malleated);
            // The malleation check is ignored; as the transaction tree itself
            // already does not permit it, it is impossible to trigger in the
            // witness tree.
            if (block.vtx[0].wit.vtxinwit.size() != 1 || block.vtx[0].wit.vtxinwit[0].scriptWitness.stack.size() != 1 || block.vtx[0].wit.vtxinwit[0].scriptWitness.stack[0].size() != 32) {
                return state.DoS(100, error("%s : invalid witness nonce size", __func__), REJECT_INVALID, "bad-witness-nonce-size", true);
            }
            CHash256().Write(hashWitness.begin(), 32).Write(&block.vtx[0].wit.vtxinwit[0].scriptWitness.stack[0][0], 32).Finalize(hashWitness.begin());
            if (memcmp(hashWitness.begin(), &block.vtx[0].vout[commitpos].scriptPubKey[6], 32)) {
                return state.DoS(100, error("%s : witness merkle commitment mismatch", __func__), REJECT_INVALID, "bad-witness-merkle-match", true);
            }
            fHaveWitness = true;
        }
    }

    // No witness data is allowed in blocks that don't commit to witness data, as this would otherwise leave room for spam
    if (!fHaveWitness) {
        for (size_t i = 0; i < block.vtx.size(); i++) {
            if (!block.vtx[i].wit.IsNull()) {
                return state.DoS(100, error("%s : unexpected witness data found", __func__), REJECT_INVALID, "unexpected-witness", true);
            }
        }
    }

    return true;
}

bool IsWitnessEnabled(const CBlockIndex* pindexPrev, const Consensus::Params& params)
{
    LOCK(cs_main);
    return (VersionBitsState(pindexPrev, params, Consensus::DEPLOYMENT_SEGWIT, versionbitscache) == THRESHOLD_ACTIVE);
}

bool AcceptBlockHeader(const CBlock& block, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex)
{
    AssertLockHeld(cs_main);

    BlockMap::iterator prev_block_it = mapBlockIndex.find(block.hashPrevBlock);
    bool usePhi2 = false;
    if (prev_block_it != mapBlockIndex.end()) {
        usePhi2 = prev_block_it->second->nHeight + 1 >= Params().SwitchPhi2Block();
    }

    // Check for duplicate
    uint256 hash = block.GetHash(usePhi2);
    BlockMap::iterator miSelf = mapBlockIndex.find(hash);
    CBlockIndex* pindex = NULL;

    // TODO : ENABLE BLOCK CACHE IN SPECIFIC CASES
    if (miSelf != mapBlockIndex.end()) {
        // Block header is already known.
        pindex = miSelf->second;
        if (ppindex)
            *ppindex = pindex;
//        if (pindex->nStatus & BLOCK_FAILED_MASK)
//            return state.Invalid(error("%s : block is marked invalid", __func__), 0, "duplicate");
        return true;
    }

    if (!CheckBlockHeader(block, state, chainparams.GetConsensus(), block.IsProofOfWork())) {
        LogPrintf("%s: CheckBlockHeader failed \n", __func__);
        return false;
    }

    // Get prev block index
    CBlockIndex* pindexPrev = NULL;
    if (hash != chainparams.GetConsensus().hashGenesisBlock) {
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi == mapBlockIndex.end())
            return state.DoS(0, error("%s : prev block %s not found", __func__, block.hashPrevBlock.GetHex()), 0, "bad-prevblk");
        pindexPrev = (*mi).second;
//        if (pindexPrev->nStatus & BLOCK_FAILED_MASK)
//            return state.DoS(100, error("%s : prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");
    }

    if (!ContextualCheckBlockHeader(block, state, chainparams.GetConsensus(), pindexPrev))
        return false;

    if (pindex == NULL)
        pindex = AddToBlockIndex(block);

    if (ppindex)
        *ppindex = pindex;

    return true;
}

bool AcceptBlock(const CBlock& block, CValidationState& state, const CChainParams& chainparams, CBlockIndex** ppindex, CDiskBlockPos* dbp)
{
    AssertLockHeld(cs_main);

    CBlockIndex*& pindex = *ppindex;

    // Get prev block index
    CBlockIndex* pindexPrev = NULL;
    //Genesis block's hash cannot be calculated using PHI2, so no need to check PHI2 block hash
    if (block.GetHash() != chainparams.GetConsensus().hashGenesisBlock) {
        BlockMap::iterator mi = mapBlockIndex.find(block.hashPrevBlock);
        if (mi == mapBlockIndex.end())
            return state.DoS(0, error("%s : prev block %s not found", __func__, block.hashPrevBlock.GetHex()), 0, "bad-prevblk");
        pindexPrev = (*mi).second;
//        if (pindexPrev->nStatus & BLOCK_FAILED_MASK)
//            return state.DoS(100, error("%s : prev block invalid", __func__), REJECT_INVALID, "bad-prevblk");
    }

    if (block.GetHash() != chainparams.GetConsensus().hashGenesisBlock && !CheckWork(block, pindexPrev))
        return false;

    if (!AcceptBlockHeader(block, state, chainparams, &pindex))
        return false;

    if (pindex->nStatus & BLOCK_HAVE_DATA) {
        // TODO: deal better with duplicate blocks.
        // return state.DoS(20, error("AcceptBlock() : already have block %d %s", pindex->nHeight, pindex->GetBlockHash().GetHex()), REJECT_DUPLICATE, "duplicate");
        return true;
    }

    if ((!CheckBlock(block, state, chainparams.GetConsensus())) || !ContextualCheckBlock(block, state, pindex->pprev)) {
        if (state.IsInvalid() && !state.CorruptionPossible()) {
            pindex->nStatus |= BLOCK_FAILED_VALID;
            setDirtyBlockIndex.insert(pindex);
        }
        return false;
    }

    int nHeight = pindex->nHeight;

    // Write block to history file
    try {
        unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
        CDiskBlockPos blockPos;
        if (dbp != NULL)
            blockPos = *dbp;
        if (!FindBlockPos(state, blockPos, nBlockSize + 8, nHeight, block.GetBlockTime(), dbp != NULL))
            return error("%s: FindBlockPos failed", __func__);
        if (dbp == NULL && !WriteBlockToDisk(block, blockPos))
            return state.Error("Failed to write block");
        if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
            return error("%s: ReceivedBlockTransactions failed", __func__);
    } catch (std::runtime_error& e) {
        return state.Error(std::string("System error: ") + e.what());
    }

    if (fCheckForPruning)
        FlushStateToDisk(state, FLUSH_STATE_NONE); // we just allocated more disk space for block files

    return true;
}

bool CBlockIndex::IsSuperMajority(int minVersion, const CBlockIndex* pstart, unsigned int nRequired, const Consensus::Params& consensusParams)
{
    unsigned int nFound = 0;
    for (int i = 0; i < consensusParams.nMajorityWindow && nFound < nRequired && pstart != NULL; i++) {
        if (pstart->nVersion >= minVersion)
            ++nFound;
        pstart = pstart->pprev;
    }
    return (nFound >= nRequired);
}

void UpdateUncommittedBlockStructures(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams)
{
    int commitpos = GetWitnessCommitmentIndex(block);
    static const std::vector<unsigned char> nonce(32, 0x00);
    if (commitpos != -1 && IsWitnessEnabled(pindexPrev, consensusParams) && block.vtx[0].wit.IsEmpty()) {
        block.vtx[0].wit.vtxinwit.resize(1);
        block.vtx[0].wit.vtxinwit[0].scriptWitness.stack.resize(1);
        block.vtx[0].wit.vtxinwit[0].scriptWitness.stack[0] = nonce;
    }
}

std::vector<unsigned char> GenerateCoinbaseCommitment(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams)
{
    std::vector<unsigned char> commitment;
    int commitpos = GetWitnessCommitmentIndex(block);
    bool fHaveWitness = false;
    for (size_t t = 1; t < block.vtx.size(); t++) {
        if (!block.vtx[t].wit.IsNull()) {
            fHaveWitness = true;
            break;
        }
    }
    std::vector<unsigned char> ret(32, 0x00);
    if (fHaveWitness && IsWitnessEnabled(pindexPrev, consensusParams)) {
        if (commitpos == -1) {
            uint256 witnessroot = BlockWitnessMerkleRoot(block, NULL);
            CHash256().Write(witnessroot.begin(), 32).Write(&ret[0], 32).Finalize(witnessroot.begin());
            CTxOut out;
            out.nValue = 0;
            out.scriptPubKey.resize(38);
            out.scriptPubKey[0] = OP_RETURN;
            out.scriptPubKey[1] = 0x24;
            out.scriptPubKey[2] = 0xaa;
            out.scriptPubKey[3] = 0x21;
            out.scriptPubKey[4] = 0xa9;
            out.scriptPubKey[5] = 0xed;
            memcpy(&out.scriptPubKey[6], witnessroot.begin(), 32);
            commitment = std::vector<unsigned char>(out.scriptPubKey.begin(), out.scriptPubKey.end());
            const_cast<std::vector<CTxOut>*>(&block.vtx[0].vout)->push_back(out);
            block.vtx[0].UpdateHash();
        }
    }
    UpdateUncommittedBlockStructures(block, pindexPrev, consensusParams);
    return commitment;
}

/** Turn the lowest '1' bit in the binary representation of a number into a '0'. */
int static inline InvertLowestOne(int n) { return n & (n - 1); }

/** Compute what height to jump back to with the CBlockIndex::pskip pointer. */
int static inline GetSkipHeight(int height)
{
    if (height < 2)
        return 0;

    // Determine which height to jump back to. Any number strictly lower than height is acceptable,
    // but the following expression seems to perform well in simulations (max 110 steps to go back
    // up to 2**18 blocks).
    return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1 : InvertLowestOne(height);
}

CBlockIndex* CBlockIndex::GetAncestor(int height)
{
    if (height > nHeight || height < 0)
        return NULL;

    CBlockIndex* pindexWalk = this;
    int heightWalk = nHeight;
    while (heightWalk > height) {
        int heightSkip = GetSkipHeight(heightWalk);
        int heightSkipPrev = GetSkipHeight(heightWalk - 1);
        if (heightSkip == height ||
            (heightSkip > height && !(heightSkipPrev < heightSkip - 2 && heightSkipPrev >= height))) {
            // Only follow pskip if pprev->pskip isn't better than pskip->pprev.
            pindexWalk = pindexWalk->pskip;
            heightWalk = heightSkip;
        } else {
            assert(pindexWalk->pprev);
            pindexWalk = pindexWalk->pprev;
            heightWalk--;
        }
    }
    return pindexWalk;
}

const CBlockIndex* CBlockIndex::GetAncestor(int height) const
{
    return const_cast<CBlockIndex*>(this)->GetAncestor(height);
}

void CBlockIndex::BuildSkip()
{
    if (pprev)
        pskip = pprev->GetAncestor(GetSkipHeight(nHeight));
}

bool ProcessNewBlock(CValidationState& state, const CChainParams& chainparams, CNode* pfrom, const CBlock* pblock, CDiskBlockPos* dbp)
{
    // Preliminary checks
    if (!CheckBlock(*pblock, state, chainparams.GetConsensus()))
        return error("%s: block not passing checks", __func__);

    // Check proof-of-stake block signature
    if (!pblock->CheckBlockSignature())
        return error("%s: bad block signature", __func__);

    bool usePhi2 = false;

    // Limited duplicity on stake: prevents block flood attack
    // Duplicate stake allowed only when there is orphan child block
    if (pblock->IsProofOfStake() && stake->IsBlockStaked(pblock) && !mapBlockIndex.count(pblock->hashPrevBlock))
        return error("%s: duplicate proof-of-stake for block %s", __func__, pblock->GetHash().GetHex());

#   if 1 // Shouldn't send messages here to sync, prev blocks should have to be existed.

    // Check if the prev block is our prev block, if not then request sync and return false
    else if (pblock->GetHash() != chainparams.GetConsensus().hashGenesisBlock && pfrom != NULL) {
        BlockMap::iterator mi = mapBlockIndex.find(pblock->hashPrevBlock);
        if (mi == mapBlockIndex.end()) {
            pfrom->PushMessage("getblocks", chainActive.GetLocator(), uint256(0));
            return false;
        } else {
            usePhi2 = mi->second->nHeight + 1 >= Params().SwitchPhi2Block();
        }
    }

#   endif

    CBlockIndex* pindex = NULL;
    while (true) {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            MilliSleep(50);
            continue;
        }

        MarkBlockAsReceived(pblock->GetHash(usePhi2));

        // Store to disk
        bool ret = AcceptBlock(*pblock, state, chainparams, &pindex, dbp);
        if (pindex && pfrom) {
            mapBlockSource[pindex->GetBlockHash()] = pfrom->GetId();
        }

        CheckBlockIndex(chainparams.GetConsensus());

        if (ret) {
            break;
        } else {
            return error("%s: block not accepted", __func__);
        }
    }

    if (ActivateBestChain(state, chainparams, pblock)) {
        stake->MarkBlockStaked(pindex->nHeight, pindex->nTime);
    } else {
        return error("%s: ActivateBestChain failed", __func__);
    }

    if (pwalletMain) {
        // If turned on MultiSend will send a transaction (or more) on the after maturity of a stake
        if (pwalletMain->isMultiSendEnabled())
            pwalletMain->MultiSend();

        //If turned on Auto Combine will scan wallet for dust to combine
        if (pwalletMain->fCombineDust)
            pwalletMain->AutoCombineDust();
    }

    auto const &hash = pindex->GetBlockHash();
    const char * const s = pindex->IsProofOfStake() ? "pos" : "pow";
    if (fDebug) {
        LogPrintf("%s: ACCEPTED %d %s (%s)\n%s\n", __func__, pindex->nHeight, hash.GetHex(), s, pblock->ToString());
    } else {
        LogPrintf("%s: ACCEPTED %d %s (%s)\n", __func__, pindex->nHeight, hash.GetHex(), s);
    }

    return true;
}

bool IsWitnessLocked(const CBlockIndex* pindexPrev){
    LOCK(cs_main);
    return true;
}

bool TestBlockValidity(CValidationState& state, const CChainParams& chainparams, const CBlock& block, CBlockIndex* const pindexPrev, bool fCheckPOW, bool fCheckMerkleRoot)
{
    AssertLockHeld(cs_main);
    assert(pindexPrev == chainActive.Tip());

    CCoinsViewCache viewNew(pcoinsTip);
    CBlockIndex index(block);
    index.pprev = pindexPrev;
    index.nHeight = pindexPrev->nHeight + 1;

    // NOTE: CheckBlockHeader is called by CheckBlock
    if (!ContextualCheckBlockHeader(block, state, chainparams.GetConsensus(), pindexPrev))
        return false;
    if (!CheckBlock(block, state, chainparams.GetConsensus(), fCheckPOW, fCheckMerkleRoot))
        return false;
    if (!ContextualCheckBlock(block, state, pindexPrev))
        return false;
    if (block.IsProofOfStake() && !stake->CheckProof(pindexPrev, block, index.hashProofOfStake))
        return false;

    dev::h256 oldHashStateRoot(globalState->rootHash()); // lux
    dev::h256 oldHashUTXORoot(globalState->rootHashUTXO()); // lux

    if (!ConnectBlock(block, state, &index, viewNew, chainparams, true)) {
        globalState->setRoot(oldHashStateRoot); // lux
        globalState->setRootUTXO(oldHashUTXORoot); // lux
        std::cout << "ConnectBlock\n";
        return false;
    }

    assert(state.IsValid());

    return true;
}


bool AbortNode(const std::string& strMessage, const std::string& userMessage)
{
    strMiscWarning = strMessage;
    LogPrintf("*** %s\n", strMessage);
    uiInterface.ThreadSafeMessageBox(
        userMessage.empty() ? _("Error: A fatal internal error occured, see debug.log for details") : userMessage,
        "", CClientUIInterface::MSG_ERROR);
    StartShutdown();
    return false;
}


/**
 * BLOCK PRUNING CODE
 */

/* Calculate the amount of disk space the block & undo files currently use */
uint64_t CalculateCurrentUsage()
{
    uint64_t retval = 0;
    BOOST_FOREACH(const CBlockFileInfo &file, vinfoBlockFile) {
        retval += file.nSize + file.nUndoSize;
    }
    return retval;
}

/* Prune a block file (modify associated database entries)*/
void PruneOneBlockFile(const int fileNumber)
{
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); ++it) {
        CBlockIndex* pindex = it->second;
        if (pindex->nFile == fileNumber) {
            pindex->nStatus &= ~BLOCK_HAVE_DATA;
            pindex->nStatus &= ~BLOCK_HAVE_UNDO;
            pindex->nFile = 0;
            pindex->nDataPos = 0;
            pindex->nUndoPos = 0;
            setDirtyBlockIndex.insert(pindex);

            // Prune from mapBlocksUnlinked -- any block we prune would have
            // to be downloaded again in order to consider its chain, at which
            // point it would be considered as a candidate for
            // mapBlocksUnlinked or setBlockIndexCandidates.
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = mapBlocksUnlinked.equal_range(pindex->pprev);
            while (range.first != range.second) {
                std::multimap<CBlockIndex *, CBlockIndex *>::iterator it = range.first;
                range.first++;
                if (it->second == pindex) {
                    mapBlocksUnlinked.erase(it);
                }
            }
        }
    }

    vinfoBlockFile[fileNumber].SetNull();
    setDirtyFileInfo.insert(fileNumber);
}


void UnlinkPrunedFiles(std::set<int>& setFilesToPrune)
{
    for (set<int>::iterator it = setFilesToPrune.begin(); it != setFilesToPrune.end(); ++it) {
        CDiskBlockPos pos(*it, 0);
        boost::filesystem::remove(GetBlockPosFilename(pos, "blk"));
        boost::filesystem::remove(GetBlockPosFilename(pos, "rev"));
        LogPrintf("Prune: %s deleted blk/rev (%05u)\n", __func__, *it);
    }
}

/* This function is called from the RPC code for pruneblockchain */
void PruneBlockFilesManual(int nManualPruneHeight)
{
    CValidationState state;
    if (!FlushStateToDisk(state, FLUSH_STATE_NONE, nManualPruneHeight)) {
        LogPrintf("%s: failed to flush state (%s)\n", __func__, FormatStateMessage(state));
    }
}


/* Calculate the block/rev files to delete based on height specified by user with RPC command pruneblockchain */
static void FindFilesToPruneManual(std::set<int>& setFilesToPrune, int nManualPruneHeight)
{
    assert(fPruneMode && nManualPruneHeight > 0);

    LOCK2(cs_main, cs_LastBlockFile);
    if (chainActive.Tip() == nullptr)
        return;

    // last block to prune is the lesser of (user-specified height, MIN_BLOCKS_TO_KEEP from the tip)
    unsigned int nLastBlockWeCanPrune = std::min((unsigned int)nManualPruneHeight, (unsigned int) (chainActive.Tip()->nHeight - MIN_BLOCKS_TO_KEEP));
    int count=0;
    for (int fileNumber = 0; fileNumber < nLastBlockFile; fileNumber++) {
        if (vinfoBlockFile[fileNumber].nSize == 0 || vinfoBlockFile[fileNumber].nHeightLast > nLastBlockWeCanPrune)
            continue;
        PruneOneBlockFile(fileNumber);
        setFilesToPrune.insert(fileNumber);
        count++;
    }
    LogPrintf("Prune (Manual): prune_height=%d removed %d blk/rev pairs\n", nLastBlockWeCanPrune, count);
}

/* Calculate the block/rev files that should be deleted to remain under target*/
void FindFilesToPrune(std::set<int>& setFilesToPrune, uint64_t nPruneAfterHeight)
{
    LOCK2(cs_main, cs_LastBlockFile);
    if (chainActive.Tip() == NULL || nPruneTarget == 0) {
        return;
    }
    if (chainActive.Tip()->nHeight <= Params().PruneAfterHeight()) {
        return;
    }

    unsigned int nLastBlockWeMustKeep = chainActive.Tip()->nHeight - MIN_BLOCKS_TO_KEEP;
    uint64_t nCurrentUsage = CalculateCurrentUsage();
    // We don't check to prune until after we've allocated new space for files
    // So we should leave a buffer under our target to account for another allocation
    // before the next pruning.
    uint64_t nBuffer = BLOCKFILE_CHUNK_SIZE + UNDOFILE_CHUNK_SIZE;
    uint64_t nBytesToPrune;
    int count=0;

    if (nCurrentUsage + nBuffer >= nPruneTarget) {
        for (int fileNumber = 0; fileNumber < nLastBlockFile; fileNumber++) {
            nBytesToPrune = vinfoBlockFile[fileNumber].nSize + vinfoBlockFile[fileNumber].nUndoSize;

            if (vinfoBlockFile[fileNumber].nSize == 0)
                continue;

            if (nCurrentUsage + nBuffer < nPruneTarget)  // are we below our target?
                break;

            // don't prune files that could have a block within MIN_BLOCKS_TO_KEEP of the main chain's tip
            if (vinfoBlockFile[fileNumber].nHeightLast > nLastBlockWeMustKeep)
                break;

            PruneOneBlockFile(fileNumber);
            // Queue up the files for removal
            setFilesToPrune.insert(fileNumber);
            nCurrentUsage -= nBytesToPrune;
            count++;
        }
    }

    LogPrint("prune", "Prune: target=%dMiB actual=%dMiB diff=%dMiB min_must_keep=%d removed %d blk/rev pairs\n",
           nPruneTarget/1024/1024, nCurrentUsage/1024/1024,
           ((int64_t)nPruneTarget - (int64_t)nCurrentUsage)/1024/1024,
           nLastBlockWeMustKeep, count);
}

bool CheckDiskSpace(uint64_t nAdditionalBytes)
{
    uint64_t nFreeBytesAvailable = filesystem::space(GetDataDir()).available;

    // Check for nMinDiskSpace bytes (currently 50MB)
    if (nFreeBytesAvailable < nMinDiskSpace + nAdditionalBytes)
        return AbortNode("Disk space is low!", _("Error: Disk space is low!"));

    return true;
}

FILE* OpenDiskFile(const CDiskBlockPos& pos, const char* prefix, bool fReadOnly)
{
    if (pos.IsNull())
        return NULL;
    boost::filesystem::path path = GetBlockPosFilename(pos, prefix);
    boost::filesystem::create_directories(path.parent_path());
    FILE* file = fopen(path.string().c_str(), "rb+");
    if (!file && !fReadOnly)
        file = fopen(path.string().c_str(), "wb+");
    if (!file) {
        LogPrintf("Unable to open file %s\n", path.string());
        return NULL;
    }
    if (pos.nPos) {
        if (fseek(file, pos.nPos, SEEK_SET)) {
            LogPrintf("Unable to seek to position %u of %s\n", pos.nPos, path.string());
            fclose(file);
            return NULL;
        }
    }
    return file;
}

FILE* OpenBlockFile(const CDiskBlockPos& pos, bool fReadOnly)
{
    return OpenDiskFile(pos, "blk", fReadOnly);
}

FILE* OpenUndoFile(const CDiskBlockPos& pos, bool fReadOnly)
{
    return OpenDiskFile(pos, "rev", fReadOnly);
}

boost::filesystem::path GetBlockPosFilename(const CDiskBlockPos& pos, const char* prefix)
{
    return GetDataDir() / "blocks" / strprintf("%s%05u.dat", prefix, pos.nFile);
}

CBlockIndex* InsertBlockIndex(uint256 hash)
{
    if (hash == 0)
        return NULL;

    // Return existing
    BlockMap::iterator mi = mapBlockIndex.find(hash);
    if (mi != mapBlockIndex.end())
        return (*mi).second;

    // Create new
    CBlockIndex* pindexNew = new CBlockIndex();
    if (!pindexNew)
        throw runtime_error("LoadBlockIndex() : new CBlockIndex failed");
    mi = mapBlockIndex.emplace(hash, pindexNew).first;

    pindexNew->phashBlock = &((*mi).first);

    return pindexNew;
}

bool static LoadBlockIndexDB()
{
    const CChainParams& chainparams = Params();
    if (!pblocktree->LoadBlockIndexGuts())
        return false;

    boost::this_thread::interruption_point();

    // Calculate nChainWork
    vector<pair<int, CBlockIndex*> > vSortedByHeight;
    vSortedByHeight.reserve(mapBlockIndex.size());
    for (auto const &item : mapBlockIndex) {
        CBlockIndex* pindex = item.second;
        vSortedByHeight.push_back(make_pair(pindex->nHeight, pindex));
    }
    sort(vSortedByHeight.begin(), vSortedByHeight.end());
    for (auto const &item : vSortedByHeight) {
        CBlockIndex* pindex = item.second;
        pindex->nChainWork = (pindex->pprev ? pindex->pprev->nChainWork : 0) + GetBlockProof(*pindex);
        pindex->nTimeMax = (pindex->pprev ? std::max(pindex->pprev->nTimeMax, pindex->nTime) : pindex->nTime);

        // We can link the chain of blocks for which we've received transactions at some point.
        // Pruned nodes may have deleted the block.
        if (pindex->nTx > 0) {
            if (pindex->pprev) {
                if (pindex->pprev->nChainTx) {
                    pindex->nChainTx = pindex->pprev->nChainTx + pindex->nTx;
                } else {
                    pindex->nChainTx = 0;
                    mapBlocksUnlinked.insert(std::make_pair(pindex->pprev, pindex));
                }
            } else {
                pindex->nChainTx = pindex->nTx;
            }
        }
        if (pindex->IsValid(BLOCK_VALID_TRANSACTIONS) && (pindex->nChainTx || pindex->pprev == NULL))
            setBlockIndexCandidates.insert(pindex);
        if (pindex->nStatus & BLOCK_FAILED_MASK && (!pindexBestInvalid || pindex->nChainWork > pindexBestInvalid->nChainWork))
            pindexBestInvalid = pindex;
        if (pindex->pprev)
            pindex->BuildSkip();
        if (pindex->IsValid(BLOCK_VALID_TREE) && (pindexBestHeader == NULL || CBlockIndexWorkComparator()(pindexBestHeader, pindex)))
            pindexBestHeader = pindex;
    }

    // Load block file info
    pblocktree->ReadLastBlockFile(nLastBlockFile);
    vinfoBlockFile.resize(nLastBlockFile + 1);
    LogPrintf("%s: last block file = %i\n", __func__, nLastBlockFile);
    for (int nFile = 0; nFile <= nLastBlockFile; nFile++) {
        pblocktree->ReadBlockFileInfo(nFile, vinfoBlockFile[nFile]);
    }
    LogPrintf("%s: last block file info: %s\n", __func__, vinfoBlockFile[nLastBlockFile].ToString());
    for (int nFile = nLastBlockFile + 1; true; nFile++) {
        CBlockFileInfo info;
        if (pblocktree->ReadBlockFileInfo(nFile, info)) {
            vinfoBlockFile.push_back(info);
        } else {
            break;
        }
    }

    // Check presence of blk files
    LogPrintf("Checking all blk files are present...\n");
    set<int> setBlkDataFiles;
    for (auto const &item : mapBlockIndex) {
        CBlockIndex* pindex = item.second;
        if (pindex->nStatus & BLOCK_HAVE_DATA) {
            setBlkDataFiles.insert(pindex->nFile);
        }
    }
    for (std::set<int>::iterator it = setBlkDataFiles.begin(); it != setBlkDataFiles.end(); it++) {
        CDiskBlockPos pos(*it, 0);
        if (CAutoFile(OpenBlockFile(pos, true), SER_DISK, CLIENT_VERSION).IsNull()) {
            return false;
        }
    }

    //Check if the shutdown procedure was followed on last client exit
    bool fLastShutdownWasPrepared = true;
    pblocktree->ReadFlag("shutdown", fLastShutdownWasPrepared);
    LogPrintf("%s: Last shutdown was prepared: %s\n", __func__, fLastShutdownWasPrepared);

    //Check for inconsistency with block file info and internal state
    if (!fLastShutdownWasPrepared && !GetBoolArg("-forcestart", false) && !GetBoolArg("-reindex", false) && (vSortedByHeight.size() != vinfoBlockFile[nLastBlockFile].nHeightLast + 1) && (vinfoBlockFile[nLastBlockFile].nHeightLast != 0)) {
        //The database is in a state where a block has been accepted and written to disk, but not
        //all of the block has perculated through the code. The block and the index should both be
        //intact (although assertions are added if they are not), and the block will be reprocessed
        //to ensure all data will be accounted for.
        LogPrintf("%s: Inconsistent State Detected mapBlockIndex.size()=%d blockFileBlocks=%d\n", __func__, vSortedByHeight.size(), vinfoBlockFile[nLastBlockFile].nHeightLast + 1);
        LogPrintf("%s: lastIndexPos=%d blockFileSize=%d\n", __func__, vSortedByHeight[vSortedByHeight.size() - 1].second->GetBlockPos().nPos,
            vinfoBlockFile[nLastBlockFile].nSize);

        //try reading the block from the last index we have
        bool isFixed = true;
        string strError = "";
        LogPrintf("%s: Attempting to re-add last block that was recorded to disk\n", __func__);

        //get the last block that was properly recorded to the block info file
        CBlockIndex* pindexLastMeta = vSortedByHeight[vinfoBlockFile[nLastBlockFile].nHeightLast + 1].second;

        //fix Assertion `hashPrevBlock == view.GetBestBlock()' failed. By adjusting height to the last recorded by coinsview
        CBlockIndex* pindexCoinsView = mapBlockIndex[pcoinsTip->GetBestBlock()];
        for (unsigned int i = vinfoBlockFile[nLastBlockFile].nHeightLast + 1; i < vSortedByHeight.size(); i++)
        {
            pindexLastMeta = vSortedByHeight[i].second;
            if(pindexLastMeta->nHeight > pindexCoinsView->nHeight)
                break;
        }

        LogPrintf("%s: Last block properly recorded: #%d %s\n", __func__, pindexLastMeta->nHeight, pindexLastMeta->GetBlockHash().GetHex());

        CBlock lastMetaBlock;
        if (!ReadBlockFromDisk(lastMetaBlock, pindexLastMeta, chainparams.GetConsensus())) {
            isFixed = false;
            strError = strprintf("failed to read block %d from disk", pindexLastMeta->nHeight);
        }

        //set the chain to the block before lastMeta so that the meta block will be seen as new
        chainActive.SetTip(pindexLastMeta->pprev);

        //Process the lastMetaBlock again, using the known location on disk
        CDiskBlockPos blockPos = pindexLastMeta->GetBlockPos();
        CValidationState state;
        ProcessNewBlock(state, chainparams, NULL, &lastMetaBlock, &blockPos);

        //ensure that everything is as it should be
        if (pcoinsTip->GetBestBlock() != vSortedByHeight[vSortedByHeight.size() - 1].second->GetBlockHash()) {
            isFixed = false;
            strError = "pcoinsTip best block is not correct";
        }

        //properly account for all of the blocks that were not in the meta data. If this is not done the file
        //positioning will be wrong and blocks will be overwritten and later cause serialization errors
        CBlockIndex *pindexLast = vSortedByHeight[vSortedByHeight.size() - 1].second;
        CBlock lastBlock;
        if (!ReadBlockFromDisk(lastBlock, pindexLast, chainparams.GetConsensus())) {
            isFixed = false;
            strError = strprintf("failed to read block %d from disk", pindexLast->nHeight);
        }
        vinfoBlockFile[nLastBlockFile].nHeightLast = pindexLast->nHeight;
        vinfoBlockFile[nLastBlockFile].nSize = pindexLast->GetBlockPos().nPos + ::GetSerializeSize(lastBlock, SER_DISK, CLIENT_VERSION);;
        setDirtyFileInfo.insert(nLastBlockFile);
        FlushStateToDisk(state, FLUSH_STATE_ALWAYS);

        //Print out file info again
        pblocktree->ReadLastBlockFile(nLastBlockFile);
        vinfoBlockFile.resize(nLastBlockFile + 1);
        LogPrintf("%s: last block file = %i\n", __func__, nLastBlockFile);
        for (int nFile = 0; nFile <= nLastBlockFile; nFile++) {
            pblocktree->ReadBlockFileInfo(nFile, vinfoBlockFile[nFile]);
        }
        LogPrintf("%s: last block file info: %s\n", __func__, vinfoBlockFile[nLastBlockFile].ToString());

        if (!isFixed) {
            strError = "Failed reading from database. " + strError + ". The block database is in an inconsistent state and may cause issues in the future."
                                                                     "To force start use -forcestart";
            uiInterface.ThreadSafeMessageBox(strError, "", CClientUIInterface::MSG_ERROR);
            abort();
        }
        LogPrintf("Passed corruption fix\n");
    }

    // Check whether we have ever pruned block & undo files
    pblocktree->ReadFlag("prunedblockfiles", fHavePruned);
    if (fHavePruned)
        LogPrintf("LoadBlockIndexDB(): Block files have previously been pruned\n");

    // Check whether we need to continue reindexing
    bool fReindexing = false;
    pblocktree->ReadReindexing(fReindexing);
    if(fReindexing) fReindex = true;

    // Check whether we have a transaction index
    pblocktree->ReadFlag("txindex", fTxIndex);
    LogPrintf("%s: transaction index %s\n", __func__, fTxIndex ? "enabled" : "disabled");

    // If this is written true before the next client init, then we know the shutdown process failed
    pblocktree->WriteFlag("shutdown", false);

    // Load pointer to end of best chain
    BlockMap::iterator it = mapBlockIndex.find(pcoinsTip->GetBestBlock());
    if (it == mapBlockIndex.end())
        return true;
    chainActive.SetTip(it->second);

    PruneBlockIndexCandidates();

    LogPrintf("%s: hashBestChain=%s height=%d date=%s progress=%f\n", __func__,
        chainActive.Tip()->GetBlockHash().ToString(), chainActive.Height(),
        DateTimeStrFormat("%Y-%m-%d %H:%M:%S", chainActive.Tip()->GetBlockTime()),
        Checkpoints::GuessVerificationProgress(chainparams.Checkpoints(), chainActive.Tip()));

    return true;
}

CVerifyDB::CVerifyDB()
{
    uiInterface.ShowProgress(_("Verifying blocks..."), 0);
}

CVerifyDB::~CVerifyDB()
{
    uiInterface.ShowProgress("", 100);
}

bool CVerifyDB::VerifyDB(const CChainParams& chainparams, CCoinsView* coinsview, int nCheckLevel, int nCheckDepth)
{
    LOCK(cs_main);
    if (chainActive.Tip() == NULL || chainActive.Tip()->pprev == NULL)
        return true;

    // Verify blocks in the best chain
    if (nCheckDepth <= 0)
        nCheckDepth = 1000000000; // suffices until the year 19000
    if (nCheckDepth > chainActive.Height())
        nCheckDepth = chainActive.Height();
    nCheckLevel = std::max(0, std::min(4, nCheckLevel));
    LogPrintf("Verifying last %i blocks at level %i\n", nCheckDepth, nCheckLevel);
    CCoinsViewCache coins(coinsview);
    CBlockIndex* pindexState = chainActive.Tip();
    CBlockIndex* pindexFailure = NULL;
    int nGoodTransactions = 0;
    CValidationState state;
    for (CBlockIndex* pindex = chainActive.Tip(); pindex && pindex->pprev; pindex = pindex->pprev) {
        boost::this_thread::interruption_point();
        uiInterface.ShowProgress(_("Verifying blocks..."), std::max(1, std::min(99, (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * (nCheckLevel >= 4 ? 50 : 100)))));
        if (pindex->nHeight < chainActive.Height() - nCheckDepth)
            break;
        CBlock block;
        // check level 0: read from disk
        if (!ReadBlockFromDisk(block, pindex, chainparams.GetConsensus()))
            return error("VerifyDB() : *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
        // check level 1: verify block validity
        if (nCheckLevel >= 1 && !CheckBlock(block, state, chainparams.GetConsensus()))
            return error("VerifyDB() : *** found bad block at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
        // check level 2: verify undo validity
        if (nCheckLevel >= 2 && pindex) {
            CBlockUndo undo;
            CDiskBlockPos pos = pindex->GetUndoPos();
            if (!pos.IsNull()) {
                if (!undo.ReadFromDisk(pos, pindex->pprev->GetBlockHash()))
                    return error("VerifyDB() : *** found bad undo data at %d, hash=%s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
            }
        }
        // check level 3: check for inconsistencies during memory-only disconnect of tip blocks
        if (nCheckLevel >= 3 && pindex == pindexState && (coins.GetCacheSize() + pcoinsTip->GetCacheSize()) <= nCoinCacheSize) {
            bool fClean = true;
            if (!DisconnectBlock(block, state, pindex, coins, &fClean))
                return error("VerifyDB() : *** irrecoverable inconsistency in block data at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            pindexState = pindex->pprev;
            if (!fClean) {
                nGoodTransactions = 0;
                pindexFailure = pindex;
            } else
                nGoodTransactions += block.vtx.size();
        }
        if (ShutdownRequested())
            return true;
    }
    if (pindexFailure)
        return error("VerifyDB() : *** coin database inconsistencies found (last %i blocks, %i good transactions before that)\n", chainActive.Height() - pindexFailure->nHeight + 1, nGoodTransactions);

    // check level 4: try reconnecting blocks
    if (nCheckLevel >= 4) {
        CBlockIndex* pindex = pindexState;
        while (pindex != chainActive.Tip()) {
            boost::this_thread::interruption_point();
            uiInterface.ShowProgress(_("Verifying blocks..."), std::max(1, std::min(99, 100 - (int)(((double)(chainActive.Height() - pindex->nHeight)) / (double)nCheckDepth * 50))));
            pindex = chainActive.Next(pindex);
            CBlock block;
            if (!ReadBlockFromDisk(block, pindex, chainparams.GetConsensus()))
                return error("VerifyDB() : *** ReadBlockFromDisk failed at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
            if (!ConnectBlock(block, state, pindex, coins, chainparams))
                return error("VerifyDB() : *** found unconnectable block at %d, hash=%s", pindex->nHeight, pindex->GetBlockHash().ToString());
        }
    }

    LogPrintf("No coin database inconsistencies in last %i blocks (%i transactions)\n", chainActive.Height() - pindexState->nHeight, nGoodTransactions);

    return true;
}

void UnloadBlockIndex()
{
    mapBlockIndex.clear();
    setBlockIndexCandidates.clear();
    chainActive.SetTip(NULL);
    pindexBestInvalid = NULL;
    BOOST_FOREACH(BlockMap::value_type& entry, mapBlockIndex) {
        delete entry.second;
    }
    mapBlockIndex.clear();
    fHavePruned = false;
}

bool LoadBlockIndex()
{
    // Load block index from databases
    if (!fReindex && !LoadBlockIndexDB())
        return false;
    return true;
}


bool InitBlockIndex(const CChainParams& chainparams)
{
    LOCK(cs_main);
    // Check whether we're already initialized
    if (chainActive.Genesis() != NULL)
        return true;

    // Use the provided setting for -txindex in the new database
    fTxIndex = GetBoolArg("-txindex", true);
    pblocktree->WriteFlag("txindex", fTxIndex);
    LogPrintf("Initializing databases...\n");

    // Only add the genesis block if not reindexing (in which case we reuse the one already on disk)
    if (!fReindex) {
        try {
            CBlock& block = const_cast<CBlock&>(chainparams.GenesisBlock());
            // Start new block file
            unsigned int nBlockSize = ::GetSerializeSize(block, SER_DISK, CLIENT_VERSION);
            CDiskBlockPos blockPos;
            CValidationState state;
            if (!FindBlockPos(state, blockPos, nBlockSize + 8, 0, block.GetBlockTime()))
                return error("LoadBlockIndex() : FindBlockPos failed");
            if (!WriteBlockToDisk(block, blockPos))
                return error("LoadBlockIndex() : writing genesis block to disk failed");
            CBlockIndex* pindex = AddToBlockIndex(block);
            if (!ReceivedBlockTransactions(block, state, pindex, blockPos))
                return error("LoadBlockIndex() : genesis block not accepted");
            if (!ActivateBestChain(state, chainparams, &block))
                return error("LoadBlockIndex() : genesis block cannot be activated");
            // Force a chainstate write so that when we VerifyDB in a moment, it doesnt check stale data
            return FlushStateToDisk(state, FLUSH_STATE_ALWAYS);
        } catch (std::runtime_error& e) {
            return error("LoadBlockIndex() : failed to initialize block database: %s", e.what());
        }
    }

    return true;
}

bool LoadExternalBlockFile(const CChainParams& chainparams, FILE* fileIn, CDiskBlockPos* dbp)
{
    // Map of disk positions for blocks with unknown parent (only used for reindex)
    static std::multimap<uint256, CDiskBlockPos> mapBlocksUnknownParent;
    int64_t nStart = GetTimeMillis();

    int nLoaded = 0;
    try {
        // This takes over fileIn and calls fclose() on it in the CBufferedFile destructor
        CBufferedFile blkdat(fileIn, 2 * MAX_BLOCK_SERIALIZED_SIZE, MAX_BLOCK_SERIALIZED_SIZE + 8, SER_DISK, CLIENT_VERSION);
        uint64_t nRewind = blkdat.GetPos();
        while (!blkdat.eof()) {
            boost::this_thread::interruption_point();

            blkdat.SetPos(nRewind);
            nRewind++;         // start one byte further next time, in case of failure
            blkdat.SetLimit(); // remove former limit
            unsigned int nSize = 0;
            try {
                // locate a header
                unsigned char buf[MESSAGE_START_SIZE];
                blkdat.FindByte(chainparams.MessageStart()[0]);
                nRewind = blkdat.GetPos() + 1;
                blkdat >> FLATDATA(buf);
                if (memcmp(buf, chainparams.MessageStart(), MESSAGE_START_SIZE))
                    continue;
                // read size
                blkdat >> nSize;
                if (nSize < 80 || nSize > MAX_BLOCK_SERIALIZED_SIZE)
                    continue;
            } catch (const std::exception&) {
                // no valid block header found; don't complain
                break;
            }
            try {
                // read block
                uint64_t nBlockPos = blkdat.GetPos();
                if (dbp)
                    dbp->nPos = nBlockPos;
                blkdat.SetLimit(nBlockPos + nSize);
                blkdat.SetPos(nBlockPos);
                CBlock block;
                blkdat >> block;
                nRewind = blkdat.GetPos();

                // detect out of order blocks, and store them for later
                uint256 hash = block.GetHash();
                if (hash != chainparams.GetConsensus().hashGenesisBlock && mapBlockIndex.find(block.hashPrevBlock) == mapBlockIndex.end()) {
                    LogPrint("reindex", "%s: Out of order block %s, parent %s not known\n", __func__, hash.ToString(),
                        block.hashPrevBlock.ToString());
                    if (dbp)
                        mapBlocksUnknownParent.insert(std::make_pair(block.hashPrevBlock, *dbp));
                    continue;
                }

                BlockMap::iterator prev_block_it = mapBlockIndex.find(block.hashPrevBlock);
                bool usePhi2 = false;
                if (prev_block_it != mapBlockIndex.end()) {
                    usePhi2 = prev_block_it->second->nHeight + 1 >= Params().SwitchPhi2Block();
                }

                if (usePhi2) {
                    hash = block.GetHash(usePhi2);
                }

                // process in case the block isn't known yet
                if (mapBlockIndex.count(hash) == 0 || (mapBlockIndex[hash]->nStatus & BLOCK_HAVE_DATA) == 0) {
                    CValidationState state;
                    if (ProcessNewBlock(state, chainparams, NULL, &block, dbp))
                        nLoaded++;
                    if (state.IsError())
                        break;
                } else if (hash != chainparams.GetConsensus().hashGenesisBlock && mapBlockIndex[hash]->nHeight % 1000 == 0) {
                    LogPrintf("Block Import: already had block %s at height %d\n", hash.ToString(), mapBlockIndex[hash]->nHeight);
                }

                // Recursively process earlier encountered successors of this block
                deque<uint256> queue;
                queue.push_back(hash);
                while (!queue.empty()) {
                    uint256 head = queue.front();
                    queue.pop_front();
                    std::pair<std::multimap<uint256, CDiskBlockPos>::iterator, std::multimap<uint256, CDiskBlockPos>::iterator> range = mapBlocksUnknownParent.equal_range(head);
                    while (range.first != range.second) {
                        std::multimap<uint256, CDiskBlockPos>::iterator it = range.first;

                        BlockMap::iterator prev_block_it = mapBlockIndex.find(block.hashPrevBlock);
                        bool usePhi2 = false;
                        if (prev_block_it != mapBlockIndex.end()) {
                            usePhi2 = prev_block_it->second->nHeight >= Params().SwitchPhi2Block();
                        }

                        uint256 hash = block.GetHash(usePhi2);
                        int nHeight = mapBlockIndex[hash]->nHeight;
                        if (ReadBlockFromDisk(block, it->second, nHeight, chainparams.GetConsensus())) {
                            LogPrintf("%s: Processing out of order child %s of %s\n", __func__, block.GetHash(usePhi2).ToString(),
                                head.ToString());
                            CValidationState dummy;
                            if (ProcessNewBlock(dummy, chainparams, NULL, &block, &it->second)) {
                                nLoaded++;
                                queue.push_back(block.GetHash(usePhi2));
                            }
                        }
                        range.first++;
                        mapBlocksUnknownParent.erase(it);
                    }
                }
            } catch (std::exception& e) {
                LogPrintf("%s : Deserialize or I/O error - %s", __func__, e.what());
            }
        }
    } catch (std::runtime_error& e) {
        AbortNode(std::string("System error: ") + e.what());
    }
    if (nLoaded > 0)
        LogPrintf("Loaded %i blocks from external file in %dms\n", nLoaded, GetTimeMillis() - nStart);
    return nLoaded > 0;
}

static void CheckBlockIndex(const Consensus::Params& consensusParams)
{
    if (!fCheckBlockIndex) return;

    LOCK(cs_main);

    // During a reindex, we read the genesis block and call CheckBlockIndex before ActivateBestChain,
    // so we have the genesis block in mapBlockIndex but no active chain.  (A few of the tests when
    // iterating the block tree require that chainActive has been initialized.)
    if (chainActive.Height() < 0) {
        assert(mapBlockIndex.size() <= 1);
        return;
    }

    // Build forward-pointing map of the entire block tree.
    std::multimap<CBlockIndex*, CBlockIndex*> forward;
    for (BlockMap::iterator it = mapBlockIndex.begin(); it != mapBlockIndex.end(); it++) {
        forward.insert(std::make_pair(it->second->pprev, it->second));
    }

    assert(forward.size() == mapBlockIndex.size());

    std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> rangeGenesis = forward.equal_range(NULL);
    CBlockIndex* pindex = rangeGenesis.first->second;
    rangeGenesis.first++;
    assert(rangeGenesis.first == rangeGenesis.second); // There is only one index entry with parent NULL.

    // Iterate over the entire block tree, using depth-first search.
    // Along the way, remember whether there are blocks on the path from genesis
    // block being explored which are the first to have certain properties.
    size_t nNodes = 0;
    int nHeight = 0;
    CBlockIndex* pindexFirstInvalid = NULL;         // Oldest ancestor of pindex which is invalid.
    CBlockIndex* pindexFirstMissing = NULL;         // Oldest ancestor of pindex which does not have BLOCK_HAVE_DATA.
    CBlockIndex* pindexFirstNeverProcessed = NULL;  // Oldest ancestor of pindex for which nTx == 0.
    CBlockIndex* pindexFirstNotTreeValid = NULL;    // Oldest ancestor of pindex which does not have BLOCK_VALID_TREE (regardless of being valid or not).
    CBlockIndex* pindexFirstNotTransactionsValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_TRANSACTIONS (regardless of being valid or not)
    CBlockIndex* pindexFirstNotChainValid = NULL;   // Oldest ancestor of pindex which does not have BLOCK_VALID_CHAIN (regardless of being valid or not).
    CBlockIndex* pindexFirstNotScriptsValid = NULL; // Oldest ancestor of pindex which does not have BLOCK_VALID_SCRIPTS (regardless of being valid or not).
    while (pindex != NULL) {
        nNodes++;
        if (pindexFirstInvalid == NULL && pindex->nStatus & BLOCK_FAILED_VALID) pindexFirstInvalid = pindex;
        if (pindexFirstMissing == NULL && !(pindex->nStatus & BLOCK_HAVE_DATA)) pindexFirstMissing = pindex;
        if (pindexFirstNeverProcessed == NULL && pindex->nTx == 0) pindexFirstNeverProcessed = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTreeValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TREE) pindexFirstNotTreeValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotTransactionsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_TRANSACTIONS) pindexFirstNotTransactionsValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotChainValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_CHAIN) pindexFirstNotChainValid = pindex;
        if (pindex->pprev != NULL && pindexFirstNotScriptsValid == NULL && (pindex->nStatus & BLOCK_VALID_MASK) < BLOCK_VALID_SCRIPTS) pindexFirstNotScriptsValid = pindex;

        // Begin: actual consistency checks.
        if (pindex->pprev == NULL) {
            // Genesis block checks.
            assert(pindex->GetBlockHash() == consensusParams.hashGenesisBlock); // Genesis block's hash must match.
            assert(pindex == chainActive.Genesis()); // The current active chain's genesis block must be this block.
        }

        if (pindex->nChainTx == 0) assert(pindex->nSequenceId == 0);  // nSequenceId can't be set for blocks that aren't linked
        // VALID_TRANSACTIONS is equivalent to nTx > 0 for all nodes (whether or not pruning has occurred).
        // HAVE_DATA is only equivalent to nTx > 0 (or VALID_TRANSACTIONS) if no pruning has occurred.
        if (!fHavePruned) {
            // If we've never pruned, then HAVE_DATA should be equivalent to nTx > 0
            assert(!(pindex->nStatus & BLOCK_HAVE_DATA) == (pindex->nTx == 0));
            assert(pindexFirstMissing == pindexFirstNeverProcessed);
        } else {
            // If we have pruned, then we can only say that HAVE_DATA implies nTx > 0
            if (pindex->nStatus & BLOCK_HAVE_DATA) assert(pindex->nTx > 0);
        }
        if (pindex->nStatus & BLOCK_HAVE_UNDO) assert(pindex->nStatus & BLOCK_HAVE_DATA);
        assert(((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TRANSACTIONS) == (pindex->nTx > 0)); // This is pruning-independent.
        // All parents having had data (at some point) is equivalent to all parents being VALID_TRANSACTIONS, which is equivalent to nChainTx being set.
        assert((pindexFirstNeverProcessed != NULL) == (pindex->nChainTx == 0)); // nChainTx != 0 is used to signal that all parent blocks have been processed (but may have been pruned).
        assert((pindexFirstNotTransactionsValid != NULL) == (pindex->nChainTx == 0));
        assert(pindex->nHeight == nHeight);                                                                          // nHeight must be consistent.
        assert(pindex->pprev == NULL || pindex->nChainWork >= pindex->pprev->nChainWork);                            // For every block except the genesis block, the chainwork must be larger than the parent's.
        assert(nHeight < 2 || (pindex->pskip && (pindex->pskip->nHeight < nHeight)));                                // The pskip pointer must point back for all but the first 2 blocks.
        assert(pindexFirstNotTreeValid == NULL);                                                                     // All mapBlockIndex entries must at least be TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_TREE) assert(pindexFirstNotTreeValid == NULL);       // TREE valid implies all parents are TREE valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_CHAIN) assert(pindexFirstNotChainValid == NULL);     // CHAIN valid implies all parents are CHAIN valid
        if ((pindex->nStatus & BLOCK_VALID_MASK) >= BLOCK_VALID_SCRIPTS) assert(pindexFirstNotScriptsValid == NULL); // SCRIPTS valid implies all parents are SCRIPTS valid
        if (pindexFirstInvalid == NULL) {
            // Checks for not-invalid blocks.
            assert((pindex->nStatus & BLOCK_FAILED_MASK) == 0); // The failed mask cannot be set for blocks without invalid parents.
        }
        if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && pindexFirstNeverProcessed == NULL) {
            if (pindexFirstInvalid == NULL) {
                // If this block sorts at least as good as the current tip and
                // is valid and we have all data for its parents, it must be in
                // setBlockIndexCandidates.  chainActive.Tip() must also be there
                // even if some data has been pruned.
                if (pindexFirstMissing == NULL || pindex == chainActive.Tip()) {
                    assert(setBlockIndexCandidates.count(pindex));
                }
                // If some parent is missing, then it could be that this block was in
                // setBlockIndexCandidates but had to be removed because of the missing data.
                // In this case it must be in mapBlocksUnlinked -- see test below.
            }
        } else { // If this block sorts worse than the current tip, it cannot be in setBlockIndexCandidates.
            assert(setBlockIndexCandidates.count(pindex) == 0);
        }
        // Check whether this block is in mapBlocksUnlinked.
        std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> rangeUnlinked = mapBlocksUnlinked.equal_range(pindex->pprev);
        bool foundInUnlinked = false;
        while (rangeUnlinked.first != rangeUnlinked.second) {
            assert(rangeUnlinked.first->first == pindex->pprev);
            if (rangeUnlinked.first->second == pindex) {
                foundInUnlinked = true;
                break;
            }
            rangeUnlinked.first++;
        }
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed != NULL && pindexFirstInvalid == NULL) {
            // If this block has block data available, some parent was never received, and has no invalid parents, it must be in mapBlocksUnlinked.
            assert(foundInUnlinked);
        }
        if (!(pindex->nStatus & BLOCK_HAVE_DATA)) assert(!foundInUnlinked); // Can't be in mapBlocksUnlinked if we don't HAVE_DATA
        if (pindexFirstMissing == NULL) assert(!foundInUnlinked); // We aren't missing data for any parent -- cannot be in mapBlocksUnlinked.
        if (pindex->pprev && (pindex->nStatus & BLOCK_HAVE_DATA) && pindexFirstNeverProcessed == NULL && pindexFirstMissing != NULL) {
            // We HAVE_DATA for this block, have received data for all parents at some point, but we're currently missing data for some parent.
            assert(fHavePruned); // We must have pruned.
            // This block may have entered mapBlocksUnlinked if:
            //  - it has a descendant that at some point had more work than the
            //    tip, and
            //  - we tried switching to that descendant but were missing
            //    data for some intermediate block between chainActive and the
            //    tip.
            // So if this block is itself better than chainActive.Tip() and it wasn't in
            // setBlockIndexCandidates, then it must be in mapBlocksUnlinked.
            if (!CBlockIndexWorkComparator()(pindex, chainActive.Tip()) && setBlockIndexCandidates.count(pindex) == 0) {
                if (pindexFirstInvalid == NULL) {
                    assert(foundInUnlinked);
                }
            }
        }
        // assert(pindex->GetBlockHash() == pindex->GetBlockHeader().GetHash()); // Perhaps too slow
        // End: actual consistency checks.

        // Try descending into the first subnode.
        std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> range = forward.equal_range(pindex);
        if (range.first != range.second) {
            // A subnode was found.
            pindex = range.first->second;
            nHeight++;
            continue;
        }
        // This is a leaf node.
        // Move upwards until we reach a node of which we have not yet visited the last child.
        while (pindex) {
            // We are going to either move to a parent or a sibling of pindex.
            // If pindex was the first with a certain property, unset the corresponding variable.
            if (pindex == pindexFirstInvalid) pindexFirstInvalid = NULL;
            if (pindex == pindexFirstMissing) pindexFirstMissing = NULL;
            if (pindex == pindexFirstNeverProcessed) pindexFirstNeverProcessed = NULL;
            if (pindex == pindexFirstNotTreeValid) pindexFirstNotTreeValid = NULL;
            if (pindex == pindexFirstNotTransactionsValid) pindexFirstNotTransactionsValid = NULL;
            if (pindex == pindexFirstNotChainValid) pindexFirstNotChainValid = NULL;
            if (pindex == pindexFirstNotScriptsValid) pindexFirstNotScriptsValid = NULL;
            // Find our parent.
            CBlockIndex* pindexPar = pindex->pprev;
            // Find which child we just visited.
            std::pair<std::multimap<CBlockIndex*, CBlockIndex*>::iterator, std::multimap<CBlockIndex*, CBlockIndex*>::iterator> rangePar = forward.equal_range(pindexPar);
            while (rangePar.first->second != pindex) {
                assert(rangePar.first != rangePar.second); // Our parent must have at least the node we're coming from as child.
                rangePar.first++;
            }
            // Proceed to the next one.
            rangePar.first++;
            if (rangePar.first != rangePar.second) {
                // Move to the sibling.
                pindex = rangePar.first->second;
                break;
            } else {
                // Move up further.
                pindex = pindexPar;
                nHeight--;
                continue;
            }
        }
    }

    // Check that we actually traversed the entire map.
    assert(nNodes == forward.size());
}

//////////////////////////////////////////////////////////////////////////////
//
// CAlert
//

string GetWarnings(string strFor)
{
    int nPriority = 0;
    string strStatusBar;
    string strRPC;

    if (!CLIENT_VERSION_IS_RELEASE)
        strStatusBar = _("This is a pre-release test build - use at your own risk - do not use for mining or merchant applications");

    if (GetBoolArg("-testsafemode", false))
        strStatusBar = strRPC = "testsafemode enabled";

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "") {
        nPriority = 1000;
        strStatusBar = strMiscWarning;
    }

    if (fLargeWorkForkFound) {
        nPriority = 2000;
        strStatusBar = strRPC = _("Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.");
    } else if (fLargeWorkInvalidChainFound) {
        nPriority = 2000;
        strStatusBar = strRPC = _("Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.");
    }

    // Alerts
    {
        LOCK(cs_mapAlerts);
        BOOST_FOREACH (PAIRTYPE(const uint256, CAlert) & item, mapAlerts) {
            const CAlert& alert = item.second;
            if (alert.AppliesToMe() && alert.nPriority > nPriority) {
                nPriority = alert.nPriority;
                strStatusBar = alert.strStatusBar;
            }
        }
    }

    if (strFor == "statusbar")
        return strStatusBar;
    else if (strFor == "rpc")
        return strRPC;
    assert(!"GetWarnings() : invalid parameter");
    return "error";
}


//////////////////////////////////////////////////////////////////////////////
//
// Messages
//


bool static AlreadyHave(const CInv& inv)
{
    switch (inv.type) {
    case MSG_TX:
    case MSG_WITNESS_TX:
        {
        bool txInMap = false;
        txInMap = mempool.exists(inv.hash);
        return txInMap || mapOrphanTransactions.count(inv.hash) ||
               pcoinsTip->HaveCoins(inv.hash);
        }
    case MSG_BLOCK:
    case MSG_WITNESS_BLOCK:
        return mapBlockIndex.count(inv.hash);
    case MSG_TXLOCK_REQUEST:
        return mapTxLockReq.count(inv.hash) ||
               mapTxLockReqRejected.count(inv.hash);
    case MSG_TXLOCK_VOTE:
        return mapTxLockVote.count(inv.hash);
    case MSG_SPORK:
        return mapSporks.count(inv.hash);
    case MSG_MASTERNODE_WINNER:
        return mapSeenMasternodeVotes.count(inv.hash);
    }
    // Don't know what it is, just say we already got one
    return true;
}


void static ProcessGetData(CNode* pfrom, const Consensus::Params& consensusParams)
{
    std::deque<CInv>::iterator it = pfrom->vRecvGetData.begin();

    vector<CInv> vNotFound;

    LOCK(cs_main);

    while (it != pfrom->vRecvGetData.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        const CInv& inv = *it;
        {
            boost::this_thread::interruption_point();
            it++;

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK || inv.type == MSG_WITNESS_BLOCK) {
                bool send = false;
                BlockMap::iterator mi = mapBlockIndex.find(inv.hash);
                if (mi != mapBlockIndex.end()) {
                    if (chainActive.Contains(mi->second)) {
                        send = true;
                    } else {
                        // To prevent fingerprinting attacks, only send blocks outside of the active
                        // chain if they are valid, and no more than a max reorg depth than the best header
                        // chain we know about.
                        send = mi->second->IsValid(BLOCK_VALID_SCRIPTS) && (pindexBestHeader != NULL) &&
                               (chainActive.Height() - mi->second->nHeight < Params().MaxReorganizationDepth());
                        if (!send) {
                            LogPrintf("ProcessGetData(): ignoring request from peer=%i for old block that isn't in the main chain\n", pfrom->GetId());
                        }
                    }
                }
                if (send && (mi->second->nStatus & BLOCK_HAVE_DATA)) {
                    // Send block from disk
                    CBlock block;
                    if (!ReadBlockFromDisk(block, (*mi).second, consensusParams))
                        assert(!"cannot load block from disk");
                    if (inv.type == MSG_BLOCK)
                        pfrom->PushMessage("block", block); //TODO: push message with flag NO_WITNESS
                    else if (inv.type == MSG_WITNESS_BLOCK)
                        pfrom->PushMessage("block", block);
                    else // MSG_FILTERED_BLOCK)
                    {
                        LOCK(pfrom->cs_filter);
                        if (pfrom->pfilter) {
                            CMerkleBlock merkleBlock(block, *pfrom->pfilter);
                            pfrom->PushMessage("merkleblock", merkleBlock);
                            // CMerkleBlock just contains hashes, so also push any transactions in the block the client did not see
                            // This avoids hurting performance by pointlessly requiring a round-trip
                            // Note that there is currently no way for a node to request any single transactions we didnt send here -
                            // they must either disconnect and retry or request the full block.
                            // Thus, the protocol spec specified allows for us to provide duplicate txn here,
                            // however we MUST always provide at least what the remote peer needs
                            typedef std::pair<unsigned int, uint256> PairType;
                            BOOST_FOREACH (PairType& pair, merkleBlock.vMatchedTxn)
                                if (!pfrom->setInventoryKnown.count(CInv(MSG_TX, pair.second)))
                                    pfrom->PushMessage("tx", block.vtx[pair.first]); //TODO: push message with flag NO_WITNESS
                        }
                        // else
                        // no response
                    }

                    // Trigger them to send a getblocks request for the next batch of inventory
                    if (inv.hash == pfrom->hashContinue) {
                        // Bypass PushInventory, this must send even if redundant,
                        // and we want it right after the last block so they don't
                        // wait for other stuff first.
                        vector<CInv> vInv;
                        vInv.push_back(CInv(MSG_BLOCK, chainActive.Tip()->GetBlockHash()));
                        pfrom->PushMessage("inv", vInv); //TODO: push message with flag NO_WITNESS
                        pfrom->hashContinue = 0;
                    }
                }
            } else if (inv.type == MSG_TX || inv.type == MSG_WITNESS_TX) {
                // Send stream from relay memory
                bool pushed = false;
                {
                    LOCK(cs_mapRelay);
                    map<CInv, CDataStream>::iterator mi = mapRelay.find(inv);
                    if (mi != mapRelay.end()) {
                        pfrom->PushMessage(inv.GetCommand(), (*mi).second); //TODO: push message with flags
                        pushed = true;
                    }
                }

                if (!pushed && inv.type == MSG_TX) { //TODO: probably should check for MSG_TX_WITNESS too
                    CTransaction tx;
                    CTransactionRef ptx = mempool.get(inv.hash);
                    if (ptx) {
                        tx = *ptx.get();
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000); //TODO: if we check for MSG_TX_WITNESS, should this value be changed?
                        ss << tx;
                        pfrom->PushMessage("tx", ss);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TXLOCK_VOTE) {
                    if (mapTxLockVote.count(inv.hash)) {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << mapTxLockVote[inv.hash];
                        pfrom->PushMessage("txlvote", ss); //TODO: push message with flags
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_TXLOCK_REQUEST) {
                    if (mapTxLockReq.count(inv.hash)) {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << mapTxLockReq[inv.hash];
                        pfrom->PushMessage("ix", ss);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_SPORK) {
                    if (mapSporks.count(inv.hash)) {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        ss.reserve(1000);
                        ss << mapSporks[inv.hash];
                        pfrom->PushMessage("spork", ss);
                        pushed = true;
                    }
                }
                if (!pushed && inv.type == MSG_MASTERNODE_WINNER) {
                    if (mapSeenMasternodeVotes.count(inv.hash)) {
                        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                        int a = 0;
                        ss.reserve(1000);
                        ss << mapSeenMasternodeVotes[inv.hash] << a;
                        pfrom->PushMessage("mnw", ss);
                        pushed = true;
                    }
                }

                if (!pushed) {
                    vNotFound.push_back(inv);
                }
            }

            // Track requests for our stuff.
            GetMainSignals().Inventory(inv.hash);

            if (inv.type == MSG_BLOCK || inv.type == MSG_FILTERED_BLOCK || inv.type == MSG_WITNESS_BLOCK)
                break;
        }
    }

    pfrom->vRecvGetData.erase(pfrom->vRecvGetData.begin(), it);

    if (!vNotFound.empty()) {
        // Let the peer know that we didn't find what it asked for, so it doesn't
        // have to wait around forever. Currently only SPV clients actually care
        // about this message: it's needed when they are recursively walking the
        // dependencies of relevant unconfirmed transactions. SPV clients want to
        // do that because they want to know about (and store and rebroadcast and
        // risk analyze) the dependencies of transactions relevant to them, without
        // having to download the entire memory pool.
        pfrom->PushMessage("notfound", vNotFound);
    }
}

uint32_t GetFetchFlags(CNode* pfrom, CBlockIndex* pprev, const Consensus::Params& chainparams) {
    uint32_t nFetchFlags = 0;
    if (IsWitnessEnabled(pprev, chainparams) && State(pfrom->GetId())->fHaveWitness) {
        nFetchFlags |= MSG_WITNESS_FLAG;
    }
    return nFetchFlags;
}

static bool ProcessMessage(CNode* pfrom, const string &strCommand, CDataStream& vRecv, int64_t nTimeReceived, const CChainParams& chainparams)
{
    RandAddSeedPerfmon();
    if (fDebug) {
        LogPrintf("received: %s (%u bytes) peer=%d\n", SanitizeString(strCommand), vRecv.size(), pfrom->id);
    }

    if (mapArgs.count("-dropmessagestest") && GetRand(atoi(mapArgs["-dropmessagestest"])) == 0) {
        LogPrintf("dropmessagestest DROPPING RECV MESSAGE\n");
        return true;
    }

    if (strCommand == "version") {
        // Each connection can only send one version message
        if (pfrom->nVersion != 0) {
            pfrom->PushMessage("reject", strCommand, REJECT_DUPLICATE, string("Duplicate version message"));
            Misbehaving(pfrom->GetId(), 1);
            return false;
        }

        int64_t nTime;
        CAddress addrMe;
        CAddress addrFrom;
        uint64_t nNonce = 1;
        uint64_t nServiceInt;
        vRecv >> pfrom->nVersion >> nServiceInt >> nTime >> addrMe;
        pfrom->nServices = ServiceFlags(nServiceInt);
        if (pfrom->nServicesExpected & ~pfrom->nServices)
        {
            LogPrint("net", "peer=%d does not offer the expected services (%08x offered, %08x expected); disconnecting\n", pfrom->id, pfrom->nServices, pfrom->nServicesExpected);
            pfrom->PushMessage("reject", strCommand, REJECT_NONSTANDARD,
                               strprintf("Expected to offer services %08x", pfrom->nServicesExpected));
            pfrom->fDisconnect = true;
            return false;
        }
        if (pfrom->nVersion < ActiveProtocol()) {
            // disconnect from peers older than this proto version
            LogPrintf("peer=%d using obsolete version %i; disconnecting\n", pfrom->id, pfrom->nVersion);
            pfrom->PushMessage("reject", strCommand, REJECT_OBSOLETE,
                strprintf("Version must be %d or greater", ActiveProtocol()));
            pfrom->fDisconnect = true;
            return false;
        }

        if (pfrom->nVersion == 10300)
            pfrom->nVersion = 300;
        if (!vRecv.empty())
            vRecv >> addrFrom >> nNonce;
        if (!vRecv.empty()) {
            vRecv >> LIMITED_STRING(pfrom->strSubVer, 256);
            pfrom->cleanSubVer = SanitizeString(pfrom->strSubVer);
        }
        if (!vRecv.empty())
            vRecv >> pfrom->nStartingHeight;
        if (!vRecv.empty())
            vRecv >> pfrom->fRelayTxes; // set to true after we get the first filter* message
        else
            pfrom->fRelayTxes = true;

        // Disconnect if we connected to ourself
        if (nNonce == nLocalHostNonce && nNonce > 1) {
            LogPrintf("connected to self at %s, disconnecting\n", pfrom->addr.ToString());
            pfrom->fDisconnect = true;
            return true;
        }

        pfrom->addrLocal = addrMe;
        if (pfrom->fInbound && addrMe.IsRoutable()) {
            SeenLocal(addrMe);
        }

        // Be shy and don't send version until we hear
        if (pfrom->fInbound)
            pfrom->PushVersion();

        pfrom->fClient = !(pfrom->nServices & NODE_NETWORK);

        if((pfrom->nServices & NODE_WITNESS))
        {
            LOCK(cs_main);
            State(pfrom->GetId())->fHaveWitness = true;
        }

        // Potentially mark this peer as a preferred download peer.
        UpdatePreferredDownload(pfrom, State(pfrom->GetId()));

        // Change version
        pfrom->PushMessage("verack");
        pfrom->ssSend.SetVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        if (!pfrom->fInbound) {
            // Advertise our address
            if (fListen && !IsInitialBlockDownload()) {
                CAddress addr = GetLocalAddress(&pfrom->addr);
                if (addr.IsRoutable()) {
                    pfrom->PushAddress(addr);
                } else if (IsPeerAddrLocalGood(pfrom)) {
                    addr.SetIP(pfrom->addrLocal);
                    pfrom->PushAddress(addr);
                }
            }

            // Get recent addresses
            if (pfrom->fOneShot || pfrom->nVersion >= CADDR_TIME_VERSION || addrman.size() < 1000) {
                pfrom->PushMessage("getaddr");
                pfrom->fGetAddr = true;
            }
            addrman.Good(pfrom->addr);
        }

        // Relay alerts
        {
            LOCK(cs_mapAlerts);
            BOOST_FOREACH (PAIRTYPE(const uint256, CAlert) & item, mapAlerts)
                item.second.RelayTo(pfrom);
        }

        pfrom->fSuccessfullyConnected = true;

        string remoteAddr;
        if (fLogIPs)
            remoteAddr = ", peeraddr=" + pfrom->addr.ToString();

        LogPrintf("receive version message: %s: version %d, blocks=%d, us=%s, peer=%d%s\n",
            pfrom->cleanSubVer, pfrom->nVersion,
            pfrom->nStartingHeight, addrMe.ToString(), pfrom->id,
            remoteAddr);

        AddTimeData(pfrom->addr, nTime);
    }


    else if (pfrom->nVersion == 0) {
        // Must have a version message before anything else
        Misbehaving(pfrom->GetId(), 1);
        return false;
    }


    else if (strCommand == "verack") {
        pfrom->SetRecvVersion(min(pfrom->nVersion, PROTOCOL_VERSION));

        // Mark this node as currently connected, so we update its timestamp later.
        if (pfrom->fNetworkNode) {
            LOCK(cs_main);
            State(pfrom->GetId())->fCurrentlyConnected = true;
        }
    }


    else if (strCommand == "addr") {
        vector<CAddress> vAddr;
        vRecv >> vAddr;

        // Don't want addr from older versions unless seeding
        if (pfrom->nVersion < CADDR_TIME_VERSION && addrman.size() > 1000)
            return true;
        if (vAddr.size() > 1000) {
            Misbehaving(pfrom->GetId(), 20);
            return error("message addr size() = %u", vAddr.size());
        }

        // Store the new addresses
        vector<CAddress> vAddrOk;
        int64_t nNow = GetAdjustedTime();
        int64_t nSince = nNow - 10 * 60;
        BOOST_FOREACH (CAddress& addr, vAddr) {
            boost::this_thread::interruption_point();

            if ((addr.nServices & REQUIRED_SERVICES) != REQUIRED_SERVICES)
                continue;

            if (addr.nTime <= 100000000 || addr.nTime > nNow + 10 * 60)
                addr.nTime = nNow - 5 * 24 * 60 * 60;
            pfrom->AddAddressKnown(addr);
            bool fReachable = IsReachable(addr);
            if (addr.nTime > nSince && !pfrom->fGetAddr && vAddr.size() <= 10 && addr.IsRoutable()) {
                // Relay to a limited number of other nodes
                {
                    LOCK(cs_vNodes);
                    // Use deterministic randomness to send to the same nodes for 24 hours
                    // at a time so the setAddrKnowns of the chosen nodes prevent repeats
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint64_t hashAddr = addr.GetHash();
                    uint256 hashRand = hashSalt ^ (hashAddr << 32) ^ ((GetTime() + hashAddr) / (24 * 60 * 60));
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    multimap<uint256, CNode*> mapMix;
                    BOOST_FOREACH (CNode* pnode, vNodes) {
                        if (pnode->nVersion < CADDR_TIME_VERSION)
                            continue;
                        unsigned int nPointer;
                        memcpy(&nPointer, &pnode, sizeof(nPointer));
                        uint256 hashKey = hashRand ^ nPointer;
                        hashKey = Hash(BEGIN(hashKey), END(hashKey));
                        mapMix.insert(make_pair(hashKey, pnode));
                    }
                    int nRelayNodes = fReachable ? 2 : 1; // limited relaying of addresses outside our network(s)
                    for (multimap<uint256, CNode*>::iterator mi = mapMix.begin(); mi != mapMix.end() && nRelayNodes-- > 0; ++mi)
                        ((*mi).second)->PushAddress(addr);
                }
            }
            // Do not store addresses outside our network
            if (fReachable)
                vAddrOk.push_back(addr);
        }
        addrman.Add(vAddrOk, pfrom->addr, 2 * 60 * 60);
        if (vAddr.size() < 1000)
            pfrom->fGetAddr = false;
        if (pfrom->fOneShot)
            pfrom->fDisconnect = true;
    }


    else if (strCommand == "inv") {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ) {
            Misbehaving(pfrom->GetId(), 20);
            return error("message inv size() = %u", vInv.size());
        }

        LOCK(cs_main);

        std::vector<CInv> vToFetch;

        for (unsigned int nInv = 0; nInv < vInv.size(); nInv++) {
            const CInv& inv = vInv[nInv];

            boost::this_thread::interruption_point();
            pfrom->AddInventoryKnown(inv);

            bool fAlreadyHave = AlreadyHave(inv);
            LogPrint("net", "got inv: %s  %s peer=%d\n", inv.ToString(), fAlreadyHave ? "have" : "new", pfrom->id);

            if (!fAlreadyHave && !fImporting && !fReindex && inv.type != MSG_BLOCK)
                pfrom->AskFor(inv);

            //TODO get fetch flags and check witness
            if (inv.type == MSG_BLOCK) {
                UpdateBlockAvailability(pfrom->GetId(), inv.hash);
                if (!fAlreadyHave && !fImporting && !fReindex && !mapBlocksInFlight.count(inv.hash)) {
                    // Add this to the list of blocks to request
                    vToFetch.push_back(inv);
                    LogPrint("net", "getblocks (%d) %s to peer=%d\n", pindexBestHeader->nHeight, inv.hash.ToString(), pfrom->id);
                }
            }

            // Track requests for our stuff
            GetMainSignals().Inventory(inv.hash);

            if (pfrom->nSendSize > (SendBufferSize() * 2)) {
                Misbehaving(pfrom->GetId(), 50);
                return error("send buffer size() = %u", pfrom->nSendSize);
            }
        }

        if (!vToFetch.empty())
            pfrom->PushMessage("getdata", vToFetch);
    }


    else if (strCommand == "getdata") {
        vector<CInv> vInv;
        vRecv >> vInv;
        if (vInv.size() > MAX_INV_SZ) {
            Misbehaving(pfrom->GetId(), 20);
            return error("message getdata size() = %u", vInv.size());
        }

        if (fDebug || (vInv.size() != 1))
            LogPrint("net", "received getdata (%u invsz) peer=%d\n", vInv.size(), pfrom->id);

        if ((fDebug && vInv.size() > 0) || (vInv.size() == 1))
            LogPrint("net", "received getdata for: %s peer=%d\n", vInv[0].ToString(), pfrom->id);

        pfrom->vRecvGetData.insert(pfrom->vRecvGetData.end(), vInv.begin(), vInv.end());
        ProcessGetData(pfrom, chainparams.GetConsensus());
    }


    else if (strCommand == "getblocks" || strCommand == "getheaders") {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);

        // Find the last block the caller has in the main chain
        CBlockIndex* pindex = FindForkInGlobalIndex(chainActive, locator);

        // Send the rest of the chain
        if (pindex)
            pindex = chainActive.Next(pindex);

        int nLimit = 500;
        LogPrintf("getblocks %d to %s limit %d from peer=%d\n", (pindex ? pindex->nHeight : -1), hashStop == uint256(0) ? "end" : hashStop.ToString(), nLimit, pfrom->id);
        for (; pindex; pindex = chainActive.Next(pindex)) {
            if (pindex->GetBlockHash() == hashStop) {
                LogPrint("net", "  getblocks stopping at %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
                break;
            }
            pfrom->PushInventory(CInv(MSG_BLOCK, pindex->GetBlockHash()));
            if (--nLimit <= 0) {
                // When this block is requested, we'll send an inv that'll make them
                // getblocks the next batch of inventory.
                LogPrint("net", "  getblocks stopping at limit %d %s\n", pindex->nHeight, pindex->GetBlockHash().ToString());
                pfrom->hashContinue = pindex->GetBlockHash();
                break;
            }
        }
    }


    else if (strCommand == "headers" && chainparams.HeadersFirstSyncingActive()) {
        CBlockLocator locator;
        uint256 hashStop;
        vRecv >> locator >> hashStop;

        LOCK(cs_main);

        if (IsInitialBlockDownload())
            return true;

        CBlockIndex* pindex = NULL;
        if (locator.IsNull()) {
            // If locator is null, return the hashStop block
            BlockMap::iterator mi = mapBlockIndex.find(hashStop);
            if (mi == mapBlockIndex.end())
                return true;
            pindex = (*mi).second;
        } else {
            // Find the last block the caller has in the main chain
            pindex = FindForkInGlobalIndex(chainActive, locator);
            if (pindex)
                pindex = chainActive.Next(pindex);
        }

        // we must use CBlocks, as CBlockHeaders won't include the 0x00 nTx count at the end
        vector<CBlock> vHeaders;
        int nLimit = MAX_HEADERS_RESULTS;
        if (fDebug)
            LogPrintf("getheaders %d to %s from peer=%d\n", (pindex ? pindex->nHeight : -1), hashStop.ToString(), pfrom->id);
        for (; pindex; pindex = chainActive.Next(pindex)) {
            vHeaders.push_back(pindex->GetBlockHeader());
            if (--nLimit <= 0 || pindex->GetBlockHash() == hashStop)
                break;
        }
        pfrom->PushMessage("headers", vHeaders);
    }


    else if (strCommand == "tx") {
        vector<uint256> vWorkQueue;
        vector<uint256> vEraseQueue;
        CTransaction tx;

        //masternode signed transaction
        bool ignoreFees = false;
        CTxIn vin;
        vector<unsigned char> vchSig;

        vRecv >> tx;

        CInv inv(MSG_TX, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        LOCK(cs_main);

        bool fMissingInputs = false;
        CValidationState state;

        mapAlreadyAskedFor.erase(inv);

#       if defined(DEBUG_DUMP_STAKING_INFO)&&defined(DEBUG_DUMP_Message_TX)
        DEBUG_DUMP_Message_TX();
#       endif
        // Check for rejected
        if (AlreadyHave(inv))
            return true;

        if (AcceptToMemoryPool(mempool, state, tx, true, &fMissingInputs, false, ignoreFees)) {
            mempool.check(pcoinsTip);
            RelayTransaction(tx);
            vWorkQueue.push_back(inv.hash);

            LogPrint("mempool", "AcceptToMemoryPool: peer=%d %s : accepted %s (poolsz %u)\n",
                pfrom->id, pfrom->cleanSubVer,
                tx.GetHash().ToString(),
                mempool.mapTx.size());

            // Recursively process any orphan transactions that depended on this one
            set<NodeId> setMisbehaving;
            for (unsigned int i = 0; i < vWorkQueue.size(); i++) {
                map<uint256, set<uint256> >::iterator itByPrev = mapOrphanTransactionsByPrev.find(vWorkQueue[i]);
                if (itByPrev == mapOrphanTransactionsByPrev.end())
                    continue;
                for (set<uint256>::iterator mi = itByPrev->second.begin();
                     mi != itByPrev->second.end();
                     ++mi) {
                    const uint256& orphanHash = *mi;
                    const CTransaction& orphanTx = mapOrphanTransactions[orphanHash].tx;
                    NodeId fromPeer = mapOrphanTransactions[orphanHash].fromPeer;
                    bool fMissingInputs2 = false;
                    // Use a dummy CValidationState so someone can't setup nodes to counter-DoS based on orphan
                    // resolution (that is, feeding people an invalid transaction based on LegitTxX in order to get
                    // anyone relaying LegitTxX banned)
                    CValidationState stateDummy;

                    if (setMisbehaving.count(fromPeer))
                        continue;
                    if (AcceptToMemoryPool(mempool, stateDummy, orphanTx, true, &fMissingInputs2)) {
                        LogPrint("mempool", "   accepted orphan tx %s\n", orphanHash.ToString());
                        RelayTransaction(orphanTx);
                        vWorkQueue.push_back(orphanHash);
                        vEraseQueue.push_back(orphanHash);
                    } else if (!fMissingInputs2) {
                        int nDos = 0;
                        if (stateDummy.IsInvalid(nDos) && nDos > 0 && (!state.CorruptionPossible() || State(fromPeer)->fHaveWitness)) {
                            // Punish peer that gave us an invalid orphan tx
                            Misbehaving(fromPeer, nDos);
                            setMisbehaving.insert(fromPeer);
                            LogPrint("mempool", "   invalid orphan tx %s\n", orphanHash.ToString());
                        }
                        // Has inputs but not accepted to mempool
                        // Probably non-standard or insufficient fee/priority
                        LogPrint("mempool", "   removed orphan tx %s\n", orphanHash.ToString());
                        vEraseQueue.push_back(orphanHash);

                    }
                    mempool.check(pcoinsTip);
                }
            }

            BOOST_FOREACH (uint256 hash, vEraseQueue)
                EraseOrphanTx(hash);
        } else if (fMissingInputs) {
            AddOrphanTx(tx, pfrom->GetId());

            // DoS prevention: do not allow mapOrphanTransactions to grow unbounded
            unsigned int nMaxOrphanTx = (unsigned int)std::max((int64_t)0, GetArg("-maxorphantx", DEFAULT_MAX_ORPHAN_TRANSACTIONS));
            unsigned int nEvicted = LimitOrphanTxSize(nMaxOrphanTx);
            if (nEvicted > 0)
                LogPrint("mempool", "mapOrphan overflow, removed %u tx\n", nEvicted);
        } else if (pfrom->fWhitelisted) {
            // Always relay transactions received from whitelisted peers, even
            // if they are already in the mempool (allowing the node to function
            // as a gateway for nodes hidden behind it).

            RelayTransaction(tx);
        }

        int nDoS = 0;
        if (state.IsInvalid(nDoS)) {
            LogPrint("mempool", "%s from peer=%d %s was not accepted into the memory pool: %s\n", tx.GetHash().ToString(),
                pfrom->id, pfrom->cleanSubVer,
                state.GetRejectReason());
            pfrom->PushMessage("reject", strCommand, state.GetRejectCode(),
                state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), inv.hash);
            if (nDoS > 0 && (!state.CorruptionPossible() || State(pfrom->id)->fHaveWitness))
                Misbehaving(pfrom->GetId(), nDoS);
        }
    }


    else if (strCommand == "headers" && chainparams.HeadersFirstSyncingActive() && !fImporting && !fReindex) { // Ignore headers received while importing
        std::vector<CBlockHeader> headers;

        // Bypass the normal CBlock deserialization, as we don't want to risk deserializing 2000 full blocks.
        unsigned int nCount = ReadCompactSize(vRecv);
        if (nCount > MAX_HEADERS_RESULTS) {
            Misbehaving(pfrom->GetId(), 20);
            return error("headers message size = %u", nCount);
        }
        headers.resize(nCount);
        for (unsigned int n = 0; n < nCount; n++) {
            vRecv >> headers[n];
            ReadCompactSize(vRecv); // ignore tx count; assume it is 0.
        }

        LOCK(cs_main);

        if (nCount == 0) {
            // Nothing interesting. Stop asking this peers for more headers.
            return true;
        }
        CBlockIndex* pindexLast = NULL;
        BOOST_FOREACH (const CBlockHeader& header, headers) {
            CValidationState state;
            if (pindexLast != NULL && header.hashPrevBlock != pindexLast->GetBlockHash()) {
                Misbehaving(pfrom->GetId(), 20);
                return error("non-continuous headers sequence");
            }

            /*TODO: this has a CBlock cast on it so that it will compile. There should be a solution for this
             * before headers are reimplemented on mainnet
             */
            if (!AcceptBlockHeader((CBlock)header, state, chainparams, &pindexLast)) {
                int nDoS;
                if (state.IsInvalid(nDoS)) {
                    if (nDoS > 0)
                        Misbehaving(pfrom->GetId(), nDoS);
                    std::string strError = "invalid header received " + header.GetHash().ToString();
                    return error(strError.c_str());
                }
            }
        }

        if (pindexLast)
            UpdateBlockAvailability(pfrom->GetId(), pindexLast->GetBlockHash());

        if (nCount == MAX_HEADERS_RESULTS && pindexLast) {
            // Headers message had its maximum size; the peer may have more headers.
            // TODO: optimize: if pindexLast is an ancestor of chainActive.Tip(), continue
            // from there instead.
            LogPrintf("more getheaders (%d) to end to peer=%d (startheight:%d)\n", pindexLast->nHeight, pfrom->id, pfrom->nStartingHeight);
            pfrom->PushMessage("getheaders", chainActive.GetLocator(pindexLast), uint256(0));
        }

        CheckBlockIndex(chainparams.GetConsensus());
    }


    else if (strCommand == "block" && !fImporting && !fReindex) { // Ignore blocks received while importing
        CBlock block;
        vRecv >> block;

        BlockMap::iterator prev_block_it = mapBlockIndex.find(block.hashPrevBlock);
        bool usePhi2 = false;
        if (prev_block_it != mapBlockIndex.end()) {
            usePhi2 = prev_block_it->second->nHeight + 1 >= Params().SwitchPhi2Block();
        }

        uint256 hashBlock = block.GetHash(usePhi2);
        CInv inv(MSG_BLOCK, hashBlock);
        LogPrint("net", "received block %s peer=%d\n", inv.hash.ToString(), pfrom->id);

        //sometimes we will be sent their most recent block and its not the one we want, in that case tell where we are
        if (!mapBlockIndex.count(block.hashPrevBlock)) {
            if (find(pfrom->vBlockRequested.begin(), pfrom->vBlockRequested.end(), hashBlock) != pfrom->vBlockRequested.end()) {
                //we already asked for this block, so lets work backwards and ask for the previous block
                pfrom->PushMessage("getblocks", chainActive.GetLocator(), block.hashPrevBlock);
                pfrom->vBlockRequested.push_back(block.hashPrevBlock);
            } else {
                //ask to sync to this block
                pfrom->PushMessage("getblocks", chainActive.GetLocator(), hashBlock);
                pfrom->vBlockRequested.push_back(hashBlock);
            }
        } else {
            pfrom->AddInventoryKnown(inv);

            CValidationState state;
            ProcessNewBlock(state, chainparams, pfrom, &block);
            int nDoS;
            if (state.IsInvalid(nDoS)) {
                pfrom->PushMessage("reject", strCommand, state.GetRejectCode(),
                    state.GetRejectReason().substr(0, MAX_REJECT_MESSAGE_LENGTH), inv.hash);
                if (nDoS > 0) {
                    TRY_LOCK(cs_main, lockMain);
                    if (lockMain) Misbehaving(pfrom->GetId(), nDoS);
                }
            }
        }

    }


    // This asymmetric behavior for inbound and outbound connections was introduced
    // to prevent a fingerprinting attack: an attacker can send specific fake addresses
    // to users' AddrMan and later request them by sending getaddr messages.
    // Making users (which are behind NAT and can only make outgoing connections) ignore
    // getaddr message mitigates the attack.
    else if ((strCommand == "getaddr") && (pfrom->fInbound)) {
        pfrom->vAddrToSend.clear();
        vector<CAddress> vAddr = addrman.GetAddr();
        BOOST_FOREACH (const CAddress& addr, vAddr)
            pfrom->PushAddress(addr);
    }


    else if (strCommand == "mempool") {
        LOCK2(cs_main, pfrom->cs_filter);

        std::vector<uint256> vtxid;
        mempool.queryHashes(vtxid);
        vector<CInv> vInv;
        BOOST_FOREACH (uint256& hash, vtxid) {
            CInv inv(MSG_TX, hash);
            CTransaction tx;
            CTransactionRef ptx = mempool.get(hash);
            if (!ptx) continue;
            tx = *ptx.get();
            if ((pfrom->pfilter && pfrom->pfilter->IsRelevantAndUpdate(tx)) ||
                (!pfrom->pfilter))
                vInv.push_back(inv);
            if (vInv.size() == MAX_INV_SZ) {
                pfrom->PushMessage("inv", vInv);
                vInv.clear();
            }
        }
        if (vInv.size() > 0)
            pfrom->PushMessage("inv", vInv);
    }


    else if (strCommand == "ping") {
        if (pfrom->nVersion > BIP0031_VERSION) {
            uint64_t nonce = 0;
            vRecv >> nonce;
            // Echo the message back with the nonce. This allows for two useful features:
            //
            // 1) A remote node can quickly check if the connection is operational
            // 2) Remote nodes can measure the latency of the network thread. If this node
            //    is overloaded it won't respond to pings quickly and the remote node can
            //    avoid sending us more work, like chain download requests.
            //
            // The nonce stops the remote getting confused between different pings: without
            // it, if the remote node sends a ping once per second and this node takes 5
            // seconds to respond to each, the 5th ping the remote sends would appear to
            // return very quickly.
            pfrom->PushMessage("pong", nonce);
        }
    }


    else if (strCommand == "pong") {
        int64_t pingUsecEnd = nTimeReceived;
        uint64_t nonce = 0;
        size_t nAvail = vRecv.in_avail();
        bool bPingFinished = false;
        std::string sProblem;

        if (nAvail >= sizeof(nonce)) {
            vRecv >> nonce;

            // Only process pong message if there is an outstanding ping (old ping without nonce should never pong)
            if (pfrom->nPingNonceSent != 0) {
                if (nonce == pfrom->nPingNonceSent) {
                    // Matching pong received, this ping is no longer outstanding
                    bPingFinished = true;
                    int64_t pingUsecTime = pingUsecEnd - pfrom->nPingUsecStart;
                    if (pingUsecTime > 0) {
                        // Successful ping time measurement, replace previous
                        pfrom->nPingUsecTime = pingUsecTime;
                    } else {
                        // This should never happen
                        sProblem = "Timing mishap";
                    }
                } else {
                    // Nonce mismatches are normal when pings are overlapping
                    sProblem = "Nonce mismatch";
                    if (nonce == 0) {
                        // This is most likely a bug in another implementation somewhere, cancel this ping
                        bPingFinished = true;
                        sProblem = "Nonce zero";
                    }
                }
            } else {
                sProblem = "Unsolicited pong without ping";
            }
        } else {
            // This is most likely a bug in another implementation somewhere, cancel this ping
            bPingFinished = true;
            sProblem = "Short payload";
        }

        if (!(sProblem.empty())) {
            LogPrint("net", "pong peer=%d %s: %s, %x expected, %x received, %u bytes\n",
                pfrom->id,
                pfrom->cleanSubVer,
                sProblem,
                pfrom->nPingNonceSent,
                nonce,
                nAvail);
        }
        if (bPingFinished) {
            pfrom->nPingNonceSent = 0;
        }
    }


    else if (fAlerts && strCommand == "alert") {
        CAlert alert;
        vRecv >> alert;

        uint256 alertHash = alert.GetHash();
        if (pfrom->setKnown.count(alertHash) == 0) {
            if (alert.ProcessAlert()) {
                // Relay
                pfrom->setKnown.insert(alertHash);
                {
                    LOCK(cs_vNodes);
                    BOOST_FOREACH (CNode* pnode, vNodes)
                        alert.RelayTo(pnode);
                }
            } else {
                // Small DoS penalty so peers that send us lots of
                // duplicate/expired/invalid-signature/whatever alerts
                // eventually get banned.
                // This isn't a Misbehaving(100) (immediate ban) because the
                // peer might be an older or different implementation with
                // a different signature key, etc.
                Misbehaving(pfrom->GetId(), 10);
            }
        }
    }


    else if (!(nLocalServices & NODE_BLOOM) &&
             (strCommand == "filterload" ||
              strCommand == "filteradd" ||
              strCommand == "filterclear")) {
        LogPrintf("bloom message=%s\n", strCommand);
        Misbehaving(pfrom->GetId(), 100);
    }


    else if (strCommand == "filterload") {
        CBloomFilter filter;
        vRecv >> filter;

        if (!filter.IsWithinSizeConstraints())
            // There is no excuse for sending a too-large filter
            Misbehaving(pfrom->GetId(), 100);
        else {
            LOCK(pfrom->cs_filter);
            delete pfrom->pfilter;
            pfrom->pfilter = new CBloomFilter(filter);
            pfrom->pfilter->UpdateEmptyFull();
        }
        pfrom->fRelayTxes = true;
    }


    else if (strCommand == "filteradd") {
        vector<unsigned char> vData;
        vRecv >> vData;

        // Nodes must NEVER send a data item > 520 bytes (the max size for a script data object,
        // and thus, the maximum size any matched object can have) in a filteradd message
        if (vData.size() > MAX_SCRIPT_ELEMENT_SIZE) {
            Misbehaving(pfrom->GetId(), 100);
        } else {
            LOCK(pfrom->cs_filter);
            if (pfrom->pfilter)
                pfrom->pfilter->insert(vData);
            else
                Misbehaving(pfrom->GetId(), 100);
        }
    }


    else if (strCommand == "filterclear") {
        LOCK(pfrom->cs_filter);
        delete pfrom->pfilter;
        pfrom->pfilter = new CBloomFilter();
        pfrom->fRelayTxes = true;
    }


    else if (strCommand == "reject") {
        if (fDebug) {
            try {
                string strMsg;
                unsigned char ccode;
                string strReason;
                vRecv >> LIMITED_STRING(strMsg, CMessageHeader::COMMAND_SIZE) >> ccode >> LIMITED_STRING(strReason, MAX_REJECT_MESSAGE_LENGTH);

                ostringstream ss;
                ss << strMsg << " code " << itostr(ccode) << ": " << strReason;

                if (strMsg == "block" || strMsg == "tx") {
                    uint256 hash;
                    vRecv >> hash;
                    ss << ": hash " << hash.ToString();
                }
                LogPrint("net", "Reject %s\n", SanitizeString(ss.str()));
            } catch (std::ios_base::failure& e) {
                // Avoid feedback loops by preventing reject messages from triggering a new reject message.
                LogPrint("net", "Unparseable reject message received\n");
            }
        }
    }


    else {
        bool processed = false;
#       if 0
        if (!processed) darksendPool.ProcessMessage(pfrom, strCommand, vRecv, processed);
        if (!processed) mnodeman.ProcessMessage(pfrom, strCommand, vRecv, processed);
        if (!processed) budget.ProcessMessage(pfrom, strCommand, vRecv, processed);
        if (!processed) masternodePayments.ProcessMessage(pfrom, strCommand, vRecv, processed);
        if (!processed) ProcessInstanTX(pfrom, strCommand, vRecv, processed);
        if (!processed) ProcessSpork(pfrom, strCommand, vRecv, processed);
        if (!processed) masternodeSync.ProcessMessage(pfrom, strCommand, vRecv, processed);
#       else
        if (!processed) ProcessDarksend(pfrom, strCommand, vRecv, processed);
        if (!processed) ProcessMasternode(pfrom, strCommand, vRecv, processed);
        if (!processed) ProcessInstantX(pfrom, strCommand, vRecv, processed);
        if (!processed) ProcessSpork(pfrom, strCommand, vRecv, processed);
#       endif
    }

    return true;
}

int ActiveProtocol()
{
    return MIN_PEER_PROTO_VERSION_BEFORE_ENFORCEMENT;
}

// requires LOCK(cs_vRecvMsg)
bool ProcessMessages(CNode* pfrom)
{
    const CChainParams& chainparams = Params();
    //if (fDebug)
    //    LogPrintf("ProcessMessages(%u messages)\n", pfrom->vRecvMsg.size());

    //
    // Message format
    //  (4) message start
    //  (12) command
    //  (4) size
    //  (4) checksum
    //  (x) data
    //
    bool fOk = true;

    if (!pfrom->vRecvGetData.empty())
        ProcessGetData(pfrom, chainparams.GetConsensus());

    // this maintains the order of responses
    if (!pfrom->vRecvGetData.empty()) return fOk;

    std::deque<CNetMessage>::iterator it = pfrom->vRecvMsg.begin();
    while (!pfrom->fDisconnect && it != pfrom->vRecvMsg.end()) {
        // Don't bother if send buffer is too full to respond anyway
        if (pfrom->nSendSize >= SendBufferSize())
            break;

        // get next message
        CNetMessage& msg = *it;

        //if (fDebug)
        //    LogPrintf("ProcessMessages(message %u msgsz, %u bytes, complete:%s)\n",
        //            msg.hdr.nMessageSize, msg.vRecv.size(),
        //            msg.complete() ? "Y" : "N");

        // end, if an incomplete message is found
        if (!msg.complete())
            break;

        // at this point, any failure means we can delete the current message
        it++;

        // Scan for message start
        if (memcmp(msg.hdr.pchMessageStart, chainparams.MessageStart(), MESSAGE_START_SIZE) != 0) {
            LogPrintf("PROCESSMESSAGE: INVALID MESSAGESTART %s peer=%d\n", SanitizeString(msg.hdr.GetCommand()), pfrom->id);
            fOk = false;
            break;
        }

        // Read header
        CMessageHeader& hdr = msg.hdr;
        if (!hdr.IsValid()) {
            LogPrintf("PROCESSMESSAGE: ERRORS IN HEADER %s peer=%d\n", SanitizeString(hdr.GetCommand()), pfrom->id);
            continue;
        }
        string strCommand = hdr.GetCommand();

        // Message size
        unsigned int nMessageSize = hdr.nMessageSize;

        // Checksum
        CDataStream& vRecv = msg.vRecv;
        uint256 hash = Hash(vRecv.begin(), vRecv.begin() + nMessageSize);
        unsigned int nChecksum = 0;
        memcpy(&nChecksum, &hash, sizeof(nChecksum));
        if (nChecksum != hdr.nChecksum) {
            LogPrintf("ProcessMessages(%s, %u bytes): CHECKSUM ERROR nChecksum=%08x hdr.nChecksum=%08x\n",
                SanitizeString(strCommand), nMessageSize, nChecksum, hdr.nChecksum);
            continue;
        }

        // Process message
        bool fRet = false;
        try {
            fRet = ProcessMessage(pfrom, strCommand, vRecv, msg.nTime, chainparams);
            boost::this_thread::interruption_point();
        } catch (std::ios_base::failure& e) {
            pfrom->PushMessage("reject", strCommand, REJECT_MALFORMED, string("error parsing message"));
            if (strstr(e.what(), "end of data")) {
                // Allow exceptions from under-length message on vRecv
                LogPrintf("ProcessMessages(%s, %u bytes): Exception '%s' caught, normally caused by a message being shorter than its stated length\n", SanitizeString(strCommand), nMessageSize, e.what());
            } else if (strstr(e.what(), "size too large")) {
                // Allow exceptions from over-long size
                LogPrintf("ProcessMessages(%s, %u bytes): Exception '%s' caught\n", SanitizeString(strCommand), nMessageSize, e.what());
            } else {
                PrintExceptionContinue(&e, "ProcessMessages()");
            }
        } catch (boost::thread_interrupted) {
            throw;
        } catch (std::exception& e) {
            PrintExceptionContinue(&e, "ProcessMessages()");
        } catch (...) {
            PrintExceptionContinue(NULL, "ProcessMessages()");
        }

        if (!fRet)
            LogPrintf("ProcessMessage(%s, %u bytes) FAILED peer=%d\n", SanitizeString(strCommand), nMessageSize, pfrom->id);

        break;
    }

    // In case the connection got shut down, its receive buffer was wiped
    if (!pfrom->fDisconnect)
        pfrom->vRecvMsg.erase(pfrom->vRecvMsg.begin(), it);

    return fOk;
}


bool SendMessages(CNode* pto, bool fSendTrickle)
{
    const Consensus::Params& consensusParams = Params().GetConsensus();
    {
        // Don't send anything until we get their version message
        if (pto->nVersion == 0)
            return true;

        //
        // Message: ping
        //
        bool pingSend = false;
        if (pto->fPingQueued) {
            // RPC ping request by user
            pingSend = true;
        }
        if (pto->nPingNonceSent == 0 && pto->nPingUsecStart + PING_INTERVAL * 1000000 < GetTimeMicros()) {
            // Ping automatically sent as a latency probe & keepalive.
            pingSend = true;
        }
        if (pingSend) {
            uint64_t nonce = 0;
            while (nonce == 0) {
                GetRandBytes((unsigned char*)&nonce, sizeof(nonce));
            }
            pto->fPingQueued = false;
            pto->nPingUsecStart = GetTimeMicros();
            if (pto->nVersion > BIP0031_VERSION) {
                pto->nPingNonceSent = nonce;
                pto->PushMessage("ping", nonce);
            } else {
                // Peer is too old to support ping command with nonce, pong will never arrive.
                pto->nPingNonceSent = 0;
                pto->PushMessage("ping");
            }
        }

        TRY_LOCK(cs_main, lockMain); // Acquire cs_main for IsInitialBlockDownload() and CNodeState()
        if (!lockMain)
            return true;

        // Address refresh broadcast
        static int64_t nLastRebroadcast;
        if (!IsInitialBlockDownload() && (GetTime() - nLastRebroadcast > 24 * 60 * 60)) {
            LOCK(cs_vNodes);
            BOOST_FOREACH (CNode* pnode, vNodes) {
                // Periodically clear setAddrKnown to allow refresh broadcasts
                if (nLastRebroadcast)
                    pnode->setAddrKnown.clear();

                // Rebroadcast our address
                AdvertizeLocal(pnode);
            }
            if (!vNodes.empty())
                nLastRebroadcast = GetTime();
        }

        //
        // Message: addr
        //
        if (fSendTrickle) {
            vector<CAddress> vAddr;
            vAddr.reserve(pto->vAddrToSend.size());
            BOOST_FOREACH (const CAddress& addr, pto->vAddrToSend) {
                // returns true if wasn't already contained in the set
                if (pto->setAddrKnown.insert(addr).second) {
                    vAddr.push_back(addr);
                    // receiver rejects addr messages larger than 1000
                    if (vAddr.size() >= 1000) {
                        pto->PushMessage("addr", vAddr);
                        vAddr.clear();
                    }
                }
            }
            pto->vAddrToSend.clear();
            if (!vAddr.empty())
                pto->PushMessage("addr", vAddr);
        }

        CNodeState& state = *State(pto->GetId());
        if (state.fShouldBan) {
            if (pto->fWhitelisted)
                LogPrintf("Warning: not punishing whitelisted peer %s!\n", pto->addr.ToString());
            else {
                pto->fDisconnect = true;
                if (pto->addr.IsLocal())
                    LogPrintf("Warning: not banning local peer %s!\n", pto->addr.ToString());
                else {
                    CNode::Ban(pto->addr, BanMisbehaving);
                }
            }
            state.fShouldBan = false;
        }

        BOOST_FOREACH (const CBlockReject& reject, state.rejects)
            pto->PushMessage("reject", (string) "block", reject.chRejectCode, reject.strRejectReason, reject.hashBlock);
        state.rejects.clear();

        // Start block sync
        if (pindexBestHeader == NULL)
            pindexBestHeader = chainActive.Tip();
        bool fFetch = state.fPreferredDownload || (nPreferredDownload == 0 && !pto->fClient && !pto->fOneShot); // Download if this is a nice peer, or we have no nice peers and this one might do.
        if (!state.fSyncStarted && !pto->fClient && fFetch /*&& !fImporting*/ && !fReindex) {
            // Only actively request headers from a single peer, unless we're close to end of initial download.
            if (nSyncStarted == 0 || pindexBestHeader->GetBlockTime() > GetAdjustedTime() - 6 * 60 * 60) { // NOTE: was "close to today" and 24h in Bitcoin
                state.fSyncStarted = true;
                nSyncStarted++;
                //CBlockIndex *pindexStart = chainActive.Tip()->pprev ? chainActive.Tip()->pprev : chainActive.Tip();
                //LogPrint("net", "initial getheaders (%d) to peer=%d (startheight:%d)\n", pindexStart->nHeight, pto->id, pto->nStartingHeight);
                //pto->PushMessage("getheaders", chainActive.GetLocator(pindexStart), uint256(0));
                pto->PushMessage("getblocks", chainActive.GetLocator(chainActive.Tip()), uint256(0));
            }
        }

        // Resend wallet transactions that haven't gotten in a block yet
        // Except during reindex, importing and IBD, when old wallet
        // transactions become unconfirmed and spams other nodes.
        if (!fReindex /*&& !fImporting && !IsInitialBlockDownload()*/) {
            GetMainSignals().Broadcast(nTimeBestReceived);
        }

        //
        // Message: inventory
        //
        vector<CInv> vInv;
        vector<CInv> vInvWait;
        {
            LOCK(pto->cs_inventory);
            vInv.reserve(pto->vInventoryToSend.size());
            vInvWait.reserve(pto->vInventoryToSend.size());
            BOOST_FOREACH (const CInv& inv, pto->vInventoryToSend) {
                if (pto->setInventoryKnown.count(inv))
                    continue;

                // trickle out tx inv to protect privacy
                if (inv.type == MSG_TX && !fSendTrickle) {
                    // 1/4 of tx invs blast to all immediately
                    static uint256 hashSalt;
                    if (hashSalt == 0)
                        hashSalt = GetRandHash();
                    uint256 hashRand = inv.hash ^ hashSalt;
                    hashRand = Hash(BEGIN(hashRand), END(hashRand));
                    bool fTrickleWait = ((hashRand & 3) != 0);

                    if (fTrickleWait) {
                        vInvWait.push_back(inv);
                        continue;
                    }
                }

                // returns true if wasn't already contained in the set
                if (pto->setInventoryKnown.insert(inv).second) {
                    vInv.push_back(inv);
                    if (vInv.size() >= 1000) {
                        pto->PushMessage("inv", vInv);
                        vInv.clear();
                    }
                }
            }
            pto->vInventoryToSend = vInvWait;
        }
        if (!vInv.empty())
            pto->PushMessage("inv", vInv);

        // Detect whether we're stalling
        int64_t nNow = GetTimeMicros();
        if (!pto->fDisconnect && state.nStallingSince && state.nStallingSince < nNow - 1000000 * BLOCK_STALLING_TIMEOUT) {
            // Stalling only triggers when the block download window cannot move. During normal steady state,
            // the download window should be much larger than the to-be-downloaded set of blocks, so disconnection
            // should only happen during initial block download.
            LogPrintf("Peer=%d is stalling block download, disconnecting\n", pto->id);
            pto->fDisconnect = true;
        }
        // In case there is a block that has been in flight from this peer for (2 + 0.5 * N) times the block interval
        // (with N the number of validated blocks that were in flight at the time it was requested), disconnect due to
        // timeout. We compensate for in-flight blocks to prevent killing off peers due to our own downstream link
        // being saturated. We only count validated in-flight blocks so peers can't advertize nonexisting block hashes
        // to unreasonably increase our timeout.
        if (!pto->fDisconnect && state.vBlocksInFlight.size() > 0 && state.vBlocksInFlight.front().nTime < nNow - 500000 * Params().TargetSpacing() * (4 + state.vBlocksInFlight.front().nValidatedQueuedBefore)) {
            LogPrintf("Timeout downloading block %s from peer=%d, disconnecting\n", state.vBlocksInFlight.front().hash.ToString(), pto->id);
            pto->fDisconnect = true;
        }

        //
        // Message: getdata (blocks)
        //
        vector<CInv> vGetData;
        if (!pto->fDisconnect && !pto->fClient && fFetch && state.nBlocksInFlight < MAX_BLOCKS_IN_TRANSIT_PER_PEER) {
            vector<CBlockIndex*> vToDownload;
            NodeId staller = -1;
            FindNextBlocksToDownload(pto->GetId(), MAX_BLOCKS_IN_TRANSIT_PER_PEER - state.nBlocksInFlight, vToDownload, staller);
            BOOST_FOREACH (CBlockIndex* pindex, vToDownload) {
                if (State(pto->GetId())->fHaveWitness || !IsWitnessEnabled(pindex->pprev, consensusParams)) {
                    uint32_t nFetchFlags = GetFetchFlags(pto, pindex->pprev, consensusParams);
                    vGetData.push_back(CInv(MSG_BLOCK | nFetchFlags, pindex->GetBlockHash()));
                    MarkBlockAsInFlight(pto->GetId(), pindex->GetBlockHash(), consensusParams, pindex);
                    LogPrintf("Requesting block %s (%d) peer=%d\n", pindex->GetBlockHash().ToString(),
                              pindex->nHeight, pto->id);
                }
            }
            if (state.nBlocksInFlight == 0 && staller != -1) {
                if (State(staller)->nStallingSince == 0) {
                    State(staller)->nStallingSince = nNow;
                    LogPrint("net", "Stall started peer=%d\n", staller);
                }
            }
        }

        //
        // Message: getdata (non-blocks)
        //
        while (!pto->fDisconnect && !pto->mapAskFor.empty() && (*pto->mapAskFor.begin()).first <= nNow) {
            const CInv& inv = (*pto->mapAskFor.begin()).second;
            if (!AlreadyHave(inv)) {
                if (fDebug)
                    LogPrint("net", "Requesting %s peer=%d\n", inv.ToString(), pto->id);
                vGetData.push_back(inv);
                if (vGetData.size() >= 1000) {
                    pto->PushMessage("getdata", vGetData);
                    vGetData.clear();
                }
            }
            pto->mapAskFor.erase(pto->mapAskFor.begin());
        }
        if (!vGetData.empty())
            pto->PushMessage("getdata", vGetData);
    }
    return true;
}


bool CBlockUndo::WriteToDisk(CDiskBlockPos& pos, const uint256& hashBlock)
{
    // Open history file to append
    CAutoFile fileout(OpenUndoFile(pos), SER_DISK, CLIENT_VERSION);
    if (fileout.IsNull())
        return error("CBlockUndo::WriteToDisk : OpenUndoFile failed");

    // Write index header
    unsigned int nSize = fileout.GetSerializeSize(*this);
    fileout << FLATDATA(Params().MessageStart()) << nSize;

    // Write undo data
    long fileOutPos = ftell(fileout.Get());
    if (fileOutPos < 0)
        return error("CBlockUndo::WriteToDisk : ftell failed");
    pos.nPos = (unsigned int)fileOutPos;
    fileout << *this;

    // calculate & write checksum
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << *this;
    fileout << hasher.GetHash();

    return true;
}

bool CBlockUndo::ReadFromDisk(const CDiskBlockPos& pos, const uint256& hashBlock)
{
    // Open history file to read
    CAutoFile filein(OpenUndoFile(pos, true), SER_DISK, CLIENT_VERSION);
    if (filein.IsNull())
        return error("CBlockUndo::ReadFromDisk : OpenBlockFile failed");

    // Read block
    uint256 hashChecksum;
    try {
        filein >> *this;
        filein >> hashChecksum;
    } catch (std::exception& e) {
        return error("%s : Deserialize or I/O error - %s", __func__, e.what());
    }

    // Verify checksum
    CHashWriter hasher(SER_GETHASH, PROTOCOL_VERSION);
    hasher << hashBlock;
    hasher << *this;
    if (hashChecksum != hasher.GetHash())
        return error("CBlockUndo::ReadFromDisk : Checksum mismatch");

    return true;
}

std::string CBlockFileInfo::ToString() const
{
    return strprintf("CBlockFileInfo(blocks=%u, size=%u, heights=%u...%u, time=%s...%s)", nBlocks, nSize, nHeightFirst, nHeightLast, DateTimeStrFormat("%Y-%m-%d", nTimeFirst), DateTimeStrFormat("%Y-%m-%d", nTimeLast));
}

int GetSpendHeight(const CCoinsViewCache& inputs)
{
    LOCK(cs_main);
    CBlockIndex* pindexPrev = mapBlockIndex.find(inputs.GetBestBlock())->second;
    return pindexPrev->nHeight + 1;
}

class CMainCleanup
{
public:
    CMainCleanup() {}
    ~CMainCleanup()
    {
        // block headers
        BlockMap::iterator it1 = mapBlockIndex.begin();
        for (; it1 != mapBlockIndex.end(); it1++)
            delete (*it1).second;
        mapBlockIndex.clear();

        // orphan transactions
        mapOrphanTransactions.clear();
        mapOrphanTransactionsByPrev.clear();
    }
} instance_of_cmaincleanup;


/////////////////////////////////////////////////////////////////////// lux
bool CheckSenderScript(const CCoinsViewCache& view, const CTransaction& tx){
    CScript script = view.AccessCoins(tx.vin[0].prevout.hash)->vout[tx.vin[0].prevout.n].scriptPubKey;
    if(!script.IsPayToPubkeyHash() && !script.IsPayToPubkey()){
        return false;
    }
    return true;
}

std::vector<ResultExecute> CallContract(const dev::Address& addrContract, std::vector<unsigned char> opcode, const dev::Address& sender, uint64_t gasLimit){
    CBlock block;
    CMutableTransaction tx;
    const CChainParams& chainparams = Params();
    CBlockIndex* pblockindex = mapBlockIndex[chainActive.Tip()->GetBlockHash()];
    ReadBlockFromDisk(block, pblockindex, chainparams.GetConsensus());
    block.nTime = GetAdjustedTime();

    if(block.IsProofOfStake())
        block.vtx.erase(block.vtx.begin()+2,block.vtx.end());
    else
        block.vtx.erase(block.vtx.begin()+1,block.vtx.end());


    LuxDGP luxDGP(globalState.get(), fGettingValuesDGP);
    uint64_t blockGasLimit = luxDGP.getBlockGasLimit(chainActive.Tip()->nHeight + 1);

    if(gasLimit == 0){
        gasLimit = blockGasLimit - 1;
    }
    dev::Address senderAddress = sender == dev::Address() ? dev::Address("ffffffffffffffffffffffffffffffffffffffff") : sender;
    tx.vout.push_back(CTxOut(0, CScript() << OP_DUP << OP_HASH160 << senderAddress.asBytes() << OP_EQUALVERIFY << OP_CHECKSIG));
    block.vtx.push_back(CTransaction(tx));

    LuxTransaction callTransaction(0, 1, dev::u256(gasLimit), addrContract, opcode, dev::u256(0));
    callTransaction.forceSender(senderAddress);
    callTransaction.setVersion(VersionVM::GetEVMDefault());


    ByteCodeExec exec(block, std::vector<LuxTransaction>(1, callTransaction), blockGasLimit);
    exec.performByteCode(dev::eth::Permanence::Reverted);
    return exec.getResult();
}

bool CheckMinGasPrice(std::vector<EthTransactionParams>& etps, const uint64_t& minGasPrice){
    for(EthTransactionParams& etp : etps){
        if(etp.gasPrice < dev::u256(minGasPrice))
            return false;
    }
    return true;
}

valtype GetSenderAddress(const CTransaction& tx, const CCoinsViewCache* coinsView, const std::vector<CTransaction>* blockTxs){
    CScript script;
    bool scriptFilled=false; //can't use script.empty() because an empty script is technically valid
    const CChainParams& chainparams = Params();

    // First check the current (or in-progress) block for zero-confirmation change spending that won't yet be in txindex
    if(blockTxs){
        for(auto btx : *blockTxs){
            if(btx.GetHash() == tx.vin[0].prevout.hash){
                script = btx.vout[tx.vin[0].prevout.n].scriptPubKey;
                scriptFilled=true;
                break;
            }
        }
    }
    if(!scriptFilled && coinsView){
        script = coinsView->AccessCoins(tx.vin[0].prevout.hash)->vout[0].scriptPubKey;
        scriptFilled = true;
    }
    if(!scriptFilled)
    {
        CTransaction txPrevout;
        uint256 hashBlock;
        if(GetTransaction(tx.vin[0].prevout.hash, txPrevout, chainparams.GetConsensus(), hashBlock, true)){
            script = txPrevout.vout[tx.vin[0].prevout.n].scriptPubKey;
        } else {
            LogPrintf("Error fetching transaction details of tx %s. This will probably cause more errors", tx.vin[0].prevout.hash.ToString());
            return valtype();
        }
    }

    CTxDestination addressBit;
    txnouttype txType=TX_NONSTANDARD;
    if(ExtractDestination(script, addressBit, &txType)){
        if ((txType == TX_PUBKEY || txType == TX_PUBKEYHASH) &&
            addressBit.type() == typeid(CKeyID)){
            CKeyID senderAddress(boost::get<CKeyID>(addressBit));
            return valtype(senderAddress.begin(), senderAddress.end());
        }
    }
    //prevout is not a standard transaction format, so just return 0
    return valtype();
}

UniValue vmLogToJSON(const ResultExecute& execRes, const CTransaction& tx, const CBlock& block){
    UniValue result(UniValue::VOBJ);
    if(tx != CTransaction())
        result.push_back(Pair("txid", tx.GetHash().GetHex()));
    result.push_back(Pair("address", execRes.execRes.newAddress.hex()));

    BlockMap::iterator prev_block_it = mapBlockIndex.find(block.hashPrevBlock);
    bool usePhi2 = false;
    if (prev_block_it != mapBlockIndex.end()) {
        usePhi2 = prev_block_it->second->nHeight + 1 >= Params().SwitchPhi2Block();
    }

    if(block.GetHash(usePhi2) != CBlock().GetHash(usePhi2)){
        result.push_back(Pair("time", block.GetBlockTime()));
        result.push_back(Pair("blockhash", block.GetHash(usePhi2).GetHex()));
        result.push_back(Pair("blockheight", chainActive.Tip()->nHeight + 1));
    } else {
        result.push_back(Pair("time", GetAdjustedTime()));
        result.push_back(Pair("blockheight", chainActive.Tip()->nHeight));
    }
    UniValue logEntries(UniValue::VARR);
    dev::eth::LogEntries logs = execRes.txRec.log();
    for(dev::eth::LogEntry log : logs){
        UniValue logEntrie(UniValue::VOBJ);
        logEntrie.push_back(Pair("address", log.address.hex()));
        UniValue topics(UniValue::VARR);
        for(dev::h256 l : log.topics){
            UniValue topicPair(UniValue::VOBJ);
            topicPair.push_back(Pair("raw", l.hex()));
            topics.push_back(topicPair);
            //TODO add "pretty" field for human readable data
        }
        UniValue dataPair(UniValue::VOBJ);
        dataPair.push_back(Pair("raw", HexStr(log.data)));
        logEntrie.push_back(Pair("data", dataPair));
        logEntrie.push_back(Pair("topics", topics));
        logEntries.push_back(logEntrie);
    }
    result.push_back(Pair("entries", logEntries));
    return result;
}

void writeVMlog(const std::vector<ResultExecute>& res, const CTransaction& tx, const CBlock& block){
    boost::filesystem::path luxDir = GetDataDir() / "vmExecLogs.json";
    std::stringstream ss;
    if(fIsVMlogFile){
        ss << ",";
    } else {
        std::ofstream file(luxDir.string(), std::ios::out | std::ios::app);
        file << "{\"logs\":[]}";
        file.close();
    }

    for(size_t i = 0; i < res.size(); i++){
        ss << vmLogToJSON(res[i], tx, block).write();
        if(i != res.size() - 1){
            ss << ",";
        } else {
            ss << "]}";
        }
    }

    std::ofstream file(luxDir.string(), std::ios::in | std::ios::out);
    file.seekp(-2, std::ios::end);
    file << ss.str();
    file.close();
    fIsVMlogFile = true;
}

bool ByteCodeExec::performByteCode(dev::eth::Permanence type){
    for(LuxTransaction& tx : txs){
        //validate VM version
        if(tx.getVersion().toRaw() != VersionVM::GetEVMDefault().toRaw()){
            return false;
        }
        dev::eth::EnvInfo envInfo(BuildEVMEnvironment());
        if(!tx.isCreation() && !globalState->addressInUse(tx.receiveAddress())){
            dev::eth::ExecutionResult execRes;
            execRes.excepted = dev::eth::TransactionException::Unknown;
            result.push_back(ResultExecute{execRes, dev::eth::TransactionReceipt(dev::h256(), dev::u256(), dev::eth::LogEntries()), CTransaction()});
            continue;
        }
        result.push_back(globalState->execute(envInfo, *globalSealEngine.get(), tx, type, OnOpFunc()));
    }
    globalState->db().commit();
    globalState->dbUtxo().commit();
    globalSealEngine.get()->deleteAddresses.clear();
    return true;
}

bool ByteCodeExec::processingResults(ByteCodeExecResult& resultBCE){
    for(size_t i = 0; i < result.size(); i++){
        uint64_t gasUsed = (uint64_t) result[i].execRes.gasUsed;
        if(result[i].execRes.excepted != dev::eth::TransactionException::None){
            if(txs[i].value() > 0){
                CMutableTransaction tx;
                tx.vin.push_back(CTxIn(h256Touint(txs[i].getHashWith()), txs[i].getNVout(), CScript() << OP_SPEND));
                CScript script(CScript() << OP_DUP << OP_HASH160 << txs[i].sender().asBytes() << OP_EQUALVERIFY << OP_CHECKSIG);
                tx.vout.push_back(CTxOut(CAmount(txs[i].value()), script));
                resultBCE.valueTransfers.push_back(CTransaction(tx));
            }
            resultBCE.usedGas += gasUsed;
        } else {
            if(txs[i].gas() > UINT64_MAX ||
               result[i].execRes.gasUsed > UINT64_MAX ||
               txs[i].gasPrice() > UINT64_MAX){
                return false;
            }
            uint64_t gas = (uint64_t) txs[i].gas();
            uint64_t gasPrice = (uint64_t) txs[i].gasPrice();

            resultBCE.usedGas += gasUsed;
            int64_t amount = (gas - gasUsed) * gasPrice;
            if(amount < 0){
                return false;
            }
            if(amount > 0){
                CScript script(CScript() << OP_DUP << OP_HASH160 << txs[i].sender().asBytes() << OP_EQUALVERIFY << OP_CHECKSIG);
                resultBCE.refundOutputs.push_back(CTxOut(amount, script));
                resultBCE.refundSender += amount;
            }
        }
        if(result[i].tx != CTransaction()){
            resultBCE.valueTransfers.push_back(result[i].tx);
        }
    }
    return true;
}

dev::eth::EnvInfo ByteCodeExec::BuildEVMEnvironment(){
    dev::eth::EnvInfo env;
    CBlockIndex* tip = chainActive.Tip();
    env.setNumber(dev::u256(tip->nHeight + 1));
    env.setTimestamp(dev::u256(block.nTime));
    env.setDifficulty(dev::u256(block.nBits));

    dev::eth::LastHashes lh;
    lh.resize(256);
    for(int i=0;i<256;i++){
        if(!tip)
            break;
        lh[i]= uintToh256(*tip->phashBlock);
        tip = tip->pprev;
    }
    env.setLastHashes(std::move(lh));
    env.setGasLimit(blockGasLimit);
    if(block.IsProofOfStake()){
        env.setAuthor(EthAddrFromScript(block.vtx[1].vout[1].scriptPubKey));
    }else {
        env.setAuthor(EthAddrFromScript(block.vtx[0].vout[0].scriptPubKey));
    }
    return env;
}

dev::Address ByteCodeExec::EthAddrFromScript(const CScript& script){
    CTxDestination addressBit;
    txnouttype txType=TX_NONSTANDARD;
    if(ExtractDestination(script, addressBit, &txType)){
        if ((txType == TX_PUBKEY || txType == TX_PUBKEYHASH) &&
            addressBit.type() == typeid(CKeyID)){
            CKeyID addressKey(boost::get<CKeyID>(addressBit));
            std::vector<unsigned char> addr(addressKey.begin(), addressKey.end());
            return dev::Address(addr);
        }
    }
    //if not standard or not a pubkey or pubkeyhash output, then return 0
    return dev::Address();
}

bool LuxTxConverter::extractionLuxTransactions(ExtractLuxTX& luxtx){
    std::vector<LuxTransaction> resultTX;
    std::vector<EthTransactionParams> resultETP;
    for(size_t i = 0; i < txBit.vout.size(); i++){
        if(txBit.vout[i].scriptPubKey.HasOpCreate() || txBit.vout[i].scriptPubKey.HasOpCall()){
            if(receiveStack(txBit.vout[i].scriptPubKey)){
                EthTransactionParams params;
                if(parseEthTXParams(params)){
                    resultTX.push_back(createEthTX(params, i));
                    resultETP.push_back(params);
                }else{
                    return false;
                }
            }else{
                return false;
            }
        }
    }
    luxtx = std::make_pair(resultTX, resultETP);
    return true;
}

bool LuxTxConverter::receiveStack(const CScript& scriptPubKey){
    EvalScript(stack, scriptPubKey, SCRIPT_EXEC_BYTE_CODE, BaseSignatureChecker(), SIGVERSION_BASE, nullptr);
    if (stack.empty())
        return false;

    CScript scriptRest(stack.back().begin(), stack.back().end());
    stack.pop_back();

    opcode = (opcodetype)(*scriptRest.begin());
    if((opcode == OP_CREATE && stack.size() < 4) || (opcode == OP_CALL && stack.size() < 5)){
        stack.clear();
        return false;
    }

    return true;
}

bool LuxTxConverter::parseEthTXParams(EthTransactionParams& params){
    try{
        dev::Address receiveAddress;
        valtype vecAddr;
        if (opcode == OP_CALL)
        {
            vecAddr = stack.back();
            stack.pop_back();
            receiveAddress = dev::Address(vecAddr);
        }
        if(stack.size() < 4)
            return false;

        if(stack.back().size() < 1){
            return false;
        }
        valtype code(stack.back());
        stack.pop_back();
        uint64_t gasPrice = CScriptNum::vch_to_uint64(stack.back());
        stack.pop_back();
        uint64_t gasLimit = CScriptNum::vch_to_uint64(stack.back());
        stack.pop_back();
        if(gasPrice > INT64_MAX || gasLimit > INT64_MAX){
            return false;
        }
        //we track this as CAmount in some places, which is an int64_t, so constrain to INT64_MAX
        if(gasPrice !=0 && gasLimit > INT64_MAX / gasPrice){
            //overflows past 64bits, reject this tx
            return false;
        }
        if(stack.back().size() > 4){
            return false;
        }
        VersionVM version = VersionVM::fromRaw((uint32_t)CScriptNum::vch_to_uint64(stack.back()));
        stack.pop_back();
        params.version = version;
        params.gasPrice = dev::u256(gasPrice);
        params.receiveAddress = receiveAddress;
        params.code = code;
        params.gasLimit = dev::u256(gasLimit);
        return true;
    }
    catch(const scriptnum_error& err){
        LogPrintf("Incorrect parameters to VM.");
        return false;
    }
}

LuxTransaction LuxTxConverter::createEthTX(const EthTransactionParams& etp, uint32_t nOut){
    LuxTransaction txEth;
    if (etp.receiveAddress == dev::Address() && opcode != OP_CALL){
        txEth = LuxTransaction(txBit.vout[nOut].nValue, etp.gasPrice, etp.gasLimit, etp.code, dev::u256(0));
    }
    else{
        txEth = LuxTransaction(txBit.vout[nOut].nValue, etp.gasPrice, etp.gasLimit, etp.receiveAddress, etp.code, dev::u256(0));
    }
    dev::Address sender(GetSenderAddress(txBit, view, blockTransactions));
    txEth.forceSender(sender);
    txEth.setHashWith(uintToh256(txBit.GetHash()));
    txEth.setNVout(nOut);
    txEth.setVersion(etp.version);

    return txEth;
}
///////////////////////////////////////////////////////////////////////
