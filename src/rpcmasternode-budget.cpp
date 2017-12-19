// Copyright (c) 2014-2015 The Dash Developers
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

#include <fstream>
using namespace json_spirit;
using namespace std;

void budgetToJSON(CBudgetProposal* pbudgetProposal, Object& bObj)
{
    CTxDestination address1;
    ExtractDestination(pbudgetProposal->GetPayee(), address1);
    CBitcoinAddress address2(address1);

    bObj.push_back(Pair("Name", pbudgetProposal->GetName()));
    bObj.push_back(Pair("URL", pbudgetProposal->GetURL()));
    bObj.push_back(Pair("Hash", pbudgetProposal->GetHash().ToString()));
    bObj.push_back(Pair("FeeHash", pbudgetProposal->nFeeTXHash.ToString()));
    bObj.push_back(Pair("BlockStart", (int64_t)pbudgetProposal->GetBlockStart()));
    bObj.push_back(Pair("BlockEnd", (int64_t)pbudgetProposal->GetBlockEnd()));
    bObj.push_back(Pair("TotalPaymentCount", (int64_t)pbudgetProposal->GetTotalPaymentCount()));
    bObj.push_back(Pair("RemainingPaymentCount", (int64_t)pbudgetProposal->GetRemainingPaymentCount()));
    bObj.push_back(Pair("PaymentAddress", address2.ToString()));
    bObj.push_back(Pair("Ratio", pbudgetProposal->GetRatio()));
    bObj.push_back(Pair("Yeas", (int64_t)pbudgetProposal->GetYeas()));
    bObj.push_back(Pair("Nays", (int64_t)pbudgetProposal->GetNays()));
    bObj.push_back(Pair("Abstains", (int64_t)pbudgetProposal->GetAbstains()));
    bObj.push_back(Pair("TotalPayment", ValueFromAmount(pbudgetProposal->GetAmount() * pbudgetProposal->GetTotalPaymentCount())));
    bObj.push_back(Pair("MonthlyPayment", ValueFromAmount(pbudgetProposal->GetAmount())));
    bObj.push_back(Pair("IsEstablished", pbudgetProposal->IsEstablished()));

    std::string strError = "";
    bObj.push_back(Pair("IsValid", pbudgetProposal->IsValid(strError)));
    bObj.push_back(Pair("IsValidReason", strError.c_str()));
    bObj.push_back(Pair("fValid", pbudgetProposal->fValid));
}

// This command is retained for backwards compatibility, but is depreciated.
// Future removal of this command is planned to keep things clean.
Value mnbudget(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp ||
        (strCommand != "vote-alias" && strCommand != "vote-many" && strCommand != "prepare" && strCommand != "submit" && strCommand != "vote" && strCommand != "getvotes" && strCommand != "getinfo" && strCommand != "show" && strCommand != "projection" && strCommand != "check" && strCommand != "nextblock"))
        throw runtime_error(
            "mnbudget \"command\"... ( \"passphrase\" )\n"
            "\nVote or show current budgets\n"
            "This command is depreciated, please see individual command documentation for future reference\n\n"

            "\nAvailable commands:\n"
            "  prepare            - Prepare proposal for network by signing and creating tx\n"
            "  submit             - Submit proposal for network\n"
            "  vote-many          - Vote on a Lux initiative\n"
            "  vote-alias         - Vote on a Lux initiative\n"
            "  vote               - Vote on a Lux initiative/budget\n"
            "  getvotes           - Show current masternode budgets\n"
            "  getinfo            - Show current masternode budgets\n"
            "  show               - Show all budgets\n"
            "  projection         - Show the projection of which proposals will be paid the next cycle\n"
            "  check              - Scan proposals and remove invalid\n"
            "  nextblock          - Get next superblock for budget system\n");

    if (strCommand == "nextblock") {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return getnextsuperblock(newParams, fHelp);
    }

    if (strCommand == "prepare") {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return preparebudget(newParams, fHelp);
    }

    if (strCommand == "submit") {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return submitbudget(newParams, fHelp);
    }

    if (strCommand == "vote" || strCommand == "vote-many" || strCommand == "vote-alias") {
        if (strCommand == "vote-alias")
            throw runtime_error(
                "vote-alias is not supported with this command\n"
                "Please use mnbudgetvote instead.\n"
            );
        return mnbudgetvote(params, fHelp);
    }

    if (strCommand == "projection") {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return getbudgetprojection(newParams, fHelp);
    }

    if (strCommand == "show" || strCommand == "getinfo") {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return getbudgetinfo(newParams, fHelp);
    }

    if (strCommand == "getvotes") {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return getbudgetvotes(newParams, fHelp);
    }

    if (strCommand == "check") {
        Array newParams(params.size() - 1);
        std::copy(params.begin() + 1, params.end(), newParams.begin());
        return checkbudgets(newParams, fHelp);
    }

    return Value::null;
}

Value preparebudget(const Array& params, bool fHelp)
{
    int nBlockMin = 0;
    CBlockIndex* pindexPrev = chainActive.Tip();

    if (fHelp || params.size() != 6)
        throw runtime_error(
            "preparebudget \"proposal-name\" \"url\" payment-count block-start \"lux-address\" monthy-payment\n"
            "\nPrepare proposal for network by signing and creating tx\n"

            "\nArguments:\n"
            "1. \"proposal-name\":  (string, required) Desired proposal name (20 character limit)\n"
            "2. \"url\":            (string, required) URL of proposal details (64 character limit)\n"
            "3. payment-count:    (numeric, required) Total number of monthly payments\n"
            "4. block-start:      (numeric, required) Starting super block height\n"
            "5. \"lux-address\":   (string, required) LUX address to send payments to\n"
            "6. monthly-payment:  (numeric, required) Monthly payment amount\n"

            "\nResult:\n"
            "\"xxxx\"       (string) proposal fee hash (if successful) or error message (if failed)\n"
            "\nExamples:\n" +
            HelpExampleCli("preparebudget", "\"test-proposal\" \"https://forum.lux.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500") +
            HelpExampleRpc("preparebudget", "\"test-proposal\" \"https://forum.lux.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500"));

    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");

    std::string strProposalName = SanitizeString(params[0].get_str());
    if (strProposalName.size() > 20)
        throw runtime_error("Invalid proposal name, limit of 20 characters.");

    std::string strURL = SanitizeString(params[1].get_str());
    if (strURL.size() > 64)
        throw runtime_error("Invalid url, limit of 64 characters.");

    int nPaymentCount = params[2].get_int();
    if (nPaymentCount < 1)
        throw runtime_error("Invalid payment count, must be more than zero.");

    // Start must be in the next budget cycle
    if (pindexPrev != NULL) nBlockMin = pindexPrev->nHeight - pindexPrev->nHeight % GetBudgetPaymentCycleBlocks() + GetBudgetPaymentCycleBlocks();

    int nBlockStart = params[3].get_int();
    if (nBlockStart % GetBudgetPaymentCycleBlocks() != 0) {
        int nNext = pindexPrev->nHeight - pindexPrev->nHeight % GetBudgetPaymentCycleBlocks() + GetBudgetPaymentCycleBlocks();
        throw runtime_error(strprintf("Invalid block start - must be a budget cycle block. Next valid block: %d", nNext));
    }

    int nBlockEnd = nBlockStart + GetBudgetPaymentCycleBlocks() * nPaymentCount; // End must be AFTER current cycle

    if (nBlockStart < nBlockMin)
        throw runtime_error("Invalid block start, must be more than current height.");

    if (nBlockEnd < pindexPrev->nHeight)
        throw runtime_error("Invalid ending block, starting block + (payment_cycle*payments) must be more than current height.");

    CBitcoinAddress address(params[4].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Lux address");

    // Parse Lux address
    CScript scriptPubKey = GetScriptForDestination(address.Get());
    CAmount nAmount = AmountFromValue(params[5]);

    //*************************************************************************

    // create transaction 15 minutes into the future, to allow for confirmation time
    CBudgetProposalBroadcast budgetProposalBroadcast(strProposalName, strURL, nPaymentCount, scriptPubKey, nAmount, nBlockStart, 0);

    std::string strError = "";
    if (!budgetProposalBroadcast.IsValid(strError, false))
        throw runtime_error("Proposal is not valid - " + budgetProposalBroadcast.GetHash().ToString() + " - " + strError);

    bool useIX = false; //true;
    // if (params.size() > 7) {
    //     if(params[7].get_str() != "false" && params[7].get_str() != "true")
    //         return "Invalid use_ix, must be true or false";
    //     useIX = params[7].get_str() == "true" ? true : false;
    // }

    CWalletTx wtx;
    if (!pwalletMain->GetBudgetSystemCollateralTX(wtx, budgetProposalBroadcast.GetHash(), useIX)) {
        throw runtime_error("Error making collateral transaction for proposal. Please check your wallet balance.");
    }

    // make our change address
    CReserveKey reservekey(pwalletMain);
    //send the tx to the network
    pwalletMain->CommitTransaction(wtx, reservekey, useIX ? "ix" : "tx");

    return wtx.GetHash().ToString();
}

Value submitbudget(const Array& params, bool fHelp)
{
    int nBlockMin = 0;
    CBlockIndex* pindexPrev = chainActive.Tip();

    if (fHelp || params.size() != 7)
        throw runtime_error(
            "submitbudget \"proposal-name\" \"url\" payment-count block-start \"lux-address\" monthy-payment \"fee-tx\"\n"
            "\nSubmit proposal to the network\n"

            "\nArguments:\n"
            "1. \"proposal-name\":  (string, required) Desired proposal name (20 character limit)\n"
            "2. \"url\":            (string, required) URL of proposal details (64 character limit)\n"
            "3. payment-count:    (numeric, required) Total number of monthly payments\n"
            "4. block-start:      (numeric, required) Starting super block height\n"
            "5. \"lux-address\":   (string, required) LUX address to send payments to\n"
            "6. monthly-payment:  (numeric, required) Monthly payment amount\n"
            "7. \"fee-tx\":         (string, required) Transaction hash from preparebudget command\n"

            "\nResult:\n"
            "\"xxxx\"       (string) proposal hash (if successful) or error message (if failed)\n"
            "\nExamples:\n" +
            HelpExampleCli("submitbudget", "\"test-proposal\" \"https://forum.lux.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500") +
            HelpExampleRpc("submitbudget", "\"test-proposal\" \"https://forum.lux.org/t/test-proposal\" 2 820800 \"D9oc6C3dttUbv8zd7zGNq1qKBGf4ZQ1XEE\" 500"));

    // Check these inputs the same way we check the vote commands:
    // **********************************************************

    std::string strProposalName = SanitizeString(params[0].get_str());
    if (strProposalName.size() > 20)
        throw runtime_error("Invalid proposal name, limit of 20 characters.");

    std::string strURL = SanitizeString(params[1].get_str());
    if (strURL.size() > 64)
        throw runtime_error("Invalid url, limit of 64 characters.");

    int nPaymentCount = params[2].get_int();
    if (nPaymentCount < 1)
        throw runtime_error("Invalid payment count, must be more than zero.");

    // Start must be in the next budget cycle
    if (pindexPrev != NULL) nBlockMin = pindexPrev->nHeight - pindexPrev->nHeight % GetBudgetPaymentCycleBlocks() + GetBudgetPaymentCycleBlocks();

    int nBlockStart = params[3].get_int();
    if (nBlockStart % GetBudgetPaymentCycleBlocks() != 0) {
        int nNext = pindexPrev->nHeight - pindexPrev->nHeight % GetBudgetPaymentCycleBlocks() + GetBudgetPaymentCycleBlocks();
        throw runtime_error(strprintf("Invalid block start - must be a budget cycle block. Next valid block: %d", nNext));
    }

    int nBlockEnd = nBlockStart + (GetBudgetPaymentCycleBlocks() * nPaymentCount); // End must be AFTER current cycle

    if (nBlockStart < nBlockMin)
        throw runtime_error("Invalid block start, must be more than current height.");

    if (nBlockEnd < pindexPrev->nHeight)
        throw runtime_error("Invalid ending block, starting block + (payment_cycle*payments) must be more than current height.");

    CBitcoinAddress address(params[4].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Lux address");

    // Parse Lux address
    CScript scriptPubKey = GetScriptForDestination(address.Get());
    CAmount nAmount = AmountFromValue(params[5]);
    uint256 hash = ParseHashV(params[6], "parameter 1");

    //create the proposal incase we're the first to make it
    CBudgetProposalBroadcast budgetProposalBroadcast(strProposalName, strURL, nPaymentCount, scriptPubKey, nAmount, nBlockStart, hash);

    std::string strError = "";
    int nConf = 0;
    if (!IsBudgetCollateralValid(hash, budgetProposalBroadcast.GetHash(), strError, budgetProposalBroadcast.nTime, nConf)) {
        throw runtime_error("Proposal FeeTX is not valid - " + hash.ToString() + " - " + strError);
    }

    if (!masternodeSync.IsBlockchainSynced()) {
        throw runtime_error("Must wait for client to sync with masternode network. Try again in a minute or so.");
    }

    // if(!budgetProposalBroadcast.IsValid(strError)){
    //     return "Proposal is not valid - " + budgetProposalBroadcast.GetHash().ToString() + " - " + strError;
    // }

    budget.mapSeenMasternodeBudgetProposals.insert(make_pair(budgetProposalBroadcast.GetHash(), budgetProposalBroadcast));
    budgetProposalBroadcast.Relay();
    if(budget.AddProposal(budgetProposalBroadcast)) {
        return budgetProposalBroadcast.GetHash().ToString();
    }
    throw runtime_error("Invalid proposal, see debug.log for details.");
}

Value mnbudgetvote(const Array& params, bool fHelp)
{
    std::string strCommand;
    if (params.size() >= 1) {
        strCommand = params[0].get_str();

        // Backwards compatibility with legacy `mnbudget` command
        if (strCommand == "vote") strCommand = "local";
        if (strCommand == "vote-many") strCommand = "many";
        if (strCommand == "vote-alias") strCommand = "alias";
    }

    if (fHelp || (params.size() == 3 && (strCommand != "local" && strCommand != "many")) || (params.size() == 4 && strCommand != "alias") ||
        params.size() > 4 || params.size() < 3)
        throw runtime_error(
            "mnbudgetvote \"local|many|alias\" \"votehash\" \"yes|no\" ( \"alias\" )\n"
            "\nVote on a budget proposal\n"

            "\nArguments:\n"
            "1. \"mode\"      (string, required) The voting mode. 'local' for voting directly from a masternode, 'many' for voting with a MN controller and casting the same vote for each MN, 'alias' for voting with a MN controller and casting a vote for a single MN\n"
            "2. \"votehash\"  (string, required) The vote hash for the proposal\n"
            "3. \"votecast\"  (string, required) Your vote. 'yes' to vote for the proposal, 'no' to vote against\n"
            "4. \"alias\"     (string, required for 'alias' mode) The MN alias to cast a vote for.\n"

            "\nResult:\n"
            "{\n"
            "  \"overall\": \"xxxx\",      (string) The overall status message for the vote cast\n"
            "  \"detail\": [\n"
            "    {\n"
            "      \"node\": \"xxxx\",      (string) 'local' or the MN alias\n"
            "      \"result\": \"xxxx\",    (string) Either 'Success' or 'Failed'\n"
            "      \"error\": \"xxxx\",     (string) Error message, if vote failed\n"
            "    }\n"
            "    ,...\n"
            "  ]\n"
            "}\n"

            "\nExamples:\n" +
            HelpExampleCli("mnbudgetvote", "\"local\" \"ed2f83cedee59a91406f5f47ec4d60bf5a7f9ee6293913c82976bd2d3a658041\" \"yes\"") +
            HelpExampleRpc("mnbudgetvote", "\"local\" \"ed2f83cedee59a91406f5f47ec4d60bf5a7f9ee6293913c82976bd2d3a658041\" \"yes\""));

    uint256 hash = ParseHashV(params[1], "parameter 1");
    std::string strVote = params[2].get_str();

    if (strVote != "yes" && strVote != "no") return "You can only vote 'yes' or 'no'";
    int nVote = VOTE_ABSTAIN;
    if (strVote == "yes") nVote = VOTE_YES;
    if (strVote == "no") nVote = VOTE_NO;

    int success = 0;
    int failed = 0;

    Array resultsObj;

    if (strCommand == "local") {
        CPubKey pubKeyMasternode;
        CKey keyMasternode;
        std::string errorMessage;

        Object statusObj;

        while (true) {
            if (!obfuScationSigner.SetKey(strMasterNodePrivKey, errorMessage, keyMasternode, pubKeyMasternode)) {
                failed++;
                statusObj.push_back(Pair("node", "local"));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Masternode signing error, could not set key correctly: " + errorMessage));
                resultsObj.push_back(statusObj);
                break;
            }

            CMasternode* pmn = mnodeman.Find(activeMasternode.vin);
            if (pmn == NULL) {
                failed++;
                statusObj.push_back(Pair("node", "local"));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Failure to find masternode in list : " + activeMasternode.vin.ToString()));
                resultsObj.push_back(statusObj);
                break;
            }

            CBudgetVote vote(activeMasternode.vin, hash, nVote);
            if (!vote.Sign(keyMasternode, pubKeyMasternode)) {
                failed++;
                statusObj.push_back(Pair("node", "local"));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Failure to sign."));
                resultsObj.push_back(statusObj);
                break;
            }

            std::string strError = "";
            if (budget.UpdateProposal(vote, NULL, strError)) {
                success++;
                budget.mapSeenMasternodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                statusObj.push_back(Pair("node", "local"));
                statusObj.push_back(Pair("result", "success"));
                statusObj.push_back(Pair("error", ""));
            } else {
                failed++;
                statusObj.push_back(Pair("node", "local"));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Error voting : " + strError));
            }
            resultsObj.push_back(statusObj);
            break;
        }

        Object returnObj;
        returnObj.push_back(Pair("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "many") {
        BOOST_FOREACH (CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {
            std::string errorMessage;
            std::vector<unsigned char> vchMasterNodeSignature;
            std::string strMasterNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyMasternode;
            CKey keyMasternode;

            Object statusObj;

            if (!obfuScationSigner.SetKey(mne.getPrivKey(), errorMessage, keyMasternode, pubKeyMasternode)) {
                failed++;
                statusObj.push_back(Pair("node", mne.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Masternode signing error, could not set key correctly: " + errorMessage));
                resultsObj.push_back(statusObj);
                continue;
            }

            CMasternode* pmn = mnodeman.Find(pubKeyMasternode);
            if (pmn == NULL) {
                failed++;
                statusObj.push_back(Pair("node", mne.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Can't find masternode by pubkey"));
                resultsObj.push_back(statusObj);
                continue;
            }

            CBudgetVote vote(pmn->vin, hash, nVote);
            if (!vote.Sign(keyMasternode, pubKeyMasternode)) {
                failed++;
                statusObj.push_back(Pair("node", mne.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Failure to sign."));
                resultsObj.push_back(statusObj);
                continue;
            }

            std::string strError = "";
            if (budget.UpdateProposal(vote, NULL, strError)) {
                budget.mapSeenMasternodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                success++;
                statusObj.push_back(Pair("node", mne.getAlias()));
                statusObj.push_back(Pair("result", "success"));
                statusObj.push_back(Pair("error", ""));
            } else {
                failed++;
                statusObj.push_back(Pair("node", mne.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", strError.c_str()));
            }

            resultsObj.push_back(statusObj);
        }

        Object returnObj;
        returnObj.push_back(Pair("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "alias") {
        std::string strAlias = params[3].get_str();
        std::vector<CMasternodeConfig::CMasternodeEntry> mnEntries;
        mnEntries = masternodeConfig.getEntries();

        BOOST_FOREACH(CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {

            if( strAlias != mne.getAlias()) continue;

            std::string errorMessage;
            std::vector<unsigned char> vchMasterNodeSignature;
            std::string strMasterNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyMasternode;
            CKey keyMasternode;

            Object statusObj;

            if(!obfuScationSigner.SetKey(mne.getPrivKey(), errorMessage, keyMasternode, pubKeyMasternode)){
                failed++;
                statusObj.push_back(Pair("node", mne.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Masternode signing error, could not set key correctly: " + errorMessage));
                resultsObj.push_back(statusObj);
                continue;
            }

            CMasternode* pmn = mnodeman.Find(pubKeyMasternode);
            if(pmn == NULL)
            {
                failed++;
                statusObj.push_back(Pair("node", mne.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Can't find masternode by pubkey"));
                resultsObj.push_back(statusObj);
                continue;
            }

            CBudgetVote vote(pmn->vin, hash, nVote);
            if(!vote.Sign(keyMasternode, pubKeyMasternode)){
                failed++;
                statusObj.push_back(Pair("node", mne.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", "Failure to sign."));
                resultsObj.push_back(statusObj);
                continue;
            }

            std::string strError = "";
            if(budget.UpdateProposal(vote, NULL, strError)) {
                budget.mapSeenMasternodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                success++;
                statusObj.push_back(Pair("node", mne.getAlias()));
                statusObj.push_back(Pair("result", "success"));
                statusObj.push_back(Pair("error", ""));
            } else {
                failed++;
                statusObj.push_back(Pair("node", mne.getAlias()));
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("error", strError.c_str()));
            }

            resultsObj.push_back(statusObj);
        }

        Object returnObj;
        returnObj.push_back(Pair("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    return Value::null;
}

Value getbudgetvotes(const Array& params, bool fHelp)
{
    if (params.size() != 1)
        throw runtime_error(
            "getbudgetvotes \"proposal-name\"\n"
            "\nPrint vote information for a budget proposal\n"

            "\nArguments:\n"
            "1. \"proposal-name\":      (string, required) Name of the proposal\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"mnId\": \"xxxx\",        (string) Hash of the masternode's collateral transaction\n"
            "    \"nHash\": \"xxxx\",       (string) Hash of the vote\n"
            "    \"Vote\": \"YES|NO\",      (string) Vote cast ('YES' or 'NO')\n"
            "    \"nTime\": xxxx,         (numeric) Time in seconds since epoch the vote was cast\n"
            "    \"fValid\": true|false,  (boolean) 'true' if the vote is valid, 'false' otherwise\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n" +
            HelpExampleCli("getbudgetvotes", "\"test-proposal\"") + HelpExampleRpc("getbudgetvotes", "\"test-proposal\""));

    std::string strProposalName = SanitizeString(params[0].get_str());

    Array ret;

    CBudgetProposal* pbudgetProposal = budget.FindProposal(strProposalName);

    if (pbudgetProposal == NULL) throw runtime_error("Unknown proposal name");

    std::map<uint256, CBudgetVote>::iterator it = pbudgetProposal->mapVotes.begin();
    while (it != pbudgetProposal->mapVotes.end()) {
        Object bObj;
        bObj.push_back(Pair("mnId", (*it).second.vin.prevout.hash.ToString()));
        bObj.push_back(Pair("nHash", (*it).first.ToString().c_str()));
        bObj.push_back(Pair("Vote", (*it).second.GetVoteString()));
        bObj.push_back(Pair("nTime", (int64_t)(*it).second.nTime));
        bObj.push_back(Pair("fValid", (*it).second.fValid));

        ret.push_back(bObj);

        it++;
    }

    return ret;
}

Value getnextsuperblock(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getnextsuperblock\n"
            "\nPrint the next super block height\n"

            "\nResult:\n"
            "n      (numeric) Block height of the next super block\n"
            "\nExamples:\n" +
            HelpExampleCli("getnextsuperblock", "") + HelpExampleRpc("getnextsuperblock", ""));

    CBlockIndex* pindexPrev = chainActive.Tip();
    if (!pindexPrev) return "unknown";

    int nNext = pindexPrev->nHeight - pindexPrev->nHeight % GetBudgetPaymentCycleBlocks() + GetBudgetPaymentCycleBlocks();
    return nNext;
}

Value getbudgetprojection(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "getbudgetprojection\n"
            "\nShow the projection of which proposals will be paid the next cycle\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"Name\": \"xxxx\",               (string) Proposal Name\n"
            "    \"URL\": \"xxxx\",                (string) Proposal URL\n"
            "    \"Hash\": \"xxxx\",               (string) Proposal vote hash\n"
            "    \"FeeHash\": \"xxxx\",            (string) Proposal fee hash\n"
            "    \"BlockStart\": n,              (numeric) Proposal starting block\n"
            "    \"BlockEnd\": n,                (numeric) Proposal ending block\n"
            "    \"TotalPaymentCount\": n,       (numeric) Number of payments\n"
            "    \"RemainingPaymentCount\": n,   (numeric) Number of remaining payments\n"
            "    \"PaymentAddress\": \"xxxx\",     (string) LUX address of payment\n"
            "    \"Ratio\": x.xxx,               (numeric) Ratio of yeas vs nays\n"
            "    \"Yeas\": n,                    (numeric) Number of yea votes\n"
            "    \"Nays\": n,                    (numeric) Number of nay votes\n"
            "    \"Abstains\": n,                (numeric) Number of abstains\n"
            "    \"TotalPayment\": xxx.xxx,      (numeric) Total payment amount\n"
            "    \"MonthlyPayment\": xxx.xxx,    (numeric) Monthly payment amount\n"
            "    \"IsEstablished\": true|false,  (boolean) Established (true) or (false)\n"
            "    \"IsValid\": true|false,        (boolean) Valid (true) or Invalid (false)\n"
            "    \"IsValidReason\": \"xxxx\",      (string) Error message, if any\n"
            "    \"fValid\": true|false,         (boolean) Valid (true) or Invalid (false)\n"
            "    \"Alloted\": xxx.xxx,           (numeric) Amount alloted in current period\n"
            "    \"TotalBudgetAlloted\": xxx.xxx (numeric) Total alloted\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n" +
            HelpExampleCli("getbudgetprojection", "") + HelpExampleRpc("getbudgetprojection", ""));

    Array ret;
    Object resultObj;
    CAmount nTotalAllotted = 0;

    std::vector<CBudgetProposal*> winningProps = budget.GetBudget();
    BOOST_FOREACH (CBudgetProposal* pbudgetProposal, winningProps) {
        nTotalAllotted += pbudgetProposal->GetAllotted();

        CTxDestination address1;
        ExtractDestination(pbudgetProposal->GetPayee(), address1);
        CBitcoinAddress address2(address1);

        Object bObj;
        budgetToJSON(pbudgetProposal, bObj);
        bObj.push_back(Pair("Alloted", ValueFromAmount(pbudgetProposal->GetAllotted())));
        bObj.push_back(Pair("TotalBudgetAlloted", ValueFromAmount(nTotalAllotted)));

        ret.push_back(bObj);
    }

    return ret;
}

Value getbudgetinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getbudgetinfo ( \"proposal\" )\n"
            "\nShow current masternode budgets\n"

            "\nArguments:\n"
            "1. \"proposal\"    (string, optional) Proposal name\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"Name\": \"xxxx\",               (string) Proposal Name\n"
            "    \"URL\": \"xxxx\",                (string) Proposal URL\n"
            "    \"Hash\": \"xxxx\",               (string) Proposal vote hash\n"
            "    \"FeeHash\": \"xxxx\",            (string) Proposal fee hash\n"
            "    \"BlockStart\": n,              (numeric) Proposal starting block\n"
            "    \"BlockEnd\": n,                (numeric) Proposal ending block\n"
            "    \"TotalPaymentCount\": n,       (numeric) Number of payments\n"
            "    \"RemainingPaymentCount\": n,   (numeric) Number of remaining payments\n"
            "    \"PaymentAddress\": \"xxxx\",     (string) LUX address of payment\n"
            "    \"Ratio\": x.xxx,               (numeric) Ratio of yeas vs nays\n"
            "    \"Yeas\": n,                    (numeric) Number of yea votes\n"
            "    \"Nays\": n,                    (numeric) Number of nay votes\n"
            "    \"Abstains\": n,                (numeric) Number of abstains\n"
            "    \"TotalPayment\": xxx.xxx,      (numeric) Total payment amount\n"
            "    \"MonthlyPayment\": xxx.xxx,    (numeric) Monthly payment amount\n"
            "    \"IsEstablished\": true|false,  (boolean) Established (true) or (false)\n"
            "    \"IsValid\": true|false,        (boolean) Valid (true) or Invalid (false)\n"
            "    \"IsValidReason\": \"xxxx\",      (string) Error message, if any\n"
            "    \"fValid\": true|false,         (boolean) Valid (true) or Invalid (false)\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n" +
            HelpExampleCli("getbudgetprojection", "") + HelpExampleRpc("getbudgetprojection", ""));

    Array ret;

    std::string strShow = "valid";
    if (params.size() == 1) {
        std::string strProposalName = SanitizeString(params[0].get_str());
        CBudgetProposal* pbudgetProposal = budget.FindProposal(strProposalName);
        if (pbudgetProposal == NULL) throw runtime_error("Unknown proposal name");
        Object bObj;
        budgetToJSON(pbudgetProposal, bObj);
        ret.push_back(bObj);
        return ret;
    }

    std::vector<CBudgetProposal*> winningProps = budget.GetAllProposals();
    BOOST_FOREACH (CBudgetProposal* pbudgetProposal, winningProps) {
        if (strShow == "valid" && !pbudgetProposal->fValid) continue;

        Object bObj;
        budgetToJSON(pbudgetProposal, bObj);

        ret.push_back(bObj);
    }

    return ret;
}

Value mnbudgetrawvote(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 6)
        throw runtime_error(
            "mnbudgetrawvote \"masternode-tx-hash\" masternode-tx-index \"proposal-hash\" yes|no time \"vote-sig\"\n"
            "\nCompile and relay a proposal vote with provided external signature instead of signing vote internally\n"

            "\nArguments:\n"
            "1. \"masternode-tx-hash\"  (string, required) Transaction hash for the masternode\n"
            "2. masternode-tx-index   (numeric, required) Output index for the masternode\n"
            "3. \"proposal-hash\"       (string, required) Proposal vote hash\n"
            "4. yes|no                (boolean, required) Vote to cast\n"
            "5. time                  (numeric, required) Time since epoch in seconds\n"
            "6. \"vote-sig\"            (string, required) External signature\n"

            "\nResult:\n"
            "\"status\"     (string) Vote status or error message\n"
            "\nExamples:\n" +
            HelpExampleCli("mnbudgetrawvote", "") + HelpExampleRpc("mnbudgetrawvote", ""));

    uint256 hashMnTx = ParseHashV(params[0], "mn tx hash");
    int nMnTxIndex = params[1].get_int();
    CTxIn vin = CTxIn(hashMnTx, nMnTxIndex);

    uint256 hashProposal = ParseHashV(params[2], "Proposal hash");
    std::string strVote = params[3].get_str();

    if (strVote != "yes" && strVote != "no") return "You can only vote 'yes' or 'no'";
    int nVote = VOTE_ABSTAIN;
    if (strVote == "yes") nVote = VOTE_YES;
    if (strVote == "no") nVote = VOTE_NO;

    int64_t nTime = params[4].get_int64();
    std::string strSig = params[5].get_str();
    bool fInvalid = false;
    vector<unsigned char> vchSig = DecodeBase64(strSig.c_str(), &fInvalid);

    if (fInvalid)
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Malformed base64 encoding");

    CMasternode* pmn = mnodeman.Find(vin);
    if (pmn == NULL) {
        return "Failure to find masternode in list : " + vin.ToString();
    }

    CBudgetVote vote(vin, hashProposal, nVote);
    vote.nTime = nTime;
    vote.vchSig = vchSig;

    if (!vote.SignatureValid(true)) {
        return "Failure to verify signature.";
    }

    std::string strError = "";
    if (budget.UpdateProposal(vote, NULL, strError)) {
        budget.mapSeenMasternodeBudgetVotes.insert(make_pair(vote.GetHash(), vote));
        vote.Relay();
        return "Voted successfully";
    } else {
        return "Error voting : " + strError;
    }
}

Value mnfinalbudget(const Array& params, bool fHelp)
{
    string strCommand;
    if (params.size() >= 1)
        strCommand = params[0].get_str();

    if (fHelp ||
        (strCommand != "suggest" && strCommand != "vote-many" && strCommand != "vote" && strCommand != "show" && strCommand != "getvotes"))
        throw runtime_error(
            "mnfinalbudget \"command\"... ( \"passphrase\" )\n"
            "Vote or show current budgets\n"
            "\nAvailable commands:\n"
            "  vote-many   - Vote on a finalized budget\n"
            "  vote        - Vote on a finalized budget\n"
            "  show        - Show existing finalized budgets\n"
            "  getvotes     - Get vote information for each finalized budget\n");

    if (strCommand == "vote-many") {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'mnfinalbudget vote-many BUDGET_HASH'");

        std::string strHash = params[1].get_str();
        uint256 hash(strHash);

        int success = 0;
        int failed = 0;

        Object resultsObj;

        BOOST_FOREACH (CMasternodeConfig::CMasternodeEntry mne, masternodeConfig.getEntries()) {
            std::string errorMessage;
            std::vector<unsigned char> vchMasterNodeSignature;
            std::string strMasterNodeSignMessage;

            CPubKey pubKeyCollateralAddress;
            CKey keyCollateralAddress;
            CPubKey pubKeyMasternode;
            CKey keyMasternode;

            Object statusObj;

            if (!obfuScationSigner.SetKey(mne.getPrivKey(), errorMessage, keyMasternode, pubKeyMasternode)) {
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Masternode signing error, could not set key correctly: " + errorMessage));
                resultsObj.push_back(Pair(mne.getAlias(), statusObj));
                continue;
            }

            CMasternode* pmn = mnodeman.Find(pubKeyMasternode);
            if (pmn == NULL) {
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Can't find masternode by pubkey"));
                resultsObj.push_back(Pair(mne.getAlias(), statusObj));
                continue;
            }


            CFinalizedBudgetVote vote(pmn->vin, hash);
            if (!vote.Sign(keyMasternode, pubKeyMasternode)) {
                failed++;
                statusObj.push_back(Pair("result", "failed"));
                statusObj.push_back(Pair("errorMessage", "Failure to sign."));
                resultsObj.push_back(Pair(mne.getAlias(), statusObj));
                continue;
            }

            std::string strError = "";
            if (budget.UpdateFinalizedBudget(vote, NULL, strError)) {
                budget.mapSeenFinalizedBudgetVotes.insert(make_pair(vote.GetHash(), vote));
                vote.Relay();
                success++;
                statusObj.push_back(Pair("result", "success"));
            } else {
                failed++;
                statusObj.push_back(Pair("result", strError.c_str()));
            }

            resultsObj.push_back(Pair(mne.getAlias(), statusObj));
        }

        Object returnObj;
        returnObj.push_back(Pair("overall", strprintf("Voted successfully %d time(s) and failed %d time(s).", success, failed)));
        returnObj.push_back(Pair("detail", resultsObj));

        return returnObj;
    }

    if (strCommand == "vote") {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'mnfinalbudget vote BUDGET_HASH'");

        std::string strHash = params[1].get_str();
        uint256 hash(strHash);

        CPubKey pubKeyMasternode;
        CKey keyMasternode;
        std::string errorMessage;

        if (!obfuScationSigner.SetKey(strMasterNodePrivKey, errorMessage, keyMasternode, pubKeyMasternode))
            return "Error upon calling SetKey";

        CMasternode* pmn = mnodeman.Find(activeMasternode.vin);
        if (pmn == NULL) {
            return "Failure to find masternode in list : " + activeMasternode.vin.ToString();
        }

        CFinalizedBudgetVote vote(activeMasternode.vin, hash);
        if (!vote.Sign(keyMasternode, pubKeyMasternode)) {
            return "Failure to sign.";
        }

        std::string strError = "";
        if (budget.UpdateFinalizedBudget(vote, NULL, strError)) {
            budget.mapSeenFinalizedBudgetVotes.insert(make_pair(vote.GetHash(), vote));
            vote.Relay();
            return "success";
        } else {
            return "Error voting : " + strError;
        }
    }

    if (strCommand == "show") {
        Object resultObj;

        std::vector<CFinalizedBudget*> winningFbs = budget.GetFinalizedBudgets();
        BOOST_FOREACH (CFinalizedBudget* finalizedBudget, winningFbs) {
            Object bObj;
            bObj.push_back(Pair("FeeTX", finalizedBudget->nFeeTXHash.ToString()));
            bObj.push_back(Pair("Hash", finalizedBudget->GetHash().ToString()));
            bObj.push_back(Pair("BlockStart", (int64_t)finalizedBudget->GetBlockStart()));
            bObj.push_back(Pair("BlockEnd", (int64_t)finalizedBudget->GetBlockEnd()));
            bObj.push_back(Pair("Proposals", finalizedBudget->GetProposals()));
            bObj.push_back(Pair("VoteCount", (int64_t)finalizedBudget->GetVoteCount()));
            bObj.push_back(Pair("Status", finalizedBudget->GetStatus()));

            std::string strError = "";
            bObj.push_back(Pair("IsValid", finalizedBudget->IsValid(strError)));
            bObj.push_back(Pair("IsValidReason", strError.c_str()));

            resultObj.push_back(Pair(finalizedBudget->GetName(), bObj));
        }

        return resultObj;
    }

    if (strCommand == "getvotes") {
        if (params.size() != 2)
            throw runtime_error("Correct usage is 'mnbudget getvotes budget-hash'");

        std::string strHash = params[1].get_str();
        uint256 hash(strHash);

        Object obj;

        CFinalizedBudget* pfinalBudget = budget.FindFinalizedBudget(hash);

        if (pfinalBudget == NULL) return "Unknown budget hash";

        std::map<uint256, CFinalizedBudgetVote>::iterator it = pfinalBudget->mapVotes.begin();
        while (it != pfinalBudget->mapVotes.end()) {
            Object bObj;
            bObj.push_back(Pair("nHash", (*it).first.ToString().c_str()));
            bObj.push_back(Pair("nTime", (int64_t)(*it).second.nTime));
            bObj.push_back(Pair("fValid", (*it).second.fValid));

            obj.push_back(Pair((*it).second.vin.prevout.ToStringShort(), bObj));

            it++;
        }

        return obj;
    }

    return Value::null;
}

Value checkbudgets(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw runtime_error(
            "checkbudgets\n"
            "\nInitiates a buddget check cycle manually\n"
            "\nExamples:\n" +
            HelpExampleCli("checkbudgets", "") + HelpExampleRpc("checkbudgets", ""));

    budget.CheckAndRemove();

    return Value::null;
}
