// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activemasternode.h"
#include "db.h"
#include "init.h"
#include "main.h"
#include "masternode-budget.h"
#include "masternode-payments.h"
#include "masternodeconfig.h"
#include "masternodeman.h"
#include "rpcserver.h"
#include "utilmoneystr.h"

#include <boost/tokenizer.hpp>

#include <fstream>
using namespace json_spirit;

void SendMoney(const CTxDestination& address, CAmount nValue, CWalletTx& wtxNew, AvailableCoinsType coin_type = ALL_COINS)
{
    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > pwalletMain->GetBalance())
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    string strError;
    if (pwalletMain->IsLocked()) {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("SendMoney() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // Parse LUX address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    if (!pwalletMain->CreateTransaction(scriptPubKey, nValue, wtxNew, reservekey, nFeeRequired, strError, NULL, coin_type)) {
        if (nValue + nFeeRequired > pwalletMain->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        LogPrintf("SendMoney() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
}

Value obfuscation(const Array& params, bool fHelp)
{
    if (fHelp || params.size() == 0)
        throw runtime_error(
            "obfuscation <LUXaddress> <amount>\n"
            "LUXaddress, reset, or auto (AutoDenominate)"
            "<amount> is a real and will be rounded to the next 0.1" +
            HelpRequiringPassphrase());

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    if (params[0].get_str() == "auto") {
        if (fMasterNode)
            return "Obfuscation is not supported from masternodes";

        return "DoAutomaticDenominating " + (obfuscationPool.DoAutomaticDenominating() ? "successful" : ("failed: " + obfuscationPool.GetStatus()));
    }

    if (params[0].get_str() == "reset") {
        obfuscationPool.Reset();
        return "successfully reset obfuscation";
    }

    if (params.size() != 2)
        throw runtime_error(
            "obfuscation <LUXaddress> <amount>\n"
            "LUXaddress, denominate, or auto (AutoDenominate)"
            "<amount> is a real and will be rounded to the next 0.1" +
            HelpRequiringPassphrase());

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid LUX address");

    // Amount
    CAmount nAmount = AmountFromValue(params[1]);

    // Wallet comments
    CWalletTx wtx;
    //    string strError = pwalletMain->SendMoneyToDestination(address.Get(), nAmount, wtx, ONLY_DENOMINATED);
    SendMoney(address.Get(), nAmount, wtx, ONLY_DENOMINATED);
    //    if (strError != "")
    //        throw JSONRPCError(RPC_WALLET_ERROR, strError);

    return wtx.GetHash().GetHex();
}


Value getpoolinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpoolinfo\n"
            "Returns an object containing anonymous pool-related information.");

    Object obj;
    obj.push_back(Pair("current_masternode", mnodeman.GetCurrentMasterNode()->addr.ToString()));
    obj.push_back(Pair("state", obfuscationPool.GetState()));
    obj.push_back(Pair("entries", obfuscationPool.GetEntriesCount()));
    obj.push_back(Pair("entries_accepted", obfuscationPool.GetCountEntriesAccepted()));
    return obj;
}


Value masternode(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-many" && strCommand != "start-all" && strCommand != "start-missing" &&
            strCommand != "start-disabled" && strCommand != "list" && strCommand != "list-conf" && strCommand != "count" && strCommand != "enforce" &&
            strCommand != "debug" && strCommand != "current" && strCommand != "winners" && strCommand != "genkey" && strCommand != "connect" &&
            strCommand != "outputs" && strCommand != "status" && strCommand != "calcscore"))
        throw runtime_error(
            "masternode \"command\"... ( \"passphrase\" )\n"
            "Set of commands to execute masternode related actions\n"
            "\nArguments:\n"
            "1. \"command\"        (string or set of strings, required) The command to execute\n"
            "2. \"passphrase\"     (string, optional) The wallet passphrase\n"
            "\nAvailable commands:\n"
            "  count        - Print count information of all known masternodes\n"
            "  current      - Print info on current masternode winner\n"
            "  debug        - Print masternode status\n"
            "  genkey       - Generate new masternodeprivkey\n"
            "  enforce      - Enforce masternode payments\n"
            "  outputs      - Print masternode compatible outputs\n"
            "  start        - Start masternode configured in lux.conf\n"
            "  start-alias  - Start single masternode by assigned alias configured in masternode.conf\n"
            "  start-<mode> - Start masternodes configured in masternode.conf (<mode>: 'all', 'missing', 'disabled')\n"
            "  status       - Print masternode status information\n"
            "  list         - Print list of all known masternodes (see masternodelist for more info)\n"
            "  list-conf    - Print masternode.conf in JSON format\n"
            "  winners      - Print list of masternode winners\n");

    if (strCommand == "list") {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return masternodelist(newParams, fHelp);
    }

    if (strCommand == "connect") {
        std::string strAddress = "";
        if (params.size() == 2) {
            strAddress = params[1].get_str();
        } else {
            throw runtime_error("Masternode address required\n");
        }

        CService addr = CService(strAddress);

        CNode* pnode = ConnectNode((CAddress)addr, NULL, false);
        if (pnode) {
            pnode->Release();
            return "successfully connected";
        } else {
            throw runtime_error("error connecting\n");
        }
    }

    if (strCommand == "count") {
        if (params.size() > 1) {
            throw runtime_error("too many parameters\n");
        }
        if (params.size() == 1) {
            Object obj;
            int nCount = 0;

            if (chainActive.Tip())
                mnodeman.GetNextMasternodeInQueueForPayment(chainActive.Tip()->nHeight, true, nCount);

            obj.push_back(Pair("total", mnodeman.size()));
            //obj.push_back(Pair("stable", mnodeman.stable_size()));
            obj.push_back(Pair("obfcompat", mnodeman.CountEnabled(ActiveProtocol())));
            obj.push_back(Pair("enabled", mnodeman.CountEnabled()));
            obj.push_back(Pair("inqueue", nCount));

            return obj;
        }
        return mnodeman.size();
    }

    if (strCommand == "current") {
        CMasternode* winner = mnodeman.GetCurrentMasterNode(1);
        if (winner) {
            Object obj;

            obj.push_back(Pair("protocol", (int64_t)winner->protocolVersion));
            obj.push_back(Pair("txhash", winner->vin.prevout.hash.ToString()));
            obj.push_back(Pair("pubkey", CBitcoinAddress(winner->pubKeyCollateralAddress.GetID()).ToString()));
            obj.push_back(Pair("lastseen", (winner->lastPing == CMasternodePing()) ? winner->sigTime : (int64_t)winner->lastPing.sigTime));
            obj.push_back(Pair("activeseconds", (winner->lastPing == CMasternodePing()) ? 0 : (int64_t)(winner->lastPing.sigTime - winner->sigTime)));
            return obj;
        }

        return "unknown";
    }

    if (strCommand == "debug") {
        if (activeMasternode.status != ACTIVE_MASTERNODE_INITIAL || !masternodeSync.IsSynced())
            return activeMasternode.GetStatus();

        CTxIn vin = CTxIn();
        CPubKey pubkey = CScript();
        CKey key;
        bool found = activeMasternode.GetMasterNodeVin(vin, pubkey, key);
        if (!found) {
            throw runtime_error("Missing masternode input, please look at the documentation for instructions on masternode creation\n");
        } else {
            return activeMasternode.GetStatus();
        }
    }

    if (strCommand == "enforce") {
        return (uint64_t)enforceMasternodePaymentsTime;
    }

    if (strCommand == "start") {
        if (!fMasterNode) throw runtime_error("you must set masternode=1 in the configuration\n");

        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2) {
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error("Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                throw runtime_error("incorrect passphrase\n");
            }
        }

        if (activeMasternode.status != ACTIVE_MASTERNODE_STARTED) {
            activeMasternode.status = ACTIVE_MASTERNODE_INITIAL; // TODO: consider better way
            activeMasternode.ManageStatus();
            pwalletMain->Lock();
        }

        return activeMasternode.GetStatus();
    }

    if (strCommand == "start-alias") {
        if (params.size() < 2) {
            throw runtime_error("command needs at least 2 parameters\n");
        }

        std::string alias = params[1].get_str();

        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 3) {
                strWalletPass = params[2].get_str().c_str();
            } else {
                throw runtime_error("Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                throw runtime_error("incorrect passphrase\n");
            }
        }

        bool found = false;

        Object statusObj;
        statusObj.push_back(Pair("alias", alias));

        BOOST_FOREACH (CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {
            if (mne.getAlias() == alias) {
                found = true;
                std::string errorMessage;

                bool result = activeMasternode.Register(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage);

                statusObj.push_back(Pair("result", result ? "successful" : "failed"));
                if (!result) {
                    statusObj.push_back(Pair("errorMessage", errorMessage));
                }
                break;
            }
        }

        if (!found) {
            statusObj.push_back(Pair("result", "failed"));
            statusObj.push_back(Pair("errorMessage", "could not find alias in config. Verify with list-conf."));
        }

        pwalletMain->Lock();
        return statusObj;
    }

    if (strCommand == "start-many" || strCommand == "start-all" || strCommand == "start-missing" || strCommand == "start-disabled") {
        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2) {
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error("Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                throw runtime_error("incorrect passphrase\n");
            }
        }

        if ((strCommand == "start-missing" || strCommand == "start-disabled") &&
            (masternodeSync.RequestedMasternodeAssets <= MASTERNODE_SYNC_LIST ||
                masternodeSync.RequestedMasternodeAssets == MASTERNODE_SYNC_FAILED)) {
            throw runtime_error("You can't use this command until masternode list is synced\n");
        }

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        int successful = 0;
        int failed = 0;

        Object resultsObj;

        BOOST_FOREACH (CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {
            std::string errorMessage;
            int nIndex;
            if(!mne.castOutputIndex(nIndex))
                continue;
            CTxIn vin = CTxIn(uint256(mne.getTxHash()), uint32_t(nIndex));
            CMasternode* pmn = mnodeman.Find(vin);

            if (strCommand == "start-missing" && pmn) continue;
            if (strCommand == "start-disabled" && pmn && pmn->IsEnabled()) continue;

            bool result = activeMasternode.Register(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage);

            Object statusObj;
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", result ? "successful" : "failed"));

            if (result) {
                successful++;
            } else {
                failed++;
                statusObj.push_back(Pair("errorMessage", errorMessage));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }
        pwalletMain->Lock();

        Object returnObj;
        returnObj.push_back(Pair("overall", strprintf("Successfully started %d masternodes, failed to start %d, total %d", successful, failed, successful + failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "create") {
        throw runtime_error("Not implemented yet, please look at the documentation for instructions on masternode creation\n");
    }

    if (strCommand == "genkey") {
        CKey secret;
        secret.MakeNewKey(false);

        return CBitcoinSecret(secret).ToString();
    }

    if (strCommand == "list-conf") {
        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        Array ret;

        BOOST_FOREACH (CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {
            int nIndex;
            if(!mne.castOutputIndex(nIndex))
                continue;
            CTxIn vin = CTxIn(uint256(mne.getTxHash()), uint32_t(nIndex));
            CMasternode* pmn = mnodeman.Find(vin);

            std::string strStatus = pmn ? pmn->Status() : "MISSING";

            Object mnObj;
            mnObj.push_back(Pair("alias", mne.getAlias()));
            mnObj.push_back(Pair("address", mne.getIp()));
            mnObj.push_back(Pair("privateKey", mne.getPrivKey()));
            mnObj.push_back(Pair("txHash", mne.getTxHash()));
            mnObj.push_back(Pair("outputIndex", mne.getOutputIndex()));
            mnObj.push_back(Pair("status", strStatus));
            ret.push_back(mnObj);
        }

        return ret;
    }

    if (strCommand == "outputs") {
        // Find possible candidates
        vector<COutput> possibleCoins = activeMasternode.SelectCoinsMasternode();

        Array ret;
        BOOST_FOREACH (COutput& out, possibleCoins) {
            Object obj;
            obj.push_back(Pair("txhash", out.tx->GetHash().ToString()));
            obj.push_back(Pair("outputidx", out.i));
            ret.push_back(obj);
        }

        return ret;
    }

    if (strCommand == "status") {
        if (!fMasterNode) throw runtime_error("This is not a masternode\n");

        CMasternode* pmn = mnodeman.Find(activeMasternode.vin);

        if (pmn) {
            Object mnObj;
            mnObj.push_back(Pair("txhash", activeMasternode.vin.prevout.hash.ToString()));
            mnObj.push_back(Pair("outputidx", (uint64_t)activeMasternode.vin.prevout.n));
            mnObj.push_back(Pair("netaddr", activeMasternode.service.ToString()));
            mnObj.push_back(Pair("addr", CBitcoinAddress(pmn->pubKeyCollateralAddress.GetID()).ToString()));
            mnObj.push_back(Pair("status", activeMasternode.status));
            mnObj.push_back(Pair("message", activeMasternode.GetStatus()));
            return mnObj;
        }
        throw runtime_error("Masternode not found\n");
    }

    if (strCommand == "winners") {
        int nHeight;
        {
            LOCK(cs_main);
            CBlockIndex* pindex = chainActive.Tip();
            if(!pindex) return 0;
            nHeight = pindex->nHeight;
        }

        int nLast = 10;
        std::string strFilter = "";

        if (params.size() >= 2) {
            nLast = atoi(params[1].get_str());
        }

        if (params.size() == 3) {
            strFilter = params[2].get_str();
        }

        Array ret;

        for (int i = nHeight - nLast; i < nHeight + 20; i++) {
            Object obj;
            obj.push_back(Pair("nHeight", i));

            std::string strPayment = GetRequiredPaymentsString(i);
            if (strFilter !="" && strPayment.find(strFilter) == std::string::npos) continue;

            if (strPayment.find(',') != std::string::npos) {
                Array winner;
                boost::char_separator<char> sep(",");
                boost::tokenizer< boost::char_separator<char> > tokens(strPayment, sep);
                BOOST_FOREACH (const string& t, tokens) {
                    Object addr;
                    std::size_t pos = t.find(":");
                    std::string strAddress = t.substr(0,pos);
                    uint64_t nVotes = atoi(t.substr(pos+1));
                    addr.push_back(Pair("address", strAddress));
                    addr.push_back(Pair("nVotes", nVotes));
                    winner.push_back(addr);
                }
                obj.push_back(Pair("winner", winner));
            } else if (strPayment.find("Unknown") == std::string::npos) {
                Object winner;
                std::size_t pos = strPayment.find(":");
                std::string strAddress = strPayment.substr(0,pos);
                uint64_t nVotes = atoi(strPayment.substr(pos+1));
                winner.push_back(Pair("address", strAddress));
                winner.push_back(Pair("nVotes", nVotes));
                obj.push_back(Pair("winner", winner));
            } else {
                Object winner;
                winner.push_back(Pair("address", strPayment));
                winner.push_back(Pair("nVotes", 0));
                obj.push_back(Pair("winner", winner));
            }

            ret.push_back(obj);
            //obj.push_back(Pair(strprintf("%d", i), strPayment));
        }

        return ret;
    }

    /*
        Shows which masternode wins by score each block
    */
    if (strCommand == "calcscore") {
        int nLast = 10;

        if (params.size() >= 2) {
            try {
                nLast = std::stoi(params[1].get_str());
            } catch (const boost::bad_lexical_cast &) {
                throw runtime_error("Exception on param 2");
            }
        }
        Object obj;

        std::vector<CMasternode> vMasternodes = mnodeman.GetFullMasternodeVector();
        for (int nHeight = chainActive.Tip()->nHeight - nLast; nHeight < chainActive.Tip()->nHeight + 20; nHeight++) {
            uint256 nHigh = 0;
            CMasternode* pBestMasternode = NULL;
            BOOST_FOREACH (CMasternode& mn, vMasternodes) {
                uint256 n = mn.CalculateScore(1, nHeight - 100);
                if (n > nHigh) {
                    nHigh = n;
                    pBestMasternode = &mn;
                }
            }
            if (pBestMasternode)
                obj.push_back(Pair(strprintf("%d", nHeight), pBestMasternode->vin.prevout.ToStringShort().c_str()));
        }

        return obj;
    }

    return Value::null;
}

Value masternodelist(const Array& params, bool fHelp)
{
    std::string strFilter = "";

    if (params.size() == 1) strFilter = params[0].get_str();

    if (fHelp || (params.size() > 1)) {
        throw runtime_error(
            "masternodelist ( \"filter\" )\n"
            "Get a ranked list of masternodes\n"

            "\nArguments:\n"
            "1. \"filter\"    (string, optional) Filter search text. Partial match by txhash, status, or addr.\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"rank\": n,           (numeric) Masternode Rank (or 0 if not enabled)\n"
            "    \"txhash\": \"hash\",    (string) Collateral transaction hash\n"
            "    \"outidx\": n,         (numeric) Collateral transaction output index\n"
            "    \"status\": s,         (string) Status (ENABLED/EXPIRED/REMOVE/etc)\n"
            "    \"addr\": \"addr\",      (string) Masternode LUX address\n"
            "    \"version\": v,        (numeric) Masternode protocol version\n"
            "    \"lastseen\": ttt,     (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last seen\n"
            "    \"activetime\": ttt,   (numeric) The time in seconds since epoch (Jan 1 1970 GMT) masternode has been active\n"
            "    \"lastpaid\": ttt,     (numeric) The time in seconds since epoch (Jan 1 1970 GMT) masternode was last paid\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n" +
            HelpExampleCli("masternodelist", "") + HelpExampleRpc("masternodelist", ""));
    }

    Array ret;
    int nHeight;
    {
        LOCK(cs_main);
        CBlockIndex* pindex = chainActive.Tip();
        if(!pindex) return 0;
        nHeight = pindex->nHeight;
    }
    std::vector<pair<int, CMasternode> > vMasternodeRanks = mnodeman.GetMasternodeRanks(nHeight);
    BOOST_FOREACH (PAIRTYPE(int, CMasternode) & s, vMasternodeRanks) {
        Object obj;
        std::string strVin = s.second.vin.prevout.ToStringShort();
        std::string strTxHash = s.second.vin.prevout.hash.ToString();
        uint32_t oIdx = s.second.vin.prevout.n;

        CMasternode* mn = mnodeman.Find(s.second.vin);

        if (strFilter != "" && strTxHash.find(strFilter) == string::npos &&
            mn->Status().find(strFilter) == string::npos &&
            CBitcoinAddress(mn->pubKeyCollateralAddress.GetID()).ToString().find(strFilter) == string::npos) continue;

        std::string strStatus = mn->Status();

        obj.push_back(Pair("rank", (strStatus == "ENABLED" ? s.first : 0)));
        obj.push_back(Pair("txhash", strTxHash));
        obj.push_back(Pair("outidx", (uint64_t)oIdx));
        obj.push_back(Pair("status", strStatus));
        obj.push_back(Pair("addr", CBitcoinAddress(mn->pubKeyCollateralAddress.GetID()).ToString()));
        obj.push_back(Pair("version", mn->protocolVersion));
        obj.push_back(Pair("lastseen", (int64_t)mn->lastPing.sigTime));
        obj.push_back(Pair("activetime", (int64_t)(mn->lastPing.sigTime - mn->sigTime)));
        obj.push_back(Pair("lastpaid", (int64_t)mn->GetLastPaid()));

        ret.push_back(obj);
    }

    return ret;
}
