// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "main.h"
#include "primitives/transaction.h"
#include "db.h"
#include "darksend.h"
#include "init.h"
#include "masternode.h"
#include "activemasternode.h"
#include "masternodeconfig.h"
#include "rpcserver.h"
#include <boost/lexical_cast.hpp>
//#include "amount.h"
#include "util.h"
#include "utilmoneystr.h"

#include <boost/tokenizer.hpp>

#include "univalue/univalue.h"

#include <fstream>

UniValue darksend(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() != 1)
        throw runtime_error(
                "darksend \"command\"\n"
                "\nArguments:\n"
                "1. \"command\"        (string or set of strings, required) The command to execute\n"
                "\nAvailable commands:\n"
                "  start       - Start mixing\n"
                "  stop        - Stop mixing\n"
                "  reset       - Reset mixing\n"
                "  status      - Print mixing status\n"
            + HelpRequiringPassphrase());

    if(params[0].get_str() == "start"){
        if (pwalletMain->IsLocked())
            throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

        if(fMasterNode)
            return "Mixing is not supported from masternodes";

        darkSendPool.DoAutomaticDenominating();
        return "DoAutomaticDenominating";
    }

    if(params[0].get_str() == "stop"){
        fEnableDarksend = false;
        return "Mixing was stopped";
    }

    if(params[0].get_str() == "reset"){
        darkSendPool.Reset();
        return "Mixing was reset";
    }

    if(params[0].get_str() == "status"){
        UniValue obj(UniValue::VOBJ);
        obj.push_back(Pair("keys_left", pwalletMain->nKeysLeftSinceAutoBackup));
        obj.push_back(Pair("warnings", (pwalletMain->nKeysLeftSinceAutoBackup < KEYS_THRESHOLD_WARNING ? "WARNING: keypool is almost depleted!" : "")));
        return obj;
    }

    return "Unknown command, please see \"help darksend\"";
}


UniValue getpoolinfo(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getpoolinfo\n"
            "Returns an object containing anonymous pool-related information.");

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("current_masternode", GetCurrentMasterNode()));
    obj.push_back(Pair("state", darkSendPool.GetState()));
    obj.push_back(Pair("entries", darkSendPool.GetEntriesCount()));
    obj.push_back(Pair("entries_accepted", darkSendPool.GetCountEntriesAccepted()));
    return obj;
}


UniValue masternode(const UniValue& params, bool fHelp) {
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-many" && strCommand != "stop" && strCommand != "stop-alias" && strCommand != "stop-many" &&
         strCommand != "list" && strCommand != "list-conf" && strCommand != "count" && strCommand != "enforce"
         && strCommand != "debug" && strCommand != "current" && strCommand != "winners" && strCommand != "genkey" && strCommand != "connect" && strCommand != "outputs"))
        throw runtime_error(
            "masternode <start|start-alias|start-many|stop|stop-alias|stop-many|list|list-conf|count|debug|current|winners|genkey|enforce|outputs> [passphrase]\n");

    if (strCommand == "stop") {
        if (!fMasterNode) return "you must set masternode=1 in the configuration";

        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2) {
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                return "incorrect passphrase";
            }
        }

        std::string errorMessage;
        if (!activeMasternode.StopMasterNode(errorMessage)) {
            return "stop failed: " + errorMessage;
        }
        pwalletMain->Lock();

        if (activeMasternode.status == MASTERNODE_STOPPED) return "successfully stopped masternode";
        if (activeMasternode.status == MASTERNODE_NOT_CAPABLE) return "not capable masternode";

        return "unknown";
    }

    if (strCommand == "stop-alias") {
        if (params.size() < 2) {
            throw runtime_error(
                "command needs at least 2 parameters\n");
        }

        std::string alias = params[1].get_str().c_str();

        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 3) {
                strWalletPass = params[2].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                return "incorrect passphrase";
            }
        }

        bool found = false;

        UniValue resultsObj(UniValue::VOBJ);
        resultsObj.push_back(Pair("alias", alias));

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            if (mne.getAlias() == alias) {
                found = true;
                std::string errorMessage;
                bool result = activeMasternode.StopMasterNode(mne.getIp(), mne.getPrivKey(), errorMessage);

                resultsObj.push_back(Pair("result", result ? "successful" : "failed"));
                if (!result) {
                    resultsObj.push_back(Pair("errorMessage", errorMessage));
                }
                break;
            }
        }

        if (!found) {
            resultsObj.push_back(Pair("result", "failed"));
            resultsObj.push_back(Pair("errorMessage", "could not find alias in config. Verify with list-conf."));
        }

        pwalletMain->Lock();
        return resultsObj;
    }

    if (strCommand == "stop-many") {
        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2) {
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                return "incorrect passphrase";
            }
        }

        int total = 0;
        int successful = 0;
        int fail = 0;

        UniValue resultsObj(UniValue::VOBJ);

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            total++;

            std::string errorMessage;
            bool result = activeMasternode.StopMasterNode(mne.getIp(), mne.getPrivKey(), errorMessage);

            UniValue statusObj(UniValue::VOBJ);
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", result ? "successful" : "failed"));

            if (result) {
                successful++;
            } else {
                fail++;
                statusObj.push_back(Pair("errorMessage", errorMessage));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }
        pwalletMain->Lock();

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", "Successfully stopped " + boost::lexical_cast<std::string>(successful) + " masternodes, failed to stop " +
                                            boost::lexical_cast<std::string>(fail) + ", total " + boost::lexical_cast<std::string>(total)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;

    }

    if (strCommand == "list") {
        std::string strCommand = "active";

        if (params.size() == 2) {
            strCommand = params[1].get_str().c_str();
        }

        if (strCommand != "active" && strCommand != "vin" && strCommand != "pubkey" && strCommand != "lastseen" && strCommand != "activeseconds" && strCommand != "rank" && strCommand != "protocol") {
            throw runtime_error(
                "list supports 'active', 'vin', 'pubkey', 'lastseen', 'activeseconds', 'rank', 'protocol'\n");
        }

        UniValue obj(UniValue::VOBJ);
        for (CMasterNode mn : vecMasternodes) {
            mn.Check();

            if (strCommand == "active") {
                obj.push_back(Pair(mn.addr.ToString().c_str(), (int) mn.IsEnabled()));
            } else if (strCommand == "vin") {
                obj.push_back(Pair(mn.addr.ToString().c_str(), mn.vin.prevout.hash.ToString().c_str()));
            } else if (strCommand == "pubkey") {
                CScript pubkey;
                pubkey = GetScriptForDestination(mn.pubkey.GetID());
                CTxDestination address1;
                ExtractDestination(pubkey, address1);
                CTxDestination address2(address1);

                obj.push_back(Pair(mn.addr.ToString().c_str(), EncodeDestination(address2)));
            } else if (strCommand == "protocol") {
                obj.push_back(Pair(mn.addr.ToString().c_str(), (int64_t) mn.protocolVersion));
            } else if (strCommand == "lastseen") {
                obj.push_back(Pair(mn.addr.ToString().c_str(), (int64_t) mn.lastTimeSeen));
            } else if (strCommand == "activeseconds") {
                obj.push_back(Pair(mn.addr.ToString().c_str(), (int64_t)(mn.lastTimeSeen - mn.now)));
            } else if (strCommand == "rank") {
                obj.push_back(Pair(mn.addr.ToString().c_str(), (int) (GetMasternodeRank(mn.vin, chainActive.Height()))));
            }
        }
        return obj;
    }
    if (strCommand == "count") return (int) vecMasternodes.size();

    if (strCommand == "start") {
        if (!fMasterNode) return "you must set masternode=1 in the configuration";

        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2) {
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                return "incorrect passphrase";
            }
        }

        if (activeMasternode.status != MASTERNODE_REMOTELY_ENABLED && activeMasternode.status != MASTERNODE_IS_CAPABLE) {
            activeMasternode.status = MASTERNODE_NOT_PROCESSED; // TODO: consider better way
            std::string errorMessage;
            activeMasternode.ManageStatus();
            pwalletMain->Lock();
        }

        if (activeMasternode.status == MASTERNODE_REMOTELY_ENABLED) return "masternode started remotely";
        if (activeMasternode.status == MASTERNODE_INPUT_TOO_NEW) return "masternode input must have at least 15 confirmations";
        if (activeMasternode.status == MASTERNODE_STOPPED) return "masternode is stopped";
        if (activeMasternode.status == MASTERNODE_IS_CAPABLE) return "successfully started masternode";
        if (activeMasternode.status == MASTERNODE_NOT_CAPABLE) return "not capable masternode: " + activeMasternode.notCapableReason;
        if (activeMasternode.status == MASTERNODE_SYNC_IN_PROCESS) return "sync in process. Must wait until client is synced to start.";

        return "unknown";
    }

    if (strCommand == "start-alias") {
        if (params.size() < 2) {
            throw runtime_error(
                "command needs at least 2 parameters\n");
        }

        std::string alias = params[1].get_str().c_str();

        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 3) {
                strWalletPass = params[2].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                return "incorrect passphrase";
            }
        }

        bool found = false;

        UniValue statusObj(UniValue::VOBJ);

        statusObj.push_back(Pair("alias", alias));

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
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

    if (strCommand == "start-many") {
        if (pwalletMain->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (params.size() == 2) {
                strWalletPass = params[1].get_str().c_str();
            } else {
                throw runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwalletMain->Unlock(strWalletPass)) {
                return "incorrect passphrase";
            }
        }

        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        int total = 0;
        int successful = 0;
        int fail = 0;

        UniValue resultsObj(UniValue::VOBJ);

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            total++;

            std::string errorMessage;
            bool result = activeMasternode.Register(mne.getIp(), mne.getPrivKey(), mne.getTxHash(), mne.getOutputIndex(), errorMessage);

            UniValue statusObj(UniValue::VOBJ);
            statusObj.push_back(Pair("alias", mne.getAlias()));
            statusObj.push_back(Pair("result", result ? "succesful" : "failed"));

            if (result) {
                successful++;
            } else {
                fail++;
                statusObj.push_back(Pair("errorMessage", errorMessage));
            }

            resultsObj.push_back(Pair("status", statusObj));
        }
        pwalletMain->Lock();

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", "Successfully started " + boost::lexical_cast<std::string>(successful) + " masternodes, failed to start " +
                                            boost::lexical_cast<std::string>(fail) + ", total " + boost::lexical_cast<std::string>(total)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "debug") {
        if (activeMasternode.status == MASTERNODE_REMOTELY_ENABLED) return "masternode started remotely";
        if (activeMasternode.status == MASTERNODE_INPUT_TOO_NEW) return "masternode input must have at least 15 confirmations";
        if (activeMasternode.status == MASTERNODE_IS_CAPABLE) return "successfully started masternode";
        if (activeMasternode.status == MASTERNODE_STOPPED) return "masternode is stopped";
        if (activeMasternode.status == MASTERNODE_NOT_CAPABLE) return "not capable masternode: " + activeMasternode.notCapableReason;
        if (activeMasternode.status == MASTERNODE_SYNC_IN_PROCESS) return "sync in process. Must wait until client is synced to start.";

        CTxIn vin = CTxIn();
        CPubKey pubkey;
        CKey key;
        bool found = activeMasternode.GetMasterNodeVin(vin, pubkey, key);
        if (!found) {
            return "Missing masternode input, please look at the documentation for instructions on masternode creation";
        } else {
            return "No problems were found";
        }
    }

    if (strCommand == "outputs"){
        // Find possible candidates
        vector<COutput> possibleCoins = activeMasternode.SelectCoinsMasternode();

        UniValue obj(UniValue::VOBJ);
        for (const auto& out : possibleCoins) {
            obj.push_back(Pair(out.tx->GetHash().ToString().c_str(), boost::lexical_cast<std::string>(out.i)));
        }
        return obj;
    }

    if (strCommand == "create") {

        return "Not implemented yet, please look at the documentation for instructions on masternode creation";
    }

    if (strCommand == "current") {
        int winner = GetCurrentMasterNode(1);
        if (winner >= 0) {
            return vecMasternodes[winner].addr.ToString().c_str();
        }

        return "unknown";
    }

    if (strCommand == "genkey") {
        CKey secret;
        secret.MakeNewKey(false);

        return CBitcoinSecret(secret).ToString();
    }

    if (strCommand == "winners") {
        UniValue obj(UniValue::VOBJ);

        for (int nHeight = chainActive.Height() - 10; nHeight < chainActive.Height() + 20; nHeight++) {
            CScript payee;
            if (masternodePayments.GetBlockPayee(nHeight, payee)) {
                CTxDestination address1;
                ExtractDestination(payee, address1);
                CTxDestination address2(address1);
                obj.push_back(Pair(boost::lexical_cast<std::string>(nHeight), EncodeDestination(address2)));
            } else {
                obj.push_back(Pair(boost::lexical_cast<std::string>(nHeight), ""));
            }
        }

        return obj;
    }

    if (strCommand == "enforce") {
        return (uint64_t) enforceMasternodePaymentsTime;
    }

    if (strCommand == "connect") {
        std::string strAddress = "";
        if (params.size() == 2) {
            strAddress = params[1].get_str().c_str();
        } else {
            throw runtime_error(
                "Masternode address required\n");
        }

        CService addr = CService(strAddress);

        if (ConnectNode(CAddress(addr, NODE_NETWORK), NULL, true)) {
            return "successfully connected";
        } else {
            return "error connecting";
        }
    }

    if (strCommand == "list-conf") {
        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        UniValue resultObj(UniValue::VOBJ);

        for (CMasternodeConfig::CMasternodeEntry mne : masternodeConfig.getEntries()) {
            UniValue mnObj(UniValue::VOBJ);
            mnObj.push_back(Pair("alias", mne.getAlias()));
            mnObj.push_back(Pair("address", mne.getIp()));
            mnObj.push_back(Pair("privateKey", mne.getPrivKey()));
            mnObj.push_back(Pair("txHash", mne.getTxHash()));
            mnObj.push_back(Pair("outputIndex", mne.getOutputIndex()));
            resultObj.push_back(Pair("masternode", mnObj));
        }

        return resultObj;
    }

    if (strCommand == "outputs") {
        // Find possible candidates
        vector<COutput> possibleCoins = activeMasternode.SelectCoinsMasternode();

        UniValue obj(UniValue::VOBJ);
        for (COutput& out : possibleCoins) {
            obj.push_back(Pair(out.tx->GetHash().ToString().c_str(), boost::lexical_cast<std::string>(out.i)));
        }

        return obj;

    }

    return NullUniValue;
}

