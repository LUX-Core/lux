// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Darkcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <main.h>
#include <primitives/transaction.h>
#include <db.h>
#include <init.h>
#include <masternode.h>
#include <activemasternode.h>
#include <masternodeconfig.h>
#include <rpc/server.h>
#include <boost/lexical_cast.hpp>
//#include "amount.h"
#include <util.h>
#include <utilmoneystr.h>

#include <boost/tokenizer.hpp>

#include <univalue.h>

#include <fstream>

void SendMoney(CWallet * const pwallet, const CTxDestination& address, CAmount nValue, CWalletTx& wtxNew, AvailableCoinsType coin_type) {
    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");

    if (nValue > pwallet->GetBalance())
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    string strError;
    if (pwallet->IsLocked()) {
        strError = "Error: Wallet locked, unable to create transaction!";
        LogPrintf("SendMoney() : %s", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    // Parse Lux address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwallet);
    CAmount nFeeRequired;
    if (!pwallet->CreateTransaction(scriptPubKey, nValue, wtxNew, reservekey, nFeeRequired, strError, NULL, coin_type)) {
        if (nValue + nFeeRequired > pwallet->GetBalance())
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        LogPrintf("SendMoney() : %s\n", strError);
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwallet->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR,
                           "Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");
}

UniValue darksend(const JSONRPCRequest& request) {
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (request.fHelp || request.params.size() == 0)
        throw std::runtime_error(
            "darksend <Luxaddress> <amount>\n"
            "Luxaddress, reset, or auto (AutoDenominate)"
            "<amount> is a real and is rounded to the nearest 0.00000001"
            + HelpRequiringPassphrase(pwallet));

    if (pwallet->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    if (request.params[0].get_str() == "auto") {
        if (fMasterNode)
            return "DarkSend is not supported from masternodes";

        darkSendPool.DoAutomaticDenominating();
        return "DoAutomaticDenominating";
    }

    if (request.params[0].get_str() == "reset") {
        darkSendPool.SetNull(true);
        darkSendPool.UnlockCoins();
        return "successfully reset darksend";
    }

    if (request.params.size() != 2)
        throw std::runtime_error(
            "darksend <Luxaddress> <amount>\n"
            "Luxaddress, denominate, or auto (AutoDenominate)"
            "<amount> is a real and is rounded to the nearest 0.00000001"
            + HelpRequiringPassphrase(pwallet));

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (!IsValidDestination(dest))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Lux address");

    // Amount
    int64_t nAmount = AmountFromValue(request.params[1]);

    // Wallet comments
    CWalletTx wtx;
    SendMoney(pwallet, dest, nAmount, wtx, ONLY_DENOMINATED);

    return wtx.GetHash().GetHex();
}


UniValue getpoolinfo(const JSONRPCRequest& request) {
    if (request.fHelp || request.params.size() != 0)
        throw std::runtime_error(
            "getpoolinfo\n"
            "Returns an object containing anonymous pool-related information.");

    UniValue obj(UniValue::VOBJ);
    obj.push_back(Pair("current_masternode", GetCurrentMasterNode()));
    obj.push_back(Pair("state", darkSendPool.GetState()));
    obj.push_back(Pair("entries", darkSendPool.GetEntriesCount()));
    obj.push_back(Pair("entries_accepted", darkSendPool.GetCountEntriesAccepted()));
    return obj;
}


UniValue masternode(const JSONRPCRequest& request) {
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    string strCommand;
    if (request.params.size() >= 1)
        strCommand = request.params[0].get_str();

    if (request.fHelp ||
        (strCommand != "start" && strCommand != "start-alias" && strCommand != "start-many" && strCommand != "stop" && strCommand != "stop-alias" && strCommand != "stop-many" &&
         strCommand != "list" && strCommand != "list-conf" && strCommand != "count" && strCommand != "enforce"
         && strCommand != "debug" && strCommand != "current" && strCommand != "winners" && strCommand != "genkey" && strCommand != "connect" && strCommand != "outputs"))
        throw std::runtime_error(
            "masternode <start|start-alias|start-many|stop|stop-alias|stop-many|list|list-conf|count|debug|current|winners|genkey|enforce|outputs> [passphrase]\n");

    if (strCommand == "stop") {
        if (!fMasterNode) return "you must set masternode=1 in the configuration";

        if (pwallet->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (request.params.size() == 2) {
                strWalletPass = request.params[1].get_str().c_str();
            } else {
                throw std::runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwallet->Unlock(strWalletPass)) {
                return "incorrect passphrase";
            }
        }

        std::string errorMessage;
        if (!activeMasternode.StopMasterNode(errorMessage)) {
            return "stop failed: " + errorMessage;
        }
        pwallet->Lock();

        if (activeMasternode.status == MASTERNODE_STOPPED) return "successfully stopped masternode";
        if (activeMasternode.status == MASTERNODE_NOT_CAPABLE) return "not capable masternode";

        return "unknown";
    }

    if (strCommand == "stop-alias") {
        if (request.params.size() < 2) {
            throw std::runtime_error(
                "command needs at least 2 parameters\n");
        }

        std::string alias = request.params[1].get_str().c_str();

        if (pwallet->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (request.params.size() == 3) {
                strWalletPass = request.params[2].get_str().c_str();
            } else {
                throw std::runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwallet->Unlock(strWalletPass)) {
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

        pwallet->Lock();
        return resultsObj;
    }

    if (strCommand == "stop-many") {
        if (pwallet->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (request.params.size() == 2) {
                strWalletPass = request.params[1].get_str().c_str();
            } else {
                throw std::runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwallet->Unlock(strWalletPass)) {
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
        pwallet->Lock();

        UniValue returnObj(UniValue::VOBJ);
        returnObj.push_back(Pair("overall", "Successfully stopped " + boost::lexical_cast<std::string>(successful) + " masternodes, failed to stop " +
                                            boost::lexical_cast<std::string>(fail) + ", total " + boost::lexical_cast<std::string>(total)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;

    }

    if (strCommand == "list") {
        std::string strCommand = "active";

        if (request.params.size() == 2) {
            strCommand = request.params[1].get_str().c_str();
        }

        if (strCommand != "active" && strCommand != "vin" && strCommand != "pubkey" && strCommand != "lastseen" && strCommand != "activeseconds" && strCommand != "rank" && strCommand != "protocol") {
            throw std::runtime_error(
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

        if (pwallet->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (request.params.size() == 2) {
                strWalletPass = request.params[1].get_str().c_str();
            } else {
                throw std::runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwallet->Unlock(strWalletPass)) {
                return "incorrect passphrase";
            }
        }

        if (activeMasternode.status != MASTERNODE_REMOTELY_ENABLED && activeMasternode.status != MASTERNODE_IS_CAPABLE) {
            activeMasternode.status = MASTERNODE_NOT_PROCESSED; // TODO: consider better way
            std::string errorMessage;
            activeMasternode.ManageStatus();
            pwallet->Lock();
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
        if (request.params.size() < 2) {
            throw std::runtime_error(
                "command needs at least 2 parameters\n");
        }

        std::string alias = request.params[1].get_str().c_str();

        if (pwallet->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (request.params.size() == 3) {
                strWalletPass = request.params[2].get_str().c_str();
            } else {
                throw std::runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwallet->Unlock(strWalletPass)) {
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

        pwallet->Lock();
        return statusObj;

    }

    if (strCommand == "start-many") {
        if (pwallet->IsLocked()) {
            SecureString strWalletPass;
            strWalletPass.reserve(100);

            if (request.params.size() == 2) {
                strWalletPass = request.params[1].get_str().c_str();
            } else {
                throw std::runtime_error(
                    "Your wallet is locked, passphrase is required\n");
            }

            if (!pwallet->Unlock(strWalletPass)) {
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
        pwallet->Lock();

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
        if (request.params.size() == 2) {
            strAddress = request.params[1].get_str().c_str();
        } else {
            throw std::runtime_error(
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
        BOOST_FOREACH(COutput& out, possibleCoins) {
            obj.push_back(Pair(out.tx->GetHash().ToString().c_str(), boost::lexical_cast<std::string>(out.i)));
        }

        return obj;

    }

    return NullUniValue;
}

static const CRPCCommand commands[] =
        { //  category              name                      actor (function)         argNames
                //  --------------------- ------------------------  -----------------------  ----------
            { "darksend",         "getpoolinfo",        &getpoolinfo,       {} },
            { "darksend",         "darksend",           &darksend,          {"Luxaddress", "amount"} },
            { "darksend",         "masternode",         &masternode,        {} },
        };

void RegisterDarksendRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}