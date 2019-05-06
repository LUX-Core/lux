// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_VALIDATION_H
#define BITCOIN_VALIDATION_H

#if defined(HAVE_CONFIG_H)
#include <config/lux-config.h>
#endif

#include <amount.h>
#include <coins.h>
#include <fs.h>
#include <protocol.h> // For CMessageHeader::MessageStartChars
#include <policy/feerate.h>
#include <script/script_error.h>
#include <sync.h>
#include <versionbits.h>

#include <algorithm>
#include <exception>
#include <map>
#include <memory>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <atomic>

/**
 * Global LuxState
 */

/////////////////////////////////////////// lux
#include <lux/luxstate.h>
#include <lux/luxDGP.h>
#include <libethereum/ChainParams.h>
#include <libethashseal/Ethash.h>
#include <libethashseal/GenesisInfo.h>
#include <script/standard.h>
#include <lux/storageresults.h>
///////////////////////////////////////////

extern std::unique_ptr<LuxState> globalState;
extern std::shared_ptr<dev::eth::SealEngineFace> globalSealEngine;
extern bool fRecordLogOpcodes;
extern bool fIsVMlogFile;
extern bool fGettingValuesDGP;

struct EthTransactionParams;
using valtype = std::vector<unsigned char>;
using ExtractLuxTX = std::pair<std::vector<LuxTransaction>, std::vector<EthTransactionParams>>;
///////////////////////////////////////////

class CBlockIndex;
class CBlockTreeDB;
class CChainParams;
class CCoinsViewDB;
class CInv;
class CConnman;
class CScriptCheck;
class CBlockPolicyEstimator;
class CTxMemPool;
class CValidationState;
struct ChainTxData;

struct PrecomputedTransactionData;
struct LockPoints;

/** Default for -whitelistrelay. */
static const bool DEFAULT_WHITELISTRELAY = true;
/** Default for -whitelistforcerelay. */
static const bool DEFAULT_WHITELISTFORCERELAY = true;
/** Default for -minrelaytxfee, minimum relay fee for transactions */
static const unsigned int DEFAULT_MIN_RELAY_TX_FEE = 1000;
/** Default for -limitancestorcount, max number of in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_LIMIT = 25;
/** Default for -limitancestorsize, maximum kilobytes of tx + all in-mempool ancestors */
static const unsigned int DEFAULT_ANCESTOR_SIZE_LIMIT = 101;
/** Default for -limitdescendantcount, max number of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_LIMIT = 25;
/** Default for -limitdescendantsize, maximum kilobytes of in-mempool descendants */
static const unsigned int DEFAULT_DESCENDANT_SIZE_LIMIT = 101;
/** Default for -mempoolexpiry, expiration time for mempool transactions in hours */
static const unsigned int DEFAULT_MEMPOOL_EXPIRY = 336;
/** Maximum kilobytes for transactions to store for processing during reorg */
static const unsigned int MAX_DISCONNECTED_TX_POOL_SIZE = 20000;
/** The maximum size of a blk?????.dat file (since 0.8) */
static const unsigned int MAX_BLOCKFILE_SIZE = 0x8000000; // 128 MiB
/** The pre-allocation chunk size for blk?????.dat files (since 0.8) */
static const unsigned int BLOCKFILE_CHUNK_SIZE = 0x1000000; // 16 MiB
/** The pre-allocation chunk size for rev?????.dat files (since 0.8) */
static const unsigned int UNDOFILE_CHUNK_SIZE = 0x100000; // 1 MiB

/** Maximum number of script-checking threads allowed */
static const int MAX_SCRIPTCHECK_THREADS = 16;
/** -par default (number of script-checking threads, 0 = auto) */
static const int DEFAULT_SCRIPTCHECK_THREADS = 0;
/** Number of blocks that can be requested at any given time from a single peer. */
static const int MAX_BLOCKS_IN_TRANSIT_PER_PEER = 16;
/** Timeout in seconds during which a peer must stall block download progress before being disconnected. */
static const unsigned int BLOCK_STALLING_TIMEOUT = 2;
/** Number of headers sent in one getheaders result. We rely on the assumption that if a peer sends
 *  less than this number, we reached its tip. Changing this value is a protocol upgrade. */
static const unsigned int MAX_HEADERS_RESULTS = 2000;
/** Maximum depth of blocks we're willing to serve as compact blocks to peers
 *  when requested. For older blocks, a regular BLOCK response will be sent. */
static const int MAX_CMPCTBLOCK_DEPTH = 5;
/** Maximum depth of blocks we're willing to respond to GETBLOCKTXN requests for. */
static const int MAX_BLOCKTXN_DEPTH = 10;
/** Size of the "block download window": how far ahead of our current height do we fetch?
 *  Larger windows tolerate larger download speed differences between peer, but increase the potential
 *  degree of disordering of blocks on disk (which make reindexing and pruning harder). We'll probably
 *  want to make this a per-peer adaptive value at some point. */
static const unsigned int BLOCK_DOWNLOAD_WINDOW = 1024;
/** Time to wait (in seconds) between writing blocks/block index to disk. */
static const unsigned int DATABASE_WRITE_INTERVAL = 60 * 60;
/** Time to wait (in seconds) between flushing chainstate to disk. */
static const unsigned int DATABASE_FLUSH_INTERVAL = 24 * 60 * 60;
/** Maximum length of reject messages. */
static const unsigned int MAX_REJECT_MESSAGE_LENGTH = 111;
/** Average delay between local address broadcasts in seconds. */
static const unsigned int AVG_LOCAL_ADDRESS_BROADCAST_INTERVAL = 24 * 60 * 60;
/** Average delay between peer address broadcasts in seconds. */
static const unsigned int AVG_ADDRESS_BROADCAST_INTERVAL = 30;
/** Average delay between trickled inventory transmissions in seconds.
 *  Blocks and whitelisted receivers bypass this, outbound peers get half this delay. */
static const unsigned int INVENTORY_BROADCAST_INTERVAL = 5;
/** Maximum number of inventory items to send per transmission.
 *  Limits the impact of low-fee transaction floods. */
static const unsigned int INVENTORY_BROADCAST_MAX = 7 * INVENTORY_BROADCAST_INTERVAL;
/** Average delay between feefilter broadcasts in seconds. */
static const unsigned int AVG_FEEFILTER_BROADCAST_INTERVAL = 10 * 60;
/** Maximum feefilter broadcast delay after significant change. */
static const unsigned int MAX_FEEFILTER_CHANGE_DELAY = 5 * 60;
/** Block download timeout base, expressed in millionths of the block interval (i.e. 10 min) */
static const int64_t BLOCK_DOWNLOAD_TIMEOUT_BASE = 1000000;
/** Additional block download timeout per parallel downloading peer (i.e. 5 min) */
static const int64_t BLOCK_DOWNLOAD_TIMEOUT_PER_PEER = 500000;

static const int64_t DEFAULT_MAX_TIP_AGE = 24 * 60 * 60;
/** Maximum age of our tip in seconds for us to be considered current for fee estimation */
static const int64_t MAX_FEE_ESTIMATION_TIP_AGE = 3 * 60 * 60;

/** Default for -permitbaremultisig */
static const bool DEFAULT_PERMIT_BAREMULTISIG = true;
static const bool DEFAULT_CHECKPOINTS_ENABLED = true;
static const bool DEFAULT_TXINDEX = false;
static const unsigned int DEFAULT_BANSCORE_THRESHOLD = 100;
/** Default for -persistmempool */
static const bool DEFAULT_PERSIST_MEMPOOL = true;
/** Default for -mempoolreplacement */
static const bool DEFAULT_ENABLE_REPLACEMENT = true;
/** Default for using fee filter */
static const bool DEFAULT_FEEFILTER = true;

/** Maximum number of headers to announce when relaying blocks with headers message.*/
static const unsigned int MAX_BLOCKS_TO_ANNOUNCE = 8;

/** Maximum number of unconnecting headers announcements before DoS score */
static const int MAX_UNCONNECTING_HEADERS = 10;

static const bool DEFAULT_PEERBLOOMFILTERS = true;

/** Default for -stopatheight */
static const int DEFAULT_STOPATHEIGHT = 0;

////////////////////////////////////////////////////// lux
static const uint64_t DEFAULT_GAS_LIMIT_OP_CREATE=2500000;
static const uint64_t DEFAULT_GAS_LIMIT_OP_SEND=250000;
static const CAmount DEFAULT_GAS_PRICE=0.00000040*COIN;
static const CAmount MAX_RPC_GAS_PRICE=0.00000100*COIN;

static const size_t MAX_CONTRACT_VOUTS = 1000;

/** Minimum gas limit that is allowed in a transaction within a block - prevent various types of tx and mempool spam **/
static const uint64_t MINIMUM_GAS_LIMIT = 10000;

static const uint64_t MEMPOOL_MIN_GAS_LIMIT = 22000;

/** The maximum allowed size for a serialized block, in bytes (only for buffer size limits) */
extern unsigned int dgpMaxBlockSerSize;
/** The maximum allowed weight for a block, see BIP 141 (network rule) */
extern unsigned int dgpMaxBlockWeight;
/** The maximum allowed size for a block excluding witness data, in bytes (network rule) */
extern unsigned int dgpMaxBlockBaseSize;

extern unsigned int dgpMaxBlockSize; // qtum

/** The maximum allowed number of signature check operations in a block (network rule) */
extern int64_t dgpMaxBlockSigOps;

extern unsigned int dgpMaxProtoMsgLength;

extern unsigned int dgpMaxTxSigOps;
void updateBlockSizeParams(unsigned int newBlockSize);


//////////////////////////////////////////////////////

struct BlockHasher
{
    size_t operator()(const uint256& hash) const { return hash.GetCheapHash(); }
};

extern CScript COINBASE_FLAGS;
extern CCriticalSection cs_main;
extern CBlockPolicyEstimator feeEstimator;
extern CTxMemPool mempool;
typedef std::unordered_map<uint256, CBlockIndex*, BlockHasher> BlockMap;
extern BlockMap& mapBlockIndex;
extern uint64_t nLastBlockTx;
extern uint64_t nLastBlockWeight;
extern const std::string strMessageMagic;
extern CWaitableCriticalSection csBestBlock;
extern CConditionVariable cvBlockChange;
extern uint256 hashBestBlock;
extern std::atomic_bool fImporting;
extern std::atomic_bool fReindex;
extern int nScriptCheckThreads;
extern bool fTxIndex;
extern bool fIsBareMultisigStd;
extern bool fRequireStandard;
extern bool fCheckBlockIndex;
extern bool fCheckpointsEnabled;
extern size_t nCoinCacheUsage;
/** A fee rate smaller than this is considered zero fee (for relaying, mining and transaction creation) */
extern CFeeRate minRelayTxFee;
/** Absolute maximum transaction fee (in satoshis) used by wallet and mempool (rejects high fee in sendrawtransaction) */
extern CAmount maxTxFee;
/** If the tip is older than this (in seconds), the node is considered to be in initial block download. */
extern int64_t nMaxTipAge;
extern bool fEnableReplacement;

/** Block hash whose ancestors we will assume to have valid scripts without checking them. */
extern uint256 hashAssumeValid;

/** Minimum work we will assume exists on some valid chain. */
extern arith_uint256 nMinimumChainWork;

/** Best header we've seen so far (used for getheaders queries' starting points). */
extern CBlockIndex *pindexBestHeader;

/** Minimum disk space required - used in CheckDiskSpace() */
static const uint64_t nMinDiskSpace = 52428800;

/** Pruning-related variables and constants */
/** True if any block files have ever been pruned. */
extern bool fHavePruned;
/** True if we're running in -prune mode. */
extern bool fPruneMode;
/** Number of MiB of block files that we're trying to stay below. */
extern uint64_t nPruneTarget;
/** Block files containing a block-height within MIN_BLOCKS_TO_KEEP of chainActive.Tip() will not be pruned. */
static const unsigned int MIN_BLOCKS_TO_KEEP = 288;
/** Minimum blocks required to signal NODE_NETWORK_LIMITED */
static const unsigned int NODE_NETWORK_LIMITED_MIN_BLOCKS = 288;

static const signed int DEFAULT_CHECKBLOCKS = 6;
static const unsigned int DEFAULT_CHECKLEVEL = 3;

// Require that user allocate at least 550MB for block & undo files (blk???.dat and rev???.dat)
// At 1MB per block, 288 blocks = 288MB.
// Add 15% for Undo data = 331MB
// Add 20% for Orphan block rate = 397MB
// We want the low water mark after pruning to be at least 397 MB and since we prune in
// full block file chunks, we need the high water mark which triggers the prune to be
// one 128MB block file + added 15% undo data = 147MB greater for a total of 545MB
// Setting the target to > than 550MB will make it likely we can respect the target.
static const uint64_t MIN_DISK_SPACE_FOR_BLOCK_FILES = 550 * 1024 * 1024;

/** 
 * Process an incoming block. This only returns after the best known valid
 * block is made active. Note that it does not, however, guarantee that the
 * specific block passed to it has been checked for validity!
 *
 * If you want to *possibly* get feedback on whether pblock is valid, you must
 * install a CValidationInterface (see validationinterface.h) - this will have
 * its BlockChecked method called whenever *any* block completes validation.
 *
 * Note that we guarantee that either the proof-of-work is valid on pblock, or
 * (and possibly also) BlockChecked will have been called.
 * 
 * Call without cs_main held.
 *
 * @param[in]   pblock  The block we want to process.
 * @param[in]   fForceProcessing Process this block even if unrequested; used for non-network block sources and whitelisted peers.
 * @param[out]  fNewBlock A boolean which is set to indicate if the block was first received via this call
 * @return True if state.IsValid()
 */
bool ProcessNewBlock(const CChainParams& chainparams, const std::shared_ptr<const CBlock> pblock, bool fForceProcessing, bool* fNewBlock);

/**
 * Process incoming block headers.
 *
 * Call without cs_main held.
 *
 * @param[in]  block The block headers themselves
 * @param[out] state This may be set to an Error state if any error occurred processing them
 * @param[in]  chainparams The params for the chain we want to connect to
 * @param[out] ppindex If set, the pointer will be set to point to the last new block index object for the given headers
 * @param[out] first_invalid First header that fails validation, if one exists
 */
bool ProcessNewBlockHeaders(const std::vector<CBlockHeader>& block, CValidationState& state, const CChainParams& chainparams, const CBlockIndex** ppindex=nullptr, CBlockHeader *first_invalid=nullptr);

/** Check whether enough disk space is available for an incoming block */
bool CheckDiskSpace(uint64_t nAdditionalBytes = 0);
/** Open a block file (blk?????.dat) */
FILE* OpenBlockFile(const CDiskBlockPos &pos, bool fReadOnly = false);
/** Translation to a filesystem path */
fs::path GetBlockPosFilename(const CDiskBlockPos &pos, const char *prefix);
/** Import blocks from an external file */
bool LoadExternalBlockFile(const CChainParams& chainparams, FILE* fileIn, CDiskBlockPos *dbp = nullptr);
/** Ensures we have a genesis block in the block tree, possibly writing one to disk. */
bool LoadGenesisBlock(const CChainParams& chainparams);
/** Load the block tree and coins database from disk,
 * initializing state if we're running with -reindex. */
bool LoadBlockIndex(const CChainParams& chainparams);
/** Update the chain tip based on database information. */
bool LoadChainTip(const CChainParams& chainparams);
/** Unload database information */
void UnloadBlockIndex();
/** Run an instance of the script checking thread */
void ThreadScriptCheck();
/** Check whether we are doing an initial block download (synchronizing from disk or network) */
bool IsInitialBlockDownload();
/** Retrieve a transaction (from memory pool, or from disk, if possible) */
bool GetTransaction(const uint256& hash, CTransactionRef& tx, const Consensus::Params& params, uint256& hashBlock, bool fAllowSlow = false, CBlockIndex* blockIndex = nullptr);
/** Find the best known block, and make it the tip of the block chain */
bool ActivateBestChain(CValidationState& state, const CChainParams& chainparams, std::shared_ptr<const CBlock> pblock = std::shared_ptr<const CBlock>());
CAmount GetBlockSubsidy(int nHeight, const Consensus::Params& consensusParams);

/** Guess verification progress (as a fraction between 0.0=genesis and 1.0=current tip). */
double GuessVerificationProgress(const ChainTxData& data, const CBlockIndex* pindex);

/** Calculate the amount of disk space the block & undo files currently use */
uint64_t CalculateCurrentUsage();

/**
 *  Mark one block file as pruned.
 */
void PruneOneBlockFile(const int fileNumber);

/**
 *  Actually unlink the specified files
 */
void UnlinkPrunedFiles(const std::set<int>& setFilesToPrune);

/** Flush all state, indexes and buffers to disk. */
void FlushStateToDisk();
/** Prune block files and flush state to disk. */
void PruneAndFlush();
/** Prune block files up to a given height */
void PruneBlockFilesManual(int nManualPruneHeight);

/** (try to) add transaction to memory pool
 * plTxnReplaced will be appended to with all transactions replaced from mempool **/
bool AcceptToMemoryPool(CTxMemPool& pool, CValidationState &state, const CTransactionRef &tx,
                        bool* pfMissingInputs, std::list<CTransactionRef>* plTxnReplaced,
                        bool bypass_limits, const CAmount nAbsurdFee);

//////////////////////////////////////////////////////////// // lux
struct CHeightTxIndexIteratorKey {
    unsigned int height;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 4;
    }
    template<typename Stream>
    void Serialize(Stream& s) const {
        ser_writedata32be(s, height);
    }
    template<typename Stream>
    void Unserialize(Stream& s) {
        height = ser_readdata32be(s);
    }

    CHeightTxIndexIteratorKey(unsigned int _height) {
        height = _height;
    }

    CHeightTxIndexIteratorKey() {
        SetNull();
    }

    void SetNull() {
        height = 0;
    }
};

struct CHeightTxIndexKey {
    unsigned int height;
    dev::h160 address;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 24;
    }
    template<typename Stream>
    void Serialize(Stream& s) const {
        ser_writedata32be(s, height);
        s << address.asBytes();
    }
    template<typename Stream>
    void Unserialize(Stream& s) {
        height = ser_readdata32be(s);
        valtype tmp;
        s >> tmp;
        address = dev::h160(tmp);
    }

    CHeightTxIndexKey(unsigned int _height, dev::h160 _address) {
        height = _height;
        address = _address;
    }

    CHeightTxIndexKey() {
        SetNull();
    }

    void SetNull() {
        height = 0;
        address.clear();
    }
};

////////////////////////////////////////////////////////////

/** Convert CValidationState to a human-readable message for logging */
std::string FormatStateMessage(const CValidationState &state);

/** Get the BIP9 state for a given deployment at the current tip. */
ThresholdState VersionBitsTipState(const Consensus::Params& params, Consensus::DeploymentPos pos);

/** Get the numerical statistics for the BIP9 state for a given deployment at the current tip. */
BIP9Stats VersionBitsTipStatistics(const Consensus::Params& params, Consensus::DeploymentPos pos);

/** Get the block height at which the BIP9 deployment switched into the state for the block building on the current tip. */
int VersionBitsTipStateSinceHeight(const Consensus::Params& params, Consensus::DeploymentPos pos);


/** Apply the effects of this transaction on the UTXO set represented by view */
void UpdateCoins(const CTransaction& tx, CCoinsViewCache& inputs, int nHeight);

/** Transaction validation functions */

/**
 * Check if transaction will be final in the next block to be created.
 *
 * Calls IsFinalTx() with current block height and appropriate block time.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckFinalTx(const CTransaction &tx, int flags = -1);

/**
 * Test whether the LockPoints height and time are still valid on the current chain
 */
bool TestLockPointValidity(const LockPoints* lp);

/**
 * Check if transaction will be BIP 68 final in the next block to be created.
 *
 * Simulates calling SequenceLocks() with data from the tip of the current active chain.
 * Optionally stores in LockPoints the resulting height and time calculated and the hash
 * of the block needed for calculation or skips the calculation and uses the LockPoints
 * passed in for evaluation.
 * The LockPoints should not be considered valid if CheckSequenceLocks returns false.
 *
 * See consensus/consensus.h for flag definitions.
 */
bool CheckSequenceLocks(const CTransaction &tx, int flags, LockPoints* lp = nullptr, bool useExistingLockPoints = false);

/**
 * Closure representing one script verification
 * Note that this stores references to the spending transaction 
 */
class CScriptCheck
{
private:
    CTxOut m_tx_out;
    const CTransaction *ptxTo;
    unsigned int nIn;
    unsigned int nFlags;
    bool cacheStore;
    ScriptError error;
    PrecomputedTransactionData *txdata;

public:
    CScriptCheck(): ptxTo(nullptr), nIn(0), nFlags(0), cacheStore(false), error(SCRIPT_ERR_UNKNOWN_ERROR) {}
    CScriptCheck(const CTxOut& outIn, const CTransaction& txToIn, unsigned int nInIn, unsigned int nFlagsIn, bool cacheIn, PrecomputedTransactionData* txdataIn) :
        m_tx_out(outIn), ptxTo(&txToIn), nIn(nInIn), nFlags(nFlagsIn), cacheStore(cacheIn), error(SCRIPT_ERR_UNKNOWN_ERROR), txdata(txdataIn) { }

    bool operator()();

    void swap(CScriptCheck &check) {
        std::swap(ptxTo, check.ptxTo);
        std::swap(m_tx_out, check.m_tx_out);
        std::swap(nIn, check.nIn);
        std::swap(nFlags, check.nFlags);
        std::swap(cacheStore, check.cacheStore);
        std::swap(error, check.error);
        std::swap(txdata, check.txdata);
    }

    ScriptError GetScriptError() const { return error; }
};

/** Initializes the script-execution cache */
void InitScriptExecutionCache();


/** Functions for disk access for blocks */
bool ReadBlockFromDisk(CBlock& block, const CDiskBlockPos& pos, const Consensus::Params& consensusParams);
bool ReadBlockFromDisk(CBlock& block, const CBlockIndex* pindex, const Consensus::Params& consensusParams);

/** Functions for validating blocks and updating the block tree */

/** Context-independent validity checks */
bool CheckBlock(const CBlock& block, CValidationState& state, const Consensus::Params& consensusParams, bool fCheckPOW = true, bool fCheckMerkleRoot = true);

/** Check a block is completely valid from start to finish (only works on top of our current best block, with cs_main held) */
bool TestBlockValidity(CValidationState& state, const CChainParams& chainparams, const CBlock& block, CBlockIndex* pindexPrev, bool fCheckPOW = true, bool fCheckMerkleRoot = true);

/** Check whether witness commitments are required for block. */
bool IsWitnessEnabled(const CBlockIndex* pindexPrev, const Consensus::Params& params);

/** When there are blocks in the active chain with missing data, rewind the chainstate and remove them from the block index */
bool RewindBlockIndex(const CChainParams& params);

/** Update uncommitted block structures (currently: only the witness nonce). This is safe for submitted blocks. */
void UpdateUncommittedBlockStructures(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams);

/** Produce the necessary coinbase commitment for a block (modifies the hash, don't call for mined blocks). */
std::vector<unsigned char> GenerateCoinbaseCommitment(CBlock& block, const CBlockIndex* pindexPrev, const Consensus::Params& consensusParams);

/** RAII wrapper for VerifyDB: Verify consistency of the block and coin databases */
class CVerifyDB {
public:
    CVerifyDB();
    ~CVerifyDB();
    bool VerifyDB(const CChainParams& chainparams, CCoinsView *coinsview, int nCheckLevel, int nCheckDepth);
};

inline CBlockIndex* LookupBlockIndex(const uint256& hash)
{
    AssertLockHeld(cs_main);
    BlockMap::const_iterator it = mapBlockIndex.find(hash);
    return it == mapBlockIndex.end() ? nullptr : it->second;
}

/** Replay blocks that aren't fully applied to the database. */
bool ReplayBlocks(const CChainParams& params, CCoinsView* view);

/** Find the last common block between the parameter chain and a locator. */
CBlockIndex* FindForkInGlobalIndex(const CChain& chain, const CBlockLocator& locator);

/** Mark a block as precious and reorganize. */
bool PreciousBlock(CValidationState& state, const CChainParams& params, CBlockIndex *pindex);

/** Mark a block as invalid. */
bool InvalidateBlock(CValidationState& state, const CChainParams& chainparams, CBlockIndex *pindex);

/** Remove invalidity status from a block and its descendants. */
bool ResetBlockFailureFlags(CBlockIndex *pindex);

/** The currently-connected chain of blocks (protected by cs_main). */
extern CChain& chainActive;

/** Global variable that points to the coins database (protected by cs_main) */
extern std::unique_ptr<CCoinsViewDB> pcoinsdbview;

/** Global variable that points to the active CCoinsView (protected by cs_main) */
extern std::unique_ptr<CCoinsViewCache> pcoinsTip;

/** Global variable that points to the active block tree (protected by cs_main) */
extern std::unique_ptr<CBlockTreeDB> pblocktree;

/**
 * Return the spend height, which is one more than the inputs.GetBestBlock().
 * While checking, GetBestBlock() refers to the parent block. (protected by cs_main)
 * This is also true for mempool checks.
 */
int GetSpendHeight(const CCoinsViewCache& inputs);

extern VersionBitsCache versionbitscache;

/**
 * Determine what nVersion a new block should use.
 */
int32_t ComputeBlockVersion(const CBlockIndex* pindexPrev, const Consensus::Params& params);

/** Reject codes greater or equal to this can be returned by AcceptToMemPool
 * for transactions, to signal internal conditions. They cannot and should not
 * be sent over the P2P network.
 */
static const unsigned int REJECT_INTERNAL = 0x100;
/** Too high fee. Can not be triggered by P2P transactions */
static const unsigned int REJECT_HIGHFEE = 0x100;

/** Get block file info entry for one block file */
CBlockFileInfo* GetBlockFileInfo(size_t n);

/** Dump the mempool to disk. */
bool DumpMempool();

/** Load the mempool from disk. */
bool LoadMempool();

int GetInputAge(CTxIn& vin);
int GetIXConfirmations(uint256 nTXHash);
bool GetCoinAge(const CTransaction& tx, const unsigned int nTxTime, uint64_t& nCoinAge);

//////////////////////////////////////////////////////// lux
std::vector<ResultExecute> CallContract(const dev::Address& addrContract, std::vector<unsigned char> opcode, const dev::Address& sender = dev::Address(), uint64_t gasLimit=0);

bool CheckSenderScript(const CCoinsViewCache& view, const CTransaction& tx);

bool CheckMinGasPrice(std::vector<EthTransactionParams>& etps, const uint64_t& minGasPrice);

struct ByteCodeExecResult;

void EnforceContractVoutLimit(ByteCodeExecResult& bcer, ByteCodeExecResult& bcerOut, const dev::h256& oldHashLuxRoot,
                              const dev::h256& oldHashStateRoot, const std::vector<LuxTransaction>& transactions);

void writeVMlog(const std::vector<ResultExecute>& res, const CTransaction& tx = CTransaction(), const CBlock& block = CBlock());

struct EthTransactionParams{
    VersionVM version;
    dev::u256 gasLimit;
    dev::u256 gasPrice;
    valtype code;
    dev::Address receiveAddress;

    bool operator!=(EthTransactionParams etp){
        if(this->version.toRaw() != etp.version.toRaw() || this->gasLimit != etp.gasLimit ||
           this->gasPrice != etp.gasPrice || this->code != etp.code ||
           this->receiveAddress != etp.receiveAddress)
            return true;
        return false;
    }
};

struct ByteCodeExecResult{
    uint64_t usedGas = 0;
    CAmount refundSender = 0;
    std::vector<CTxOut> refundOutputs;
    std::vector<CTransaction> valueTransfers;
};

class LuxTxConverter{

public:

    LuxTxConverter(CTransaction tx, CCoinsViewCache* v = NULL, const std::vector<CTransaction>* blockTxs = NULL) : txBit(tx), view(v), blockTransactions(blockTxs){}

    bool extractionLuxTransactions(ExtractLuxTX& luxTx);

private:

    bool receiveStack(const CScript& scriptPubKey);

    bool parseEthTXParams(EthTransactionParams& params);

    LuxTransaction createEthTX(const EthTransactionParams& etp, const uint32_t nOut);

    const CTransaction txBit;
    const CCoinsViewCache* view;
    std::vector<valtype> stack;
    opcodetype opcode;
    const std::vector<CTransaction> *blockTransactions;

};

class ByteCodeExec {

public:

    ByteCodeExec(const CBlock& _block, std::vector<LuxTransaction> _txs, const uint64_t _blockGasLimit) : txs(_txs), block(_block), blockGasLimit(_blockGasLimit) {}

    bool performByteCode(dev::eth::Permanence type = dev::eth::Permanence::Committed);

    bool processingResults(ByteCodeExecResult& result);

    std::vector<ResultExecute>& getResult(){ return result; }

private:

    dev::eth::EnvInfo BuildEVMEnvironment();

    dev::Address EthAddrFromScript(const CScript& scriptIn);

    std::vector<LuxTransaction> txs;

    std::vector<ResultExecute> result;

    const CBlock& block;

    const uint64_t blockGasLimit;

};
////////////////////////////////////////////////////////

#endif // BITCOIN_VALIDATION_H
