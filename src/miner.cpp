// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "coins.h"
#include "consensus/consensus.h"
#include "consensus/merkle.h"
#include "consensus/validation.h"
#include "hash.h"
//#include "validation.h"
#include "net.h"
#include "policy/policy.h"
#include "pow.h"
#include "stake.h"
#include "primitives/transaction.h"
#include "script/standard.h"
#include "timedata.h"
#include "txmempool.h"
#include "util.h"
#include "utilmoneystr.h"
#include "validationinterface.h"
#include "masternode.h"

#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <algorithm>
#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>
#include <queue>
#include <utility>

void FormatHashBuffers(CBlock* pblock, char* pmidstate, char* pdata)
{
    struct {
        struct unnamed
        {
            int nVersion;
            uint256 hashPrevBlock;
            uint256 hashMerkleRoot;
            unsigned int nTime;
            unsigned int nBits;
            unsigned int nNonce;
            uint256 hashStateRoot;
            uint256 hashUTXORoot;
        } block;
    } tmp;

    memset(&tmp, 0, sizeof(tmp));

    tmp.block.nVersion       = pblock->nVersion;
    tmp.block.hashPrevBlock  = pblock->hashPrevBlock;
    tmp.block.hashMerkleRoot = pblock->hashMerkleRoot;
    tmp.block.nTime          = pblock->nTime;
    tmp.block.nBits          = pblock->nBits;
    tmp.block.nNonce         = pblock->nNonce;
    // SC fields
    tmp.block.hashStateRoot  = pblock->hashStateRoot;
    tmp.block.hashUTXORoot   = pblock->hashUTXORoot;

    // Byte swap all the input buffer
    for (unsigned int i = 0; i < sizeof(tmp)/4; i++)
        ((unsigned int*)&tmp)[i] = ByteReverse(((unsigned int*)&tmp)[i]);

    // Precalc the first half of the first hash algo, which stays constant
    // midstate requires cubehash on phi2, but this field is not used...
    sph_cubehash512_context ctx_cubehash;
    sph_cubehash512_init(&ctx_cubehash);
    sph_cubehash512(&ctx_cubehash, pdata, 64);
    memcpy(pmidstate, ctx_cubehash.state, 128);

    if (pblock->nVersion & (1 << 30))
        memcpy(pdata, &tmp.block, 144);
    else
        memcpy(pdata, &tmp.block, 80);
}

//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;
uint64_t nLastBlockWeight = 0;
int64_t nLastCoinStakeSearchInterval = 0;
//unsigned int nMinerSleep = STAKER_POLLING_PERIOD;

class ScoreCompare
{
public:
    ScoreCompare() {}

    bool operator()(const CTxMemPool::txiter a, const CTxMemPool::txiter b)
    {
        return CompareTxMemPoolEntryByScore()(*b,*a); // Convert to less than
    }
};

int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev, bool isProofOfStake)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime = std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime());

    if (nOldTime < nNewTime)
        pblock->nTime = nNewTime;

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams, isProofOfStake);

    return nNewTime - nOldTime;
}

BlockAssembler::BlockAssembler(const CChainParams& _chainparams)
        : chainparams(_chainparams)
{
    // Block resource limits
    // If neither -blockmaxsize or -blockmaxweight is given, limit to DEFAULT_BLOCK_MAX_*
    // If only one is given, only restrict the specified resource.
    // If both are given, restrict both.
    nBlockMaxWeight = DEFAULT_BLOCK_MAX_WEIGHT;
    nBlockMaxSize = DEFAULT_BLOCK_MAX_SIZE;
    bool fWeightSet = false;
    if (IsArgSet("-blockmaxweight")) {
        nBlockMaxWeight = GetArg("-blockmaxweight", DEFAULT_BLOCK_MAX_WEIGHT);
        nBlockMaxSize = dgpMaxBlockSerSize;
        fWeightSet = true;
    }
    if (IsArgSet("-blockmaxsize")) {
        nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
        if (!fWeightSet) {
            nBlockMaxWeight = nBlockMaxSize * WITNESS_SCALE_FACTOR;
        }
    }
    if (IsArgSet("-blockmintxfee")) {
        CAmount n = 0;
        ParseMoney(GetArg("-blockmintxfee", ""), n);
        blockMinFeeRate = CFeeRate(n);
    } else {
        blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);
    }

    // Limit weight to between 4K and dgpMaxBlockWeight-4K for sanity:
    nBlockMaxWeight = std::max((unsigned int)4000, std::min((unsigned int)(dgpMaxBlockWeight-4000), nBlockMaxWeight));
    // Limit size to between 1K and dgpMaxBlockSerSize-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(dgpMaxBlockSerSize-1000), nBlockMaxSize));
    // Whether we need to account for byte usage (in addition to weight usage)
    fNeedSizeAccounting = (nBlockMaxSize < dgpMaxBlockSerSize-1000);
}

void BlockAssembler::resetBlock()
{
    inBlock.clear();

    // Reserve space for coinbase tx
    nBlockSize = 1000;
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;
    fIncludeWitness = false;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFees = 0;

    lastFewTxs = 0;
    blockFinished = false;
}

void BlockAssembler::RebuildRefundTransaction(){
    int refundtx=0; //0 for coinbase in PoW
    CMutableTransaction contrTx(originalRewardTx);
    if (!pblock->IsProofOfStake()) {
        refundtx=0;

        CAmount powReward = GetProofOfWorkReward(0, nHeight);
        CAmount totalReward = powReward + nFees;
        CAmount minerReward = 0;
        CAmount mnReward = 0;
        CScript mnPayee;

        if (nHeight >= chainparams.FirstSplitRewardBlock() && SelectMasternodePayee(mnPayee)) {
            contrTx.vout.resize(2);
            // set masternode payee and 20% reward
            mnReward = powReward * 0.2;
            contrTx.vout[1].scriptPubKey = mnPayee;
            contrTx.vout[1].nValue = mnReward;

            //CTxDestination txDest;
            //ExtractDestination(mnPayee, txDest);
            //LogPrintf("%s: Masternode payment to %s (pow)\n", __func__, EncodeDestination(txDest));

            // miner's reward is everything that left
            minerReward = totalReward - mnReward;
        } else {
            minerReward = totalReward;
        }
        minerReward -= bceResult.refundSender;
        contrTx.vout[0].nValue = minerReward;

        int i=contrTx.vout.size();
        contrTx.vout.resize(contrTx.vout.size()+bceResult.refundOutputs.size());
        for(CTxOut& vout : bceResult.refundOutputs){
            contrTx.vout[i]=vout;
            i++;
        }
    } else {
        refundtx=1;
//        contrTx.vout[refundtx].nValue -= bceResult.refundSender;
//
//        int i=contrTx.vout.size();
//        contrTx.vout.resize(contrTx.vout.size()+bceResult.refundOutputs.size());
//        for(CTxOut& vout : bceResult.refundOutputs){
//            contrTx.vout[i]=vout;
//            i++;
//        }
    }

    //note, this will need changed for MPoS

    pblock->vtx[refundtx] = std::move(CTransaction(contrTx));
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlockWithKey(CReserveKey& reservekey, bool fMineWitnessTx, bool fProofOfStake, int64_t* pTotalFees, int32_t txProofTime, int32_t nTimeLimit)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
        return NULL;

    CScript scriptPubKey = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKey, fMineWitnessTx, fProofOfStake, pTotalFees, txProofTime, nTimeLimit);
}


std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, bool fMineWitnessTx, bool fProofOfStake, int64_t* pTotalFees, int32_t txProofTime, int32_t nTimeLimit)
{
    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if(!pblocktemplate.get())
        return nullptr;
    pblock = &pblocktemplate->block; // pointer for convenience

    this->nTimeLimit = nTimeLimit;

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    // Add dummy coinstake tx as second transaction
    if(fProofOfStake)
        pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    //LOCK2(cs_main, mempool.cs);
    LOCK(cs_main);
    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return nullptr;

    nHeight = pindexPrev->nHeight + 1;

    pblock->nVersion = ComputeBlockVersion(pindexPrev, chainparams.GetConsensus());
    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (chainparams.MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    if(txProofTime == 0) {
        txProofTime = GetAdjustedTime();
    }

    pblock->nTime = txProofTime;
    if (!fProofOfStake)
        UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev, fProofOfStake);
    pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus(),fProofOfStake);
    const int64_t nMedianTimePast = pindexPrev->GetMedianTimePast();

    nLockTimeCutoff = (STANDARD_LOCKTIME_VERIFY_FLAGS & LOCKTIME_MEDIAN_TIME_PAST)
                      ? nMedianTimePast
                      : pblock->GetBlockTime();


    nLastBlockTx = nBlockTx;
    nLastBlockSize = nBlockSize;
    nLastBlockWeight = nBlockWeight;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);

    // Decide whether to include witness transactions
    // This is only needed in case the witness softfork activation is reverted
    // (which would require a very deep reorganization) or when
    // -promiscuousmempoolflags is used.
    // TODO: replace this with a call to main to assess validity of a mempool
    // transaction (which in most cases can be a no-op).
    fIncludeWitness = IsWitnessEnabled(pindexPrev, chainparams.GetConsensus()) && fMineWitnessTx;
    if (fProofOfStake){
        // Height first in coinbase required for block.version=2
        coinbaseTx.vin[0].scriptSig = (CScript() << nHeight) + COINBASE_FLAGS;
        assert(coinbaseTx.vin[0].scriptSig.size() <= 100);
        coinbaseTx.vout[0].SetEmpty();
    } else {
        CAmount powReward = GetProofOfWorkReward(0, nHeight);
        CAmount totalReward = powReward + nFees;
        CAmount minerReward = 0;
        CAmount mnReward = 0;
        CScript mnPayee;

        if (nHeight >= chainparams.FirstSplitRewardBlock() && SelectMasternodePayee(mnPayee)) {
            coinbaseTx.vout.resize(2);
            // set masternode 20% reward
            mnReward = powReward * 0.2;
            coinbaseTx.vout[1].scriptPubKey = mnPayee;
            coinbaseTx.vout[1].nValue = mnReward;

            CTxDestination txDest;
            ExtractDestination(mnPayee, txDest);
            LogPrintf("%s: Masternode payment to %s (pow)\n", __func__, EncodeDestination(txDest));

            // miner's reward is everything that is left
            minerReward = totalReward - mnReward;
        } else {
            minerReward = totalReward;
        }

        coinbaseTx.vin[0].scriptSig = CScript() << nHeight << OP_0;
        coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;
        coinbaseTx.vout[0].nValue = minerReward;
    }

    originalRewardTx = coinbaseTx;

    pblock->vtx[0] = std::move(coinbaseTx);

    if (fProofOfStake && !stake->CreateBlockStake(pwalletMain, pblock))
        return nullptr;

    if (fProofOfStake)
        originalRewardTx = CMutableTransaction(pblock->vtx[1]);

    //////////////////////////////////////////////////////// lux
    LuxDGP luxDGP(globalState.get(), fGettingValuesDGP);
    globalSealEngine->setLuxSchedule(luxDGP.getGasSchedule(nHeight));
    uint32_t blockSizeDGP = luxDGP.getBlockSize(nHeight);
    minGasPrice = luxDGP.getMinGasPrice(nHeight);
    if(IsArgSet("-staker-min-tx-gas-price")) {
        CAmount stakerMinGasPrice;
        if(ParseMoney(GetArg("-staker-min-tx-gas-price", ""), stakerMinGasPrice)) {
            minGasPrice = std::max(minGasPrice, (uint64_t)stakerMinGasPrice);
        }
    }
    hardBlockGasLimit = luxDGP.getBlockGasLimit(nHeight);
    softBlockGasLimit = GetArg("-staker-soft-block-gas-limit", hardBlockGasLimit);
    softBlockGasLimit = std::min(softBlockGasLimit, hardBlockGasLimit);
    txGasLimit = GetArg("-staker-max-tx-gas-limit", softBlockGasLimit);

    nBlockMaxSize = blockSizeDGP ? blockSizeDGP : nBlockMaxSize;

    CBlockIndex* pindexState = chainActive.Tip();
    dev::h256 oldHashStateRoot = getGlobalStateRoot(pindexState);
    dev::h256 oldHashUTXORoot = getGlobalStateUTXO(pindexState);

    addPriorityTxs(minGasPrice);
    addPackageTxs(minGasPrice);

    if (chainActive.Height() >= chainparams.FirstSCBlock()) {
        pblock->hashStateRoot = h256Touint(getGlobalStateRoot(pindexState));
        pblock->hashUTXORoot = h256Touint(getGlobalStateUTXO(pindexState));

        // restore old roots state after txs/scs are processed
        setGlobalStateRoot(oldHashStateRoot);
        setGlobalStateUTXO(oldHashUTXORoot);
    }

    //this should already be populated by AddBlock in case of contracts, but if no contracts
    //then it won't get populated
    RebuildRefundTransaction();

    if (fProofOfStake && bceResult.refundOutputs.size()) {
        // for now, avoid processing SC in PoS blocks
        return nullptr;
    }
    ////////////////////////////////////////////////////////

    pblocktemplate->vchCoinbaseCommitment = GenerateCoinbaseCommitment(*pblock, pindexPrev, chainparams.GetConsensus(), fProofOfStake);
    pblocktemplate->vTxFees[0] = -nFees;

    uint64_t nSerializeSize = GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION);
    LogPrint("miner", "CreateNewBlock(): total size: %u block weight: %u txs: %u fees: %ld sigops %d\n", nSerializeSize, GetBlockCost(*pblock), nBlockTx, nFees, nBlockSigOpsCost);

    // The total fee is the Fees minus the Refund
    if (pTotalFees)
        *pTotalFees = nFees - bceResult.refundSender;

    // Fill in header
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    pblock->nNonce         = 0;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(pblock->vtx[0]);

    CValidationState state;
    if (!TestBlockValidity(state, chainparams, *pblock, pindexPrev, false, false)) {
        if (!fProofOfStake) LogPrintf("%s: TestBlockValidity failed \n", __func__);
        return nullptr;
    }

    return std::move(pblocktemplate);
}

bool BlockAssembler::isStillDependent(CTxMemPool::txiter iter)
{
    for (CTxMemPool::txiter parent : mempool.GetMemPoolParents(iter))
    {
        if (!inBlock.count(parent)) {
            return true;
        }
    }
    return false;
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        }
        else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost)
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= nBlockMaxWeight)
        return false;
    if (nBlockSigOpsCost + packageSigOpsCost >= (uint64_t)dgpMaxBlockSigOps)
        return false;
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
// - premature witness (in case segwit transactions are added to mempool before
//   segwit activation)
// - serialized size (in case -blockmaxsize is in use)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package)
{
    uint64_t nPotentialBlockSize = nBlockSize; // only used with fNeedSizeAccounting
    for (const CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, nLockTimeCutoff))
            return false;
        if (!fIncludeWitness && it->GetTx().HasWitness())
            return false;
        if (fNeedSizeAccounting) {
            uint64_t nTxSize = ::GetSerializeSize(it->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
            if (nPotentialBlockSize + nTxSize >= nBlockMaxSize) {
                return false;
            }
            nPotentialBlockSize += nTxSize;
        }
    }
    return true;
}

bool BlockAssembler::TestForBlock(CTxMemPool::txiter iter)
{
    if (nBlockWeight + iter->GetTxWeight() >= nBlockMaxWeight) {
        // If the block is so close to full that no more txs will fit
        // or if we've tried more than 50 times to fill remaining space
        // then flag that the block is finished
        if (nBlockWeight >  nBlockMaxWeight - 400 || lastFewTxs > 50) {
            blockFinished = true;
            return false;
        }
        // Once we're within 4000 weight of a full block, only look at 50 more txs
        // to try to fill the remaining space.
        if (nBlockWeight > nBlockMaxWeight - 4000) {
            lastFewTxs++;
        }
        return false;
    }

    if (fNeedSizeAccounting) {
        if (nBlockSize + ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION) >= nBlockMaxSize) {
            if (nBlockSize >  nBlockMaxSize - 100 || lastFewTxs > 50) {
                blockFinished = true;
                return false;
            }
            if (nBlockSize > nBlockMaxSize - 1000) {
                lastFewTxs++;
            }
            return false;
        }
    }

    if (nBlockSigOpsCost + iter->GetSigOpCost() >= (uint64_t)dgpMaxBlockSigOps) {
        // If the block has room for no more sig ops then
        // flag that the block is finished
        if (nBlockSigOpsCost > (uint64_t)dgpMaxBlockSigOps - 8) {
            blockFinished = true;
            return false;
        }
        // Otherwise attempt to find another tx with fewer sigops
        // to put in the block.
        return false;
    }

    // Must check that lock times are still valid
    // This can be removed once MTP is always enforced
    // as long as reorgs keep the mempool consistent.
    if (!IsFinalTx(iter->GetTx(), nHeight, nLockTimeCutoff))
        return false;

    return true;
}

bool BlockAssembler::CheckBlockBeyondFull()
{
    if (nBlockSize > dgpMaxBlockSerSize) {
        return false;
    }

    if (nBlockSigOpsCost * WITNESS_SCALE_FACTOR > (uint64_t)dgpMaxBlockSigOps) {
        return false;
    }
    return true;
}

bool BlockAssembler::AttemptToAddContractToBlock(CTxMemPool::txiter iter, uint64_t minGasPrice) {
    if (nTimeLimit != 0 && GetAdjustedTime() >= nTimeLimit - BYTECODE_TIME_BUFFER) {
        return false;
    }
    if (GetBoolArg("-disablecontractstaking", false))
    {
        return false;
    }

    CBlockIndex* pindexState = chainActive.Tip();
    dev::h256 oldHashStateRoot = getGlobalStateRoot(pindexState);
    dev::h256 oldHashUTXORoot = getGlobalStateUTXO(pindexState);

    // operate on local vars first, then later apply to `this`
    uint64_t nBlockWeight = this->nBlockWeight;
    uint64_t nBlockSize = this->nBlockSize;
    uint64_t nBlockSigOpsCost = this->nBlockSigOpsCost;

    LuxTxConverter convert(iter->GetTx(), NULL, &pblock->vtx);

    ExtractLuxTX resultConverter;
    if(!convert.extractionLuxTransactions(resultConverter)){
        //this check already happens when accepting txs into mempool
        //therefore, this can only be triggered by using raw transactions on the staker itself
        return false;
    }
    std::vector<LuxTransaction> luxTransactions = resultConverter.first;
    dev::u256 txGas = 0;
    for(LuxTransaction luxTransaction : luxTransactions){
        txGas += luxTransaction.gas();
        if(txGas > txGasLimit) {
            // Limit the tx gas limit by the soft limit if such a limit has been specified.
            return false;
        }

        if(bceResult.usedGas + luxTransaction.gas() > softBlockGasLimit){
            //if this transaction's gasLimit could cause block gas limit to be exceeded, then don't add it
            return false;
        }
        if(luxTransaction.gasPrice() < minGasPrice){
            //if this transaction's gasPrice is less than the current DGP minGasPrice don't add it
            return false;
        }
    }
    // We need to pass the DGP's block gas limit (not the soft limit) since it is consensus critical.
    ByteCodeExec exec(*pblock, luxTransactions, hardBlockGasLimit);
    if(!exec.performByteCode()){
        //error, don't add contract
        setGlobalStateRoot(oldHashStateRoot);
        setGlobalStateUTXO(oldHashUTXORoot);
        return false;
    }

    ByteCodeExecResult testExecResult;
    if(!exec.processingResults(testExecResult)){
        setGlobalStateRoot(oldHashStateRoot);
        setGlobalStateUTXO(oldHashUTXORoot);
        return false;
    }

    if(bceResult.usedGas + testExecResult.usedGas > softBlockGasLimit){
        //if this transaction could cause block gas limit to be exceeded, then don't add it
        setGlobalStateRoot(oldHashStateRoot);
        setGlobalStateUTXO(oldHashUTXORoot);
        return false;
    }

    //apply contractTx costs to local state
    if (fNeedSizeAccounting) {
        nBlockSize += ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
    }
    nBlockWeight += iter->GetTxWeight();
    nBlockSigOpsCost += iter->GetSigOpCost();
    //apply value-transfer txs to local state
    for (CTransaction &t : testExecResult.valueTransfers) {
        if (fNeedSizeAccounting) {
            nBlockSize += ::GetSerializeSize(t, SER_NETWORK, PROTOCOL_VERSION);
        }
        nBlockWeight += GetTransactionCost(t);
        nBlockSigOpsCost += GetLegacySigOpCount(t);
    }

    int proofTx = pblock->IsProofOfStake() ? 1 : 0;

    //calculate sigops from new refund/proof tx

    //first, subtract old proof tx
    nBlockSigOpsCost -= GetLegacySigOpCount(pblock->vtx[proofTx]);

    // manually rebuild refundtx
    CMutableTransaction contrTx(pblock->vtx[proofTx]);
    //note, this will need changed for MPoS
    int i=contrTx.vout.size();
    contrTx.vout.resize(contrTx.vout.size()+testExecResult.refundOutputs.size());
    for(CTxOut& vout : testExecResult.refundOutputs){
        contrTx.vout[i]=vout;
        i++;
    }
    nBlockSigOpsCost += GetLegacySigOpCount(contrTx);
    //all contract costs now applied to local state

    //Check if block will be too big or too expensive with this contract execution
    if (nBlockSigOpsCost * WITNESS_SCALE_FACTOR > (uint64_t)dgpMaxBlockSigOps ||
        nBlockSize > dgpMaxBlockSerSize) {
        //contract will not be added to block, so revert state to before we tried
        setGlobalStateRoot(oldHashStateRoot);
        setGlobalStateUTXO(oldHashUTXORoot);
        return false;
    }

    //block is not too big, so apply the contract execution and it's results to the actual block

    //apply local bytecode to global bytecode state
    bceResult.usedGas += testExecResult.usedGas;
    bceResult.refundSender += testExecResult.refundSender;
    bceResult.refundOutputs.insert(bceResult.refundOutputs.end(), testExecResult.refundOutputs.begin(), testExecResult.refundOutputs.end());
    bceResult.valueTransfers = std::move(testExecResult.valueTransfers);

    pblock->vtx.emplace_back(iter->GetTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    if (fNeedSizeAccounting) {
        this->nBlockSize += ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
    }
    this->nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    this->nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    for (CTransaction &t : bceResult.valueTransfers) {
        pblock->vtx.emplace_back(std::move(t));
        if (fNeedSizeAccounting) {
            this->nBlockSize += ::GetSerializeSize(t, SER_NETWORK, PROTOCOL_VERSION);
        }
        this->nBlockWeight += GetTransactionCost(t);
        this->nBlockSigOpsCost += GetLegacySigOpCount(t);
        ++nBlockTx;
    }
    //calculate sigops from new refund/proof tx
    this->nBlockSigOpsCost -= GetLegacySigOpCount(pblock->vtx[proofTx]);
    RebuildRefundTransaction();
    this->nBlockSigOpsCost += GetLegacySigOpCount(pblock->vtx[proofTx]);

    bceResult.valueTransfers.clear();

    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblock->vtx.emplace_back(iter->GetTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    if (fNeedSizeAccounting) {
        nBlockSize += ::GetSerializeSize(iter->GetTx(), SER_NETWORK, PROTOCOL_VERSION);
    }
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    nFees += iter->GetFee();
    inBlock.insert(iter);

    bool fPrintPriority = GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        double dPriority = iter->GetPriority(nHeight);
        CAmount dummy;
        mempool.ApplyDeltas(iter->GetTx().GetHash(), dPriority, dummy);
        LogPrintf("priority %.1f fee %s txid %s\n",
                  dPriority,
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }

}

void BlockAssembler::UpdatePackagesForAdded(const CTxMemPool::setEntries& alreadyAdded,
                                            indexed_modified_transaction_set &mapModifiedTx)
{
    for (const CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc))
                continue;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                modEntry.nSizeWithAncestors -= it->GetTxSize();
                modEntry.nModFeesWithAncestors -= it->GetModifiedFee();
                modEntry.nSigOpCostWithAncestors -= it->GetSigOpCost();
                mapModifiedTx.insert(modEntry);
            } else {
                mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
            }
        }
    }
}

// Skip entries in mapTx that are already in a block or are present
// in mapModifiedTx (which implies that the mapTx ancestor state is
// stale due to ancestor inclusion in the block)
// Also skip transactions that we've already failed to add. This can happen if
// we consider a transaction in mapModifiedTx and it fails: we can then
// potentially consider it again while walking mapTx.  It's currently
// guaranteed to fail again, but as a belt-and-suspenders check we put it in
// failedTx and avoid re-evaluation, since the re-evaluation would be using
// cached size/sigops/fee values that are not actually correct.
bool BlockAssembler::SkipMapTxEntry(CTxMemPool::txiter it, indexed_modified_transaction_set &mapModifiedTx, CTxMemPool::setEntries &failedTx)
{
    assert (it != mempool.mapTx.end());
    if (mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it))
        return true;
    return false;
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, CTxMemPool::txiter entry, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(uint64_t minGasPrice)
{
    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    // Start by adding all descendants of previously added txs to mapModifiedTx
    // and modifying them for their already included ancestors
    UpdatePackagesForAdded(inBlock, mapModifiedTx);

    CTxMemPool::indexed_transaction_set::index<ancestor_score_or_gas_price>::type::iterator mi = mempool.mapTx.get<ancestor_score_or_gas_price>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score_or_gas_price>().end() || !mapModifiedTx.empty())
    {
        if(nTimeLimit != 0 && GetAdjustedTime() >= nTimeLimit){
            //no more time to add transactions, just exit
            return;
        }
        // First try to find a new transaction in mapTx to evaluate.
        if (mi != mempool.mapTx.get<ancestor_score_or_gas_price>().end() &&
            SkipMapTxEntry(mempool.mapTx.project<0>(mi), mapModifiedTx, failedTx)) {
            ++mi;
            continue;
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score_or_gas_price>().begin();
        if (mi == mempool.mapTx.get<ancestor_score_or_gas_price>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score_or_gas_price>().end() &&
                    CompareModifiedEntry()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score_or_gas_price>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        CTxMemPool::setEntries ancestors;
        uint64_t nNoLimit = std::numeric_limits<uint64_t>::max();
        std::string dummy;
        mempool.CalculateMemPoolAncestors(*iter, ancestors, nNoLimit, nNoLimit, nNoLimit, nNoLimit, dummy, false);

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score_or_gas_price>().erase(modit);
                failedTx.insert(iter);
            }
            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                                                                 nBlockMaxWeight - 4000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, iter, sortedEntries);

        bool wasAdded=true;
        for (size_t i=0; i<sortedEntries.size(); ++i) {
            if(!wasAdded || (nTimeLimit != 0 && GetAdjustedTime() >= nTimeLimit))
            {
                //if out of time, or earlier ancestor failed, then skip the rest of the transactions
                mapModifiedTx.erase(sortedEntries[i]);
                wasAdded=false;
                continue;
            }
            const CTransaction& tx = sortedEntries[i]->GetTx();
            if(wasAdded) {
                if (tx.HasCreateOrCall()) {
                    wasAdded = AttemptToAddContractToBlock(sortedEntries[i], minGasPrice);
                    if(!wasAdded){
                        if(fUsingModified) {
                            //this only needs to be done once to mark the whole package (everything in sortedEntries) as failed
                            mapModifiedTx.get<ancestor_score_or_gas_price>().erase(modit);
                            failedTx.insert(iter);
                        }
                    }
                } else {
                    AddToBlock(sortedEntries[i]);
                }
            }
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        if(!wasAdded){
            //skip UpdatePackages if a transaction failed to be added (match TestPackage logic)
            continue;
        }

        // Update transactions that depend on each of these
        UpdatePackagesForAdded(ancestors, mapModifiedTx);
    }
}

void BlockAssembler::addPriorityTxs(uint64_t minGasPrice)
{
    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    if (nBlockPrioritySize == 0) {
        return;
    }

    bool fSizeAccounting = fNeedSizeAccounting;
    fNeedSizeAccounting = true;

    // This vector will be sorted into a priority queue:
    std::vector<TxCoinAgePriority> vecPriority;
    TxCoinAgePriorityCompare pricomparer;
    std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash> waitPriMap;
    typedef std::map<CTxMemPool::txiter, double, CTxMemPool::CompareIteratorByHash>::iterator waitPriIter;
    double actualPriority = -1;

    vecPriority.reserve(mempool.mapTx.size());
    for (CTxMemPool::indexed_transaction_set::iterator mi = mempool.mapTx.begin();
         mi != mempool.mapTx.end(); ++mi)
    {
        double dPriority = mi->GetPriority(nHeight);
        CAmount dummy;
        mempool.ApplyDeltas(mi->GetTx().GetHash(), dPriority, dummy);
        vecPriority.push_back(TxCoinAgePriority(dPriority, mi));
    }
    std::make_heap(vecPriority.begin(), vecPriority.end(), pricomparer);

    CTxMemPool::txiter iter;
    while (!vecPriority.empty() && !blockFinished) { // add a tx from priority queue to fill the blockprioritysize
        iter = vecPriority.front().second;
        actualPriority = vecPriority.front().first;
        std::pop_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
        vecPriority.pop_back();

        // If tx already in block, skip
        if (inBlock.count(iter)) {
            assert(false); // shouldn't happen for priority txs
            continue;
        }

        // cannot accept witness transactions into a non-witness block
        if (!fIncludeWitness && iter->GetTx().HasWitness())
            continue;


        if(nTimeLimit != 0 && GetAdjustedTime() >= nTimeLimit)
        {
            break;
        }

        // If tx is dependent on other mempool txs which haven't yet been included
        // then put it in the waitSet
        if (isStillDependent(iter)) {
            waitPriMap.insert(std::make_pair(iter, actualPriority));
            continue;
        }

        // If this tx fits in the block add it, otherwise keep looping
        if (TestForBlock(iter)) {

            const CTransaction& tx = iter->GetTx();
            bool wasAdded=true;
            if(tx.HasCreateOrCall()) {
                wasAdded = AttemptToAddContractToBlock(iter, minGasPrice);
            }else {
                AddToBlock(iter);
            }

            // If now that this txs is added we've surpassed our desired priority size
            // or have dropped below the AllowFreeThreshold, then we're done adding priority txs
            if (nBlockSize >= nBlockPrioritySize || !AllowFree(actualPriority)) {
                break;
            }
            if(wasAdded) {
                // This tx was successfully added, so
                // add transactions that depend on this one to the priority queue to try again
                for (CTxMemPool::txiter child : mempool.GetMemPoolChildren(iter)) {
                    waitPriIter wpiter = waitPriMap.find(child);
                    if (wpiter != waitPriMap.end()) {
                        vecPriority.push_back(TxCoinAgePriority(wpiter->second, child));
                        std::push_heap(vecPriority.begin(), vecPriority.end(), pricomparer);
                        waitPriMap.erase(wpiter);
                    }
                }
            }
        }
    }
    fNeedSizeAccounting = fSizeAccounting;
}

void IncrementExtraNonce(CBlock* pblock, const CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock)
    {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;
    unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = std::move(CTransaction(txCoinbase));
    pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
}

bool ProcessBlockFound(CBlock* pblock, CWallet& wallet, CReserveKey& reservekey)
{
    // Found a solution (stake)
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("LUXMiner : generated block is stale");

        for(const CTxIn& vin : pblock->vtx[1].vin) {
            if (wallet.IsSpent(vin.prevout.hash, vin.prevout.n)) {
                return error("LUXMiner : Gen block stake is invalid - UTXO spent");
            }
        }
    }

    CAmount generated = GetProofOfStakeReward(0, 0, chainActive.Height()+1);
    generated -= GetMasternodePosReward(chainActive.Height()+1, generated);
    LogPrintf("generated %s\n", FormatMoney(generated));

    // Remove key from key pool
    reservekey.KeepKey();

    bool usePhi2;
    {
        LOCK(cs_main);
        CBlockIndex* pindexPrev = LookupBlockIndex(pblock->hashPrevBlock);
        usePhi2 = pindexPrev ? pindexPrev->nHeight + 1 >= Params().SwitchPhi2Block() : false;
    }

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash(usePhi2)] = 0;
    }

    // Process this block the same as if we had received it from another node
    const CChainParams& chainparams = Params();
    CValidationState state;
    if (!ProcessNewBlock(state, chainparams, NULL, pblock)) {
        return error("LUXMiner : ProcessNewBlock, block not accepted");
    }

    return true;
}
