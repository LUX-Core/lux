// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "coincontrol.h"
#include "consensus/validation.h"
#include "core_io.h"
#include "init.h"
#include "keystore.h"
#include "main.h"
#include "merkleblock.h"
#include "net.h"
#include "primitives/transaction.h"
#include "rpcserver.h"
#include "script/script.h"
#include "script/sign.h"
#include "script/standard.h"
#include "uint256.h"
#include "utilmoneystr.h"
#include "univalue/univalue.h"

#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <stdint.h>

#include <boost/assign/list_of.hpp>


using namespace boost;
using namespace boost::assign;
using namespace std;

void ScriptPubKeyToJSON(const CScript& scriptPubKey, UniValue& out, bool fIncludeHex)
{
    txnouttype type;
    vector<CTxDestination> addresses;
    int nRequired;

    out.push_back(Pair("asm", scriptPubKey.ToString()));
    if (fIncludeHex)
        out.push_back(Pair("hex", HexStr(scriptPubKey.begin(), scriptPubKey.end())));

    if (!ExtractDestinations(scriptPubKey, type, addresses, nRequired)) {
        out.push_back(Pair("type", GetTxnOutputType(type)));
        return;
    }

    out.push_back(Pair("reqSigs", nRequired));
    out.push_back(Pair("type", GetTxnOutputType(type)));

    UniValue a(UniValue::VARR);
    for (const CTxDestination& addr : addresses)
        a.push_back(EncodeDestination(addr));
    out.push_back(Pair("addresses", a));
}

void TxToJSON(const CTransaction& tx, const uint256 hashBlock, UniValue& entry)
{
    uint256 txid = tx.GetHash();
    entry.push_back(Pair("txid", tx.GetHash().GetHex()));
    entry.push_back(Pair("hash", tx.GetWitnessHash().GetHex()));
    entry.push_back(Pair("size", (int)::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION)));
    entry.push_back(Pair("vsize", (int)::GetVirtualTransactionSize(tx)));
    entry.push_back(Pair("version", tx.nVersion));
    entry.push_back(Pair("locktime", (int64_t)tx.nLockTime));
    UniValue vin(UniValue::VARR);
    for (unsigned int i = 0; i < tx.vin.size(); i++) {
        const CTxIn& txin = tx.vin[i];
        UniValue in(UniValue::VOBJ);
        if (tx.IsCoinBase())
            in.push_back(Pair("coinbase", HexStr(txin.scriptSig.begin(), txin.scriptSig.end())));
        else {
            in.push_back(Pair("txid", txin.prevout.hash.GetHex()));
            in.push_back(Pair("vout", (int64_t)txin.prevout.n));
            UniValue o(UniValue::VOBJ);
            o.push_back(Pair("asm", txin.scriptSig.ToString()));
            o.push_back(Pair("hex", HexStr(txin.scriptSig.begin(), txin.scriptSig.end())));
            in.push_back(Pair("scriptSig", o));
        }
        // Add address and value info if spentindex enabled
        CSpentIndexValue spentInfo;
        CSpentIndexKey spentKey(txin.prevout.hash, txin.prevout.n);
        if (GetSpentIndex(spentKey, spentInfo)) {
           in.push_back(Pair("value", ValueFromAmount(spentInfo.satoshis)));
           in.push_back(Pair("valueSat", spentInfo.satoshis));
           if (spentInfo.hashType == 1) {
               in.push_back(Pair("address", EncodeDestination(CKeyID(spentInfo.hashBytes))));
           } else if (spentInfo.hashType == 2) {
               in.push_back(Pair("address", EncodeDestination(CScriptID(spentInfo.hashBytes))));
           } else if (spentInfo.hashType == 4) {
               in.push_back(Pair("address", EncodeDestination(WitnessV0KeyHash(spentInfo.hashBytes))));
           }
        }
        if (!tx.wit.IsNull()) {
            if (!tx.wit.vtxinwit[i].IsNull()) {
                UniValue txinwitness(UniValue::VARR);
                for (unsigned int j = 0; j < tx.wit.vtxinwit[i].scriptWitness.stack.size(); j++) {
                    std::vector<unsigned char> item = tx.wit.vtxinwit[i].scriptWitness.stack[j];
                    txinwitness.push_back(HexStr(item.begin(), item.end()));
                }
                in.push_back(Pair("txinwitness", txinwitness));
            }

        }
        in.push_back(Pair("sequence", (int64_t)txin.nSequence));
        vin.push_back(in);
    }
    entry.push_back(Pair("vin", vin));
    UniValue vout(UniValue::VARR);
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut& txout = tx.vout[i];
        UniValue out(UniValue::VOBJ);
        out.push_back(Pair("value", ValueFromAmount(txout.nValue)));
        out.push_back(Pair("n", (int64_t)i));
        UniValue o(UniValue::VOBJ);
        ScriptPubKeyToJSON(txout.scriptPubKey, o, true);
        out.push_back(Pair("scriptPubKey", o));
        vout.push_back(out);
        // Add spent information if spentindex is enabled (DASH style)
        CSpentIndexValue spentInfo;
        CSpentIndexKey spentKey(txid, i);
        if (GetSpentIndex(spentKey, spentInfo)) {
            out.push_back(Pair("spentTxId", spentInfo.txhash.GetHex()));
            out.push_back(Pair("spentIndex", (int)spentInfo.inputIndex));
            out.push_back(Pair("spentHeight", spentInfo.blockHeight));
        }
    }
    entry.push_back(Pair("vout", vout));

    if (hashBlock != uint256()) {
        entry.push_back(Pair("blockhash", hashBlock.GetHex()));
        CBlockIndex* pindex = LookupBlockIndex(hashBlock);
        if (pindex) {
            if (chainActive.Contains(pindex)) {
                entry.push_back(Pair("confirmations", 1 + chainActive.Height() - pindex->nHeight));
                entry.push_back(Pair("time", pindex->GetBlockTime()));
                entry.push_back(Pair("blocktime", pindex->GetBlockTime()));
            } else {
                entry.push_back(Pair("confirmations", -1));
            }
        }
    }
}

UniValue getrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getrawtransaction \"txid\" ( verbose )\n"
            "\nNOTE: By default this function only works sometimes. This is when the tx is in the mempool\n"
            "or there is an unspent output in the utxo for this transaction. To make it always work,\n"
            "you need to maintain a transaction index, using the -txindex command line option.\n"
            "\nReturn the raw transaction data.\n"
            "\nIf verbose=0, returns a string that is serialized, hex-encoded data for 'txid'.\n"
            "If verbose is non-zero, returns an Object with information about 'txid'.\n"

            "\nArguments:\n"
            "1. \"txid\"      (string, required) The transaction id\n"
            "2. verbose       (numeric, optional, default=0) If 0, return a string, other return a json object\n"

            "\nResult (if verbose is not set or set to 0):\n"
            "\"data\"      (string) The serialized, hex-encoded data for 'txid'\n"

            "\nResult (if verbose > 0):\n"
            "{\n"
            "  \"hex\" : \"data\",       (string) The serialized, hex-encoded data for 'txid'\n"
            "  \"txid\" : \"id\",        (string) The transaction id (same as provided)\n"
            "  \"hash\" : \"id\",        (string) The transaction hash (differs from txid for witness transactions)\n"
            "  \"size\" : n,             (numeric) The serialized transaction size\n"
            "  \"vsize\" : n,            (numeric) The virtual transaction size (differs from size for witness transactions)\n"
            "  \"version\" : n,          (numeric) The version\n"
            "  \"locktime\" : ttt,       (numeric) The lock time\n"
            "  \"vin\" : [               (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",    (string) The transaction id\n"
            "       \"vout\": n,         (numeric) \n"
            "       \"scriptSig\": {     (json object) The script\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "       },\n"
            "       \"sequence\": n      (numeric) The script sequence number\n"
            "       \"txinwitness\": [\"hex\", ...] (array of string) hex-encoded witness data (if any)\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [              (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,            (numeric) The value in btc\n"
            "       \"n\" : n,                    (numeric) index\n"
            "       \"scriptPubKey\" : {          (json object)\n"
            "         \"asm\" : \"asm\",          (string) the asm\n"
            "         \"hex\" : \"hex\",          (string) the hex\n"
            "         \"reqSigs\" : n,            (numeric) The required sigs\n"
            "         \"type\" : \"pubkeyhash\",  (string) The type, eg 'pubkeyhash'\n"
            "         \"addresses\" : [           (json array of string)\n"
            "           \"luxaddress\"        (string) lux address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"blockhash\" : \"hash\",   (string) the block hash\n"
            "  \"confirmations\" : n,      (numeric) The confirmations\n"
            "  \"time\" : ttt,             (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT)\n"
            "  \"blocktime\" : ttt         (numeric) The block time in seconds since epoch (Jan 1 1970 GMT)\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("getrawtransaction", "\"mytxid\"") + HelpExampleCli("getrawtransaction", "\"mytxid\" 1") + HelpExampleRpc("getrawtransaction", "\"mytxid\", 1"));

    LOCK(cs_main);

    uint256 hash = ParseHashV(params[0], "parameter 1");

    bool fVerbose = false;
    if (params.size() > 1)
        fVerbose = (params[1].get_int() != 0);

    CTransaction tx;
    uint256 hashBlock = uint256();
    if (!GetTransaction(hash, tx, Params().GetConsensus(), hashBlock, true))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available about transaction");

    string strHex = EncodeHexTx(tx);

    if (!fVerbose)
        return strHex;

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hex", strHex));
    TxToJSON(tx, hashBlock, result);
    return result;
}

#ifdef ENABLE_WALLET
UniValue listunspent(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listunspent ( minconf maxconf  [\"address\",...] )\n"
            "\nReturns array of unspent transaction outputs\n"
            "with between minconf and maxconf (inclusive) confirmations.\n"
            "Optionally filter to only include txouts paid to specified addresses.\n"
            "\nArguments:\n"
            "1. minconf          (numeric, optional, default=1) The minimum confirmations to filter\n"
            "2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter\n"
            "3. \"addresses\"    (string) A json array of lux addresses to filter\n"
            "    [\n"
            "      \"address\"   (string) lux address\n"
            "      ,...\n"
            "    ]\n"
            "\nResult\n"
            "[                   (array of json object)\n"
            "  {\n"
            "    \"txid\" : \"txid\",        (string) the transaction id \n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"address\" : \"address\",  (string) the lux address\n"
            "    \"account\" : \"account\",  (string) The associated account, or \"\" for the default account\n"
            "    \"scriptPubKey\" : \"key\", (string) the script key\n"
            "    \"amount\" : x.xxx,         (numeric) the transaction amount in btc\n"
            "    \"confirmations\" : n       (numeric) The number of confirmations\n"
            "    \"ds_rounds\" : n           (numeric) The number of ds round\n"
            "    \"spendable\" : xxx,        (bool) Whether we have the private keys to spend this output\n"
            "    \"redeemScript\" : n        (string) The redeemScript if scriptPubKey is P2SH\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples\n" +
            HelpExampleCli("listunspent", "") + HelpExampleCli("listunspent", "6 9999999 \"[\\\"1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\"") + HelpExampleRpc("listunspent", "6, 9999999 \"[\\\"1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\\\",\\\"1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\\\"]\""));

    RPCTypeCheck(params, list_of(UniValue::VNUM)(UniValue::VNUM)(UniValue::VARR));

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    int nMaxDepth = 9999999;
    if (params.size() > 1)
        nMaxDepth = params[1].get_int();

    std::set<CTxDestination> destinations;
    if (!params[2].isNull()) {
        UniValue inputs = params[2].get_array();
        for (unsigned int idx = 0; idx < inputs.size(); idx++) {
            const UniValue& input = inputs[idx];
            CTxDestination dest = DecodeDestination(input.get_str());
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, std::string("Invalid Bitcoin address: ") + input.get_str());
            }
            if (!destinations.insert(dest).second) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, std::string("Invalid parameter, duplicated address: ") + input.get_str());
            }
        }
    }

    UniValue results(UniValue::VARR);
    vector<COutput> vecOutputs;
    assert(pwalletMain != NULL);
    LOCK2(cs_main, pwalletMain->cs_wallet);
    pwalletMain->AvailableCoins(vecOutputs, false);
    for (const COutput& out : vecOutputs) {
        if (out.nDepth < nMinDepth || out.nDepth > nMaxDepth)
            continue;

        CTxDestination address;
        const CScript& scriptPubKey = out.tx->vout[out.i].scriptPubKey;
        bool fValidAddress = ExtractDestination(scriptPubKey, address);

        if (destinations.size() && (!fValidAddress || !destinations.count(address)))
            continue;

        UniValue entry(UniValue::VOBJ);
        entry.push_back(Pair("txid", out.tx->GetHash().GetHex()));
        entry.push_back(Pair("vout", out.i));

        if (fValidAddress) {
            entry.push_back(Pair("address", EncodeDestination(address)));

            if (pwalletMain->mapAddressBook.count(address)) {
                entry.push_back(Pair("account", pwalletMain->mapAddressBook[address].name));
            }

            if (scriptPubKey.IsPayToScriptHash()) {
                const CScriptID& hash = boost::get<CScriptID>(address);
                CScript redeemScript;
                if (pwalletMain->GetCScript(hash, redeemScript))
                    entry.push_back(Pair("redeemScript", HexStr(redeemScript.begin(), redeemScript.end())));
            }
        }
        entry.push_back(Pair("amount", ValueFromAmount(out.tx->vout[out.i].nValue)));
        entry.push_back(Pair("confirmations", out.nDepth));
        entry.push_back(Pair("ds_rounds", pwalletMain->GetInputDarkSendRounds(CTxIn(out.tx->GetHash(), out.i))));
        entry.push_back(Pair("spendable", out.fSpendable));
        results.push_back(entry);
    }

    return results;
}

UniValue fundrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw std::runtime_error(
            "fundrawtransaction \"hexstring\" ( options iswitness )\n"
            "\nAdd inputs to a transaction until it has enough in value to meet its out value.\n"
            "This will not modify existing inputs, and will add at most one change output to the outputs.\n"
            "No existing outputs will be modified.\n"
            "Note that inputs which were signed may need to be resigned after completion since in/outputs have been added.\n"
            "The inputs added will not be signed, use signrawtransaction for that.\n"
            "Note that all existing inputs must have their previous output transaction be in the wallet.\n"
            "Note that all inputs selected must be of standard form and P2SH scripts must be\n"
            "in the wallet using importaddress or addmultisigaddress (to calculate fees).\n"
            "You can see whether this is the case by checking the \"solvable\" field in the listunspent output.\n"
            "Only pay-to-pubkey, multisig, and P2SH versions thereof are currently supported for watch-only\n"
            "\nArguments:\n"
            "1. \"hexstring\"           (string, required) The hex string of the raw transaction\n"
            "2. options                 (object, optional)\n"
            "   {\n"
            "     \"changeAddress\"          (string, optional, default pool address) The lux address to receive the change\n"
            "     \"changePosition\"         (numeric, optional, default random) The index of the change output\n"
            "     \"includeWatching\"        (boolean, optional, default false) Also select inputs which are watch only\n"
            "     \"lockUnspents\"           (boolean, optional, default false) Lock selected unspent outputs\n"
            "     \"feeRate\"           (numeric, optional, default not set: makes wallet determine the fee) Set a specific feerate (" + CURRENCY_UNIT + " per KB)\n"
            "     \"subtractFeeFromOutputs\" (array, optional) A json array of integers.\n"
            "                              The fee will be equally deducted from the amount of each specified output.\n"
            "                              The outputs are specified by their zero-based index, before any change output is added.\n"
            "                              Those recipients will receive less lux than you enter in their corresponding amount field.\n"
            "                              If no outputs are specified here, the sender pays the fee.\n"
            "                                  [vout_index,...]\n"
            "   }\n"
            "                         for backward compatibility: passing in a true instead of an object will result in {\"includeWatching\":true}\n"
            "3. iswitness               (boolean, optional) Whether the transaction hex is a serialized witness transaction \n"
            "                              If iswitness is not present, heuristic tests will be used in decoding\n"

            "\nResult:\n"
            "{\n"
            "  \"hex\":       \"value\", (string)  The resulting raw transaction (hex-encoded string)\n"
            "  \"fee\":       n,         (numeric) Fee in " + CURRENCY_UNIT + " the resulting transaction pays\n"
            "  \"changepos\": n          (numeric) The position of the added change output, or -1\n"
            "}\n"
            "\nExamples:\n"
            "\nCreate a transaction with no inputs\n"
            + HelpExampleCli("createrawtransaction", "\"[]\" \"{\\\"myaddress\\\":0.01}\"") +
            "\nAdd sufficient unsigned inputs to meet the output value\n"
            + HelpExampleCli("fundrawtransaction", "\"rawtransactionhex\"") +
            "\nSign the transaction\n"
            + HelpExampleCli("signrawtransaction", "\"fundedtransactionhex\"") +
            "\nSend the transaction\n"
            + HelpExampleCli("sendrawtransaction", "\"signedtransactionhex\"")
            );

    RPCTypeCheck(params, {UniValue::VSTR});

    EnsureWalletIsUnlocked();

    assert(pwalletMain != nullptr);
    LOCK2(cs_main, pwalletMain->cs_wallet);

    CCoinControl coinControl;
    int changePosition = -1;
    bool lockUnspents = false;
    UniValue subtractFeeFromOutputs;
    set<int> setSubtractFeeFromOutputs;
    bool overrideEstimatedFeerate = false;
    CFeeRate feeRate = CFeeRate(0);

    if (!params[1].isNull()) {
        if (params[1].type() == UniValue::VBOOL) {
            // backward compatibility bool only fallback
            coinControl.fAllowWatchOnly = params[1].get_bool();
        } else {
            RPCTypeCheck(params, {UniValue::VSTR, UniValue::VOBJ, UniValue::VBOOL});

            UniValue options = params[1];

            RPCTypeCheckObj(options,
                {
                    {"changeAddress", UniValue::VSTR},
                    {"changePosition", UniValue::VNUM},
                    {"includeWatching", UniValue::VBOOL},
                    {"lockUnspents", UniValue::VBOOL},
                    {"feeRate", UniValueType()},
                    {"reserveChangeKey", UniValue::VBOOL}, // DEPRECATED (and ignored), should be removed in 0.16 or so.
                    {"subtractFeeFromOutputs", (UniValue::VARR)},
                },
                true);

            if (options.exists("changeAddress")) {
                CTxDestination dest = DecodeDestination(options["changeAddress"].get_str());

                if (!IsValidDestination(dest)) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "changeAddress must be a valid lux address");
                }

                coinControl.destChange = dest;
            }

            if (options.exists("changePosition"))
                changePosition = options["changePosition"].get_int();

            if (options.exists("includeWatching"))
                coinControl.fAllowWatchOnly = options["includeWatching"].get_bool();

            if (options.exists("lockUnspents"))
                lockUnspents = options["lockUnspents"].get_bool();

            if (options.exists("feeRate")) {
            feeRate = CFeeRate(AmountFromValue(options["feeRate"]));
            overrideEstimatedFeerate = true;
             }

            if (options.exists("subtractFeeFromOutputs"))
            subtractFeeFromOutputs = options["subtractFeeFromOutputs"].get_array();
        }
    }

    // parse hex string from parameter
    CTransaction ctx;
    bool try_no_witness = params[2].isNull() ? true : !params[2].get_bool();
    if (!DecodeHexTx(ctx, params[0].get_str(), try_no_witness)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    }

    CMutableTransaction tx(ctx);

    if (tx.vout.size() == 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "TX must have at least one output");

    if (changePosition != -1 && (changePosition < 0 || (unsigned int)changePosition > tx.vout.size()))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "changePosition out of bounds");

    for (unsigned int idx = 0; idx < subtractFeeFromOutputs.size(); idx++) {
        int pos = subtractFeeFromOutputs[idx].get_int();
        if (setSubtractFeeFromOutputs.count(pos))
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, duplicated position: %d", pos));
        if (pos < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, negative position: %d", pos));
        if (pos >= int(tx.vout.size()))
            throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Invalid parameter, position too large: %d", pos));
        setSubtractFeeFromOutputs.insert(pos);
    }

    CAmount nFeeOut;
    std::string strFailReason;

    if (!pwalletMain->FundTransaction(tx, nFeeOut, overrideEstimatedFeerate, changePosition, strFailReason, lockUnspents, setSubtractFeeFromOutputs, &coinControl)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strFailReason);
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hex", EncodeHexTx(tx)));
    result.push_back(Pair("changepos", changePosition));
    result.push_back(Pair("fee", ValueFromAmount(nFeeOut)));

    return result;
}


#endif

UniValue createrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "createrawtransaction [{\"txid\":\"id\",\"vout\":n},...] {\"address\":amount,\"data\":\"hex\",...} ( locktime ) ( replaceable )\n"
            "\nCreate a transaction spending the given inputs and creating new outputs.\n"
                "Outputs can be addresses or data.\n"
            "Returns hex-encoded raw transaction.\n"
            "Note that the transaction's inputs are not signed, and\n"
            "it is not stored in the wallet or transmitted to the network.\n"

            "\nArguments:\n"
            "1. \"inputs\"                (array, required) A json array of json objects\n"
            "     [\n"
            "       {\n"
            "         \"txid\":\"id\",  (string, required) The transaction id\n"
            "         \"vout\":n,        (numeric, required) The output number\n"
            "         \"sequence\":n      (numeric, optional) The sequence number\n"
            "       } \n"
            "       ,...\n"
            "     ]\n"
            "2. \"outputs\"               (object, required) a json object with outputs\n"
            "    {\n"
            "      \"address\": x.xxx,    (numeric or string, required) The key is the lux address, the numeric value (can be string) is the " + CURRENCY_UNIT + " amount\n"
            "      \"data\": \"hex\"      (string, required) The key is \"data\", the value is hex encoded data\n"
            "      \"contract\":{\n"
            "         \"contractAddress\":\"address\", (string, required) Valid contract address (valid hash160 hex data)\n"
            "         \"data\":\"hex\",                (string, required) Hex data to add in the call output\n"
            "         \"amount\":x.xxx,                (numeric, optional) Value in LUX to send with the call, should be a valid amount, default 0\n"
            "         \"gasLimit\":x,                  (numeric, optional) The gas limit for the transaction\n"
            "         \"gasPrice\":x.xxx               (numeric, optional) The gas price for the transaction\n"
            "       } \n"
            "      ,...\n"
            "    }\n"

            "\nResult:\n"
            "\"transaction\"            (string) hex string of the transaction\n"
            "\nExamples\n" +
            HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"address\\\":0.01}\"") + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"address\\\":0.01}\""));"\nExamples:\n" + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"address\\\":0.01}\"") + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"data\\\":\\\"00010203\\\"}\"") + HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"contract\\\":{\\\"contractAddress\\\":\\\"mycontract\\\",""\\\"data\\\":\\\"00\\\", \\\"gasLimit\\\":250000, \\\"gasPrice\\\":0.00000040, \\\"amount\\\":0}}\"") + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"address\\\":0.01}\"") + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\", \"{\\\"data\\\":\\\"00010203\\\"}\"") + HelpExampleRpc("createrawtransaction", "\"[{\\\"txid\\\":\\\"myid\\\",\\\"vout\\\":0}]\" \"{\\\"contract\\\":{\\\"contractAddress\\\":\\\"mycontract\\\",""\\\"data\\\":\\\"00\\\", \\\"gasLimit\\\":250000, \\\"gasPrice\\\":0.00000040, \\\"amount\\\":0}}\"");

    RPCTypeCheck(params, {UniValue::VARR, UniValue::VOBJ, UniValue::VNUM, UniValue::VBOOL}, true);
    if (params[0].isNull() || params[1].isNull())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, arguments 1 and 2 must be non-null");

    UniValue inputs = params[0].get_array();
    UniValue sendTo = params[1].get_obj();

    CMutableTransaction rawTx;


    for (unsigned int i = 0; i < inputs.size(); i++) {
         UniValue input = inputs[i];

        const UniValue& o = input.get_obj();

        uint256 txid = ParseHashO(o, "txid");

        const UniValue& vout_v = find_value(o, "vout");
        if (!vout_v.isNum())
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, missing vout key");
        int nOutput = vout_v.get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        CTxIn in(COutPoint(txid, nOutput));
        rawTx.vin.push_back(in);
    }

    set<CTxDestination > destinations;
    vector<string> addrList = sendTo.getKeys();
    for (const string& name_ : addrList) {

        if (name_ == "data") {
            vector<unsigned char> data = ParseHexV(sendTo[name_].getValStr(),"Data");

            CTxOut out(0, CScript() << OP_RETURN << data);
            rawTx.vout.push_back(out);
        } else if (name_ == "contract") {
            // Get the call object
            UniValue Contract = sendTo[name_];
            if(!Contract.isObject())
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, need to be object: ")+name_);

            // Get dgp gas limit and gas price
            LOCK(cs_main);
            LuxDGP luxDGP(globalState.get(), fGettingValuesDGP);
            uint64_t blockGasLimit = luxDGP.getBlockGasLimit(chainActive.Height());
            uint64_t minGasPrice = CAmount(luxDGP.getMinGasPrice(chainActive.Height()));
            CAmount nGasPrice = (minGasPrice>DEFAULT_GAS_PRICE)?minGasPrice:DEFAULT_GAS_PRICE;

            // Get the contract address
            if(!Contract.exists("contractAddress") || !Contract["contractAddress"].isStr())
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, contract address is mandatory."));

           string contractaddress = Contract["contractAddress"].get_str();
           if(contractaddress.size() != 40 || !CheckHex(contractaddress))
               throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Incorrect contract address");

            dev::Address addrAccount(contractaddress);
            if(!globalState->addressInUse(addrAccount))
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "contract address does not exist");

            // Get the contract data
            if(!Contract.exists("data") || !Contract["data"].isStr())
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, contract data is mandatory."));

            string datahex = Contract["data"].get_str();
            if(datahex.size() % 2 != 0 || !CheckHex(datahex))
                throw JSONRPCError(RPC_TYPE_ERROR, "Invalid data (data not hex)");

            // Get amount
            CAmount nAmount = 0;
            if (Contract.exists("amount")){
                nAmount = AmountFromValue(Contract["amount"]);
                if (nAmount < 0)
                    throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount for call contract");
            }

            // Get gas limit
            uint64_t nGasLimit=DEFAULT_GAS_LIMIT_OP_SEND;
            if (Contract.exists("gasLimit")){
                nGasLimit = Contract["gasLimit"].get_int64();
                if (nGasLimit > blockGasLimit)throw JSONRPCError(RPC_TYPE_ERROR, "Invalid value for gasLimit (Maximum is: "+i64tostr(blockGasLimit)+")");
                if (nGasLimit < MINIMUM_GAS_LIMIT)throw JSONRPCError(RPC_TYPE_ERROR, "Invalid value for gasLimit (Minimum is: "+i64tostr(MINIMUM_GAS_LIMIT)+")");
                if (nGasLimit <= 0)throw JSONRPCError(RPC_TYPE_ERROR, "Invalid value for gasLimit");
            }

            // Get gas price
            if (Contract.exists("gasPrice")){
                UniValue uGasPrice = Contract["gasPrice"];
                if(!ParseMoney(uGasPrice.getValStr(), nGasPrice))
                {
                    throw JSONRPCError(RPC_TYPE_ERROR, "Invalid value for gasPrice");
                }
                CAmount maxRpcGasPrice = GetArg("-rpcmaxgasprice", MAX_RPC_GAS_PRICE);
                if (nGasPrice > (int64_t)maxRpcGasPrice)
                    throw JSONRPCError(RPC_TYPE_ERROR, "Invalid value for gasPrice, Maximum allowed in RPC calls is: "+FormatMoney(maxRpcGasPrice)+" (use -rpcmaxgasprice to change it)");
                if (nGasPrice < (int64_t)minGasPrice)
                    throw JSONRPCError(RPC_TYPE_ERROR, "Invalid value for gasPrice (Minimum is: "+FormatMoney(minGasPrice)+")");
                if (nGasPrice <= 0)
                    throw JSONRPCError(RPC_TYPE_ERROR, "Invalid value for gasPrice");
            }

            // Add call contract output
            CScript scriptPubKey = CScript() << CScriptNum(VersionVM::GetEVMDefault().toRaw()) << CScriptNum(nGasLimit) << CScriptNum(nGasPrice) << ParseHex(datahex) << ParseHex(contractaddress) << OP_CALL;
            CTxOut out(nAmount, scriptPubKey);
            rawTx.vout.push_back(out);
        } else {
            CTxDestination destination = DecodeDestination(name_);
            if (!IsValidDestination(destination)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Lux address: ")+name_);
            }

            if (!destinations.insert(destination).second) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ") + name_);
            }

        CScript scriptPubKey = GetScriptForDestination(destination);
        CAmount nAmount = AmountFromValue(sendTo[name_]);
        CTxOut out(nAmount, scriptPubKey);
        rawTx.vout.push_back(out);
        }
    }

    return EncodeHexTx(rawTx);
}

UniValue decoderawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "decoderawtransaction \"hexstring\"\n"
            "\nReturn a JSON object representing the serialized, hex-encoded transaction.\n"

            "\nArguments:\n"
            "1. \"hex\"      (string, required) The transaction hex string\n"

            "\nResult:\n"
            "{\n"
            "  \"txid\" : \"id\",        (string) The transaction id\n"
            "  \"hash\" : \"id\",        (string) The transaction hash (differs from txid for witness transactions)\n"
            "  \"size\" : n,             (numeric) The transaction size\n"
            "  \"vsize\" : n,            (numeric) The virtual transaction size (differs from size for witness transactions)\n"
            "  \"version\" : n,          (numeric) The version\n"
            "  \"locktime\" : ttt,       (numeric) The lock time\n"
            "  \"vin\" : [               (array of json objects)\n"
            "     {\n"
            "       \"txid\": \"id\",    (string) The transaction id\n"
            "       \"vout\": n,         (numeric) The output number\n"
            "       \"scriptSig\": {     (json object) The script\n"
            "         \"asm\": \"asm\",  (string) asm\n"
            "         \"hex\": \"hex\"   (string) hex\n"
            "       },\n"
            "       \"txinwitness\": [\"hex\", ...] (array of string) hex-encoded witness data (if any)\n"
            "       \"sequence\": n     (numeric) The script sequence number\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "  \"vout\" : [             (array of json objects)\n"
            "     {\n"
            "       \"value\" : x.xxx,            (numeric) The value in btc\n"
            "       \"n\" : n,                    (numeric) index\n"
            "       \"scriptPubKey\" : {          (json object)\n"
            "         \"asm\" : \"asm\",          (string) the asm\n"
            "         \"hex\" : \"hex\",          (string) the hex\n"
            "         \"reqSigs\" : n,            (numeric) The required sigs\n"
            "         \"type\" : \"pubkeyhash\",  (string) The type, eg 'pubkeyhash'\n"
            "         \"addresses\" : [           (json array of string)\n"
            "           \"12tvKAXCxZjSmdNbao16dKXC8tRWfcF5oc\"   (string) lux address\n"
            "           ,...\n"
            "         ]\n"
            "       }\n"
            "     }\n"
            "     ,...\n"
            "  ],\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("decoderawtransaction", "\"hexstring\"") + HelpExampleRpc("decoderawtransaction", "\"hexstring\""));

    LOCK(cs_main);

    RPCTypeCheck(params, list_of(UniValue::VSTR));

    CTransaction tx;

    if (!DecodeHexTx(tx, params[0].get_str(), true))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");

    UniValue result(UniValue::VOBJ);
    TxToJSON(tx, uint256(), result);

    return result;
}

UniValue decodescript(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "decodescript \"hex\"\n"
            "\nDecode a hex-encoded script.\n"
            "\nArguments:\n"
            "1. \"hex\"     (string) the hex encoded script\n"
            "\nResult:\n"
            "{\n"
            "  \"asm\":\"asm\",   (string) Script public key\n"
            "  \"hex\":\"hex\",   (string) hex encoded public key\n"
            "  \"type\":\"type\", (string) The output type\n"
            "  \"reqSigs\": n,    (numeric) The required signatures\n"
            "  \"addresses\": [   (json array of string)\n"
            "     \"address\"     (string) lux address\n"
            "     ,...\n"
            "  ],\n"
            "  \"p2sh\",\"address\" (string) script address\n"
            "}\n"
            "\nExamples:\n" +
            HelpExampleCli("decodescript", "\"hexstring\"") + HelpExampleRpc("decodescript", "\"hexstring\""));

    LOCK(cs_main);

    RPCTypeCheck(params, list_of(UniValue::VSTR));

    UniValue r(UniValue::VOBJ);
    CScript script;
    if (params[0].get_str().size() > 0) {
        vector<unsigned char> scriptData(ParseHexV(params[0], "argument"));
        script = CScript(scriptData.begin(), scriptData.end());
    } else {
        // Empty scripts are valid
    }
    ScriptPubKeyToJSON(script, r, false);

    r.push_back(Pair("p2sh", EncodeDestination(CScriptID(script))));
    return r;
}

UniValue signrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "signrawtransaction \"hexstring\" ( [{\"txid\":\"id\",\"vout\":n,\"scriptPubKey\":\"hex\",\"redeemScript\":\"hex\"},...] [\"privatekey1\",...] sighashtype )\n"
            "\nSign inputs for raw transaction (serialized, hex-encoded).\n"
            "The second optional argument (may be null) is an array of previous transaction outputs that\n"
            "this transaction depends on but may not yet be in the block chain.\n"
            "The third optional argument (may be null) is an array of base58-encoded private\n"
            "keys that, if given, will be the only keys used to sign the transaction.\n"
#ifdef ENABLE_WALLET
            + HelpRequiringPassphrase() + "\n"
#endif

            "\nArguments:\n"
            "1. \"hexstring\"     (string, required) The transaction hex string\n"
            "2. \"prevtxs\"       (string, optional) An json array of previous dependent transaction outputs\n"
            "     [               (json array of json objects, or 'null' if none provided)\n"
            "       {\n"
            "         \"txid\":\"id\",             (string, required) The transaction id\n"
            "         \"vout\":n,                  (numeric, required) The output number\n"
            "         \"scriptPubKey\": \"hex\",   (string, required) script key\n"
            "         \"redeemScript\": \"hex\",   (string, required for P2SH or P2WSH) redeem script\n"
            "         \"amount\": value            (numeric, required) The amount spent\n"
            "       }\n"
            "       ,...\n"
            "    ]\n"
            "3. \"privatekeys\"     (string, optional) A json array of base58-encoded private keys for signing\n"
            "    [                  (json array of strings, or 'null' if none provided)\n"
            "      \"privatekey\"   (string) private key in base58-encoding\n"
            "      ,...\n"
            "    ]\n"
            "4. \"sighashtype\"     (string, optional, default=ALL) The signature hash type. Must be one of\n"
            "       \"ALL\"\n"
            "       \"NONE\"\n"
            "       \"SINGLE\"\n"
            "       \"ALL|ANYONECANPAY\"\n"
            "       \"NONE|ANYONECANPAY\"\n"
            "       \"SINGLE|ANYONECANPAY\"\n"

            "\nResult:\n"
            "{\n"
            "  \"hex\" : \"value\",           (string) The hex-encoded raw transaction with signature(s)\n"
            "  \"complete\" : true|false,   (boolean) If the transaction has a complete set of signatures\n"
            "  \"errors\" : [                 (json array of objects) Script verification errors (if there are any)\n"
            "    {\n"
            "      \"txid\" : \"hash\",           (string) The hash of the referenced, previous transaction\n"
            "      \"vout\" : n,                (numeric) The index of the output to spent and used as input\n"
            "      \"scriptSig\" : \"hex\",       (string) The hex-encoded signature script\n"
            "      \"sequence\" : n,            (numeric) Script sequence number\n"
            "      \"error\" : \"text\"           (string) Verification or signing error related to the input\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("signrawtransaction", "\"myhex\"")
            + HelpExampleRpc("signrawtransaction", "\"myhex\"")
        );

    LOCK(cs_main);


    RPCTypeCheck(params, list_of(UniValue::VSTR)(UniValue::VARR)(UniValue::VARR)(UniValue::VSTR), true);

    vector<unsigned char> txData(ParseHexV(params[0], "argument 1"));
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    vector<CMutableTransaction> txVariants;
    while (!ssData.empty()) {
        try {
            CMutableTransaction tx;
            ssData >> tx;
            txVariants.push_back(tx);
        } catch (const std::exception&) {
            throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
        }
    }

    if (txVariants.empty())
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Missing transaction");

    // mergedTx will end up with all the signatures; it
    // starts as a clone of the rawtx:
    CMutableTransaction mergedTx(txVariants[0]);
    bool fComplete = true;

    // Fetch previous transactions (inputs):
    CCoinsView viewDummy;
    CCoinsViewCache view(&viewDummy);
    {
        LOCK(mempool.cs);
        CCoinsViewCache& viewChain = *pcoinsTip;
        CCoinsViewMemPool viewMempool(&viewChain, mempool);
        view.SetBackend(viewMempool); // temporarily switch cache backend to db+mempool view

        for (const CTxIn& txin : mergedTx.vin) {
            const uint256& prevHash = txin.prevout.hash;
            CCoins coins;
            view.AccessCoins(prevHash); // this is certainly allowed to fail
        }

        view.SetBackend(viewDummy); // switch back to avoid locking mempool for too long
    }

    bool fGivenKeys = false;
    CBasicKeyStore tempKeystore;
    if (params.size() > 2 && !params[2].isNull()) {
        fGivenKeys = true;
        UniValue keys = params[2].get_array();
        for (unsigned int idx = 0; idx < keys.size(); idx++) {
            UniValue k = keys[idx];
            CBitcoinSecret vchSecret;
            bool fGood = vchSecret.SetString(k.get_str());
            if (!fGood)
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key");
            CKey key = vchSecret.GetKey();
            if (!key.IsValid())
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key outside allowed range");
            tempKeystore.AddKey(key);
        }
    }
#ifdef ENABLE_WALLET
    else if (pwalletMain)
        EnsureWalletIsUnlocked();
#endif

    // Add previous txouts given in the RPC call:
    if (params.size() > 1 && !params[1].isNull()) {
        UniValue prevTxs = params[1].get_array();
        for (unsigned int idx = 0; idx < prevTxs.size(); idx++) {

            UniValue p = prevTxs[idx];
            if (!p.isObject())
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "expected object with {\"txid'\",\"vout\",\"scriptPubKey\"}");

            UniValue prevOut = p.get_obj();

            RPCTypeCheckObj(prevOut, { {"txid", UniValueType(UniValue::VSTR)}, {"vout", UniValueType(UniValue::VNUM)}, {"scriptPubKey", UniValueType(UniValue::VSTR)}, });

            uint256 txid = ParseHashO(prevOut, "txid");

            int nOut = find_value(prevOut, "vout").get_int();
            if (nOut < 0)
                throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "vout must be positive");

            vector<unsigned char> pkData(ParseHexO(prevOut, "scriptPubKey"));
            CScript scriptPubKey(pkData.begin(), pkData.end());

            {
                CCoinsModifier coins = view.ModifyCoins(txid);
                if (coins->IsAvailable(nOut) && coins->vout[nOut].scriptPubKey != scriptPubKey) {
                    string err("Previous output scriptPubKey mismatch:\n");
                    err = err + coins->vout[nOut].scriptPubKey.ToString() + "\nvs:\n" +
                          scriptPubKey.ToString();
                    throw JSONRPCError(RPC_DESERIALIZATION_ERROR, err);
                }
                if ((unsigned int)nOut >= coins->vout.size())
                    coins->vout.resize(nOut + 1);
                coins->vout[nOut].scriptPubKey = scriptPubKey;
                coins->vout[nOut].nValue = 0;
                if (prevOut.exists("amount")) {
                    coins->vout[nOut].nValue = AmountFromValue(find_value(prevOut, "amount"));
                }
            }

            // if redeemScript given and not using the local wallet (private keys
            // given), add redeemScript to the tempKeystore so it can be signed:
            if (fGivenKeys && (scriptPubKey.IsPayToScriptHash() || scriptPubKey.IsPayToWitnessScriptHash())) {
                RPCTypeCheckObj(prevOut, { {"txid", UniValueType(UniValue::VSTR)}, {"vout", UniValueType(UniValue::VNUM)}, {"scriptPubKey", UniValueType(UniValue::VSTR)}, {"redeemScript", UniValueType(UniValue::VSTR)},});
                UniValue v = find_value(prevOut, "redeemScript");
                if (!(v.isNull())) {
                    vector<unsigned char> rsData(ParseHexV(v, "redeemScript"));
                    CScript redeemScript(rsData.begin(), rsData.end());
                    tempKeystore.AddCScript(redeemScript);
                    // Automatically also add the P2WSH wrapped version of the script (to deal with P2SH-P2WSH).
                    tempKeystore.AddCScript(GetScriptForWitness(redeemScript));
                }
            }
        }
    }

#ifdef ENABLE_WALLET
    const CKeyStore& keystore = ((fGivenKeys || !pwalletMain) ? tempKeystore : *pwalletMain);
#else
    const CKeyStore& keystore = tempKeystore;
#endif

    int nHashType = SIGHASH_ALL;
    if (params.size() > 3 && !params[3].isNull()) {
        static map<string, int> mapSigHashValues =
            boost::assign::map_list_of(string("ALL"), int(SIGHASH_ALL))(string("ALL|ANYONECANPAY"), int(SIGHASH_ALL | SIGHASH_ANYONECANPAY))(string("NONE"), int(SIGHASH_NONE))(string("NONE|ANYONECANPAY"), int(SIGHASH_NONE | SIGHASH_ANYONECANPAY))(string("SINGLE"), int(SIGHASH_SINGLE))(string("SINGLE|ANYONECANPAY"), int(SIGHASH_SINGLE | SIGHASH_ANYONECANPAY));
        string strHashType = params[3].get_str();
        if (mapSigHashValues.count(strHashType))
            nHashType = mapSigHashValues[strHashType];
        else
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid sighash param");
    }

    bool fHashSingle = ((nHashType & ~SIGHASH_ANYONECANPAY) == SIGHASH_SINGLE);

    // Use CTransaction for the constant parts of the
    // transaction to avoid rehashing.
    const CTransaction txConst(mergedTx);
    // Sign what we can:
    for (unsigned int i = 0; i < mergedTx.vin.size(); i++) {
        CTxIn& txin = mergedTx.vin[i];
        const CCoins* coins = view.AccessCoins(txin.prevout.hash);
        if (coins == NULL || !coins->IsAvailable(txin.prevout.n)) {
            fComplete = false;
            continue;
        }
        const CScript& prevPubKey = coins->vout[txin.prevout.n].scriptPubKey;
        const CAmount& amount = coins->vout[txin.prevout.n].nValue;

        txin.scriptSig.clear();
        SignatureData sigdata;
        // Only sign SIGHASH_SINGLE if there's a corresponding output:
        if (!fHashSingle || (i < mergedTx.vout.size()))
            ProduceSignature(MutableTransactionSignatureCreator(&keystore, &mergedTx, i, amount, nHashType), prevPubKey, sigdata);
        sigdata = CombineSignatures(prevPubKey, TransactionSignatureChecker(&txConst, i, amount), sigdata, DataFromTransaction(mergedTx, i));

        UpdateTransaction(mergedTx, i, sigdata);

        if (!VerifyScript(txin.scriptSig, prevPubKey, mergedTx.wit.vtxinwit.size() > i ? &mergedTx.wit.vtxinwit[i].scriptWitness : nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, TransactionSignatureChecker(&txConst, i, amount)))
            fComplete = false;
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hex", EncodeHexTx(mergedTx)));
    result.push_back(Pair("complete", fComplete));

    return result;
}

UniValue sendrawtransaction(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "sendrawtransaction \"hexstring\" ( allowhighfees )\n"
            "\nSubmits raw transaction (serialized, hex-encoded) to local node and network.\n"
            "\nAlso see createrawtransaction and signrawtransaction calls.\n"
            "\nArguments:\n"
            "1. \"hexstring\"    (string, required) The hex string of the raw transaction)\n"
            "2. allowhighfees    (boolean, optional, default=false) Allow high fees\n"
            "\nResult:\n"
            "\"hex\"             (string) The transaction hash in hex\n"
            "\nExamples:\n"
            "\nCreate a transaction\n" +
            HelpExampleCli("createrawtransaction", "\"[{\\\"txid\\\" : \\\"mytxid\\\",\\\"vout\\\":0}]\" \"{\\\"myaddress\\\":0.01}\"") +
            "Sign the transaction, and get back the hex\n" + HelpExampleCli("signrawtransaction", "\"myhex\"") +
            "\nSend the transaction (signed hex)\n" + HelpExampleCli("sendrawtransaction", "\"signedhex\"") +
            "\nAs a json rpc call\n" + HelpExampleRpc("sendrawtransaction", "\"signedhex\""));

    LOCK(cs_main);

    RPCTypeCheck(params, list_of(UniValue::VSTR)(UniValue::VBOOL));

    // parse hex string from parameter
    CTransaction tx;
    if (!DecodeHexTx(tx, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    uint256 hashTx = tx.GetHash();

    bool fOverrideFees = false;
    if (params.size() > 1)
        fOverrideFees = params[1].get_bool();

    CCoinsViewCache& view = *pcoinsTip;
    const CCoins* existingCoins = view.AccessCoins(hashTx);
    bool fHaveMempool = mempool.exists(hashTx);
    bool fHaveChain = existingCoins && existingCoins->nHeight < 1000000000;
    if (!fHaveMempool && !fHaveChain) {
        // push to local node and sync with wallets
        CValidationState state;
        bool fMissingInputs;
        if (!AcceptToMemoryPool(mempool, state, tx, false, &fMissingInputs, !fOverrideFees)) {
            if (state.IsInvalid()) {
                throw JSONRPCError(RPC_TRANSACTION_REJECTED,
                                   strprintf("%i: %s", state.GetRejectCode(), state.GetRejectReason()));
            } else {
                if (fMissingInputs) {
                    throw JSONRPCError(RPC_TRANSACTION_ERROR, "Missing inputs");
                }
                throw JSONRPCError(RPC_TRANSACTION_ERROR, state.GetRejectReason());
            }
        }
    } else if (fHaveChain) {
        throw JSONRPCError(RPC_TRANSACTION_ALREADY_IN_CHAIN, "transaction already in block chain");
    }
    RelayTransaction(tx);

    return hashTx.GetHex();
}

UniValue gethexaddress(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw std::runtime_error(
                "gethexaddress \"address\"\n"

                "\nConverts a base58 pubkeyhash address to a hex address for use in smart contracts.\n"

                "\nArguments:\n"
                "1. \"address\"      (string, required) The base58 address\n"

                "\nResult:\n"
                "\"hexaddress\"      (string) The raw hex pubkeyhash address for use in smart contracts\n"

                "\nExamples:\n"
                + HelpExampleCli("gethexaddress", "\"address\"")
                + HelpExampleRpc("gethexaddress", "\"address\"")
        );

    CTxDestination address = DecodeDestination(params[0].get_str());
    if (!IsValidDestination(address))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Luxcore address");

    CKeyID *keyid = boost::get<CKeyID>(&address);

    if(!keyid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Only pubkeyhash addresses are supported");

    return HexStr(valtype(keyid->begin(),keyid->end()));
}

UniValue fromhexaddress(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() < 1 || params.size() > 1)
        throw std::runtime_error(
                "fromhexaddress \"hexaddress\"\n"

                "\nConverts a raw hex address to a base58 pubkeyhash address\n"

                "\nArguments:\n"
                "1. \"hexaddress\"      (string, required) The raw hex address\n"

                "\nResult:\n"
                "\"address\"      (string) The base58 pubkeyhash address\n"

                "\nExamples:\n"
                + HelpExampleCli("fromhexaddress", "\"hexaddress\"")
                + HelpExampleRpc("fromhexaddress", "\"hexaddress\"")
        );
    if (params[0].get_str().size() != 40)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid pubkeyhash hex size (should be 40 hex characters)");

    uint160 key(ParseHex(params[0].get_str()));
    CKeyID keyid(key);

    if(!IsValidDestination(CTxDestination(keyid)))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Luxcore address");

    return EncodeDestination(CTxDestination(keyid));
}
