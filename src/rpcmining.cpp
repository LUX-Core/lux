// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "base58.h"
#include "chainparams.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "init.h"
#include "main.h"
#include "miner.h"
#include "net.h"
#include "pow.h"
#include "masternode.h"
#include "primitives/transaction.h"
#include "rpcserver.h"
#include "util.h"
#include "script/script.h"
#include "script/script_error.h"
#include "validationinterface.h"
#include "versionbits.h"
#ifdef ENABLE_WALLET
#include "db.h"
#include "wallet.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>

#include "univalue/univalue.h"

using namespace std;

/**
 * Return average network hashes per second based on the last 'lookup' blocks,
 * or from the last difficulty change if 'lookup' is nonpositive.
 * If 'height' is nonnegative, compute the estimate at the time when a given block was found.
 */
UniValue GetNetworkHashPS(int lookup, int height)
{
    CBlockIndex* pb = chainActive.Tip();

    if (height >= 0 && height < chainActive.Height())
        pb = chainActive[height];

    if (pb == NULL || !pb->nHeight)
        return 0;

    // If lookup is -1, then use blocks since last difficulty change.
    if (lookup <= 0)
        lookup = pb->nHeight % 2016 + 1;

    // If lookup is larger than chain, then set it to chain length.
    if (lookup > pb->nHeight)
        lookup = pb->nHeight;

    CBlockIndex* pb0 = pb;
    int64_t minTime = pb0->GetBlockTime();
    int64_t maxTime = minTime;
    for (int i = 0; i < lookup; i++) {
        pb0 = pb0->pprev;
        int64_t time = pb0->GetBlockTime();
        minTime = std::min(time, minTime);
        maxTime = std::max(time, maxTime);
    }

    // In case there's a situation where minTime == maxTime, we don't want a divide by zero exception.
    if (minTime == maxTime)
        return 0;

    uint256 workDiff = pb->nChainWork - pb0->nChainWork;
    int64_t timeDiff = maxTime - minTime;

    return (int64_t)(workDiff.getdouble() / timeDiff);
}

UniValue getnetworkhashps(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 2)
        throw runtime_error(
            "getnetworkhashps ( blocks height )\n"
            "\nReturns the estimated network hashes per second based on the last n blocks.\n"
            "Pass in [blocks] to override # of blocks, -1 specifies since last difficulty change.\n"
            "Pass in [height] to estimate the network speed at the time when a certain block was found.\n"
            "\nArguments:\n"
            "1. blocks     (numeric, optional, default=120) The number of blocks, or -1 for blocks since last difficulty change.\n"
            "2. height     (numeric, optional, default=-1) To estimate at the time of the given height.\n"
            "\nResult:\n"
            "x             (numeric) Hashes per second estimated\n"
            "\nExamples:\n" +
            HelpExampleCli("getnetworkhashps", "") + HelpExampleRpc("getnetworkhashps", ""));

    LOCK(cs_main);

    return GetNetworkHashPS(params.size() > 0 ? params[0].get_int() : 120, params.size() > 1 ? params[1].get_int() : -1);
}

#ifdef ENABLE_WALLET
UniValue getgenerate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getgenerate\n"
            "\nReturn if the server is set to generate coins or not. The default is false.\n"
            "It is set with the command line argument -gen (or lux.conf setting gen)\n"
            "It can also be set with the setgenerate call.\n"
            "\nResult\n"
            "true|false      (boolean) If the server is set to generate coins or not\n"
            "\nExamples:\n" +
            HelpExampleCli("getgenerate", "") + HelpExampleRpc("getgenerate", ""));

    LOCK(cs_main);

    return GetBoolArg("-gen", false);
}


UniValue setgenerate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setgenerate generate ( genproclimit )\n"
            "\nSet 'generate' true or false to turn generation on or off.\n"
            "Generation is limited to 'genproclimit' processors, -1 is unlimited.\n"
            "See the getgenerate call for the current setting.\n"
            "\nArguments:\n"
            "1. generate         (boolean, required) Set to true to turn on generation, false to turn off.\n"
            "2. genproclimit     (numeric, optional) Set the processor limit for when generation is on. Can be -1 for unlimited.\n"
            "                    Note: in -regtest mode, genproclimit controls how many blocks are generated immediately.\n"
            "\nResult\n"
            "[ blockhashes ]     (array, -regtest only) hashes of blocks generated\n"
            "\nExamples:\n"
            "\nSet the generation on with a limit of one processor\n" +
            HelpExampleCli("setgenerate", "true 1") +
            "\nCheck the setting\n" + HelpExampleCli("getgenerate", "") +
            "\nTurn off generation\n" + HelpExampleCli("setgenerate", "false") +
            "\nUsing json rpc\n" + HelpExampleRpc("setgenerate", "true, 1"));

//    if (pwalletMain == NULL)
//        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (disabled)");
//
//    bool fGenerate = true;
//    if (params.size() > 0)
//        fGenerate = params[0].get_bool();
//
//    int nGenProcLimit = -1;
//    if (params.size() > 1) {
//        nGenProcLimit = params[1].get_int();
//        if (nGenProcLimit == 0)
//            fGenerate = false;
//    }
//
//    // -regtest mode: don't return until nGenProcLimit blocks are generated
//    if (fGenerate && Params().MineBlocksOnDemand()) {
//        int nHeightStart = 0;
//        int nHeightEnd = 0;
//        int nHeight = 0;
//        int nGenerate = (nGenProcLimit > 0 ? nGenProcLimit : 1);
//        CReserveKey reservekey(pwalletMain);
//
//        { // Don't keep cs_main locked
//            LOCK(cs_main);
//            nHeightStart = chainActive.Height();
//            nHeight = nHeightStart;
//            nHeightEnd = nHeightStart + nGenerate;
//        }
//        unsigned int nExtraNonce = 0;
//        UniValue blockHashes(UniValue::VARR);
//        while (nHeight < nHeightEnd) {
//            unique_ptr<CBlockTemplate> pblocktemplate(CreateNewBlockWithKey(reservekey, pwalletMain, false));
//            if (!pblocktemplate.get())
//                throw JSONRPCError(RPC_INTERNAL_ERROR, "Wallet keypool empty");
//            CBlock* pblock = &pblocktemplate->block;
//            {
//                LOCK(cs_main);
//                IncrementExtraNonce(pblock, chainActive.Tip(), nExtraNonce);
//            }
//            //TODO: Phi2_hash hardfork block here !!!
//            while (!ShutdownRequested() && !CheckProofOfWork(pblock->GetHash(/*nHeight+1 >= Params().SwitchPhi2Block()*/), pblock->nBits, Params().GetConsensus())) {
//                // Yes, there is a chance every nonce could fail to satisfy the -regtest
//                // target -- 1 in 2^(2^32). That ain't gonna happen.
//                ++pblock->nNonce;
//            }
//            CValidationState state;
//            if (!ShutdownRequested() && !ProcessNewBlock(state, Params(), NULL, pblock))
//                throw JSONRPCError(RPC_INTERNAL_ERROR, "ProcessNewBlock, block not accepted");
//            ++nHeight;
//            blockHashes.push_back(pblock->GetHash().GetHex());
//        }
//        return blockHashes;
//    } else { // Not -regtest: start generate thread, return immediately
//        mapArgs["-gen"] = (fGenerate ? "1" : "0");
//        mapArgs["-genproclimit"] = itostr(nGenProcLimit);
////        GenerateBitcoins(pwalletMain, nGenProcLimit);
//    }

    return NullUniValue;
}

UniValue gethashespersec(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "gethashespersec\n"
            "\nReturns a recent hashes per second performance measurement while generating.\n"
            "See the getgenerate and setgenerate calls to turn generation on and off.\n"
            "\nResult:\n"
            "n            (numeric) The recent hashes per second when generation is on (will return 0 if generation is off)\n"
            "\nExamples:\n" +
            HelpExampleCli("gethashespersec", "") + HelpExampleRpc("gethashespersec", ""));

//    if (GetTimeMillis() - nHPSTimerStart > 8000)
//        return (int64_t)0;
    return (int64_t)0;/*dHashesPerSec*/;
}
#endif


UniValue getmininginfo(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getmininginfo\n"
            "\nReturns a json object containing mining-related information."
            "\nResult:\n"
            "{\n"
            "  \"blocks\": nnn,             (numeric) The current block\n"
            "  \"currentblocksize\": nnn,   (numeric) The last block size\n"
            "  \"currentblocktx\": nnn,     (numeric) The last block transaction\n"
            "  \"difficulty\": xxx.xxxxx    (numeric) The current difficulty\n"
            "  \"errors\": \"...\"          (string) Current errors\n"
#ifdef ENABLE_CPUMINER
            "  \"generate\": true|false     (boolean) If the generation is on or off (see getgenerate or setgenerate calls)\n"
            "  \"genproclimit\": n          (numeric) The processor limit for generation. -1 if no generation. (see getgenerate or setgenerate calls)\n"
            "  \"hashespersec\": n          (numeric) The hashes per second of the generation, or 0 if no generation.\n"
#endif
            "  \"pooledtx\": n              (numeric) The size of the mem pool\n"
            "  \"testnet\": true|false      (boolean) If using testnet or not\n"
            "  \"chain\": \"xxxx\",         (string) current network name as defined in BIP70 (main, test, regtest)\n"
            "  \"algo\" : \"phi2\"          (string) The current proof-of-work algorithm name\n"
            "  \"segwit\": true|false       (boolean) If segwit txs are enabled or not\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("getmininginfo", "") + HelpExampleRpc("getmininginfo", ""));

    UniValue obj(UniValue::VOBJ);
    CBlockIndex* powTip = GetLastBlockOfType(0);
    obj.push_back(Pair("blocks", (int)chainActive.Height()));
    obj.push_back(Pair("currentblocksize", (uint64_t)nLastBlockSize));
    obj.push_back(Pair("currentblocktx", (uint64_t)nLastBlockTx));
    obj.push_back(Pair("difficulty", (double)GetDifficulty(powTip)));
    obj.push_back(Pair("errors", GetWarnings("statusbar")));
    obj.push_back(Pair("networkhashps", getnetworkhashps(params, false)));
    obj.push_back(Pair("pooledtx", (uint64_t)mempool.size()));
    obj.push_back(Pair("testnet", Params().NetworkIDString() != "main"));
    obj.push_back(Pair("chain", Params().NetworkIDString()));
    obj.push_back(Pair("algo", (chainActive.Height()+1) < Params().SwitchPhi2Block() ? "phi1612" : "phi2"));
    obj.push_back(Pair("segwit", IsWitnessEnabled(chainActive.Tip(), Params().GetConsensus()) ));
#ifdef ENABLE_CPUMINER
    obj.push_back(Pair("generate", getgenerate(params, false)));
    obj.push_back(Pair("genproclimit", (int)GetArg("-genproclimit", -1)));
    obj.push_back(Pair("hashespersec", gethashespersec(params, false)));
#endif
    return obj;
}


// NOTE: Unlike wallet RPC (which use BTC values), mining RPCs follow GBT (BIP 22) in using satoshi amounts
UniValue prioritisetransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 3)
        throw runtime_error(
            "prioritisetransaction <txid> <priority delta> <fee delta>\n"
            "Accepts the transaction into mined blocks at a higher (or lower) priority\n"
            "\nArguments:\n"
            "1. \"txid\"       (string, required) The transaction id.\n"
            "2. priority delta (numeric, required) The priority to add or subtract.\n"
            "                  The transaction selection algorithm considers the tx as it would have a higher priority.\n"
            "                  (priority of a transaction is calculated: coinage * value_in_duffs / txsize) \n"
            "3. fee delta      (numeric, required) The fee value (in duffs) to add (or subtract, if negative).\n"
            "                  The fee is not actually paid, only the algorithm for selecting transactions into a block\n"
            "                  considers the transaction as it would have paid a higher (or lower) fee.\n"
            "\nResult\n"
            "true              (boolean) Returns true\n"
            "\nExamples:\n" +
            HelpExampleCli("prioritisetransaction", "\"txid\" 0.0 10000") + HelpExampleRpc("prioritisetransaction", "\"txid\", 0.0, 10000"));

    LOCK(cs_main);

    uint256 hash = ParseHashStr(params[0].get_str(), "txid");

    CAmount nAmount = params[2].get_int64();

    mempool.PrioritiseTransaction(hash, params[0].get_str(), params[1].get_real(), nAmount);
    return true;
}


// NOTE: Assumes a conclusive result; if result is inconclusive, it must be handled by caller
static UniValue BIP22ValidationResult(const CValidationState& state)
{
    if (state.IsValid())
        return NullUniValue;

    std::string strRejectReason = state.GetRejectReason();
    if (state.IsError())
        throw JSONRPCError(RPC_VERIFY_ERROR, strRejectReason);
    if (state.IsInvalid()) {
        if (strRejectReason.empty())
            return "rejected";
        return strRejectReason;
    }
    // Should be impossible
    return "valid?";
}

std::string gbt_vb_name(const Consensus::DeploymentPos pos) {
    const struct BIP9DeploymentInfo& vbinfo = VersionBitsDeploymentInfo[pos];
    std::string s = vbinfo.name;
    if (!vbinfo.gbt_force) {
        s.insert(s.begin(), '!');
    }
    return s;
}

UniValue getblocktemplate(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getblocktemplate ( \"jsonrequestobject\" )\n"
            "\nIf the request parameters include a 'mode' key, that is used to explicitly select between the default 'template' request or a 'proposal'.\n"
            "It returns data needed to construct a block to work on.\n"
            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"

            "\nArguments:\n"
            "1. \"jsonrequestobject\"       (string, optional) A json object in the following spec\n"
            "     {\n"
            "       \"mode\":\"template\"    (string, optional) This must be set to \"template\", \"proposal\" (see BIP 23), or omitted\n"
            "       \"capabilities\":[     (array, optional) A list of strings\n"
            "           \"support\"          (string) client side supported feature, 'longpoll', 'coinbasetxn', 'coinbasevalue', 'proposal', 'serverlist', 'workid'\n"
            "           ,...\n"
            "       ],\n"
            "       \"rules\":[            (array, optional) A list of strings\n"
            "           \"support\"          (string) client side supported softfork deployment\n"
            "           ,...\n"
            "       ]\n"
            "     }\n"
            "\n"

            "\nResult:\n"
            "{\n"
            "  \"version\" : n,                    (numeric) The preferred block version\n"
            "  \"rules\" : [ \"rulename\", ... ],    (array of strings) specific block rules that are to be enforced\n"
            "  \"vbavailable\" : {                 (json object) set of pending, supported versionbit (BIP 9) softfork deployments\n"
            "      \"rulename\" : bitnumber        (numeric) identifies the bit number as indicating acceptance and readiness for the named softfork rule\n"
            "      ,...\n"
            "  },\n"
            "  \"vbrequired\" : n,                 (numeric) bit mask of versionbits the server requires set in submissions\n"
            "  \"previousblockhash\" : \"xxxx\",     (string) The hash of current highest block\n"
            "  \"stateroot\" : \"xxxx\",             (string) The state root hash of current block for smart contracts\n"
            "  \"utxoroot\" : \"xxxx\",              (string) The UTXO root hash of current block for smart contracts\n"
            "  \"screfund\" : [{                   (array) smart contract(s) refund address(es)\n"
            "       \"payee\" : \"xxx\",           (string) smart contract refund address\n"
            "       \"script\" : \"xxxx\",         (string) scriptPubKey in hexadecimal\n"
            "       \"amount\": n                  (numeric) required amount to refund\n"
            "  }],\n"
            "  \"transactions\" : [                (array) contents of non-coinbase transactions that should be included in the next block\n"
            "      {\n"
            "         \"data\" : \"xxxx\",          (string) transaction data encoded in hexadecimal (byte-for-byte)\n"
            "         \"txid\" : \"xxxx\",          (string) transaction id encoded in little-endian hexadecimal\n"
            "         \"hash\" : \"xxxx\",          (string) hash encoded in little-endian hexadecimal (including witness data)\n"
            "         \"depends\" : [              (array) array of numbers \n"
            "             n                        (numeric) transactions before this one (by 1-based index in 'transactions' list) that must be present in the final block if this one is\n"
            "             ,...\n"
            "         ],\n"
            "         \"fee\": n,                   (numeric) difference in value between transaction inputs and outputs (in Satoshis); for coinbase transactions, this is a negative Number of the total collected block fees (ie, not including the block subsidy); if key is not present, fee is unknown and clients MUST NOT assume there isn't one\n"
            "         \"sigops\" : n,               (numeric) total SigOps cost, as counted for purposes of block limits; if key is not present, sigop cost is unknown and clients MUST NOT assume it is zero\n"
            "         \"cost\" : n,                 (numeric) total transaction size cost, as counted for purposes of block limits\n"
            "         \"required\" : true|false     (boolean) if provided and true, this transaction must be in the final block\n"
            "      }\n"
            "      ,...\n"
            "  ],\n"
            "  \"coinbaseaux\" : {                  (json object) data that should be included in the coinbase's scriptSig content\n"
            "      \"flags\" : \"flags\"            (string) \n"
            "  },\n"
            "  \"coinbasevalue\" : n,               (numeric) maximum allowable input to coinbase transaction, including the generation award and transaction fees (in duffs)\n"
            "  \"coinbasetxn\" : { ... },           (json object) information for coinbase transaction\n"
            "  \"target\" : \"xxxx\",               (string) The hash target\n"
            "  \"mintime\" : xxx,                   (numeric) The minimum timestamp appropriate for next block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"mutable\" : [                      (array of string) list of ways the block template may be changed \n"
            "     \"value\"                         (string) A way the block template may be changed, e.g. 'time', 'transactions', 'prevblock'\n"
            "     ,...\n"
            "  ],\n"
            "  \"noncerange\" : \"00000000ffffffff\",   (string) A range of valid nonces\n"
            "  \"sigoplimit\" : n,                 (numeric) cost limit of sigops in blocks\n"
            "  \"sizelimit\" : n,                  (numeric) limit of block size\n"
            "  \"costlimit\" : n,                  (numeric) limit of block cost\n"
            "  \"curtime\" : ttt,                  (numeric) current timestamp in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"bits\" : \"xxx\",                 (string) compressed target of next block\n"
            "  \"height\" : n                      (numeric) The height of the next block\n"
            "  \"masternode\" : {                  (json object) required masternode payee that must be included in the next block\n"
            "       \"payee\" : \"xxx\",           (string) required payee for the next block\n"
            "       \"script\" : \"xxxx\",         (string) payee scriptPubKey\n"
            "       \"amount\": n                  (numeric) required amount to pay\n"
            "  },\n"
            "  \"masternode_payments_started\" :   true|false, (boolean) true, if masternode payments started\n"
            "  \"masternode_payments_enforced\" :  true|false, (boolean) true, if masternode payments are enforced\n"
            "  \"votes\" : [\n                     (array) show vote candidates\n"
            "        { ... }                       (json object) vote candidate\n"
            "        ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getblocktemplate", "") + HelpExampleRpc("getblocktemplate", ""));

    LOCK(cs_main);

    std::string strMode = "template";
    UniValue lpval = NullUniValue;
    std::set<std::string> setClientRules;
    int64_t nMaxVersionPreVB = -1;
    if (params.size() > 0)
    {
        const UniValue& oparam = params[0].get_obj();
        const UniValue& modeval = find_value(oparam, "mode");
        if (modeval.isStr())
            strMode = modeval.get_str();
        else if (modeval.isNull()) {
            /* Do nothing */
        } else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");
        lpval = find_value(oparam, "longpollid");

        if (strMode == "proposal" || strMode == "submit") {
            const UniValue& dataval = find_value(oparam, "data");
            if (!dataval.isStr())
                throw JSONRPCError(RPC_TYPE_ERROR, "Missing data String key for proposal");

            CBlock block;
            if (!DecodeHexBlk(block, dataval.get_str()))
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");

            CBlockIndex* pindexPrev = LookupBlockIndex(block.hashPrevBlock);
            bool usePhi2 = pindexPrev ? pindexPrev->nHeight + 1 >= Params().SwitchPhi2Block() : false;

            uint256 hash = block.GetHash(usePhi2);
            CBlockIndex* pindex = LookupBlockIndex(hash);
            if (pindex) {
                if (pindex->IsValid(BLOCK_VALID_SCRIPTS))
                    return "duplicate";
                if (pindex->nStatus & BLOCK_FAILED_MASK)
                    return "duplicate-invalid";
                return "duplicate-inconclusive";
            }

            pindexPrev = chainActive.Tip();
            // TestBlockValidity only supports blocks built on the current Tip
            if (block.hashPrevBlock != pindexPrev->GetBlockHash())
                return "inconclusive-not-best-prevblk";

            if (block.vtx[0].wit.vtxinwit.size() < 1) {
                UpdateUncommittedBlockStructures(block, pindexPrev, Params().GetConsensus());
            }

            CValidationState state;
            TestBlockValidity(state, Params(), block, pindexPrev, false, true);
            return BIP22ValidationResult(state);
        }

        const UniValue& aClientRules = find_value(oparam, "rules");
        if (aClientRules.isArray()) {
            for (unsigned int i = 0; i < aClientRules.size(); ++i) {
                const UniValue& v = aClientRules[i];
                setClientRules.insert(v.get_str());
            }
        } else {
            // NOTE: It is important that this NOT be read if versionbits is supported
            const UniValue& uvMaxVersion = find_value(oparam, "maxversion");
            if (uvMaxVersion.isNum()) {
                nMaxVersionPreVB = uvMaxVersion.get_int64();
            }
        }
    }

    if (strMode != "template")
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid mode");

    // Disable checking block downloading and number of connected nodes for segwittest network
    // because it is tested locally, without any nodes connected, and with significant amount of time between blocks
    if (Params().NetworkID() != CBaseChainParams::SEGWITTEST) {
        if (vNodes.empty())
            throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "LUX is not connected!");

        if (IsInitialBlockDownload())
            throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "LUX is downloading blocks...");
    }

    static unsigned int nTransactionsUpdatedLast;

    if (!lpval.isNull()) {
        // Wait to respond until either the best block changes, OR a minute has passed and there are more transactions
        uint256 hashWatchedChain = uint256();
        boost::system_time checktxtime;
        unsigned int nTransactionsUpdatedLastLP;

        if (lpval.isStr()) {
            // Format: <hashBestChain><nTransactionsUpdatedLast>
            std::string lpstr = lpval.get_str();

            hashWatchedChain.SetHex(lpstr.substr(0, 64));
            nTransactionsUpdatedLastLP = atoi64(lpstr.substr(64));
        } else {
            // NOTE: Spec does not specify behaviour for non-string longpollid, but this makes testing easier
            hashWatchedChain = chainActive.Tip()->GetBlockHash();
            nTransactionsUpdatedLastLP = nTransactionsUpdatedLast;
        }

// Release the wallet and main lock while waiting
        LEAVE_CRITICAL_SECTION(cs_main);
        {
            checktxtime = boost::get_system_time() + boost::posix_time::minutes(1);

            boost::unique_lock<boost::mutex> lock(csBestBlock);
            while (chainActive.Tip()->GetBlockHash() == hashWatchedChain && IsRPCRunning()) {
                if (!cvBlockChange.timed_wait(lock, checktxtime)) {
                    // Timeout: Check transactions for update
                    if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLastLP)
                        break;
                    checktxtime += boost::posix_time::seconds(10);
                }
            }
        }
        ENTER_CRITICAL_SECTION(cs_main);

        if (!IsRPCRunning())
            throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Shutting down");
        // TODO: Maybe recheck connections/IBD and (if something wrong) send an expires-immediately template to stop miners?
    }

    const struct BIP9DeploymentInfo& segwit_info = VersionBitsDeploymentInfo[Consensus::DEPLOYMENT_SEGWIT];
    // If the caller is indicating segwit support, then allow CreateNewBlock()
    // to select witness transactions, after segwit activates (otherwise
    // don't).
    bool fSupportsSegwit = setClientRules.find(segwit_info.name) != setClientRules.end();

    // Update block
    static CBlockIndex* pindexPrev;
    static int64_t nStart;
    static std::unique_ptr<CBlockTemplate> pblocktemplate;
    // Cache whether the last invocation was with segwit support, to avoid returning
    // a segwit-block to a non-segwit caller.
    static bool fLastTemplateSupportsSegwit = true;
    if (pindexPrev != chainActive.Tip() ||
        (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 5) ||
        fLastTemplateSupportsSegwit != fSupportsSegwit) {
        // Clear pindexPrev so future calls make a new block, despite any failures from here on
        pindexPrev = NULL;

        // Store the chainActive.Tip() used before CreateNewBlock, to avoid races
        nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
        CBlockIndex* pindexPrevNew = chainActive.Tip();
        nStart = GetTime();
        fLastTemplateSupportsSegwit = fSupportsSegwit;

        // Create new block
        CScript scriptDummy = CScript() << OP_TRUE;
        pblocktemplate = BlockAssembler(Params()).CreateNewBlock(scriptDummy, fSupportsSegwit);
        if (!pblocktemplate)
            throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");

        // Need to update only after we know CreateNewBlock succeeded
        pindexPrev = pindexPrevNew;
    }
    CBlock* pblock = &pblocktemplate->block; // pointer for convenience
    const Consensus::Params& consensusParams = Params().GetConsensus();

    // Update nTime
    UpdateTime(pblock, consensusParams, pindexPrev, false);
    pblock->nNonce = 0;

    // NOTE: If at some point we support pre-segwit miners post-segwit-activation, this needs to take segwit support into consideration
    const bool fPreSegWit = (THRESHOLD_ACTIVE != VersionBitsState(pindexPrev, consensusParams, Consensus::DEPLOYMENT_SEGWIT, versionbitscache));

    UniValue aCaps(UniValue::VARR); aCaps.push_back("proposal");

    UniValue transactions(UniValue::VARR);
    map<uint256, int64_t> setTxIndex;
    int i = 0;
    for (CTransaction& tx : pblock->vtx) {
        uint256 txHash = tx.GetHash();
        setTxIndex[txHash] = i++;

        if (tx.IsCoinBase())
            continue;

        UniValue entry(UniValue::VOBJ);

        entry.push_back(Pair("data", EncodeHexTx(tx)));
        entry.push_back(Pair("txid", txHash.GetHex()));
        entry.push_back(Pair("hash", tx.GetWitnessHash().GetHex()));

        UniValue deps(UniValue::VARR);
        for (const CTxIn &in : tx.vin) {
            if (setTxIndex.count(in.prevout.hash))
                deps.push_back(setTxIndex[in.prevout.hash]);
        }
        entry.push_back(Pair("depends", deps));

        int index_in_template = i - 1;
        entry.push_back(Pair("fee", pblocktemplate->vTxFees[index_in_template]));
        int64_t nTxSigOps = pblocktemplate->vTxSigOpsCost[index_in_template];
        if (fPreSegWit) {
            //TODO
            //assert(nTxSigOps % WITNESS_SCALE_FACTOR == 0);
            nTxSigOps /= WITNESS_SCALE_FACTOR;
        }
        entry.push_back(Pair("sigops", nTxSigOps));
        entry.push_back(Pair("cost", GetTransactionCost(tx)));

        transactions.push_back(entry);
    }

    UniValue coinbasetxn(UniValue::VARR);
    map<uint256, int64_t> setTxIndex1;
    int j = 0;
    for (CTransaction& tx : pblock->vtx) {//Incase if multi coinbase
        if (tx.IsCoinBase()) {
            uint256 txHash = tx.GetHash();
            setTxIndex1[txHash] = j++;

            UniValue entry(UniValue::VOBJ);

            entry.push_back(Pair("data", EncodeHexTx(tx)));
            entry.push_back(Pair("txid", txHash.GetHex()));
            entry.push_back(Pair("hash", tx.GetWitnessHash().GetHex()));

            UniValue deps(UniValue::VARR);
            for (const CTxIn& in : tx.vin) {
                if (setTxIndex.count(in.prevout.hash))
                    deps.push_back(setTxIndex[in.prevout.hash]);
            }
            entry.push_back(Pair("depends", deps));

            int index_in_template = j - 1;
            entry.push_back(Pair("fee", pblocktemplate->vTxFees[index_in_template]));
            entry.push_back(Pair("sigops", pblocktemplate->vTxSigOpsCost[index_in_template]));

            coinbasetxn.push_back(entry);
        }
    }

    UniValue aux(UniValue::VOBJ);
    aux.push_back(Pair("flags", HexStr(COINBASE_FLAGS.begin(), COINBASE_FLAGS.end())));

    uint256 hashTarget = uint256().SetCompact(pblock->nBits);

    static UniValue aMutable(UniValue::VARR);
    if (aMutable.empty()) {
        aMutable.push_back("time");
        aMutable.push_back("transactions");
        aMutable.push_back("prevblock");
    }

    UniValue aVotes(UniValue::VARR);
    UniValue superBlock(UniValue::VARR);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("capabilities", aCaps));

    UniValue aRules(UniValue::VARR);
    UniValue vbavailable(UniValue::VOBJ);
    for (int i = 0; i < (int)Consensus::MAX_VERSION_BITS_DEPLOYMENTS; ++i) {
        Consensus::DeploymentPos pos = Consensus::DeploymentPos(i);
        ThresholdState state = VersionBitsState(pindexPrev, consensusParams, pos, versionbitscache);
        switch (state) {
            case THRESHOLD_DEFINED:
            case THRESHOLD_FAILED:
                // Not exposed to GBT at all
                break;
            case THRESHOLD_LOCKED_IN:
                // Ensure bit is set in block version
                pblock->nVersion |= VersionBitsMask(consensusParams, pos);
                // FALL THROUGH to get vbavailable set...
            case THRESHOLD_STARTED:
            {
                const struct BIP9DeploymentInfo& vbinfo = VersionBitsDeploymentInfo[pos];
                vbavailable.push_back(Pair(gbt_vb_name(pos), consensusParams.vDeployments[pos].bit));
                if (setClientRules.find(vbinfo.name) == setClientRules.end()) {
                    if (!vbinfo.gbt_force) {
                        // If the client doesn't support this, don't indicate it in the [default] version
                        pblock->nVersion &= ~VersionBitsMask(consensusParams, pos);
                    }
                }
                break;
            }
            case THRESHOLD_ACTIVE:
            {
                // Add to rules only
                const struct BIP9DeploymentInfo& vbinfo = VersionBitsDeploymentInfo[pos];
                aRules.push_back(gbt_vb_name(pos));
                if (setClientRules.find(vbinfo.name) == setClientRules.end()) {
                    // Not supported by the client; make sure it's safe to proceed
                    if (!vbinfo.gbt_force) {
                        // If we do anything other than throw an exception here, be sure version/force isn't sent to old clients
                        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Support for '%s' rule requires explicit client support", vbinfo.name));
                    }
                }
                break;
            }
        }
    }
    result.push_back(Pair("version", pblock->nVersion));
    result.push_back(Pair("rules", aRules));
    result.push_back(Pair("vbavailable", vbavailable));
    result.push_back(Pair("vbrequired", int(0)));

    if (nMaxVersionPreVB >= 2) {
        // If VB is supported by the client, nMaxVersionPreVB is -1, so we won't get here
        // Because BIP 34 changed how the generation transaction is serialized, we can only use version/force back to v2 blocks
        // This is safe to do [otherwise-]unconditionally only because we are throwing an exception above if a non-force deployment gets activated
        // Note that this can probably also be removed entirely after the first BIP9 non-force deployment (ie, probably segwit) gets activated
        aMutable.push_back("version/force");
    }

    int64_t nHeight = pindexPrev->nHeight + 1;
    result.push_back(Pair("previousblockhash", pblock->hashPrevBlock.GetHex()));

    // smart contracts
    if (nHeight >= Params().FirstSCBlock()) {
        // gas refund
        UniValue scrObjArray(UniValue::VARR);
        for (size_t v=2; v < pblock->vtx[0].vout.size(); v++) {
            UniValue aSCrefund(UniValue::VOBJ);
            CTxDestination scTxDest;
            ExtractDestination(pblock->vtx[0].vout[v].scriptPubKey, scTxDest);
            aSCrefund.push_back(Pair("payee", EncodeDestination(scTxDest)));
            aSCrefund.push_back(Pair("script", HexStr(pblock->vtx[0].vout[v].scriptPubKey)));
            aSCrefund.push_back(Pair("amount", (int64_t)pblock->vtx[0].vout[v].nValue));
            scrObjArray.push_back(aSCrefund);
        }
        result.push_back(Pair("stateroot", pblock->hashStateRoot.GetHex()));
        result.push_back(Pair("utxoroot", pblock->hashUTXORoot.GetHex()));
        result.push_back(Pair("screfund", scrObjArray));
    }

    result.push_back(Pair("transactions", transactions));
    result.push_back(Pair("coinbaseaux", aux));
    result.push_back(Pair("coinbasevalue", (int64_t)pblock->vtx[0].GetValueOut()));
    result.push_back(Pair("coinbasetxn", coinbasetxn[0]));
    result.push_back(Pair("longpollid", chainActive.Tip()->GetBlockHash().GetHex() + i64tostr(nTransactionsUpdatedLast)));
    result.push_back(Pair("target", hashTarget.GetHex()));
    result.push_back(Pair("mintime", (int64_t)pindexPrev->GetMedianTimePast() + 1));
    result.push_back(Pair("mutable", aMutable));
    result.push_back(Pair("noncerange", "00000000ffffffff"));
    int64_t nSigOpLimit = MAX_BLOCK_SIGOPS;
    int64_t nSizeLimit = MAX_BLOCK_BASE_SIZE;
    if (fPreSegWit) {
        assert(nSigOpLimit % WITNESS_SCALE_FACTOR == 0);
        nSigOpLimit /= WITNESS_SCALE_FACTOR;
        assert(nSizeLimit % WITNESS_SCALE_FACTOR == 0);
        nSizeLimit /= WITNESS_SCALE_FACTOR;
    }
    result.push_back(Pair("sigoplimit", nSigOpLimit));
    result.push_back(Pair("sizelimit", nSizeLimit));
    result.push_back(Pair("curtime", pblock->GetBlockTime()));
    result.push_back(Pair("bits", strprintf("%08x", pblock->nBits)));
    result.push_back(Pair("height", nHeight));
    if (!pblocktemplate->vchCoinbaseCommitment.empty() && fSupportsSegwit) {
        result.push_back(Pair("default_witness_commitment", HexStr(pblocktemplate->vchCoinbaseCommitment.begin(), pblocktemplate->vchCoinbaseCommitment.end())));
    }
    result.push_back(Pair("votes", aVotes));

    bool mnStarted = nHeight >= Params().FirstSplitRewardBlock();
    UniValue aMasternode(UniValue::VOBJ);
    if (mnStarted && pblock->vtx[0].vout.size() > 1) {
        CTxDestination mnTxDest;
        // todo: check if vout offset is always correct (with segwit)
        ExtractDestination(pblock->vtx[0].vout[1].scriptPubKey, mnTxDest);
        aMasternode.push_back(Pair("payee", EncodeDestination(mnTxDest)));
        aMasternode.push_back(Pair("script", HexStr(pblock->vtx[0].vout[1].scriptPubKey)));
        aMasternode.push_back(Pair("amount", (int64_t)pblock->vtx[0].vout[1].nValue));
    }

    result.push_back(Pair("masternode", aMasternode));
    result.push_back(Pair("masternode_payments_started", mnStarted));
    result.push_back(Pair("masternode_payments_enforced", mnStarted && MasternodePaymentsEnabled()));

    return result;
}

UniValue getwork(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() > 1)
        throw runtime_error(
                "getwork [data]\n"
                "If [data] is not specified, returns formatted hash data to work on:\n"
                "  \"algo\" : current proof-of-work algorithm name\n"
                "  \"midstate\" : precomputed hash state after hashing the first half of the data (DEPRECATED)\n" // deprecated
                "  \"data\" : block data\n"
                "  \"target\" : little endian hash target\n"
                "If [data] is specified, tries to solve the block and returns true if it was successful.");


    // Disable checking block downloading and number of connected nodes for segwittest network
    // because it is tested locally, without any nodes connected, and with significant amount of time between blocks
    if (Params().NetworkID() != CBaseChainParams::SEGWITTEST) {
         if (vNodes.empty())
             throw JSONRPCError(RPC_CLIENT_NOT_CONNECTED, "Lux is not connected!");

         if (IsInitialBlockDownload())
             throw JSONRPCError(RPC_CLIENT_IN_INITIAL_DOWNLOAD, "Lux is downloading blocks...");
    }

    if (chainActive.Height() >= Params().LAST_POW_BLOCK())
        throw JSONRPCError(RPC_MISC_ERROR, "No more PoW blocks");

    typedef map<uint256, pair<CBlock*, CScript> > mapNewBlock_t;
    static mapNewBlock_t mapNewBlock;    // FIXME: thread safety
    static std::vector<CBlockTemplate*> vNewBlockTemplate;

    if (params.size() == 0) {
        // Update block
        static unsigned int nTransactionsUpdatedLast;
        static CBlockIndex* pindexPrev;
        static int64_t nStart;
        static std::unique_ptr<CBlockTemplate> pblocktemplate;

        if (pindexPrev != chainActive.Tip() ||
            (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)) {
            if (pindexPrev != chainActive.Tip()) {
                // Deallocate old blocks since they're obsolete now
                mapNewBlock.clear();
                for (CBlockTemplate* pblocktemplate : vNewBlockTemplate)
                    delete pblocktemplate;
                vNewBlockTemplate.clear();
            }

            // Clear pindexPrev so future getworks make a new block, despite any failures from here on
            pindexPrev = nullptr;

            // Store the chainActive.Tip() used before CreateNewBlock, to avoid races
            nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrevNew = chainActive.Tip();
            nStart = GetTime();

            // Create new block
            CReserveKey reservekey(pwalletMain);

            CPubKey pubkey;
            reservekey.GetReservedKey(pubkey, true);

            pblocktemplate = BlockAssembler(Params()).CreateNewBlockWithKey(reservekey, false);

            if (!pblocktemplate)
                throw JSONRPCError(RPC_OUT_OF_MEMORY, "Out of memory");

            // Need to update only after we know CreateNewBlock succeeded
            pindexPrev = pindexPrevNew;
        }

        CBlock* pblock = &pblocktemplate->block; // pointer for convenience

        // Update nTime
        UpdateTime(pblock, Params().GetConsensus(), pindexPrev, false);
        pblock->nNonce = 0;

        // Update nExtraNonce
        static unsigned int nExtraNonce = 0;
        IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

        // Save
        mapNewBlock[pblock->hashMerkleRoot] = make_pair(pblock, pblock->vtx[0].vin[0].scriptSig);

        char pmidstate[128] = { 0 };
        char pdata[144] = { 0 };
        FormatHashBuffers(pblock, pmidstate, pdata);

        uint256 hashTarget = uint256().SetCompact(pblock->nBits);

        UniValue result(UniValue::VOBJ);
        result.push_back(Pair("algo", (chainActive.Height()+1) < Params().SwitchPhi2Block() ? "phi1612" : "phi2"));
        result.push_back(Pair("midstate", HexStr(BEGIN(pmidstate), END(pmidstate))));
        if (pblock->nVersion & (1 << 30))
            result.push_back(Pair("data", HexStr(BEGIN(pdata), END(pdata))));
        else
            result.push_back(Pair("data", HexStr(BEGIN(pdata), &pdata[128])));
        result.push_back(Pair("target",   HexStr(BEGIN(hashTarget), END(hashTarget))));
        return result;
    } else {
        // Parse parameters
        vector<unsigned char> vchData = ParseHex(params[0].get_str());
        if (vchData.size() != 80 && vchData.size() != 128 && vchData.size() != 144)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");

        CBlock* pdata = (CBlock*)&vchData[0];
        // Byte reverse
        for (size_t i = 0; i < vchData.size()/sizeof(unsigned int); i++)
            ((unsigned int*)pdata)[i] = ByteReverse(((unsigned int*)pdata)[i]);

        // Get saved block
        if (!mapNewBlock.count(pdata->hashMerkleRoot))
            return false;
        CBlock* pblock = mapNewBlock[pdata->hashMerkleRoot].first;

        pblock->nTime = pdata->nTime;
        pblock->nNonce = pdata->nNonce;
        if (pdata->nVersion & (1 << 30)) {
            pblock->hashStateRoot = pdata->hashStateRoot;
            pblock->hashUTXORoot = pdata->hashUTXORoot;
        }

        CMutableTransaction newTx(pblock->vtx[0]);
        // Use CMutableTransaction when creating a new transaction instead of CTransaction.  CTransaction public variables are all const now.
        newTx.vin[0].scriptSig = mapNewBlock[pdata->hashMerkleRoot].second; // Oh, why? because vin is const in CTransaction now.
        pblock->vtx[0] = newTx;
        pblock->hashMerkleRoot = pblock->BuildMerkleTree();

        CValidationState state;
        return ProcessNewBlock(state, Params(), NULL, pblock);
    }
}

class submitblock_StateCatcher : public CValidationInterface
{
public:
    uint256 hash;
    bool found;
    bool usePhi2;
    CValidationState state;

    submitblock_StateCatcher(const uint256& hashIn, bool usePhi2) : hash(hashIn), found(false), usePhi2(usePhi2), state(){};

protected:
    virtual void BlockChecked(const CBlock& block, const CValidationState& stateIn)
    {
        if (block.GetHash(usePhi2) != hash)
            return;
        found = true;
        state = stateIn;
    };
};

UniValue submitblock(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2) {
        throw runtime_error(
            "submitblock \"hexdata\" ( \"jsonparametersobject\" )\n"
            "\nAttempts to submit new block to network.\n"
            "The 'jsonparametersobject' parameter is currently ignored.\n"
            "See https://en.bitcoin.it/wiki/BIP_0022 for full specification.\n"

            "\nArguments\n"
            "1. \"hexdata\"    (string, required) the hex-encoded block data to submit\n"
            "2. \"jsonparametersobject\"     (string, optional) object of optional parameters\n"
            "    {\n"
            "      \"workid\" : \"id\"    (string, optional) if the server provided a workid, it MUST be included with submissions\n"
            "    }\n"
            "\nResult:\n"
            "\nExamples:\n" +
            HelpExampleCli("submitblock", "\"mydata\"") + HelpExampleRpc("submitblock", "\"mydata\""));
    }

    CBlock block;
    if (!DecodeHexBlk(block, params[0].get_str())) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Block decode failed");
    }

    CValidationState state;
    bool usePhi2, fBlockPresent = false;
    {
        LOCK(cs_main);
        CBlockIndex* pindexPrev = LookupBlockIndex(block.hashPrevBlock);
        UpdateUncommittedBlockStructures(block, pindexPrev, Params().GetConsensus());

        usePhi2 = pindexPrev ? pindexPrev->nHeight + 1 >= Params().SwitchPhi2Block() : false;
        uint256 hash = block.GetHash(usePhi2);
        CBlockIndex* pindex = LookupBlockIndex(hash);
        if (pindex) {
            if (pindex->IsValid(BLOCK_VALID_SCRIPTS)) {
                return "duplicate";
            }
            if (pindex->nStatus & BLOCK_FAILED_MASK) {
                return "duplicate-invalid";
            }
            // Otherwise, we might only have the header - process the block before returning
            fBlockPresent = true;
        }
        bool fValid = TestBlockValidity(state, Params(), block, pindexPrev, false, true);
        if (!state.IsValid()) {
            LogPrintf("%s: block state is not valid!\n", __func__);
        }
        if (!fValid) {
            LogPrintf("%s: block is not valid!\n", __func__);
        }
    }

    submitblock_StateCatcher sc(block.GetHash(usePhi2), usePhi2);
    RegisterValidationInterface(&sc);
    bool fAccepted = ProcessNewBlock(state, Params(), NULL, &block);
    UnregisterValidationInterface(&sc);
    if (fBlockPresent) {
        if (fAccepted && !sc.found) {
            return "duplicate-inconclusive";
        }
        return "duplicate";
    }
    if (fAccepted) {
        if (!sc.found) {
            return "inconclusive";
            }
        state = sc.state;
    } else if (state.IsValid()) {
        LogPrintf("%s: block not accepted but state is valid!\n", __func__);
        return "rejected";
    }
    return BIP22ValidationResult(state);
}

UniValue estimatefee(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "estimatefee nblocks\n"
            "\nEstimates the approximate fee per kilobyte\n"
            "needed for a transaction to begin confirmation\n"
            "within nblocks blocks.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric)\n"
            "\nResult:\n"
            "n :    (numeric) estimated fee-per-kilobyte\n"
            "\n"
            "-1.0 is returned if not enough transactions and\n"
            "blocks have been observed to make an estimate.\n"
            "\nExample:\n" +
            HelpExampleCli("estimatefee", "6"));

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));

    int nBlocks = params[0].get_int();
    if (nBlocks < 1)
        nBlocks = 1;

    CFeeRate feeRate = mempool.estimateFee(nBlocks);
    if (feeRate == CFeeRate(0))
        return -1.0;

    return ValueFromAmount(feeRate.GetFeePerK());
}

UniValue estimatepriority(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "estimatepriority nblocks\n"
            "\nEstimates the approximate priority\n"
            "a zero-fee transaction needs to begin confirmation\n"
            "within nblocks blocks.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric)\n"
            "\nResult:\n"
            "n :    (numeric) estimated priority\n"
            "\n"
            "-1.0 is returned if not enough transactions and\n"
            "blocks have been observed to make an estimate.\n"
            "\nExample:\n" +
            HelpExampleCli("estimatepriority", "6"));

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));

    int nBlocks = params[0].get_int();
    if (nBlocks < 1)
        nBlocks = 1;

    return mempool.estimatePriority(nBlocks);
}

UniValue estimatesmartfee(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "estimatesmartfee nblocks\n"
            "\nWARNING: This interface is unstable and may disappear or change!\n"
            "\nEstimates the approximate fee per kilobyte needed for a transaction to begin\n"
            "confirmation within nblocks blocks if possible and return the number of blocks\n"
            "for which the estimate is valid.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric)\n"
            "\nResult:\n"
            "{\n"
            "  \"feerate\" : x.x,     (numeric) estimate fee-per-kilobyte (in BTC)\n"
            "  \"blocks\" : n         (numeric) block number where estimate was found\n"
            "}\n"
            "\n"
            "A negative value is returned if not enough transactions and blocks\n"
            "have been observed to make an estimate for any number of blocks.\n"
            "However it will not return a value below the mempool reject fee.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatesmartfee", "6")
            );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));

    int nBlocks = params[0].get_int();

    UniValue result(UniValue::VOBJ);
    int answerFound;
    CFeeRate feeRate = mempool.estimateSmartFee(nBlocks, &answerFound);
    result.push_back(Pair("feerate", feeRate == CFeeRate(0) ? -1.0 : ValueFromAmount(feeRate.GetFeePerK())));
    result.push_back(Pair("blocks", answerFound));
    return result;
}

UniValue estimatesmartpriority(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "estimatesmartpriority nblocks\n"
            "\nWARNING: This interface is unstable and may disappear or change!\n"
            "\nEstimates the approximate priority a zero-fee transaction needs to begin\n"
            "confirmation within nblocks blocks if possible and return the number of blocks\n"
            "for which the estimate is valid.\n"
            "\nArguments:\n"
            "1. nblocks     (numeric)\n"
            "\nResult:\n"
            "{\n"
            "  \"priority\" : x.x,    (numeric) estimated priority\n"
            "  \"blocks\" : n         (numeric) block number where estimate was found\n"
            "}\n"
            "\n"
            "A negative value is returned if not enough transactions and blocks\n"
            "have been observed to make an estimate for any number of blocks.\n"
            "However if the mempool reject fee is set it will return 1e9 * MAX_MONEY.\n"
            "\nExample:\n"
            + HelpExampleCli("estimatesmartpriority", "6")
            );

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VNUM));

    int nBlocks = params[0].get_int();

    UniValue result(UniValue::VOBJ);
    int answerFound;
    double priority = mempool.estimateSmartPriority(nBlocks, &answerFound);
    result.push_back(Pair("priority", priority));
    result.push_back(Pair("blocks", answerFound));
    return result;
}
