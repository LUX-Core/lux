// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uint256.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "base58.h"
#include "protocol.h"
#include "instantx.h"
#include "masternode.h"
#include "activemasternode.h"
#include "darksend.h"
#include "spork.h"
#include "consensus/validation.h"
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

std::map<uint256, CTransaction> mapTxLockReq;
std::map<uint256, CTransaction> mapTxLockReqRejected;
std::map<uint256, CConsensusVote> mapTxLockVote;
std::map<uint256, CTransactionLock> mapTxLocks;
std::map<COutPoint, uint256> mapLockedInputs;
std::map<uint256, int64_t> mapUnknownVotes; //track votes with no tx for DOS
int nCompleteTXLocks = 0;

//txlock - Locks transaction
//
//step 1.) Broadcast intention to lock transaction inputs, "txlreg", CTransaction
//step 2.) Top 10 masternodes, open connect to top 1 masternode. Send "txvote", CTransaction, Signature, Approve
//step 3.) Top 1 masternode, waits for 10 messages. Upon success, sends "txlock'

void ProcessInstantX(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isInstantXCommand) {
    if (!IsSporkActive(SPORK_7_INSTANTX)) return;
    if (IsInitialBlockDownload()) return;

    if (strCommand == "txlreq") {
        isInstantXCommand = true;

        //LogPrintf("ProcessMessageInstantX::txlreq\n");
        CDataStream vMsg(vRecv);
        CTransaction tx;
        vRecv >> tx;

        CInv inv(MSG_TXLOCK_REQUEST, tx.GetHash());
        pfrom->AddInventoryKnown(inv);

        if (mapTxLockReq.count(tx.GetHash()) || mapTxLockReqRejected.count(tx.GetHash())) {
            return;
        }

        if (!IsIXTXValid(tx)) {
            return;
        }

        for (const CTxOut o : tx.vout) {
            if (!o.scriptPubKey.IsNormalPaymentScript()) {
                printf("ProcessInstantX::txlreq - Invalid Script %s\n", tx.ToString().c_str());
                return;
            }
        }

        int nBlockHeight = CreateNewLock(tx);

        bool fMissingInputs = false;
        CValidationState state;


        if (AcceptToMemoryPool(mempool, state, tx, true, &fMissingInputs)) {
            vector<CInv> vInv;
            vInv.push_back(inv);
            LOCK(cs_vNodes);
            for (CNode* pnode : vNodes)
                if (pnode)
                    pnode->PushMessage("inv", vInv);

            DoConsensusVote(tx, nBlockHeight);

            mapTxLockReq.insert(make_pair(tx.GetHash(), tx));

            LogPrintf("ProcessInstantX::txlreq - Transaction Lock Request: %s %s : accepted %s\n",
                pfrom->addr.ToString().c_str(), pfrom->cleanSubVer.c_str(),
                tx.GetHash().ToString().c_str()
            );

            return;

        } else {
            mapTxLockReqRejected.insert(make_pair(tx.GetHash(), tx));

            // can we get the conflicting transaction as proof?

            LogPrintf("ProcessInstantX::txlreq - Transaction Lock Request: %s %s : rejected %s\n",
                pfrom->addr.ToString().c_str(), pfrom->cleanSubVer.c_str(),
                tx.GetHash().ToString().c_str()
            );

            for (const CTxIn& in : tx.vin) {
                if (!mapLockedInputs.count(in.prevout)) {
                    mapLockedInputs.insert(make_pair(in.prevout, tx.GetHash()));
                }
            }

            // resolve conflicts
            std::map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(tx.GetHash());
            if (i != mapTxLocks.end()) {
                //we only care if we have a complete tx lock
                if ((*i).second.CountSignatures() >= INSTANTX_SIGNATURES_REQUIRED) {
                    if (!CheckForConflictingLocks(tx)) {
                        LogPrintf("ProcessInstantX::txlreq - Found Existing Complete IX Lock\n");

                        CValidationState state;
                        // DisconnectBlockAndInputs(state, tx);
                        mapTxLockReq.insert(make_pair(tx.GetHash(), tx));
                    }
                }
            }

            return;
        }
    }

    else if (strCommand == "txlvote") { //InstantX Lock Consensus Votes
        isInstantXCommand = true;

        CConsensusVote ctx;
        vRecv >> ctx;

        CInv inv(MSG_TXLOCK_VOTE, ctx.GetHash());
        pfrom->AddInventoryKnown(inv);

        if (mapTxLockVote.count(ctx.GetHash())) {
            return;
        }

        mapTxLockVote.insert(make_pair(ctx.GetHash(), ctx));

        if (ProcessConsensusVote(ctx)) {
            //Spam/Dos protection
            /*
                Masternodes will sometimes propagate votes before the transaction is known to the client.
                This tracks those messages and allows it at the same rate of the rest of the network, if
                a peer violates it, it will simply be ignored
            */
            if (!mapTxLockReq.count(ctx.txHash) && !mapTxLockReqRejected.count(ctx.txHash)) {
                if (!mapUnknownVotes.count(ctx.vinMasternode.prevout.hash)) {
                    mapUnknownVotes[ctx.vinMasternode.prevout.hash] = GetTime() + (60 * 10);
                }

                if (mapUnknownVotes[ctx.vinMasternode.prevout.hash] > GetTime() &&
                    mapUnknownVotes[ctx.vinMasternode.prevout.hash] - GetAverageVoteTime() > 60 * 10) {
                    LogPrintf("ProcessInstantX::txlreq - masternode is spamming transaction votes: %s %s\n",
                              ctx.vinMasternode.ToString().c_str(),
                              ctx.txHash.ToString().c_str()
                    );
                    return;
                } else {
                    mapUnknownVotes[ctx.vinMasternode.prevout.hash] = GetTime() + (60 * 10);
                }
            }
            vector<CInv> vInv;
            vInv.push_back(inv);
            LOCK(cs_vNodes);
            for (CNode * pnode : vNodes)
                if (pnode)
                    pnode->PushMessage("inv", vInv);

        }

        return;
    }
}

bool IsIXTXValid(const CTransaction& txCollateral) {
    if (txCollateral.vout.size() < 1) return false;
    if (txCollateral.nLockTime != 0) return false;

    int64_t nValueIn = 0;
    int64_t nValueOut = 0;
    bool missingTx = false;

    for (const CTxOut o : txCollateral.vout)
        nValueOut += o.nValue;

    for (const CTxIn i : txCollateral.vin) {
        CTransaction tx2;
        uint256 hash;
        //if(GetTransaction(i.prevout.hash, tx2, hash, true)){
        if (GetTransaction(i.prevout.hash, tx2, Params().GetConsensus(), hash)) {
            if (tx2.vout.size() > i.prevout.n) {
                nValueIn += tx2.vout[i.prevout.n].nValue;
            }
        } else {
            missingTx = true;
        }
    }

    if (nValueOut > GetSporkValue(SPORK_2_MAX_VALUE) * COIN) {
        if (fDebug) LogPrintf ("IsIXTXValid - Transaction value too high - %s\n", txCollateral.ToString().c_str());
        return false;
    }

    if (missingTx) {
        if (fDebug) LogPrintf ("IsIXTXValid - Unknown inputs in IX transaction - %s\n", txCollateral.ToString().c_str());
        /*
            This happens sometimes for an unknown reason, so we'll return that it's a valid transaction.
            If someone submits an invalid transaction it will be rejected by the network anyway and this isn't
            very common, but we don't want to block IX just because the client can't figure out the fee.
        */
        return true;
    }

    if (nValueIn - nValueOut < COIN * 0.01) {
        if (fDebug) LogPrintf ("IsIXTXValid - did not include enough fees in transaction %d\n%s\n", nValueOut - nValueIn, txCollateral.ToString().c_str());
        return false;
    }

    return true;
}

int64_t CreateNewLock(CTransaction tx) {

    int64_t nTxAge = 0;
    for (CTxIn i : reverse_iterate(tx.vin)) {
        nTxAge = GetInputAge(i);
        if (nTxAge < 6) {
            LogPrintf("CreateNewLock - Transaction not found / too new: %d / %s\n", nTxAge, tx.GetHash().ToString().c_str());
            return 0;
        }
    }

    /*
        Use a blockheight newer than the input.
        This prevents attackers from using transaction mallibility to predict which masternodes
        they'll use.
    */
    int nBlockHeight = (chainActive.Height() - nTxAge) + 4;

    if (!mapTxLocks.count(tx.GetHash())) {
        LogPrintf("CreateNewLock - New Transaction Lock %s !\n", tx.GetHash().ToString().c_str());

        CTransactionLock newLock;
        newLock.nBlockHeight = nBlockHeight;
        newLock.nExpiration = GetTime() + (60 * 60); //locks expire after 15 minutes (6 confirmations)
        newLock.nTimeout = GetTime() + (60 * 5);
        newLock.txHash = tx.GetHash();
        mapTxLocks.insert(make_pair(tx.GetHash(), newLock));
    } else {
        mapTxLocks[tx.GetHash()].nBlockHeight = nBlockHeight;
        if (fDebug) LogPrintf("CreateNewLock - Transaction Lock Exists %s !\n", tx.GetHash().ToString().c_str());
    }

    return nBlockHeight;
}

// check if we need to vote on this transaction
void DoConsensusVote(CTransaction& tx, int64_t nBlockHeight) {
    if (!fMasterNode) return;

    int n = GetMasternodeRank(activeMasternode.vin, nBlockHeight, MIN_PROTO_VERSION);

    if (n == -1) {
        if (fDebug) LogPrintf("InstantX::DoConsensusVote - Unknown Masternode\n");
        return;
    }

    if (n > INSTANTX_SIGNATURES_TOTAL) {
        if (fDebug) LogPrintf("InstantX::DoConsensusVote - Masternode not in the top %d (%d)\n", INSTANTX_SIGNATURES_TOTAL, n);
        return;
    }
    /*
        nBlockHeight calculated from the transaction is the authoritive source
    */

    if (fDebug) LogPrintf("InstantX::DoConsensusVote - In the top %d (%d)\n", INSTANTX_SIGNATURES_TOTAL, n);

    CConsensusVote ctx;
    ctx.vinMasternode = activeMasternode.vin;
    ctx.txHash = tx.GetHash();
    ctx.nBlockHeight = nBlockHeight;
    if (!ctx.Sign()) {
        LogPrintf("InstantX::DoConsensusVote - Failed to sign consensus vote\n");
        return;
    }
    if (!ctx.SignatureValid()) {
        LogPrintf("InstantX::DoConsensusVote - Signature invalid\n");
        return;
    }

    mapTxLockVote[ctx.GetHash()] = ctx;

    CInv inv(MSG_TXLOCK_VOTE, ctx.GetHash());

    vector<CInv> vInv;
    vInv.push_back(inv);
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (pnode)
            pnode->PushMessage("inv", vInv);
    }

}

//received a consensus vote
bool ProcessConsensusVote(CConsensusVote& ctx) {
    int n = GetMasternodeRank(ctx.vinMasternode, ctx.nBlockHeight, MIN_PROTO_VERSION);

    int x = GetMasternodeByVin(ctx.vinMasternode);
    if (x != -1) {
        if (fDebug) LogPrintf("InstantX::ProcessConsensusVote - Masternode ADDR %s %d\n", vecMasternodes[x].addr.ToString().c_str(), n);
    }

    if (n == -1) {
        //can be caused by past versions trying to vote with an invalid protocol
        if (fDebug) LogPrintf("InstantX::ProcessConsensusVote - Unknown Masternode\n");
        return false;
    }

    if (n > INSTANTX_SIGNATURES_TOTAL) {
        if (fDebug) LogPrintf("InstantX::ProcessConsensusVote - Masternode not in the top %d (%d) - %s\n", INSTANTX_SIGNATURES_TOTAL, n, ctx.GetHash().ToString().c_str());
        return false;
    }

    if (!ctx.SignatureValid()) {
        LogPrintf("InstantX::ProcessConsensusVote - Signature invalid\n");
        //don't ban, it could just be a non-synced masternode
        return false;
    }

    if (!mapTxLocks.count(ctx.txHash)) {
        LogPrintf("InstantX::ProcessConsensusVote - New Transaction Lock %s !\n", ctx.txHash.ToString().c_str());

        CTransactionLock newLock;
        newLock.nBlockHeight = 0;
        newLock.nExpiration = GetTime() + (60 * 60);
        newLock.nTimeout = GetTime() + (60 * 5);
        newLock.txHash = ctx.txHash;
        mapTxLocks.insert(make_pair(ctx.txHash, newLock));
    } else {
        if (fDebug) LogPrintf("InstantX::ProcessConsensusVote - Transaction Lock Exists %s !\n", ctx.txHash.ToString().c_str());
    }

    //compile consessus vote
    std::map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(ctx.txHash);
    if (i != mapTxLocks.end()) {
        (*i).second.AddSignature(ctx);

        if (fDebug) LogPrintf("InstantX::ProcessConsensusVote - Transaction Lock Votes %d - %s !\n", (*i).second.CountSignatures(), ctx.GetHash().ToString().c_str());

        if ((*i).second.CountSignatures() >= INSTANTX_SIGNATURES_REQUIRED) {
            if (fDebug) LogPrintf("InstantX::ProcessConsensusVote - Transaction Lock Is Complete %s !\n", (*i).second.GetHash().ToString().c_str());

            CTransaction& tx = mapTxLockReq[ctx.txHash];
            if (!CheckForConflictingLocks(tx)) {

#ifdef ENABLE_WALLET
                if(pwalletMain){
                    pwalletMain->UpdatedTransaction((*i).second.txHash);
                        nCompleteTXLocks++;

                }
#endif

                if (mapTxLockReq.count(ctx.txHash)) {
                    for (const CTxIn& in : tx.vin){
                        if (!mapLockedInputs.count(in.prevout)) {
                            mapLockedInputs.insert(make_pair(in.prevout, ctx.txHash));
                        }
                    }
                }

                // resolve conflicts

                //if this tx lock was rejected, we need to remove the conflicting blocks
                if (mapTxLockReqRejected.count((*i).second.txHash)) {
                    CValidationState state;
                    // DisconnectBlockAndInputs(state, mapTxLockReqRejected[(*i).second.txHash]);
                }
            }
        }
        return true;
    }


    return false;
}

bool CheckForConflictingLocks(CTransaction& tx) {
    /*
        It's possible (very unlikely though) to get 2 conflicting transaction locks approved by the network.
        In that case, they will cancel each other out.

        Blocks could have been rejected during this time, which is OK. After they cancel out, the client will
        rescan the blocks and find they're acceptable and then take the chain with the most work.
    */
    for (const CTxIn& in : tx.vin) {
        if (mapLockedInputs.count(in.prevout)) {
            if (mapLockedInputs[in.prevout] != tx.GetHash()) {
                LogPrintf("InstantX::CheckForConflictingLocks - found two complete conflicting locks - removing both. %s %s", tx.GetHash().ToString().c_str(),
                          mapLockedInputs[in.prevout].ToString().c_str());
                if (mapTxLocks.count(tx.GetHash())) mapTxLocks[tx.GetHash()].nExpiration = GetTime();
                if (mapTxLocks.count(mapLockedInputs[in.prevout])) mapTxLocks[mapLockedInputs[in.prevout]].nExpiration = GetTime();
                return true;
            }
        }
    }

    return false;
}

int64_t GetAverageVoteTime() {
    std::map<uint256, int64_t>::iterator it = mapUnknownVotes.begin();
    int64_t total = 0;
    int64_t count = 0;

    while (it != mapUnknownVotes.end()) {
        total += it->second;
        count++;
        it++;
    }

    return total / count;
}

void CleanTransactionLocksList() {
    if (chainActive.Tip() == NULL) return;

    std::map<uint256, CTransactionLock>::iterator it = mapTxLocks.begin();

    while (it != mapTxLocks.end()) {
        if (GetTime() > it->second.nExpiration) { //keep them for an hour
            LogPrintf("Removing old transaction lock %s\n", it->second.txHash.ToString().c_str());

            if (mapTxLockReq.count(it->second.txHash)) {
                CTransaction& tx = mapTxLockReq[it->second.txHash];

                for (const CTxIn& in : tx.vin)
                    mapLockedInputs.erase(in.prevout);

                mapTxLockReq.erase(it->second.txHash);
                mapTxLockReqRejected.erase(it->second.txHash);

                for (CConsensusVote& v : it->second.vecConsensusVotes)
                    mapTxLockVote.erase(v.GetHash());
            }

            mapTxLocks.erase(it++);
        } else {
            it++;
        }
    }

}

uint256 CConsensusVote::GetHash() const {
    return vinMasternode.prevout.hash + vinMasternode.prevout.n + txHash;
}


bool CConsensusVote::SignatureValid() {
    std::string errorMessage;
    std::string strMessage = txHash.ToString().c_str() + boost::lexical_cast<std::string>(nBlockHeight);

    int n = GetMasternodeByVin(vinMasternode);

    if (n == -1) {
        LogPrintf("InstantX::CConsensusVote::SignatureValid() - Unknown Masternode\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(vecMasternodes[n].pubkey2, vchMasterNodeSignature, strMessage, errorMessage)) {
        LogPrintf("InstantX::CConsensusVote::SignatureValid() - Verify message failed\n");
        return false;
    }

    return true;
}

bool CConsensusVote::Sign() {
    std::string errorMessage;

    CKey key2;
    CPubKey pubkey2;
    std::string strMessage = txHash.ToString().c_str() + boost::lexical_cast<std::string>(nBlockHeight);

    if (!darkSendSigner.SetKey(strMasterNodePrivKey, errorMessage, key2, pubkey2)) {
        LogPrintf("CActiveMasternode::RegisterAsMasterNode() - ERROR: Invalid masternodeprivkey: '%s'\n", errorMessage.c_str());
        return false;
    }

    CScript pubkey;
    pubkey = GetScriptForDestination(pubkey2.GetID());
    CTxDestination address1;
    ExtractDestination(pubkey, address1);

    if (!darkSendSigner.SignMessage(strMessage, errorMessage, vchMasterNodeSignature, key2)) {
        LogPrintf("CActiveMasternode::RegisterAsMasterNode() - Sign message failed");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubkey2, vchMasterNodeSignature, strMessage, errorMessage)) {
        LogPrintf("CActiveMasternode::RegisterAsMasterNode() - Verify message failed");
        return false;
    }

    return true;
}


bool CTransactionLock::SignaturesValid() {

    for (CConsensusVote vote : vecConsensusVotes) {
        int n = GetMasternodeRank(vote.vinMasternode, vote.nBlockHeight, MIN_PROTO_VERSION);

        if (n == -1) {
            LogPrintf("InstantX::DoConsensusVote - Unknown Masternode\n");
            return false;
        }

        if (n > INSTANTX_SIGNATURES_TOTAL) {
            LogPrintf("InstantX::DoConsensusVote - Masternode not in the top %s\n", INSTANTX_SIGNATURES_TOTAL);
            return false;
        }

        if (!vote.SignatureValid()) {
            LogPrintf("InstantX::CTransactionLock::SignaturesValid - Signature not valid\n");
            return false;
        }
    }

    return true;
}

void CTransactionLock::AddSignature(CConsensusVote& cv) {
    vecConsensusVotes.push_back(cv);
}

int CTransactionLock::CountSignatures() {
    /*
        Only count signatures where the BlockHeight matches the transaction's blockheight.
        The votes have no proof it's the correct blockheight
    */

    if (nBlockHeight == 0) return -1;

    int n = 0;
    for (CConsensusVote v : vecConsensusVotes) {
        if (v.nBlockHeight == nBlockHeight) {
            n++;
        }
    }
    return n;
}
