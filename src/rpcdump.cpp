// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base58.h"
#include "bip38.h"
#include "init.h"
#include "main.h"
#include "rpcserver.h"
#include "script/script.h"
#include "script/standard.h"
#include "sync.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "wallet.h"
#include "merkleblock.h"
#include "core_io.h"

#include <fstream>
#include <secp256k1.h>
#include <stdint.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>
#include <openssl/aes.h>
#include <openssl/sha.h>

#include "univalue/univalue.h"

using namespace std;

void EnsureWalletIsUnlocked();
bool EnsureWalletIsAvailable(bool avoidException);

std::string static EncodeDumpTime(int64_t nTime)
{
    return DateTimeStrFormat("%Y-%m-%dT%H:%M:%SZ", nTime);
}

int64_t static DecodeDumpTime(const std::string& str)
{
    static const boost::posix_time::ptime epoch = boost::posix_time::from_time_t(0);
    static const std::locale loc(std::locale::classic(),
        new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ"));
    std::istringstream iss(str);
    iss.imbue(loc);
    boost::posix_time::ptime ptime(boost::date_time::not_a_date_time);
    iss >> ptime;
    if (ptime.is_not_a_date_time())
        return 0;
    return (ptime - epoch).total_seconds();
}

std::string static EncodeDumpString(const std::string& str)
{
    std::stringstream ret;
    for (unsigned char c : str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

std::string DecodeDumpString(const std::string& str)
{
    std::stringstream ret;
    for (unsigned int pos = 0; pos < str.length(); pos++) {
        unsigned char c = str[pos];
        if (c == '%' && pos + 2 < str.length()) {
            c = (((str[pos + 1] >> 6) * 9 + ((str[pos + 1] - '0') & 15)) << 4) |
                ((str[pos + 2] >> 6) * 9 + ((str[pos + 2] - '0') & 15));
            pos += 2;
        }
        ret << c;
    }
    return ret.str();
}

UniValue importprivkey(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "importprivkey \"luxprivkey\" ( \"label\" rescan )\n"
            "\nAdds a private key (as returned by dumpprivkey) to your wallet.\n"
            "\nArguments:\n"
            "1. \"luxprivkey\"   (string, required) The private key (see dumpprivkey)\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nExamples:\n"
            "\nDump a private key\n" +
            HelpExampleCli("dumpprivkey", "\"myaddress\"") +
            "\nImport the private key with rescan\n" + HelpExampleCli("importprivkey", "\"mykey\"") +
            "\nImport using a label and without rescan\n" + HelpExampleCli("importprivkey", "\"mykey\" \"testing\" false") +
            "\nAs a JSON-RPC call\n" + HelpExampleRpc("importprivkey", "\"mykey\", \"testing\", false"));

    // Whether to perform rescan after import
    bool fRescan = true;

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        EnsureWalletIsUnlocked();

        string strSecret = params[0].get_str();
        string strLabel = "";
        if (params.size() > 1)
            strLabel = params[1].get_str();

        if (params.size() > 2)
            fRescan = params[2].get_bool();

        CBitcoinSecret vchSecret;
        bool fGood = vchSecret.SetString(strSecret);

        if (!fGood) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid private key encoding");
        if (fWalletUnlockStakingOnly)
            throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Wallet is unlocked for staking only.");

        CKey key = vchSecret.GetKey();
        if (!key.IsValid()) throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key outside allowed range");

        CPubKey pubkey = key.GetPubKey();
        assert(key.VerifyPubKey(pubkey));
        CKeyID vchAddress = pubkey.GetID();
        {
            pwalletMain->MarkDirty();
            // We don't know which corresponding address will be used; label them all
            for (const auto& dest : GetAllDestinationsForKey(pubkey)) {
                pwalletMain->SetAddressBook(dest, strLabel, "receive");
            }

            // Don't throw error in case a key is already there
            if (pwalletMain->HaveKey(vchAddress))
                return NullUniValue;

            pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;

            if (!pwalletMain->AddKeyPubKey(key, pubkey))
                throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");

            // whenever a key is imported, we need to scan the whole chain
            pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'
            pwalletMain->LearnAllRelatedScripts(pubkey);
        }
    }

    string msg = "";
    if (fRescan) {
        pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true);

        if (pwalletMain->IsAbortingRescan()) {
            msg =  _("Rescan aborted by user. Please restart your Luxcore wallet with '-rescan' option. Otherwise, transaction data");
            msg += _(" or address book entries might be missing or incorrect");
            LogPrintf("%s: %s\n", __func__, msg);
            throw JSONRPCError(RPC_MISC_ERROR, msg);
        }
        else
        {
            msg =  _("'importprivkey' with 'Rescan' option finished successfully. Please restart your Luxcore wallet. Otherwise, transaction data");
            msg += _(" or address book entries might be missing or incorrect");
        }
    }
    else
    {
        msg =  _("'importprivkey' finished successfully. Please restart Luxcore wallet with '-rescan' option. Otherwise, transaction data");
        msg += _(" or address book entries might be missing or incorrect");
    }

    LogPrintf("%s: %s\n", __func__, msg);

    return msg;
}

void ImportAddress(const CTxDestination& dest, const std::string& strLabel);
void ImportScript(const CScript& script, const std::string& strLabel, bool isRedeemScript)
{
    if (!isRedeemScript && ::IsMine(*pwalletMain, script) == ISMINE_SPENDABLE)
        throw JSONRPCError(RPC_WALLET_ERROR, "The wallet already contains the private key for this address or script");

    pwalletMain->MarkDirty();

    if (!pwalletMain->HaveWatchOnly(script) && !pwalletMain->AddWatchOnly(script))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding address to wallet");

    if (isRedeemScript) {
        if (!pwalletMain->HaveCScript(script) && !pwalletMain->AddCScript(script))
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding p2sh redeemScript to wallet");
        ImportAddress(GetScriptForDestination(CScriptID(script)), strLabel);
    } else {
        CTxDestination destination;
        if (ExtractDestination(script, destination)) {
            pwalletMain->SetAddressBook(destination, strLabel, "receive");
        }
    }
}

void ImportAddress(const CTxDestination& dest, const std::string& strLabel)
{
    CScript script = GetScriptForDestination(dest);
    ImportScript(script, strLabel, false);
    // add to address book or update label
    if (IsValidDestination(dest))
        pwalletMain->SetAddressBook(dest, strLabel, "receive");
}

UniValue importaddress(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "importaddress \"address\" ( \"label\" rescan p2sh)\n"
            "\nAdds an address or script (in hex) that can be watched as if it were in your wallet but cannot be used to spend.\n"
            "\nArguments:\n"
            "1. \"address\"          (string, required) The address\n"
            "2. \"label\"            (string, optional, default=\"\") An optional label\n"
            "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
            "4. p2sh                 (boolean, optional, default=false) Add the P2SH version of the script as well\n"
            "\nNote: This call can take minutes to complete if rescan is true.\n"
            "\nExamples:\n"
            "\nImport an address with rescan\n" +
            HelpExampleCli("importaddress", "\"myaddress\"") +
            "\nImport using a label without rescan\n" + HelpExampleCli("importaddress", "\"myaddress\" \"testing\" false") +
            "\nAs a JSON-RPC call\n" + HelpExampleRpc("importaddress", "\"myaddress\", \"testing\", false"));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CScript script;

    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (params.size() > 2)
        fRescan = params[2].get_bool();

    // Whether to import a p2sh version, too
    bool fP2SH = false;
    if (params.size() > 3)
        fP2SH = params[3].get_bool();

    CTxDestination dest = DecodeDestination(params[0].get_str());
    if (IsValidDestination(dest)) {
        if (fP2SH)
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Cannot use the p2sh flag with an address - use a script instead");
        ImportAddress(dest, strLabel);
    } else if (IsHex(params[0].get_str())) {
        std::vector<unsigned char> data(ParseHex(params[0].get_str()));
        ImportScript(CScript(data.begin(), data.end()), strLabel, fP2SH);
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid LUX address or script");
    }

    if (fRescan)
    {
        pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true);
        pwalletMain->ReacceptWalletTransactions();
    }

    return NullUniValue;
}

UniValue importprunedfunds(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 2)
        throw runtime_error(
                "importprunedfunds\n"
                "\nImports funds without rescan. Corresponding address or script must previously be included in wallet. Aimed towards pruned wallets. The end-user is responsible to import additional transactions that subsequently spend the imported outputs or rescan after the point in the blockchain the transaction is included.\n"
                "\nArguments:\n"
                "1. \"rawtransaction\" (string, required) A raw transaction in hex funding an already-existing address in wallet\n"
                "2. \"txoutproof\"     (string, required) The hex output from gettxoutproof that contains the transaction\n"
        );

    CTransaction tx;
    if (!DecodeHexTx(tx, params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "TX decode failed");
    uint256 hashTx = tx.GetHash();
    CWalletTx wtx(pwalletMain,tx);

    CDataStream ssMB(ParseHexV(params[1], "proof"), SER_NETWORK, PROTOCOL_VERSION);
    CMerkleBlock merkleBlock;
    ssMB >> merkleBlock;

    //Search partial merkle tree in proof for our transaction and index in valid block
    vector<uint256> vMatch;
    vector<unsigned int> vIndex;
    unsigned int txnIndex = 0;
    if (merkleBlock.txn.ExtractMatches(vMatch, vIndex) == merkleBlock.header.hashMerkleRoot) {

        LOCK(cs_main);

        if (!mapBlockIndex.count(merkleBlock.header.GetHash()) || !chainActive.Contains(mapBlockIndex[merkleBlock.header.GetHash()]))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found in chain");

        vector<uint256>::const_iterator it;
        if ((it = std::find(vMatch.begin(), vMatch.end(), hashTx))==vMatch.end()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction given doesn't exist in proof");
        }

        txnIndex = vIndex[it - vMatch.begin()];
    }
    else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Something wrong with merkleblock");
    }

    wtx.nIndex = txnIndex;
    wtx.hashBlock = merkleBlock.header.GetHash();

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (pwalletMain->IsMine(wtx)) {
        pwalletMain->AddToWallet(wtx, false);
        return NullUniValue;
    }

    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No addresses in wallet correspond to included transaction");
}

UniValue removeprunedfunds(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 1)
        throw runtime_error(
                "removeprunedfunds \"txid\"\n"
                "\nDeletes the specified transaction from the wallet. Meant for use with pruned wallets and as a companion to importprunedfunds. This will effect wallet balances.\n"
                "\nArguments:\n"
                "1. \"txid\"           (string, required) The hex-encoded id of the transaction you are deleting\n"
                "\nExamples:\n"
                + HelpExampleCli("removeprunedfunds", "\"e103bd3a6d46e518c395ebafddc63011c958566a54a259979242e2861b6d7010\"") +
                "\nAs a JSON-RPC call\n"
                + HelpExampleRpc("removprunedfunds", "\"e103bd3a6d46e518c395ebafddc63011c958566a54a259979242e2861b6d7010\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint256 hash;
    hash.SetHex(params[0].get_str());
    vector<uint256> vHash;
    vHash.push_back(hash);
    vector<uint256> vHashOut;

    if(pwalletMain->ZapSelectTx(vHash, vHashOut) != DB_LOAD_OK) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Could not properly delete the transaction.");
    }

    if(vHashOut.empty()) {
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Transaction does not exist in wallet.");
    }

    ThreadFlushWalletDB(pwalletMain->strWalletFile);

    return NullUniValue;
}

UniValue importpubkey(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
                "importpubkey \"pubkey\" ( \"label\" rescan )\n"
                "\nAdds a public key (in hex) that can be watched as if it were in your wallet but cannot be used to spend.\n"
                "\nArguments:\n"
                "1. \"pubkey\"           (string, required) The hex-encoded public key\n"
                "2. \"label\"            (string, optional, default=\"\") An optional label\n"
                "3. rescan               (boolean, optional, default=true) Rescan the wallet for transactions\n"
                "\nNote: This call can take minutes to complete if rescan is true.\n"
                "\nExamples:\n"
                "\nImport a public key with rescan\n"
                + HelpExampleCli("importpubkey", "\"mypubkey\"") +
                "\nImport using a label without rescan\n"
                + HelpExampleCli("importpubkey", "\"mypubkey\" \"testing\" false") +
                "\nAs a JSON-RPC call\n"
                + HelpExampleRpc("importpubkey", "\"mypubkey\", \"testing\", false")
        );

    string strLabel = "";
    if (params.size() > 1)
        strLabel = params[1].get_str();

    // Whether to perform rescan after import
    bool fRescan = true;
    if (params.size() > 2)
        fRescan = params[2].get_bool();

    {
        CTxDestination dest;
        if (!IsHex(params[0].get_str()))
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey must be a hex string");
        std::vector<unsigned char> data(ParseHex(params[0].get_str()));
        CPubKey pubKey(data.begin(), data.end());
        if (!pubKey.IsFullyValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Pubkey is not a valid public key");

        LOCK2(cs_main, pwalletMain->cs_wallet);

        ImportAddress(dest, strLabel);
        ImportScript(GetScriptForRawPubKey(pubKey), strLabel, false);

        if (fRescan) {
            pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true);
            pwalletMain->ReacceptWalletTransactions();
        }
    }

    return NullUniValue;
}

UniValue importwallet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "importwallet \"filename\"\n"
            "\nImports keys from a wallet dump file (see dumpwallet).\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The wallet file\n"
            "\nExamples:\n"
            "\nDump the wallet\n" +
            HelpExampleCli("dumpwallet", "\"test\"") +
            "\nImport the wallet\n" + HelpExampleCli("importwallet", "\"test\"") +
            "\nImport using the json rpc call\n" + HelpExampleRpc("importwallet", "\"test\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    ifstream file;
    file.open(params[0].get_str().c_str(), std::ios::in | std::ios::ate);
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    int64_t nTimeBegin = chainActive.Tip()->GetBlockTime();

    bool fGood = true;

    int64_t nFilesize = std::max((int64_t)1, (int64_t)file.tellg());
    file.seekg(0, file.beg);

    pwalletMain->ShowProgress(_("Importing..."), 0); // show progress dialog in GUI
    while (file.good()) {
        pwalletMain->ShowProgress("", std::max(1, std::min(99, (int)(((double)file.tellg() / (double)nFilesize) * 100))));
        std::string line;
        std::getline(file, line);
        if (line.empty() || line[0] == '#')
            continue;

        std::vector<std::string> vstr;
        boost::split(vstr, line, boost::is_any_of(" "));
        if (vstr.size() < 2)
            continue;
        CBitcoinSecret vchSecret;
        if (!vchSecret.SetString(vstr[0]))
            continue;
        CKey key = vchSecret.GetKey();
        CPubKey pubkey = key.GetPubKey();
        assert(key.VerifyPubKey(pubkey));
        CKeyID keyid = pubkey.GetID();
        if (pwalletMain->HaveKey(keyid)) {
            LogPrintf("Skipping import of %s (key already present)\n", EncodeDestination(keyid));
            continue;
        }
        int64_t nTime = DecodeDumpTime(vstr[1]);
        std::string strLabel;
        bool fLabel = true;
        for (unsigned int nStr = 2; nStr < vstr.size(); nStr++) {
            if (boost::algorithm::starts_with(vstr[nStr], "#"))
                break;
            if (vstr[nStr] == "change=1")
                fLabel = false;
            if (vstr[nStr] == "reserve=1")
                fLabel = false;
            if (boost::algorithm::starts_with(vstr[nStr], "label=")) {
                strLabel = DecodeDumpString(vstr[nStr].substr(6));
                fLabel = true;
            }
        }
        LogPrintf("Importing %s...\n", EncodeDestination(keyid));
        if (!pwalletMain->AddKeyPubKey(key, pubkey)) {
            fGood = false;
            continue;
        }
        pwalletMain->mapKeyMetadata[keyid].nCreateTime = nTime;
        if (fLabel)
            pwalletMain->SetAddressBook(keyid, strLabel, "receive");
        nTimeBegin = std::min(nTimeBegin, nTime);
    }
    file.close();
    pwalletMain->ShowProgress("", 100); // hide progress dialog in GUI
#if 0
    CBlockIndex* pindex = chainActive.Tip();
    while (pindex && pindex->pprev && pindex->GetBlockTime() > nTimeBegin - 7200)
        pindex = pindex->pprev;
#endif
    if (!pwalletMain->nTimeFirstKey || nTimeBegin < pwalletMain->nTimeFirstKey)
        pwalletMain->nTimeFirstKey = nTimeBegin;

    CBlockIndex* pindex = chainActive.FindEarliestAtLeast(nTimeBegin - 7200);
    LogPrintf("Rescanning last %i blocks\n", pindex ? chainActive.Height() - pindex->nHeight + 1 : 0);
    pwalletMain->ScanForWalletTransactions(pindex);
    pwalletMain->MarkDirty();

    if (!fGood)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error adding some keys to wallet");

    return NullUniValue;
}

UniValue dumpprivkey(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "dumpprivkey \"luxaddress\"\n"
            "\nReveals the private key corresponding to 'luxaddress'.\n"
            "Then the importprivkey can be used with this output\n"
            "\nArguments:\n"
            "1. \"luxaddress\"   (string, required) The lux address for the private key\n"
            "\nResult:\n"
            "\"key\"                (string) The private key\n"
            "\nExamples:\n" +
            HelpExampleCli("dumpprivkey", "\"myaddress\"") + HelpExampleCli("importprivkey", "\"mykey\"") + HelpExampleRpc("dumpprivkey", "\"myaddress\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid LUX address");
        if (fWalletUnlockStakingOnly)
            throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Wallet is unlocked for staking only.");
    }
    auto keyid = GetKeyForDestination(*pwalletMain, dest);
    if (keyid == CKeyID())
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    CKey vchSecret;
    if (!pwalletMain->GetKey(keyid, vchSecret))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + strAddress + " is not known");
    return CBitcoinSecret(vchSecret).ToString();
}

UniValue dumphdinfo(const UniValue& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return NullUniValue;

    if (fHelp || params.size() != 0)
        throw runtime_error(
                "dumphdinfo\n"
                "Returns an object containing sensitive private info about this HD wallet.\n"
                "\nResult:\n"
                "{\n"
                "  \"hdseed\": \"seed\",                    (string) The HD seed (bip32, in hex)\n"
                "  \"mnemonic\": \"words\",                 (string) The mnemonic for this HD wallet (bip39, english words) \n"
                "  \"mnemonicpassphrase\": \"passphrase\",  (string) The mnemonic passphrase for this HD wallet (bip39)\n"
                "}\n"
                "\nExamples:\n"
                + HelpExampleCli("dumphdinfo", "")
                + HelpExampleRpc("dumphdinfo", "")
        );

    LOCK(pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    // add the base58check encoded extended master if the wallet uses HD
    CHDChain hdChainCurrent;
    if (!pwalletMain->GetHDChain(hdChainCurrent))
        throw JSONRPCError(RPC_WALLET_ERROR, "This wallet is not an HD wallet.");

    if (!pwalletMain->GetDecryptedHDChain(hdChainCurrent))
        throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot decrypt HD seed");

    SecureString ssMnemonic;
    SecureString ssMnemonicPassphrase;
    hdChainCurrent.GetMnemonic(ssMnemonic, ssMnemonicPassphrase);

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("hdseed", HexStr(hdChainCurrent.GetSeed())));
    obj.push_back(Pair("mnemonic", ssMnemonic.c_str()));
    obj.push_back(Pair("mnemonicpassphrase", ssMnemonicPassphrase.c_str()));

    return obj;
}


UniValue dumpwallet(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "dumpwallet \"filename\"\n"
            "\nDumps all wallet keys in a human-readable format.\n"
            "\nArguments:\n"
            "1. \"filename\"    (string, required) The filename\n"
            "\nExamples:\n" +
            HelpExampleCli("dumpwallet", "\"test\"") + HelpExampleRpc("dumpwallet", "\"test\""));

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    boost::filesystem::path filepath = params[0].get_str();
    filepath = boost::filesystem::absolute(filepath);

    /* Prevent arbitrary files from being overwritten. There have been reports
     * that users have overwritten wallet files this way:
     * https://github.com/bitcoin/bitcoin/issues/9934
     * It may also avoid other security issues.
     */
    if (boost::filesystem::exists(filepath)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, filepath.string() + " already exists. If you are sure this is what you want, move it out of the way first");
    }

    ofstream file;
    file.open(params[0].get_str().c_str());
    if (!file.is_open())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Cannot open wallet dump file");

    std::map<CKeyID, int64_t> mapKeyBirth;
    std::set<CKeyID> setKeyPool;
    pwalletMain->GetKeyBirthTimes(mapKeyBirth);
    pwalletMain->GetAllReserveKeys(setKeyPool);

    // sort time/key pairs
    std::vector<std::pair<int64_t, CKeyID> > vKeyBirth;
    for (std::map<CKeyID, int64_t>::const_iterator it = mapKeyBirth.begin(); it != mapKeyBirth.end(); it++) {
        vKeyBirth.push_back(std::make_pair(it->second, it->first));
    }
    mapKeyBirth.clear();
    std::sort(vKeyBirth.begin(), vKeyBirth.end());

    // produce output
    file << strprintf("# Wallet dump created by LUX %s (%s)\n", CLIENT_BUILD, CLIENT_DATE);
    file << strprintf("# * Created on %s\n", EncodeDumpTime(GetTime()));
    file << strprintf("# * Best block at time of backup was %i (%s),\n", chainActive.Height(), chainActive.Tip()->GetBlockHash().ToString());
    file << strprintf("#   mined on %s\n", EncodeDumpTime(chainActive.Tip()->GetBlockTime()));
    file << "\n";

    // add the base58check encoded extended master if the wallet uses HD
    CHDChain hdChainCurrent;
    if (pwalletMain->GetHDChain(hdChainCurrent)) {

        if (!pwalletMain->GetDecryptedHDChain(hdChainCurrent))
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Cannot decrypt HD chain");

        SecureString ssMnemonic;
        SecureString ssMnemonicPassphrase;
        hdChainCurrent.GetMnemonic(ssMnemonic, ssMnemonicPassphrase);
        file << "# mnemonic: " << ssMnemonic << "\n";
        file << "# mnemonic passphrase: " << ssMnemonicPassphrase << "\n\n";

        SecureVector vchSeed = hdChainCurrent.GetSeed();
        file << "# HD seed: " << HexStr(vchSeed) << "\n\n";

        CExtKey masterKey;
        masterKey.SetMaster(&vchSeed[0], vchSeed.size());

        CBitcoinExtKey b58extkey;
        b58extkey.SetKey(masterKey);

        file << "# extended private masterkey: " << b58extkey.ToString() << "\n";

        CExtPubKey masterPubkey;
        masterPubkey = masterKey.Neuter();

        CBitcoinExtPubKey b58extpubkey;
        b58extpubkey.SetKey(masterPubkey);
        file << "# extended public masterkey: " << b58extpubkey.ToString() << "\n\n";

        for (size_t i = 0; i < hdChainCurrent.CountAccounts(); ++i) {
            CHDAccount acc;
            if (hdChainCurrent.GetAccount(i, acc)) {
                file << "# external chain counter: " << acc.nExternalChainCounter << "\n";
                file << "# internal chain counter: " << acc.nInternalChainCounter << "\n\n";
            } else {
                file << "# WARNING: ACCOUNT " << i << " IS MISSING!" << "\n\n";
            }
        }
    }
    for (std::vector<std::pair<int64_t, CKeyID> >::const_iterator it = vKeyBirth.begin(); it != vKeyBirth.end(); it++) {

        const CKeyID &keyid = it->second;
        std::string strTime = EncodeDumpTime(it->first);
        std::string strAddr = EncodeDestination(keyid);
        CKey key;
        if (pwalletMain->GetKey(keyid, key)) {
            file << strprintf("%s %s ", CBitcoinSecret(key).ToString(), strTime);
            if (pwalletMain->mapAddressBook.count(keyid)) {
                file << strprintf("label=%s", EncodeDumpString(pwalletMain->mapAddressBook[keyid].name));
            } else if (setKeyPool.count(keyid)) {
                file << "reserve=1";
            } else {
                file << "change=1";
            }
            file << strprintf(" # addr=%s%s\n", strAddr, (pwalletMain->mapHdPubKeys.count(keyid) ? " hdkeypath="+pwalletMain->mapHdPubKeys[keyid].GetKeyPath() : ""));
        }
    }
    file << "\n";
    file << "# End of dump\n";
    file.close();
    return NullUniValue;
}

UniValue bip38encrypt(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "bip38encrypt \"luxaddress\"\n"
            "\nEncrypts a private key corresponding to 'luxaddress'.\n"
            "\nArguments:\n"
            "1. \"luxaddress\"   (string, required) The lux address for the private key (you must hold the key already)\n"
            "2. \"passphrase\"   (string, required) The passphrase you want the private key to be encrypted with - Valid special chars: !#$%&'()*+,-./:;<=>?`{|}~ \n"
            "\nResult:\n"
            "\"key\"                (string) The encrypted private key\n"
            "\nExamples:\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    string strPassphrase = params[1].get_str();

    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid LUX address");
    const CKeyID *keyID = boost::get<CKeyID>(&dest);
    if (!keyID)
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to a key");
    CKey vchSecret;
    if (!pwalletMain->GetKey(*keyID, vchSecret))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key for address " + strAddress + " is not known");

    uint256 privKey = vchSecret.GetPrivKey_256();
    string encryptedOut = BIP38_Encrypt(strAddress, strPassphrase, privKey, vchSecret.IsCompressed());

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("Addess", strAddress));
    result.push_back(Pair("Encrypted Key", encryptedOut));

    return result;
}

UniValue bip38decrypt(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw runtime_error(
            "bip38decrypt \"luxaddress\"\n"
            "\nDecrypts and then imports password protected private key.\n"
            "\nArguments:\n"
            "1. \"passphrase\"   (string, required) The passphrase you want the private key to be encrypted with\n"
            "2. \"encryptedkey\"   (string, required) The encrypted private key\n"

            "\nResult:\n"
            "\"key\"                (string) The decrypted private key\n"
            "\nExamples:\n");

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    /** Collect private key and passphrase **/
    string strPassphrase = params[0].get_str();
    string strKey = params[1].get_str();

    uint256 privKey;
    bool fCompressed;
    if (!BIP38_Decrypt(strPassphrase, strKey, privKey, fCompressed))
        throw JSONRPCError(RPC_WALLET_ERROR, "Failed To Decrypt");

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("privatekey", HexStr(privKey)));

    CKey key;
    key.Set(privKey.begin(), privKey.end(), fCompressed);

    if (!key.IsValid())
        throw JSONRPCError(RPC_WALLET_ERROR, "Private Key Not Valid");

    CPubKey pubkey = key.GetPubKey();
    pubkey.IsCompressed();
    assert(key.VerifyPubKey(pubkey));
    result.push_back(Pair("Address", EncodeDestination(pubkey.GetID())));
    CKeyID vchAddress = pubkey.GetID();
    {
        pwalletMain->MarkDirty();
        pwalletMain->SetAddressBook(vchAddress, "", "receive");

        // Don't throw error in case a key is already there
        if (pwalletMain->HaveKey(vchAddress))
            throw JSONRPCError(RPC_WALLET_ERROR, "Key already held by wallet");

        pwalletMain->mapKeyMetadata[vchAddress].nCreateTime = 1;

        if (!pwalletMain->AddKeyPubKey(key, pubkey))
            throw JSONRPCError(RPC_WALLET_ERROR, "Error adding key to wallet");

        // whenever a key is imported, we need to scan the whole chain
        pwalletMain->nTimeFirstKey = 1; // 0 would be considered 'no value'
        pwalletMain->ScanForWalletTransactions(chainActive.Genesis(), true);
    }

    return result;
}
