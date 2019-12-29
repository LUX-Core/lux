// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "consensus/validation.h"
#include "darksend.h"
#include "main.h"
#include "init.h"
#include "script/sign.h"
#include "script/interpreter.h"
#include "util.h"
#include "masternode.h"
#include "instantx.h"
#include "ui_interface.h"
#include "wallet.h"
//#include "random.h"

#include <openssl/rand.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/lexical_cast.hpp>

#include <algorithm>
#include <boost/assign/list_of.hpp>

using namespace std;
using namespace boost;

CCriticalSection cs_darksend;

/** The main object for accessing darksend */
CDarkSendPool darkSendPool;
/** A helper object for signing messages from masternodes */
CDarkSendSigner darkSendSigner;
/** The current darksends in progress on the network */
std::vector<CDarksendQueue> vecDarksendQueue;
/** Keep track of the used masternodes */
std::vector<CTxIn> vecMasternodesUsed;
// keep track of the scanning errors I've seen
map<uint256, CDarksendBroadcastTx> mapDarksendBroadcastTxes;
//
CActiveMasternode activeMasternode;

// count peers we've requested the list from
int RequestedMasterNodeList = 0;

/* *** BEGIN DARKSEND MAGIC  **********
    Copyright 2014, Darkcoin Developers
        eduffield - evan@darkcoin.io
*/

void ProcessMessageDarksend(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isDarksend) {
    if (IsInitialBlockDownload()) return;

    if (strCommand == "dsf") { //DarkSend Final tx
        isDarksend = true;

        if (pfrom->nVersion < darkSendPool.MIN_PEER_PROTO_VERSION) {
            return;
        }

        if ((CNetAddr) darkSendPool.submittedToMasternode != (CNetAddr) pfrom->addr) {
            //LogPrintf("dsc - message doesn't match current masternode - %s != %s\n", darkSendPool.submittedToMasternode.ToString().c_str(), pfrom->addr.ToString().c_str());
            return;
        }

        int sessionID;
        CTransaction txNew;
        vRecv >> sessionID >> txNew;

        if (darkSendPool.sessionID != sessionID) {
            if (fDebug) LogPrintf("dsf - message doesn't match current darksend session %d %d\n", darkSendPool.sessionID, sessionID);
            return;
        }

        //check to see if input is spent already? (and probably not confirmed)
        darkSendPool.SignFinalTransaction(txNew, pfrom);
    }

    else if (strCommand == "dsc") { //DarkSend Complete
        isDarksend = true;

        if (pfrom->nVersion < darkSendPool.MIN_PEER_PROTO_VERSION) {
            return;
        }

        if ((CNetAddr) darkSendPool.submittedToMasternode != (CNetAddr) pfrom->addr) {
            //LogPrintf("dsc - message doesn't match current masternode - %s != %s\n", darkSendPool.submittedToMasternode.ToString().c_str(), pfrom->addr.ToString().c_str());
            return;
        }

        int sessionID;
        bool error;
        std::string lastMessage;
        vRecv >> sessionID >> error >> lastMessage;

        if (darkSendPool.sessionID != sessionID) {
            if (fDebug) LogPrintf("dsc - message doesn't match current darksend session %d %d\n", darkSendPool.sessionID, sessionID);
            return;
        }

        darkSendPool.CompletedTransaction(error, lastMessage);
    }

    else if (strCommand == "dsa") { //DarkSend Acceptable
        isDarksend = true;

        if (pfrom->nVersion < darkSendPool.MIN_PEER_PROTO_VERSION) {
            std::string strError = _("Incompatible version.");
            LogPrintf("dsa -- incompatible version! \n");
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, strError);

            return;
        }

        if (!fMasterNode) {
            std::string strError = _("This is not a masternode.");
            LogPrintf("dsa -- not a masternode! \n");
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, strError);

            return;
        }

        int nDenom;
        CTransaction txCollateral;
        vRecv >> nDenom >> txCollateral;

        std::string error = "";
        int mn = GetMasternodeByVin(activeMasternode.vin);
        if (mn == -1) {
            std::string strError = _("Not in the masternode list.");
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, strError);
            return;
        }

        if (darkSendPool.sessionUsers == 0) {
            if (vecMasternodes[mn].nLastDsq != 0 &&
                vecMasternodes[mn].nLastDsq + CountMasternodesAboveProtocol(darkSendPool.MIN_PEER_PROTO_VERSION) / 5 > darkSendPool.nDsqCount) {
                //LogPrintf("dsa -- last dsq too recent, must wait. %s \n", vecMasternodes[mn].addr.ToString().c_str());
                std::string strError = _("Last Darksend was too recent.");
                pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, strError);
                return;
            }
        }

        if (!darkSendPool.IsCompatibleWithSession(nDenom, txCollateral, error)) {
            LogPrintf("dsa -- not compatible with existing transactions! \n");
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);
            return;
        } else {
            LogPrintf("dsa -- is compatible, please submit! \n");
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_ACCEPTED, error);
            return;
        }
    } else if (strCommand == "dsq") { //DarkSend Queue
        isDarksend = true;

        if (pfrom->nVersion < darkSendPool.MIN_PEER_PROTO_VERSION) {
            return;
        }

        CDarksendQueue dsq;
        vRecv >> dsq;


        CService addr;
        if (!dsq.GetAddress(addr)) return;
        if (!dsq.CheckSignature()) return;

        if (dsq.IsExpired()) return;

        int mn = GetMasternodeByVin(dsq.vin);
        if (mn == -1) return;

        // if the queue is ready, submit if we can
        if (dsq.ready) {
            if ((CNetAddr) darkSendPool.submittedToMasternode != (CNetAddr) addr) {
                LogPrintf("dsq - message doesn't match current masternode - %s != %s\n", darkSendPool.submittedToMasternode.ToString().c_str(), pfrom->addr.ToString().c_str());
                return;
            }

            if (fDebug) LogPrintf("darksend queue is ready - %s\n", addr.ToString().c_str());
            darkSendPool.PrepareDarksendDenominate();
        } else {
            for (CDarksendQueue q : vecDarksendQueue) {
                if (q.vin == dsq.vin) return;
            }

            if (fDebug) LogPrintf("dsq last %d last2 %d count %d\n", vecMasternodes[mn].nLastDsq, vecMasternodes[mn].nLastDsq + (int) vecMasternodes.size() / 5, darkSendPool.nDsqCount);
            //don't allow a few nodes to dominate the queuing process
            if (vecMasternodes[mn].nLastDsq != 0 &&
                vecMasternodes[mn].nLastDsq + CountMasternodesAboveProtocol(darkSendPool.MIN_PEER_PROTO_VERSION) / 5 > darkSendPool.nDsqCount) {
                if (fDebug) LogPrintf("dsq -- masternode sending too many dsq messages. %s \n", vecMasternodes[mn].addr.ToString().c_str());
                return;
            }
            darkSendPool.nDsqCount++;
            vecMasternodes[mn].nLastDsq = darkSendPool.nDsqCount;
            vecMasternodes[mn].allowFreeTx = true;

            if (fDebug) LogPrintf("dsq - new darksend queue object - %s\n", addr.ToString().c_str());
            vecDarksendQueue.push_back(dsq);
            dsq.Relay();
            dsq.time = GetTime();
        }

    }

    else if (strCommand == "dsi") { //DarkSend vIn
        isDarksend = true;

        std::string error = "";
        if (pfrom->nVersion < darkSendPool.MIN_PEER_PROTO_VERSION) {
            LogPrintf("dsi -- incompatible version! \n");
            error = _("Incompatible version.");
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);

            return;
        }

        if (!fMasterNode) {
            LogPrintf("dsi -- not a masternode! \n");
            error = _("This is not a masternode.");
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);

            return;
        }

        std::vector<CTxIn> in;
        int64_t nAmount;
        CTransaction txCollateral;
        std::vector<CTxOut> out;
        vRecv >> in >> nAmount >> txCollateral >> out;

        //do we have enough users in the current session?
        if (!darkSendPool.IsSessionReady()) {
            LogPrintf("dsi -- session not complete! \n");
            error = _("Session not complete!");
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);
            return;
        }

        //do we have the same denominations as the current session?
        if (!darkSendPool.IsCompatibleWithEntries(out)) {
            LogPrintf("dsi -- not compatible with existing transactions! \n");
            error = _("Not compatible with existing transactions.");
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);
            return;
        }

        //check it like a transaction
        {
            int64_t nValueIn = 0;
            int64_t nValueOut = 0;
            bool missingTx = false;

            CValidationState state;
            CMutableTransaction tx;

            for (CTxOut o : out) {
                nValueOut += o.nValue;
                tx.vout.push_back(o);

                if (o.scriptPubKey.size() != 25) {
                    LogPrintf("dsi - non-standard pubkey detected! %s\n", o.scriptPubKey.ToString().c_str());
                    error = _("Non-standard public key detected.");
                    pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);
                    return;
                }
                if (!o.scriptPubKey.IsNormalPaymentScript()) {
                    LogPrintf("dsi - invalid script! %s\n", o.scriptPubKey.ToString().c_str());
                    error = _("Invalid script detected.");
                    pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);
                    return;
                }
            }

            for (const CTxIn i : in) {
                tx.vin.push_back(i);

                if (fDebug) LogPrintf("dsi -- tx in %s\n", i.ToString().c_str());

                CTransaction tx2;
                uint256 hash;
                if (GetTransaction(i.prevout.hash, tx2, Params().GetConsensus(), hash)) {
                    if (tx2.vout.size() > i.prevout.n) {
                        nValueIn += tx2.vout[i.prevout.n].nValue;
                    }
                } else {
                    missingTx = true;
                }
            }

            if (nValueIn > DARKSEND_POOL_MAX) {
                LogPrintf("dsi -- more than darksend pool max! %s\n", tx.ToString().c_str());
                error = _("Value more than Darksend pool maximum allows.");
                pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);
                return;
            }

            if (!missingTx) {
                if (nValueIn - nValueOut > nValueIn * .01) {
                    LogPrintf("dsi -- fees are too high! %s\n", tx.ToString().c_str());
                    error = _("Transaction fees are too high.");
                    pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);
                    return;
                }
            } else {
                LogPrintf("dsi -- missing input tx! %s\n", tx.ToString().c_str());
                error = _("Missing input transaction information.");
                pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);
                return;
            }

            bool* pfMissingInputs = nullptr;
            if (!AcceptableInputs(mempool, state, CTransaction(tx), false, pfMissingInputs)) {
                LogPrintf("dsi -- transaction not valid! \n");
                error = _("Transaction not valid.");
                pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);
                return;
            }
        }

        if (darkSendPool.AddEntry(in, nAmount, txCollateral, out, error)) {
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_ACCEPTED, error);
            darkSendPool.Check();

            RelayDarkSendStatus(darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_RESET);
        } else {
            pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_REJECTED, error);
        }
    }

    else if (strCommand == "dssub") { //DarkSend Subscribe To
        isDarksend = true;

        if (pfrom->nVersion < darkSendPool.MIN_PEER_PROTO_VERSION) {
            return;
        }

        if (!fMasterNode) return;

        std::string error = "";
        pfrom->PushMessage("dssu", darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_RESET, error);
        return;
    }

    else if (strCommand == "dssu") { //DarkSend status update
        isDarksend = true;

        if (pfrom->nVersion < darkSendPool.MIN_PEER_PROTO_VERSION) {
            return;
        }

        if ((CNetAddr) darkSendPool.submittedToMasternode != (CNetAddr) pfrom->addr) {
            //LogPrintf("dssu - message doesn't match current masternode - %s != %s\n", darkSendPool.submittedToMasternode.ToString().c_str(), pfrom->addr.ToString().c_str());
            return;
        }

        int sessionID;
        int state;
        int entriesCount;
        int accepted;
        std::string error;
        vRecv >> sessionID >> state >> entriesCount >> accepted >> error;

        if (fDebug) LogPrintf("dssu - state: %i entriesCount: %i accepted: %i error: %s \n", state, entriesCount, accepted, error.c_str());

        if ((accepted != 1 && accepted != 0) && darkSendPool.sessionID != sessionID) {
            LogPrintf("dssu - message doesn't match current darksend session %d %d\n", darkSendPool.sessionID, sessionID);
            return;
        }

        darkSendPool.StatusUpdate(state, entriesCount, accepted, error, sessionID);

    }

    else if (strCommand == "dss") { //DarkSend Sign Final Tx
        isDarksend = true;

        if (pfrom->nVersion < darkSendPool.MIN_PEER_PROTO_VERSION) {
            return;
        }

        vector<CTxIn> sigs;
        vRecv >> sigs;

        std::atomic<bool> success{false};
        int count = 0;

        LogPrintf(" -- sigs count %d %d\n", (int) sigs.size(), count);

        for (const CTxIn item : sigs) {
            if (darkSendPool.AddScriptSig(item)) success = true;
            if (fDebug) LogPrintf(" -- sigs count %d %d\n", (int) sigs.size(), count);
            count++;
        }

        if (success) {
            darkSendPool.Check();
            RelayDarkSendStatus(darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_RESET);
        }
    }
}

int randomizeList(int i) { return std::rand() % i; }

// Recursively determine the rounds of a given input (How deep is the darksend chain for a given input)
int GetInputDarksendRounds(CTxIn in, int rounds) {
    if (rounds >= 17) return rounds;
    if (rounds > nDarksendRounds || nDarksendRounds <= 0 || !fEnableDarksend) return rounds; // once we hit the max rounds setting, there's no point going further back (this func is only used for selecting qualifying txs)

    std::string padding = "";
    padding.insert(0, ((rounds + 1) * 5) + 3, ' ');

    const CWalletTx* wtx = pwalletMain->GetWalletTx(in.prevout.hash);
    if (wtx != NULL) {
        // bounds check
        if (in.prevout.n >= wtx->vout.size()) return -4;

        if (wtx->vout[in.prevout.n].nValue == DARKSEND_FEE) return -3;

        //make sure the final output is non-denominate
        if (rounds == 0 && !pwalletMain->IsDenominatedAmount(wtx->vout[in.prevout.n].nValue)) return -2; //NOT DENOM

        bool found = false;
        for (CTxOut out : wtx->vout) {
            found = pwalletMain->IsDenominatedAmount(out.nValue);
            if (found) break; // no need to loop more
        }
        if (!found) return rounds - 1; //NOT FOUND, "-1" because of the pre-mixing creation of denominated amounts

        // find my vin and look that up
        for (CTxIn in2 : wtx->vin) {
            if (pwalletMain->IsMine(in2)) {
                //LogPrintf("rounds :: %s %s %d NEXT\n", padding.c_str(), in.ToString().c_str(), rounds);
                int n = GetInputDarksendRounds(in2, rounds + 1);
                if (n != -3) return n;
            }
        }
    }

    return rounds - 1;
}

void CDarkSendPool::Reset() {
    cachedLastSuccess = 0;
    vecMasternodesUsed.clear();
    UnlockCoins();
    SetNull();
}

void CDarkSendPool::SetNull(bool clearEverything) {
    finalTransaction.vin.clear();
    finalTransaction.vout.clear();

    entries.clear();

    state = POOL_STATUS_ACCEPTING_ENTRIES;

    lastTimeChanged = GetTimeMillis();

    entriesCount = 0;
    lastEntryAccepted = 0;
    countEntriesAccepted = 0;
    lastNewBlock = 0;

    sessionUsers = 0;
    sessionDenom = 0;
    sessionFoundMasternode = false;
    vecSessionCollateral.clear();
    txCollateral = CTransaction();

    if (clearEverything) {
        myEntries.clear();

        if (fMasterNode) {
            sessionID = 1 + (rand() % 999999);
        } else {
            sessionID = 0;
        }
    }

    // -- seed random number generator (used for ordering output lists)
    unsigned int seed = 0;
    GetRandBytes((unsigned char*) &seed, sizeof(seed));
    std::srand(seed);
}

bool CDarkSendPool::SetCollateralAddress(std::string strAddress) {
    CTxDestination dest = DecodeDestination(strAddress);
    if (!IsValidDestination(dest)) {
        LogPrintf("CDarkSendPool::SetCollateralAddress - Invalid DarkSend collateral address\n");
        return false;
    }
    collateralPubKey = GetScriptForDestination(dest);
    return true;
}

//
// Unlock coins after Darksend fails or succeeds
//
void CDarkSendPool::UnlockCoins() {
    for (CTxIn v : lockedCoins)
        pwalletMain->UnlockCoin(v.prevout);

    lockedCoins.clear();
}

//
// Check the Darksend progress and send client updates if a masternode
//
void CDarkSendPool::Check() {
    if (fDebug) LogPrintf("CDarkSendPool::Check()\n");
    if (fDebug) LogPrintf("CDarkSendPool::Check() - entries count %lu\n", entries.size());

    // If entries is full, then move on to the next phase
    if (state == POOL_STATUS_ACCEPTING_ENTRIES && (int) entries.size() >= GetMaxPoolTransactions()) {
        if (fDebug) LogPrintf("CDarkSendPool::Check() -- ACCEPTING OUTPUTS\n");
        UpdateState(POOL_STATUS_FINALIZE_TRANSACTION);
    }

    // create the finalized transaction for distribution to the clients
    if (state == POOL_STATUS_FINALIZE_TRANSACTION && finalTransaction.vin.empty() && finalTransaction.vout.empty()) {
        if (fDebug) LogPrintf("CDarkSendPool::Check() -- FINALIZE TRANSACTIONS\n");
        UpdateState(POOL_STATUS_SIGNING);

        if (fMasterNode) {
            // make our new transaction
            CMutableTransaction txNew;
            for (unsigned int i = 0; i < entries.size(); i++) {
                for (const CTxOut v : entries[i].vout)
                    txNew.vout.push_back(v);

                for (const CDarkSendEntryVin s : entries[i].sev)
                    txNew.vin.push_back(s.vin);
            }
            // shuffle the outputs for improved anonymity
            std::random_shuffle(txNew.vout.begin(), txNew.vout.end(), randomizeList);

            if (fDebug) LogPrintf("Transaction 1: %s\n", txNew.ToString().c_str());

            SignFinalTransaction(CTransaction(txNew), NULL);

            // request signatures from clients
            RelayDarkSendFinalTransaction(sessionID, txNew);
        }
    }

    // collect signatures from clients

    // If we have all of the signatures, try to compile the transaction
    if (state == POOL_STATUS_SIGNING && SignaturesComplete()) {
        if (fDebug) LogPrintf("CDarkSendPool::Check() -- SIGNING\n");
        UpdateState(POOL_STATUS_TRANSMISSION);

        CWalletTx txNew = CWalletTx(pwalletMain, finalTransaction);

        LOCK2(cs_main, pwalletMain->cs_wallet);
        {
            if (fMasterNode) { //only the main node is master atm
                if (fDebug) LogPrintf("Transaction 2: %s\n", txNew.ToString().c_str());

                // See if the transaction is valid
                if (!txNew.AcceptToMemoryPool(true)) {
                    LogPrintf("CDarkSendPool::Check() - CommitTransaction : Error: Transaction not valid\n");
                    SetNull();
                    pwalletMain->Lock();

                    // not much we can do in this case
                    UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);
                    RelayDarkSendCompletedTransaction(sessionID, true, "Transaction not valid, please try again");
                    return;
                }

                LogPrintf("CDarkSendPool::Check() -- IS MASTER -- TRANSMITTING DARKSEND\n");

                // sign a message

                int64_t sigTime = GetAdjustedTime();
                std::string strMessage = txNew.GetHash().ToString() + boost::lexical_cast<std::string>(sigTime);
                std::string strError = "";
                std::vector<unsigned char> vchSig;
                CKey key2;
                CPubKey pubkey2;

                if (!darkSendSigner.SetKey(strMasterNodePrivKey, strError, key2, pubkey2)) {
                    LogPrintf("CDarkSendPool::Check() - ERROR: Invalid masternodeprivkey: '%s'\n", strError.c_str());
                    return;
                }

                if (!darkSendSigner.SignMessage(strMessage, strError, vchSig, key2)) {
                    LogPrintf("CDarkSendPool::Check() - Sign message failed\n");
                    return;
                }

                if (!darkSendSigner.VerifyMessage(pubkey2, vchSig, strMessage, strError)) {
                    LogPrintf("CDarkSendPool::Check() - Verify message failed\n");
                    return;
                }

                if (!mapDarksendBroadcastTxes.count(txNew.GetHash())) {
                    CDarksendBroadcastTx dstx;
                    dstx.tx = txNew;
                    dstx.vin = activeMasternode.vin;
                    dstx.vchSig = vchSig;
                    dstx.sigTime = sigTime;

                    mapDarksendBroadcastTxes.insert(make_pair(txNew.GetHash(), dstx));
                }

                // Broadcast the transaction to the network
                txNew.fTimeReceivedIsTxTime = true;
                txNew.RelayWalletTransaction();

                // Tell the clients it was successful
                RelayDarkSendCompletedTransaction(sessionID, false, _("Transaction created successfully."));

                // Randomly charge clients
                ChargeRandomFees();
            }
        }
    }

    // move on to next phase, allow 3 seconds incase the masternode wants to send us anything else
    if ((state == POOL_STATUS_TRANSMISSION && fMasterNode) || (state == POOL_STATUS_SIGNING && completedTransaction)) {
        if (fDebug) LogPrintf("CDarkSendPool::Check() -- COMPLETED -- RESETTING \n");
        SetNull(true);
        UnlockCoins();
        if (fMasterNode) RelayDarkSendStatus(darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_RESET);
        pwalletMain->Lock();
    }

    // reset if we're here for 10 seconds
    if ((state == POOL_STATUS_ERROR || state == POOL_STATUS_SUCCESS) && GetTimeMillis() - lastTimeChanged >= 10000) {
        if (fDebug) LogPrintf("CDarkSendPool::Check() -- RESETTING MESSAGE \n");
        SetNull(true);
        if (fMasterNode) RelayDarkSendStatus(sessionID, GetState(), GetEntriesCount(), MASTERNODE_RESET);
        UnlockCoins();
    }
}

//
// Charge clients a fee if they're abusive
//
// Why bother? Darksend uses collateral to ensure abuse to the process is kept to a minimum.
// The submission and signing stages in darksend are completely separate. In the cases where
// a client submits a transaction then refused to sign, there must be a cost. Otherwise they
// would be able to do this over and over again and bring the mixing to a hault.
//
// How does this work? Messages to masternodes come in via "dsi", these require a valid collateral
// transaction for the client to be able to enter the pool. This transaction is kept by the masternode
// until the transaction is either complete or fails.
//
void CDarkSendPool::ChargeFees() {
    if (fMasterNode) {
        //we don't need to charge collateral for every offence.
        int offences = 0;
        int r = rand() % 100;
        if (r > 33) return;

        if (state == POOL_STATUS_ACCEPTING_ENTRIES) {
            for (const CTransaction& txCollateral : vecSessionCollateral) {
                bool found = false;
                for (const CDarkSendEntry& v : entries) {
                    if (v.collateral == txCollateral) {
                        found = true;
                    }
                }

                // This queue entry didn't send us the promised transaction
                if (!found) {
                    LogPrintf("CDarkSendPool::ChargeFees -- found uncooperative node (didn't send transaction). Found offence.\n");
                    offences++;
                }
            }
        }

        if (state == POOL_STATUS_SIGNING) {
            // who didn't sign?
            for (const CDarkSendEntry v : entries) {
                for (const CDarkSendEntryVin s : v.sev) {
                    if (!s.isSigSet) {
                        LogPrintf("CDarkSendPool::ChargeFees -- found uncooperative node (didn't sign). Found offence\n");
                        offences++;
                    }
                }
            }
        }

        r = rand() % 100;
        int target = 0;

        //mostly offending?
        if (offences >= POOL_MAX_TRANSACTIONS - 1 && r > 33) return;

        //everyone is an offender? That's not right
        if (offences >= POOL_MAX_TRANSACTIONS) return;

        //charge one of the offenders randomly
        if (offences > 1) target = 50;

        //pick random client to charge
        r = rand() % 100;

        if (state == POOL_STATUS_ACCEPTING_ENTRIES) {
            for (const CTransaction& txCollateral : vecSessionCollateral) {
                bool found = false;
                for (const CDarkSendEntry& v : entries) {
                    if (v.collateral == txCollateral) {
                        found = true;
                    }
                }

                // This queue entry didn't send us the promised transaction
                if (!found && r > target) {
                    LogPrintf("CDarkSendPool::ChargeFees -- found uncooperative node (didn't send transaction). charging fees.\n");

                    CWalletTx wtxCollateral = CWalletTx(pwalletMain, txCollateral);

                    // Broadcast
                    if (!wtxCollateral.AcceptToMemoryPool(true)) {
                        // This must not fail. The transaction has already been signed and recorded.
                        LogPrintf("CDarkSendPool::ChargeFees() : Error: Transaction not valid");
                    }
                    wtxCollateral.RelayWalletTransaction();
                    return;
                }
            }
        }

        if (state == POOL_STATUS_SIGNING) {
            // who didn't sign?
            for (const CDarkSendEntry v : entries) {
                for (const CDarkSendEntryVin s : v.sev) {
                    if (!s.isSigSet && r > target) {
                        LogPrintf("CDarkSendPool::ChargeFees -- found uncooperative node (didn't sign). charging fees.\n");

                        CWalletTx wtxCollateral = CWalletTx(pwalletMain, v.collateral);

                        // Broadcast
                        if (!wtxCollateral.AcceptToMemoryPool(true)) {
                            // This must not fail. The transaction has already been signed and recorded.
                            LogPrintf("CDarkSendPool::ChargeFees() : Error: Transaction not valid");
                        }
                        wtxCollateral.RelayWalletTransaction();
                        return;
                    }
                }
            }
        }
    }
}

// charge the collateral randomly
//  - Darksend is completely free, to pay miners we randomly pay the collateral of users.
void CDarkSendPool::ChargeRandomFees() {
    if (fMasterNode) {
        int i = 0;

        for (const CTransaction& txCollateral : vecSessionCollateral) {
            int r = rand() % 1000;

            /*
                Collateral Fee Charges:

                Being that DarkSend has "no fees" we need to have some kind of cost associated
                with using it to stop abuse. Otherwise it could serve as an attack vector and
                allow endless transaction that would bloat Lux and make it unusable. To
                stop these kinds of attacks 1 in 50 successful transactions are charged. This
                adds up to a cost of 0.002Lux per transaction on average.
            */
            if (r <= 20) {
                LogPrintf("CDarkSendPool::ChargeRandomFees -- charging random fees. %u\n", i);

                CWalletTx wtxCollateral = CWalletTx(pwalletMain, txCollateral);

                // Broadcast
                if (!wtxCollateral.AcceptToMemoryPool(true)) {
                    // This must not fail. The transaction has already been signed and recorded.
                    LogPrintf("CDarkSendPool::ChargeRandomFees() : Error: Transaction not valid");
                }
                wtxCollateral.RelayWalletTransaction();
            }
        }
    }
}

//
// Check for various timeouts (queue objects, darksend, etc)
//
void CDarkSendPool::CheckTimeout() {
    if (!fEnableDarksend && !fMasterNode) return;

    // catching hanging sessions
    if (!fMasterNode) {
        if (state == POOL_STATUS_TRANSMISSION) {
            if (fDebug) LogPrintf("CDarkSendPool::CheckTimeout() -- Session complete -- Running Check()\n");
            Check();
        }
    }

    // check darksend queue objects for timeouts
    int c = 0;
    vector<CDarksendQueue>::iterator it;
    for (it = vecDarksendQueue.begin(); it < vecDarksendQueue.end(); it++) {
        if ((*it).IsExpired()) {
            if (fDebug) LogPrintf("CDarkSendPool::CheckTimeout() : Removing expired queue entry - %d\n", c);
            vecDarksendQueue.erase(it);
            break;
        }
        c++;
    }

    /* Check to see if we're ready for submissions from clients */
    if (state == POOL_STATUS_QUEUE && sessionUsers == GetMaxPoolTransactions()) {
        CDarksendQueue dsq;
        dsq.nDenom = sessionDenom;
        dsq.vin = activeMasternode.vin;
        dsq.time = GetTime();
        dsq.ready = true;
        dsq.Sign();
        dsq.Relay();

        UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);
    }

    int addLagTime = 0;
    if (!fMasterNode) addLagTime = 5000; //if we're the client, give the server a few extra seconds before resetting.

    if (state == POOL_STATUS_ACCEPTING_ENTRIES || state == POOL_STATUS_QUEUE) {
        c = 0;

        // if it's a masternode, the entries are stored in "entries", otherwise they're stored in myEntries
        std::vector<CDarkSendEntry>* vec = &myEntries;
        if (fMasterNode) vec = &entries;

        // check for a timeout and reset if needed
        vector<CDarkSendEntry>::iterator it2;
        for (it2 = vec->begin(); it2 < vec->end(); it2++) {
            if ((*it2).IsExpired()) {
                if (fDebug) LogPrintf("CDarkSendPool::CheckTimeout() : Removing expired entry - %d\n", c);
                vec->erase(it2);
                if (entries.size() == 0 && myEntries.size() == 0) {
                    SetNull(true);
                    UnlockCoins();
                }
                if (fMasterNode) {
                    RelayDarkSendStatus(darkSendPool.sessionID, darkSendPool.GetState(), darkSendPool.GetEntriesCount(), MASTERNODE_RESET);
                }
                break;
            }
            c++;
        }

        if (GetTimeMillis() - lastTimeChanged >= (DARKSEND_QUEUE_TIMEOUT * 1000) + addLagTime) {
            lastTimeChanged = GetTimeMillis();

            ChargeFees();
            // reset session information for the queue query stage (before entering a masternode, clients will send a queue request to make sure they're compatible denomination wise)
            sessionUsers = 0;
            sessionDenom = 0;
            sessionFoundMasternode = false;
            vecSessionCollateral.clear();

            UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);
        }
    } else if (GetTimeMillis() - lastTimeChanged >= (DARKSEND_QUEUE_TIMEOUT * 1000) + addLagTime) {
        if (fDebug) LogPrintf("CDarkSendPool::CheckTimeout() -- Session timed out (30s) -- resetting\n");
        SetNull();
        UnlockCoins();

        UpdateState(POOL_STATUS_ERROR);
        lastMessage = _("Session timed out (30 seconds), please resubmit.");
    }

    if (state == POOL_STATUS_SIGNING && GetTimeMillis() - lastTimeChanged >= (DARKSEND_SIGNING_TIMEOUT * 1000) + addLagTime) {
        if (fDebug) LogPrintf("CDarkSendPool::CheckTimeout() -- Session timed out -- restting\n");
        ChargeFees();
        SetNull();
        UnlockCoins();
        //add my transactions to the new session

        UpdateState(POOL_STATUS_ERROR);
        lastMessage = _("Signing timed out, please resubmit.");
    }
}

// check to see if the signature is valid
bool CDarkSendPool::SignatureValid(const CScript& newSig, const CTxIn& newVin) {
    CMutableTransaction txNew;
    txNew.vin.clear();
    txNew.vout.clear();

    int found = -1;
    CScript sigPubKey = CScript();
    unsigned int i = 0;

    for (CDarkSendEntry e : entries) {
        for (const CTxOut out : e.vout)
            txNew.vout.push_back(out);

        for (const CDarkSendEntryVin s : e.sev) {
            txNew.vin.push_back(s.vin);

            if (s.vin == newVin) {
                found = i;
                sigPubKey = s.vin.prevPubKey;
            }
            i++;
        }
    }

    if (found >= 0) { //might have to do this one input at a time?
        int n = found;
        txNew.vin[n].scriptSig = newSig;
        if (fDebug) LogPrintf("CDarkSendPool::SignatureValid() - Sign with sig %s\n", newSig.ToString().substr(0, 24).c_str());

        //TODO: !IMPORTANT Script verifying may not work with witness scripts for dark send TXs, should be thoroughly tested

        CTransaction txPrev;
        uint256 prevBlockHash;
        //Find previous transaction with the same output as txNew input
        if (!GetTransaction(txNew.vin[n].prevout.hash, txPrev, Params().GetConsensus(), prevBlockHash)) {
            if (fDebug) LogPrintf("CDarkSendPool::SignatureValid() - Signing - Failed to get previous transaction %u\n", n);
            return false;
        }

        const CAmount& amount = txPrev.vout[txNew.vin[n].prevout.n].nValue;
        CScriptWitness* witness;
        int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC;
        //If transaction contains witness, then witness script should be verified
        if (n < (int) txNew.wit.vtxinwit.size()) {
            witness = &txNew.wit.vtxinwit[n].scriptWitness;
            flags |= SCRIPT_VERIFY_WITNESS;
        } else {
            witness = nullptr;
        }

        if (!VerifyScript(txNew.vin[n].scriptSig, sigPubKey, witness, flags, MutableTransactionSignatureChecker(&txNew, n, amount))) {
            if (fDebug) LogPrintf("CDarkSendPool::SignatureValid() - Signing - Error signing input %u\n", n);
            return false;
        }
    }

    if (fDebug) LogPrintf("CDarkSendPool::SignatureValid() - Signing - Succesfully signed input\n");
    return true;
}

// check to make sure the collateral provided by the client is valid
bool CDarkSendPool::IsCollateralValid(const CTransaction& txCollateral) {
    if (txCollateral.vout.size() < 1) return false;
    if (txCollateral.nLockTime != 0) return false;

    int64_t nValueIn = 0;
    int64_t nValueOut = 0;
    bool missingTx = false;

    for (const CTxOut o : txCollateral.vout) {
        nValueOut += o.nValue;

        if (!o.scriptPubKey.IsNormalPaymentScript()) {
            LogPrintf ("CDarkSendPool::IsCollateralValid - Invalid Script %s\n", txCollateral.ToString().c_str());
            return false;
        }
    }

    for (const CTxIn i : txCollateral.vin) {
        CTransaction tx2;
        uint256 hash;
        if (GetTransaction(i.prevout.hash, tx2, Params().GetConsensus(), hash)) {
            if (tx2.vout.size() > i.prevout.n) {
                nValueIn += tx2.vout[i.prevout.n].nValue;
            }
        } else {
            missingTx = true;
        }
    }

    if (missingTx) {
        if (fDebug) LogPrintf ("CDarkSendPool::IsCollateralValid - Unknown inputs in collateral transaction - %s\n", txCollateral.ToString().c_str());
        return false;
    }

    //collateral transactions are required to pay out DARKSEND_COLLATERAL as a fee to the miners
    if (nValueIn - nValueOut < DARKSEND_COLLATERAL) {
        if (fDebug) LogPrintf ("CDarkSendPool::IsCollateralValid - did not include enough fees in transaction %d\n%s\n", nValueOut - nValueIn, txCollateral.ToString().c_str());
        return false;
    }

    if (fDebug) LogPrintf("CDarkSendPool::IsCollateralValid %s\n", txCollateral.ToString().c_str());

    CValidationState state;
    bool* pfMissingInputs = nullptr;
    if (!AcceptableInputs(mempool, state, txCollateral, false, pfMissingInputs)) {
        if (fDebug) LogPrintf ("CDarkSendPool::IsCollateralValid - didn't pass IsAcceptable\n");
        return false;
    }

    return true;
}


//
// Add a clients transaction to the pool
//
bool CDarkSendPool::AddEntry(const std::vector <CTxIn>& newInput, const int64_t& nAmount, const CTransaction& txCollateral, const std::vector <CTxOut>& newOutput, std::string& error) {
    if (!fMasterNode) return false;

    for (CTxIn in : newInput) {
        if (in.prevout.IsNull() || nAmount < 0) {
            if (fDebug) LogPrintf ("CDarkSendPool::AddEntry - input not valid!\n");
            error = _("Input is not valid.");
            sessionUsers--;
            return false;
        }
    }

    if (!IsCollateralValid(txCollateral)) {
        if (fDebug) LogPrintf ("CDarkSendPool::AddEntry - collateral not valid!\n");
        error = _("Collateral is not valid.");
        sessionUsers--;
        return false;
    }

    if ((int) entries.size() >= GetMaxPoolTransactions()) {
        if (fDebug) LogPrintf ("CDarkSendPool::AddEntry - entries is full!\n");
        error = _("Entries are full.");
        sessionUsers--;
        return false;
    }

    for (CTxIn in : newInput) {
        if (fDebug) LogPrintf("looking for vin -- %s\n", in.ToString().c_str());
        for (const CDarkSendEntry v : entries) {
            for (const CDarkSendEntryVin s : v.sev) {
                if (s.vin == in) {
                    if (fDebug) LogPrintf ("CDarkSendPool::AddEntry - found in vin\n");
                    error = _("Already have that input.");
                    sessionUsers--;
                    return false;
                }
            }
        }
    }

    if (state == POOL_STATUS_ACCEPTING_ENTRIES) {
        CDarkSendEntry v;
        v.Add(newInput, nAmount, txCollateral, newOutput);
        entries.push_back(v);

        if (fDebug) LogPrintf("CDarkSendPool::AddEntry -- adding %s\n", newInput[0].ToString().c_str());
        error = "";

        return true;
    }

    if (fDebug) LogPrintf ("CDarkSendPool::AddEntry - can't accept new entry, wrong state!\n");
    error = _("Wrong state.");
    sessionUsers--;
    return false;
}

bool CDarkSendPool::AddScriptSig(const CTxIn newVin) {
    if (fDebug) LogPrintf("CDarkSendPool::AddScriptSig -- new sig  %s\n", newVin.scriptSig.ToString().substr(0, 24).c_str());

    for (const CDarkSendEntry v : entries) {
        for (const CDarkSendEntryVin s : v.sev) {
            if (s.vin.scriptSig == newVin.scriptSig) {
                LogPrintf("CDarkSendPool::AddScriptSig - already exists \n");
                return false;
            }
        }
    }

    if (!SignatureValid(newVin.scriptSig, newVin)) {
        if (fDebug) LogPrintf("CDarkSendPool::AddScriptSig - Invalid Sig\n");
        return false;
    }

    if (fDebug) LogPrintf("CDarkSendPool::AddScriptSig -- sig %s\n", newVin.ToString().c_str());

    if (state == POOL_STATUS_SIGNING) {
        for (CTxIn & vin : finalTransaction.vin) {
            if (newVin.prevout == vin.prevout && vin.nSequence == newVin.nSequence) {
                vin.scriptSig = newVin.scriptSig;
                vin.prevPubKey = newVin.prevPubKey;
                if (fDebug) LogPrintf("CDarkSendPool::AddScriptSig -- adding to finalTransaction  %s\n", newVin.scriptSig.ToString().substr(0, 24).c_str());
            }
        }
        for (unsigned int i = 0; i < entries.size(); i++) {
            if (entries[i].AddSig(newVin)) {
                if (fDebug) LogPrintf("CDarkSendPool::AddScriptSig -- adding  %s\n", newVin.scriptSig.ToString().substr(0, 24).c_str());
                return true;
            }
        }
    }

    LogPrintf("CDarkSendPool::AddScriptSig -- Couldn't set sig!\n");
    return false;
}

// check to make sure everything is signed
bool CDarkSendPool::SignaturesComplete() {
    for (const CDarkSendEntry v : entries) {
        for (const CDarkSendEntryVin s : v.sev) {
            if (!s.isSigSet) return false;
        }
    }
    return true;
}

//
// Execute a darksend denomination via a masternode.
// This is only ran from clients
//
void CDarkSendPool::SendDarksendDenominate(std::vector <CTxIn>& vin, std::vector <CTxOut>& vout, int64_t amount) {
    if (darkSendPool.txCollateral == CMutableTransaction()) {
        LogPrintf ("CDarksendPool:SendDarksendDenominate() - Darksend collateral not set");
        return;
    }

    // lock the funds we're going to use
    for (CTxIn in : txCollateral.vin)
        lockedCoins.push_back(in);

    for (CTxIn in : vin)
        lockedCoins.push_back(in);

    // we should already be connected to a masternode
    if (!sessionFoundMasternode) {
        LogPrintf("CDarkSendPool::SendDarksendDenominate() - No masternode has been selected yet.\n");
        UnlockCoins();
        SetNull(true);
        return;
    }

    if (!CheckDiskSpace())
        return;

    if (fMasterNode) {
        LogPrintf("CDarkSendPool::SendDarksendDenominate() - DarkSend from a masternode is not supported currently.\n");
        return;
    }

    UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);

    LogPrintf("CDarkSendPool::SendDarksendDenominate() - Added transaction to pool.\n");

    ClearLastMessage();

    //check it against the memory pool to make sure it's valid
    {
        int64_t nValueOut = 0;

        CValidationState state;
        CMutableTransaction tx;

        for (const CTxOut o : vout) {
            nValueOut += o.nValue;
            tx.vout.push_back(o);
        }

        for (const CTxIn i : vin) {
            tx.vin.push_back(i);

            if (fDebug) LogPrintf("dsi -- tx in %s\n", i.ToString().c_str());
        }

        bool* pfMissingInputs = nullptr;
        if (!AcceptableInputs(mempool, state, CTransaction(tx), false, pfMissingInputs)) {
            LogPrintf("dsi -- transaction not valid! %s \n", tx.ToString().c_str());
            return;
        }
    }

    // store our entry for later use
    CDarkSendEntry e;
    e.Add(vin, amount, txCollateral, vout);
    myEntries.push_back(e);

    // relay our entry to the master node
    RelayDarkSendIn(vin, amount, txCollateral, vout);
    Check();
}

// Incoming message from masternode updating the progress of darksend
//    newAccepted:  -1 mean's it'n not a "transaction accepted/not accepted" message, just a standard update
//                  0 means transaction was not accepted
//                  1 means transaction was accepted

bool CDarkSendPool::StatusUpdate(int newState, int newEntriesCount, int newAccepted, std::string& error, int newSessionID) {
    if (fMasterNode) return false;
    if (state == POOL_STATUS_ERROR || state == POOL_STATUS_SUCCESS) return false;

    UpdateState(newState);
    entriesCount = newEntriesCount;

    if (error.size() > 0) strAutoDenomResult = _("Masternode:") + " " + error;

    if (newAccepted != -1) {
        lastEntryAccepted = newAccepted;
        countEntriesAccepted += newAccepted;
        if (newAccepted == 0) {
            UpdateState(POOL_STATUS_ERROR);
            lastMessage = error;
        }

        if (newAccepted == 1) {
            sessionID = newSessionID;
            LogPrintf("CDarkSendPool::StatusUpdate - set sessionID to %d\n", sessionID);
            sessionFoundMasternode = true;
        }
    }

    if (newState == POOL_STATUS_ACCEPTING_ENTRIES) {
        if (newAccepted == 1) {
            LogPrintf("CDarkSendPool::StatusUpdate - entry accepted! \n");
            sessionFoundMasternode = true;
            //wait for other users. Masternode will report when ready
            UpdateState(POOL_STATUS_QUEUE);
        } else if (newAccepted == 0 && sessionID == 0 && !sessionFoundMasternode) {
            LogPrintf("CDarkSendPool::StatusUpdate - entry not accepted by masternode \n");
            UnlockCoins();
            UpdateState(POOL_STATUS_ACCEPTING_ENTRIES);
            DoAutomaticDenominating(); //try another masternode
        }
        if (sessionFoundMasternode) return true;
    }

    return true;
}

//
// After we receive the finalized transaction from the masternode, we must
// check it to make sure it's what we want, then sign it if we agree.
// If we refuse to sign, it's possible we'll be charged collateral
//
bool CDarkSendPool::SignFinalTransaction(const CTransaction& finalTransactionNew, CNode* node) {
    if (fDebug) LogPrintf("CDarkSendPool::AddFinalTransaction - Got Finalized Transaction\n");

    if (!finalTransaction.vin.empty()) {
        LogPrintf("CDarkSendPool::AddFinalTransaction - Rejected Final Transaction!\n");
        return false;
    }

    finalTransaction = finalTransactionNew;
    LogPrintf("CDarkSendPool::SignFinalTransaction %s\n", finalTransaction.ToString().c_str());

    vector <CTxIn> sigs;

    //make sure my inputs/outputs are present, otherwise refuse to sign
    for (const CDarkSendEntry e : myEntries) {
        for (const CDarkSendEntryVin s : e.sev) {
            /* Sign my transaction and all outputs */
            int mine = -1;
            CScript prevPubKey = CScript();
            CTxIn vin = CTxIn();

            for (unsigned int i = 0; i < finalTransaction.vin.size(); i++) {
                if (finalTransaction.vin[i] == s.vin) {
                    mine = i;
                    prevPubKey = s.vin.prevPubKey;
                    vin = s.vin;
                }
            }

            if (mine >= 0) { //might have to do this one input at a time?
                int foundOutputs = 0;
                int64_t nValue1 = 0;
                int64_t nValue2 = 0;

                for (unsigned int i = 0; i < finalTransaction.vout.size(); i++) {
                    for (const CTxOut o : e.vout) {
                        if (finalTransaction.vout[i] == o) {
                            foundOutputs++;
                            nValue1 += finalTransaction.vout[i].nValue;
                        }
                    }
                }

                for (const CTxOut o : e.vout)
                    nValue2 += o.nValue;

                int targetOuputs = e.vout.size();
                if (foundOutputs < targetOuputs || nValue1 != nValue2) {
                    // in this case, something went wrong and we'll refuse to sign. It's possible we'll be charged collateral. But that's
                    // better then signing if the transaction doesn't look like what we wanted.
                    LogPrintf("CDarkSendPool::Sign - My entries are not correct! Refusing to sign. %d entries %d target. \n", foundOutputs, targetOuputs);
                    return false;
                }

                if (fDebug) LogPrintf("CDarkSendPool::Sign - Signing my input %i\n", mine);
                const CAmount& amount = finalTransaction.vout[finalTransaction.vin[mine].prevout.n].nValue;
                SignatureData sigdata;
                bool isSigned = ProduceSignature(MutableTransactionSignatureCreator(pwalletMain, &finalTransaction, (unsigned int) mine, amount, SIGHASH_ALL), prevPubKey, sigdata); // changes scriptSig
                if (!isSigned) {
                    if (fDebug) LogPrintf("CDarkSendPool::Sign - Unable to sign my own transaction! \n");
                    // not sure what to do here, it will timeout...?
                }
                UpdateTransaction(finalTransaction, (unsigned int) mine, sigdata);

                sigs.push_back(finalTransaction.vin[mine]);
                if (fDebug) LogPrintf(" -- dss %d %d %s\n", mine, (int) sigs.size(), finalTransaction.vin[mine].scriptSig.ToString().c_str());
            }

        }

        if (fDebug) LogPrintf("CDarkSendPool::Sign - txNew:\n%s", finalTransaction.ToString().c_str());
    }

    // push all of our signatures to the masternode
    if (sigs.size() > 0 && node != NULL)
        node->PushMessage("dss", sigs);

    return true;
}

void CDarkSendPool::NewBlock() {
    if (fDebug) LogPrintf("CDarkSendPool::NewBlock \n");

    //we we're processing lots of blocks, we'll just leave
    if (GetTime() - lastNewBlock < 10) return;
    lastNewBlock = GetTime();

    darkSendPool.CheckTimeout();

    if (!fEnableDarksend) return;

    if (!fMasterNode) {
        //denominate all non-denominated inputs every 25 minutes.
        if (chainActive.Height() % 10 == 0) UnlockCoins();
        ProcessMasternodeConnections();
    }
}

// Darksend transaction was completed (failed or successed)
void CDarkSendPool::CompletedTransaction(bool error, std::string lastMessageNew) {
    if (fMasterNode) return;

    if (error) {
        LogPrintf("CompletedTransaction -- error \n");
        UpdateState(POOL_STATUS_ERROR);
    } else {
        LogPrintf("CompletedTransaction -- success \n");
        UpdateState(POOL_STATUS_SUCCESS);

        myEntries.clear();

        // To avoid race conditions, we'll only let DS run once per block
        cachedLastSuccess = chainActive.Height();
    }
    lastMessage = lastMessageNew;

    completedTransaction = true;
    Check();
    UnlockCoins();
}

void CDarkSendPool::ClearLastMessage() {
    lastMessage = "";
}

//
// Passively run Darksend in the background to anonymize funds based on the given configuration.
//
// This does NOT run by default for daemons, only for QT.
//
bool CDarkSendPool::DoAutomaticDenominating(bool fDryRun, bool ready) {
    LOCK(cs_darksend);

    if (IsInitialBlockDownload()) return false;

    if (fMasterNode) return false;
    if (state == POOL_STATUS_ERROR || state == POOL_STATUS_SUCCESS) return false;

    if (chainActive.Height() - cachedLastSuccess < minBlockSpacing) {
        LogPrintf("CDarkSendPool::DoAutomaticDenominating - Last successful darksend action was too recent\n");
        strAutoDenomResult = _("Last successful darksend action was too recent.");
        return false;
    }
    if (!fEnableDarksend) {
        //if(fDebug) LogPrintf("CDarkSendPool::DoAutomaticDenominating - Darksend is disabled\n");
        strAutoDenomResult = _("Darksend is disabled.");
        return false;
    }

    if (!fDryRun && pwalletMain->IsLocked()) {
        strAutoDenomResult = _("Wallet is locked.");
        return false;
    }

    if (darkSendPool.GetState() != POOL_STATUS_ERROR && darkSendPool.GetState() != POOL_STATUS_SUCCESS) {
        if (darkSendPool.GetMyTransactionCount() > 0) {
            return true;
        }
    }

    if (vecMasternodes.size() == 0) {
        if (fDebug) LogPrintf("CDarkSendPool::DoAutomaticDenominating - No masternodes detected\n");
        strAutoDenomResult = _("No masternodes detected.");
        return false;
    }

    // ** find the coins we'll use
    std::vector<CTxIn> vCoins;
    std::vector<CTxIn> vCoinsResult;
    std::vector<COutput> vCoins2;
    int64_t nValueMin = CENT;
    int64_t nValueIn = 0;

    // should not be less than fees in DARKSEND_FEE + few (lets say 5) smallest denoms
    int64_t nLowestDenom = DARKSEND_FEE + darkSendDenominations[darkSendDenominations.size() - 1] * 5;

    // if there are no DS collateral inputs yet
    if (!pwalletMain->HasCollateralInputs())
        // should have some additional amount for them
        nLowestDenom += (DARKSEND_COLLATERAL * 4) + DARKSEND_FEE * 2;

    int64_t nBalanceNeedsAnonymized = nAnonymizeLuxAmount * COIN - pwalletMain->GetAnonymizedBalance();

    // if balanceNeedsAnonymized is more than pool max, take the pool max
    if (nBalanceNeedsAnonymized > DARKSEND_POOL_MAX) nBalanceNeedsAnonymized = DARKSEND_POOL_MAX;

    // if balanceNeedsAnonymized is more than non-anonymized, take non-anonymized
    int64_t nBalanceNotYetAnonymized = pwalletMain->GetBalance() - pwalletMain->GetAnonymizedBalance();
    if (nBalanceNeedsAnonymized > nBalanceNotYetAnonymized) nBalanceNeedsAnonymized = nBalanceNotYetAnonymized;

    if (nBalanceNeedsAnonymized < nLowestDenom) {
//        if(nBalanceNeedsAnonymized > nValueMin)
//            nBalanceNeedsAnonymized = nLowestDenom;
//        else
//        {
        LogPrintf("DoAutomaticDenominating : No funds detected in need of denominating \n");
        strAutoDenomResult = _("No funds detected in need of denominating.");
        return false;
//        }
    }

    if (fDebug) LogPrintf("DoAutomaticDenominating : nLowestDenom=%d, nBalanceNeedsAnonymized=%d\n", nLowestDenom, nBalanceNeedsAnonymized);

    // select coins that should be given to the pool
    if (!pwalletMain->SelectCoinsDark(nValueMin, nBalanceNeedsAnonymized, vCoins, nValueIn, 0, nDarksendRounds)) {
        nValueIn = 0;
        vCoins.clear();

        if (pwalletMain->SelectCoinsDark(nValueMin, 9999999 * COIN, vCoins, nValueIn, -2, 0)) {
            if (!fDryRun) return CreateDenominated(nBalanceNeedsAnonymized);
            return true;
        } else {
            LogPrintf("DoAutomaticDenominating : Can't denominate - no compatible inputs left\n");
            strAutoDenomResult = _("Can't denominate: no compatible inputs left.");
            return false;
        }

    }

    //check to see if we have the collateral sized inputs, it requires these
    if (!pwalletMain->HasCollateralInputs()) {
        if (!fDryRun) MakeCollateralAmounts();
        return true;
    }

    if (fDryRun) return true;

    // initial phase, find a masternode
    if (!sessionFoundMasternode) {
        int nUseQueue = rand() % 100;

        sessionTotalValue = pwalletMain->GetTotalValue(vCoins);

        //randomize the amounts we mix
        if (sessionTotalValue > nBalanceNeedsAnonymized) sessionTotalValue = nBalanceNeedsAnonymized;

        double fLuxSubmitted = (sessionTotalValue / CENT);
        LogPrintf("Submitting Darksend for %f Lux CENT - sessionTotalValue %d\n", fLuxSubmitted, sessionTotalValue);

        if (pwalletMain->GetDenominatedBalance(true, true) > 0) { //get denominated unconfirmed inputs
            LogPrintf("DoAutomaticDenominating -- Found unconfirmed denominated outputs, will wait till they confirm to continue.\n");
            strAutoDenomResult = _("Found unconfirmed denominated outputs, will wait till they confirm to continue.");
            return false;
        }

        //don't use the queues all of the time for mixing
        if (nUseQueue > 33) {

            // Look through the queues and see if anything matches
            for (CDarksendQueue & dsq : vecDarksendQueue) {
                CService addr;
                if (dsq.time == 0) continue;

                if (!dsq.GetAddress(addr)) continue;
                if (dsq.IsExpired()) continue;

                int protocolVersion;
                if (!dsq.GetProtocolVersion(protocolVersion)) continue;
                if (protocolVersion < MIN_PEER_PROTO_VERSION) continue;

                //non-denom's are incompatible
                if ((dsq.nDenom & (1 << 4))) continue;

                //don't reuse masternodes
                for (CTxIn usedVin : vecMasternodesUsed) {
                    if (dsq.vin == usedVin) {
                        continue;
                    }
                }

                // Try to match their denominations if possible
                if (!pwalletMain->SelectCoinsByDenominations(dsq.nDenom, nValueMin, nBalanceNeedsAnonymized, vCoins, vCoins2, nValueIn, 0, nDarksendRounds)) {
                    LogPrintf("DoAutomaticDenominating - Couldn't match denominations %d\n", dsq.nDenom);
                    continue;
                }

                // connect to masternode and submit the queue request
                if (ConnectNode(CAddress(addr, NODE_NETWORK), NULL, true)) {
                    submittedToMasternode = addr;

                    LOCK(cs_vNodes);
                    for (CNode * pnode : vNodes) {
                        if (!pnode) continue;
                        if ((CNetAddr) pnode->addr != (CNetAddr) submittedToMasternode) continue;

                        std::string strReason;
                        if (txCollateral == CMutableTransaction()) {
                            if (!pwalletMain->CreateCollateralTransaction(txCollateral, strReason)) {
                                LogPrintf("DoAutomaticDenominating -- dsa error:%s\n", strReason.c_str());
                                return false;
                            }
                        }

                        vecMasternodesUsed.push_back(dsq.vin);
                        sessionDenom = dsq.nDenom;

                        pnode->PushMessage("dsa", sessionDenom, txCollateral);
                        LogPrintf("DoAutomaticDenominating --- connected (from queue), sending dsa for %d %d - %s\n", sessionDenom, GetDenominationsByAmount(sessionTotalValue),
                                  pnode->addr.ToString().c_str());
                        strAutoDenomResult = "";
                        return true;
                    }
                } else {
                    LogPrintf("DoAutomaticDenominating --- error connecting \n");
                    strAutoDenomResult = _("Error connecting to masternode.");
                    return DoAutomaticDenominating();
                }

                dsq.time = 0; //remove node
            }
        }

        //shuffle masternodes around before we try to connect
        std::random_shuffle(vecMasternodes.begin(), vecMasternodes.end());
        int i = 0;

        // otherwise, try one randomly
        while (i < 10) {
            //don't reuse masternodes
            for (CTxIn usedVin : vecMasternodesUsed) {
                if (vecMasternodes[i].vin == usedVin) {
                    i++;
                    continue;
                }
            }
            if (vecMasternodes[i].protocolVersion < MIN_PEER_PROTO_VERSION) {
                i++;
                continue;
            }

            if (vecMasternodes[i].nLastDsq != 0 &&
                vecMasternodes[i].nLastDsq + CountMasternodesAboveProtocol(darkSendPool.MIN_PEER_PROTO_VERSION) / 5 > darkSendPool.nDsqCount) {
                i++;
                continue;
            }

            lastTimeChanged = GetTimeMillis();
            LogPrintf("DoAutomaticDenominating -- attempt %d connection to masternode %s\n", i, vecMasternodes[i].addr.ToString().c_str());
            if (ConnectNode(CAddress(vecMasternodes[i].addr, NODE_NETWORK), NULL, true)) {
                submittedToMasternode = vecMasternodes[i].addr;

                LOCK(cs_vNodes);
                for (CNode * pnode : vNodes) {
                    if (!pnode) continue;
                    if ((CNetAddr) pnode->addr != (CNetAddr) vecMasternodes[i].addr) continue;

                    std::string strReason;
                    if (txCollateral == CMutableTransaction()) {
                        if (!pwalletMain->CreateCollateralTransaction(txCollateral, strReason)) {
                            LogPrintf("DoAutomaticDenominating -- create collateral error:%s\n", strReason.c_str());
                            return false;
                        }
                    }

                    vecMasternodesUsed.push_back(vecMasternodes[i].vin);

                    std::vector<int64_t> vecAmounts;
                    pwalletMain->ConvertList(vCoins, vecAmounts);
                    sessionDenom = GetDenominationsByAmounts(vecAmounts);

                    pnode->PushMessage("dsa", sessionDenom, txCollateral);
                    LogPrintf("DoAutomaticDenominating --- connected, sending dsa for %d - denom %d\n", sessionDenom, GetDenominationsByAmount(sessionTotalValue));
                    strAutoDenomResult = "";
                    return true;
                }
            } else {
                i++;
                continue;
            }
        }

        strAutoDenomResult = _("No compatible masternode found.");
        return false;
    }

    strAutoDenomResult = "";
    if (!ready) return true;

    if (sessionDenom == 0) return true;

    return false;
}


bool CDarkSendPool::PrepareDarksendDenominate() {
    // Submit transaction to the pool if we get here, use sessionDenom so we use the same amount of money
    std::string strError = pwalletMain->PrepareDarksendDenominate(0, nDarksendRounds);
    LogPrintf("DoAutomaticDenominating : Running darksend denominate. Return '%s'\n", strError.c_str());

    if (strError == "") return true;

    strAutoDenomResult = strError;
    LogPrintf("DoAutomaticDenominating : Error running denominate, %s\n", strError.c_str());
    return false;
}

bool CDarkSendPool::SendRandomPaymentToSelf() {
    int64_t nBalance = pwalletMain->GetBalance();
    int64_t nPayment = (nBalance * 0.35) + (rand() % nBalance);

    if (nPayment > nBalance) nPayment = nBalance - (0.1 * COIN);

    // make our change address
    CReserveKey reservekey(pwalletMain);

    CScript scriptDenom;
    CPubKey vchPubKey;
    assert(reservekey.GetReservedKey(vchPubKey, false)); // should never fail, as we just unlocked
    scriptDenom = GetScriptForDestination(vchPubKey.GetID());

    CWalletTx wtx;
    int64_t nFeeRet = 0;
    std::string strFail = "";
    std::vector<CRecipient> vecSend;

    // ****** Add fees ************ /
    CRecipient recipient = {scriptDenom, nPayment, false};
    vecSend.push_back(recipient);

    CCoinControl* coinControl = NULL;
    int nChangePos = -1;
    //int32_t nChangePos;
    //bool success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePos, strFail, coinControl, ONLY_DENOMINATED);
    bool success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePos, strFail, coinControl, ONLY_DENOMINATED);
    if (!success) {
        LogPrintf("SendRandomPaymentToSelf: Error - %s\n", strFail.c_str());
        return false;
    }

    pwalletMain->CommitTransaction(wtx, reservekey);

    LogPrintf("SendRandomPaymentToSelf Success: tx %s\n", wtx.GetHash().GetHex().c_str());

    return true;
}

// Split up large inputs or create fee sized inputs
bool CDarkSendPool::MakeCollateralAmounts() {
    // make our change address
    CReserveKey reservekey(pwalletMain);

    CScript scriptChange;
    CPubKey vchPubKey;
    assert(reservekey.GetReservedKey(vchPubKey, false)); // should never fail, as we just unlocked
    scriptChange = GetScriptForDestination(vchPubKey.GetID());

    CWalletTx wtx;
    int64_t nFeeRet = 0;
    std::string strFail = "";
    std::vector<CRecipient> vecSend;

    CRecipient recipient = {scriptChange, (DARKSEND_COLLATERAL * 2) + DARKSEND_FEE, false};
    vecSend.push_back(recipient);

    CCoinControl* coinControl = NULL;
    int nChangePos = -1;
    //int32_t nChangePos;
    // try to use non-denominated and not mn-like funds
    //bool success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePos, strFail, coinControl, ONLY_NONDENOMINATED_NOTMN);
    bool success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePos, strFail, coinControl, ONLY_NONDENOMINATED_NOTMN);
    if (!success) {
        // if we failed (most likeky not enough funds), try to use denominated instead -
        // MN-like funds should not be touched in any case and we can't mix denominated without collaterals anyway
        //success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePos, strFail, coinControl, ONLY_DENOMINATED);
        success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePos, strFail, coinControl, ONLY_DENOMINATED);
        if (!success) {
            LogPrintf("MakeCollateralAmounts: Error - %s\n", strFail.c_str());
            return false;
        }
    }

    // use the same cachedLastSuccess as for DS mixinx to prevent race
    if (pwalletMain->CommitTransaction(wtx, reservekey))
        cachedLastSuccess = chainActive.Height();

    LogPrintf("MakeCollateralAmounts Success: tx %s\n", wtx.GetHash().GetHex().c_str());

    return true;
}

// Create denominations
bool CDarkSendPool::CreateDenominated(int64_t nTotalValue) {
    // make our change address
    CReserveKey reservekey(pwalletMain);

    CScript scriptChange;
    CPubKey vchPubKey;
    assert(reservekey.GetReservedKey(vchPubKey, false)); // should never fail, as we just unlocked
    scriptChange = GetScriptForDestination(vchPubKey.GetID());

    CWalletTx wtx;
    int64_t nFeeRet = 0;
    std::string strFail = "";
    std::vector<CRecipient> vecSend;
    int64_t nValueLeft = nTotalValue;

    // ****** Add collateral outputs ************ /
    if (!pwalletMain->HasCollateralInputs()) {
        CRecipient recipient = {scriptChange, (DARKSEND_COLLATERAL * 2) + DARKSEND_FEE, false};
        vecSend.push_back(recipient);
    }

    // ****** Add denoms ************ /
    for (int64_t v : reverse_iterate(darkSendDenominations)) {
        int nOutputs = 0;

        // add each output up to 10 times until it can't be added again
        while (nValueLeft - v >= DARKSEND_FEE && nOutputs <= 10) {
            CScript scriptChange;
            CPubKey vchPubKey;
            //use a unique change address
            assert(reservekey.GetReservedKey(vchPubKey, false)); // should never fail, as we just unlocked
            scriptChange = GetScriptForDestination(vchPubKey.GetID());
            reservekey.KeepKey();

            CRecipient recipient = {scriptChange, v, false};
            vecSend.push_back(recipient);

            //increment outputs and subtract denomination amount
            nOutputs++;
            nValueLeft -= v;
            LogPrintf("CreateDenominated1 %d\n", nValueLeft);
        }

        if (nValueLeft == 0) break;
    }
    LogPrintf("CreateDenominated2 %d\n", nValueLeft);

    // if we have anything left over, it will be automatically send back as change - there is no need to send it manually

    CCoinControl* coinControl = NULL;
    int nChangePos = -1;
    //bool success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePos, strFail, coinControl, ONLY_NONDENOMINATED_NOTMN);
    bool success = pwalletMain->CreateTransaction(vecSend, wtx, reservekey, nFeeRet, nChangePos, strFail, coinControl, ONLY_NONDENOMINATED_NOTMN);
    if (!success) {
        LogPrintf("CreateDenominated: Error - %s\n", strFail.c_str());
        return false;
    }

    // use the same cachedLastSuccess as for DS mixinx to prevent race
    if (pwalletMain->CommitTransaction(wtx, reservekey))
        cachedLastSuccess = chainActive.Height();

    LogPrintf("CreateDenominated Success: tx %s\n", wtx.GetHash().GetHex().c_str());

    return true;
}

bool CDarkSendPool::IsCompatibleWithEntries(std::vector <CTxOut> vout) {
    for (const CDarkSendEntry v : entries) {
        LogPrintf(" IsCompatibleWithEntries %d %d\n", GetDenominations(vout), GetDenominations(v.vout));
        if (GetDenominations(vout) != GetDenominations(v.vout)) return false;
    }

    return true;
}

bool CDarkSendPool::IsCompatibleWithSession(int64_t nDenom, CTransaction txCollateral, std::string& strReason) {
    LogPrintf("CDarkSendPool::IsCompatibleWithSession - sessionDenom %d sessionUsers %d\n", sessionDenom, sessionUsers);

    if (!unitTest && !IsCollateralValid(txCollateral)) {
        if (fDebug) LogPrintf ("CDarkSendPool::IsCompatibleWithSession - collateral not valid!\n");
        strReason = _("Collateral not valid.");
        return false;
    }

    if (sessionUsers < 0) sessionUsers = 0;

    if (sessionUsers == 0) {
        sessionDenom = nDenom;
        sessionUsers++;
        lastTimeChanged = GetTimeMillis();
        entries.clear();

        if (!unitTest) {
            //broadcast that I'm accepting entries, only if it's the first entry though
            CDarksendQueue dsq;
            dsq.nDenom = nDenom;
            dsq.vin = activeMasternode.vin;
            dsq.time = GetTime();
            dsq.Sign();
            dsq.Relay();
        }

        UpdateState(POOL_STATUS_QUEUE);
        vecSessionCollateral.push_back(txCollateral);
        return true;
    }

    if ((state != POOL_STATUS_ACCEPTING_ENTRIES && state != POOL_STATUS_QUEUE) || sessionUsers >= GetMaxPoolTransactions()) {
        if ((state != POOL_STATUS_ACCEPTING_ENTRIES && state != POOL_STATUS_QUEUE)) strReason = _("Incompatible mode.");
        if (sessionUsers >= GetMaxPoolTransactions()) strReason = _("Masternode queue is full.");
        LogPrintf("CDarkSendPool::IsCompatibleWithSession - incompatible mode, return false %d %d\n", state != POOL_STATUS_ACCEPTING_ENTRIES, sessionUsers >= GetMaxPoolTransactions());
        return false;
    }

    if (nDenom != sessionDenom) {
        strReason = _("No matching denominations found for mixing.");
        return false;
    }

    LogPrintf("CDarkSendPool::IsCompatibleWithSession - compatible\n");

    sessionUsers++;
    lastTimeChanged = GetTimeMillis();
    vecSessionCollateral.push_back(txCollateral);

    return true;
}

//create a nice string to show the denominations
void CDarkSendPool::GetDenominationsToString(int nDenom, std::string& strDenom) {
    // Function returns as follows:
    //
    // bit 0 - 100Lux+1 ( bit on if present )
    // bit 1 - 10Lux+1
    // bit 2 - 1Lux+1
    // bit 3 - .1Lux+1
    // bit 3 - non-denom


    strDenom = "";

    if (nDenom & (1 << 0)) {
        if (strDenom.size() > 0) strDenom += "+";
        strDenom += "100";
    }

    if (nDenom & (1 << 1)) {
        if (strDenom.size() > 0) strDenom += "+";
        strDenom += "10";
    }

    if (nDenom & (1 << 2)) {
        if (strDenom.size() > 0) strDenom += "+";
        strDenom += "1";
    }

    if (nDenom & (1 << 3)) {
        if (strDenom.size() > 0) strDenom += "+";
        strDenom += "0.1";
    }
}

// return a bitshifted integer representing the denominations in this list
int CDarkSendPool::GetDenominations(const std::vector<CTxOut>& vout) {
    std::vector< pair<int64_t, int> > denomUsed;

    // make a list of denominations, with zero uses
    for (int64_t d : darkSendDenominations)
        denomUsed.push_back(make_pair(d, 0));

    // look for denominations and update uses to 1
    for (CTxOut out : vout) {
        bool found = false;
        for (PAIRTYPE(int64_t, int) &s : denomUsed) {
            if (out.nValue == s.first) {
                s.second = 1;
                found = true;
            }
        }
        if (!found) return 0;
    }

    int denom = 0;
    int c = 0;
    // if the denomination is used, shift the bit on.
    // then move to the next
    for (PAIRTYPE(int64_t, int) &s : denomUsed)
        denom |= s.second << c++;

    // Function returns as follows:
    //
    // bit 0 - 100Lux+1 ( bit on if present )
    // bit 1 - 10Lux+1
    // bit 2 - 1Lux+1
    // bit 3 - .1Lux+1

    return denom;
}


int CDarkSendPool::GetDenominationsByAmounts(std::vector <int64_t>& vecAmount) {
    CScript e = CScript();
    std::vector<CTxOut> vout1;

    // Make outputs by looping through denominations, from small to large
    for (int64_t v : reverse_iterate(vecAmount)) {
        int nOutputs = 0;

        CTxOut o(v, e);
        vout1.push_back(o);
        nOutputs++;
    }

    return GetDenominations(vout1);
}

int CDarkSendPool::GetDenominationsByAmount(int64_t nAmount, int nDenomTarget) {
    CScript e = CScript();
    int64_t nValueLeft = nAmount;

    std::vector<CTxOut> vout1;

    // Make outputs by looping through denominations, from small to large
    for (int64_t v : reverse_iterate(darkSendDenominations)) {
        if (nDenomTarget != 0) {
            bool fAccepted = false;
            if ((nDenomTarget & (1 << 0)) && v == ((100000 * COIN) + 100000000)) { fAccepted = true; }
            else if ((nDenomTarget & (1 << 1)) && v == ((10000 * COIN) + 10000000)) { fAccepted = true; }
            else if ((nDenomTarget & (1 << 2)) && v == ((1000 * COIN) + 1000000)) { fAccepted = true; }
            else if ((nDenomTarget & (1 << 3)) && v == ((100 * COIN) + 100000)) { fAccepted = true; }
            else if ((nDenomTarget & (1 << 4)) && v == ((10 * COIN) + 10000)) { fAccepted = true; }
            else if ((nDenomTarget & (1 << 5)) && v == ((1 * COIN) + 1000)) { fAccepted = true; }
            else if ((nDenomTarget & (1 << 6)) && v == ((.1 * COIN) + 100)) { fAccepted = true; }
            if (!fAccepted) continue;
        }

        int nOutputs = 0;

        // add each output up to 10 times until it can't be added again
        while (nValueLeft - v >= 0 && nOutputs <= 10) {
            CTxOut o(v, e);
            vout1.push_back(o);
            nValueLeft -= v;
            nOutputs++;
        }
        LogPrintf("GetDenominationsByAmount --- %d nOutputs %d\n", v, nOutputs);
    }

    //add non-denom left overs as change
    if (nValueLeft > 0) {
        CTxOut o(nValueLeft, e);
        vout1.push_back(o);
    }

    return GetDenominations(vout1);
}

bool CDarkSendSigner::IsVinAssociatedWithPubkey(CTxIn& vin, CPubKey& pubkey) {
    CScript payee2;
    payee2 = GetScriptForDestination(pubkey.GetID());

    CTransaction txVin;
    uint256 hash;
    if (GetTransaction(vin.prevout.hash, txVin, Params().GetConsensus(), hash)) {
        CTxOut out = txVin.vout[vin.prevout.n];
        if ((out.nValue == GetMNCollateral(chainActive.Height()) * COIN)
            && (out.scriptPubKey == payee2)) {
            return true;
        }
    }

    return false;
}

bool CDarkSendSigner::SetKey(std::string strSecret, std::string& errorMessage, CKey& key, CPubKey& pubkey) {
    CBitcoinSecret vchSecret;
    bool fGood = vchSecret.SetString(strSecret);

    if (!fGood) {
        errorMessage = _("Invalid private key.");
        return false;
    }

    key = vchSecret.GetKey();
    pubkey = key.GetPubKey();

    return true;
}

bool CDarkSendSigner::SignMessage(std::string strMessage, std::string& errorMessage, vector<unsigned char>& vchSig, CKey key) {
    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    if (!key.SignCompact(ss.GetHash(), vchSig)) {
        errorMessage = _("Signing failed.");
        return false;
    }

    return true;
}

bool CDarkSendSigner::VerifyMessage(CPubKey pubkey, vector<unsigned char>& vchSig, std::string strMessage, std::string& errorMessage) {
    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    CPubKey pubkey2;
    if (!pubkey2.RecoverCompact(ss.GetHash(), vchSig)) {
        errorMessage = _("Error recovering public key.");
        return false;
    }

    if (fDebug && pubkey2.GetID() != pubkey.GetID())
        LogPrintf("CDarkSendSigner::VerifyMessage -- keys don't match: %s %s", pubkey2.GetID().ToString(), pubkey.GetID().ToString());

    return (pubkey2.GetID() == pubkey.GetID());
}

bool CDarksendQueue::Sign() {
    if (!fMasterNode) return false;

    std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nDenom) + boost::lexical_cast<std::string>(time) + boost::lexical_cast<std::string>(ready);

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if (!darkSendSigner.SetKey(strMasterNodePrivKey, errorMessage, key2, pubkey2)) {
        LogPrintf("CDarksendQueue():Relay - ERROR: Invalid masternodeprivkey: '%s'\n", errorMessage.c_str());
        return false;
    }

    if (!darkSendSigner.SignMessage(strMessage, errorMessage, vchSig, key2)) {
        LogPrintf("CDarksendQueue():Relay - Sign message failed");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubkey2, vchSig, strMessage, errorMessage)) {
        LogPrintf("CDarksendQueue():Relay - Verify message failed");
        return false;
    }

    return true;
}

bool CDarksendQueue::Relay() {

    LOCK(cs_vNodes);
    for (CNode * pnode : vNodes) {
        if (pnode) {
            // always relay to everyone
            pnode->PushMessage("dsq", (*this));
        }
    }

    return true;
}

bool CDarksendQueue::CheckSignature() {
    for (CMasterNode & mn : vecMasternodes) {

        if (mn.vin == vin) {
            std::string strMessage = vin.ToString() + boost::lexical_cast<std::string>(nDenom) + boost::lexical_cast<std::string>(time) + boost::lexical_cast<std::string>(ready);

            std::string errorMessage = "";
            if (!darkSendSigner.VerifyMessage(mn.pubkey2, vchSig, strMessage, errorMessage)) {
                return error("CDarksendQueue::CheckSignature() - Got bad masternode address signature %s \n", vin.ToString().c_str());
            }

            return true;
        }
    }

    return false;
}


//TODO: Rename/move to core
void ThreadCheckDarkSendPool() {
    // Make this thread recognisable as the wallet flushing thread
    RenameThread("Lux-darksend");

    unsigned int c = 0;
    std::string errorMessage;

    while (!ShutdownRequested()) {
        c++;

        MilliSleep(2500);
        //LogPrintf("ThreadCheckDarkSendPool::check timeout\n");
        darkSendPool.CheckTimeout();

        if (c % 60 == 0) {
        {
                LOCK(cs_masternodes);
                vector<CMasterNode>::iterator it = vecMasternodes.begin();
                //check them separately
                while (it != vecMasternodes.end()) {
                    (*it).Check();
                    if ((*it).enabled == 4 || (*it).enabled == 3) {
                        LogPrintf("Removing inactive masternode %s\n", (*it).addr.ToString().c_str());
                        it = vecMasternodes.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            masternodePayments.CleanPaymentList();
            CleanTransactionLocksList();
        }

        //try to sync the masternode list and payment list every 5 seconds from at least 5 nodes
        if (c % 5 == 0 && RequestedMasterNodeList < 5) {
            bool fIsInitialDownload = IsInitialBlockDownload();
            if (!fIsInitialDownload) {
                LOCK(cs_vNodes);
                for (CNode * pnode : vNodes) {
                    if (pnode && pnode->nVersion >= darkSendPool.MIN_PEER_PROTO_VERSION) {

                        //keep track of who we've asked for the list
                        if (pnode->HasFulfilledRequest("mnsync")) continue;
                        pnode->FulfilledRequest("mnsync");

                        LogPrintf("Successfully synced, asking for Masternode list and payment list\n");

                        pnode->PushMessage("dseg", CTxIn()); //request full mn list
                        pnode->PushMessage("mnget"); //sync payees
                        pnode->PushMessage("getsporks"); //get current network sporks
                        RequestedMasterNodeList++;
                    }
                }
            }
        }

        if (c % MASTERNODE_PING_SECONDS == 0) {
            activeMasternode.ManageStatus();
        }

        if (c % 60 == 0) {
            //if we've used 1/5 of the masternode list, then clear the list.
            if ((int) vecMasternodesUsed.size() > (int) vecMasternodes.size() / 5)
                vecMasternodesUsed.clear();
        }

        //auto denom every 1 minutes (liquidity provides try less often)
        if (c % 60 * (nLiquidityProvider + 1) == 0) {
            if (nLiquidityProvider != 0) {
                int nRand = rand() % (101 + nLiquidityProvider);
                //about 1/100 chance of starting over after 4 rounds.
                if (nRand == 50 + nLiquidityProvider && pwalletMain->GetAverageAnonymizedRounds() > 8) {
                    darkSendPool.SendRandomPaymentToSelf();
                    int nLeftToAnon = ((pwalletMain->GetBalance() - pwalletMain->GetAnonymizedBalance()) / COIN) - 3;
                    if (nLeftToAnon > 999) nLeftToAnon = 999;
                    nAnonymizeLuxAmount = (rand() % nLeftToAnon) + 3;
                } else {
                    darkSendPool.DoAutomaticDenominating();
                }
            } else {
                darkSendPool.DoAutomaticDenominating();
            }
        }
    }
}
