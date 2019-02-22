// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

//#include "bignum.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "util.h"
#include "script/script.h"
#include "consensus/validation.h"
#include "base58.h"
#include "protocol.h"
#include "spork.h"
#include "main.h"
#include <boost/lexical_cast.hpp>

using namespace std;
using namespace boost;

class CSporkMessage;
class CSporkManager;

std::map<uint256, CSporkMessage> mapSporks;
std::map<int, CSporkMessage> mapSporksActive;
CSporkManager sporkManager;

void ProcessSpork(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isSporkCommand) {
    if (strCommand == "spork") {
        isSporkCommand = true;

        CDataStream vMsg(vRecv);
        CSporkMessage spork;
        vRecv >> spork;

        if (chainActive.Tip() == nullptr) return;

        uint256 hash = spork.GetHash();
        // Seach by ID only
        if (mapSporks.count(hash) && mapSporksActive.count(spork.nSporkID)) {
            if (mapSporksActive[spork.nSporkID].nTimeSigned >= spork.nTimeSigned) {
                if (fDebug) LogPrintf("spork - seen %s block %d \n", hash.ToString().c_str(), chainActive.Height());
                return;
            } else {
                if (fDebug) LogPrintf("spork - got updated spork %s block %d \n", hash.ToString().c_str(), chainActive.Height());
            }
        }

        LogPrintf("spork - new %s ID %d Time %d bestHeight %d\n", hash.ToString().c_str(), spork.nSporkID, spork.nValue, chainActive.Height());

        if (!sporkManager.CheckSignature(spork)) {
            LogPrintf("spork - invalid signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapSporks[hash] = spork;
        mapSporksActive[spork.nSporkID] = spork;
        sporkManager.Relay(spork);

        //does a task if needed
        ExecuteSpork(spork.nSporkID, spork.nValue);
    }

    else if (strCommand == "getsporks") {
        isSporkCommand = true;

        std::map<int, CSporkMessage>::iterator it = mapSporksActive.begin();
        while (it != mapSporksActive.end()) {
            pfrom->PushMessage("spork", it->second);
            it++;
        }
    }
}

// grab the spork, otherwise say it's off
bool IsSporkActive(int nSporkID) {
    int64_t r = -1;

    if (mapSporksActive.count(nSporkID)) {
        r = mapSporksActive[nSporkID].nValue;
    } else {
        if (nSporkID == SPORK_1_MASTERNODE_PAYMENTS_ENFORCEMENT) r = SPORK_1_MASTERNODE_PAYMENTS_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_2_MAX_VALUE) r = SPORK_2_MAX_VALUE_DEFAULT;
        if (nSporkID == SPORK_3_REPLAY_BLOCKS) r = SPORK_3_REPLAY_BLOCKS_DEFAULT;
        if (nSporkID == SPORK_5_REPLAY_BLOCKS) r = SPORK_5_REPLAY_BLOCKS_DEFAULT;
        if (nSporkID == SPORK_6_RECONSIDER_BLOCKS) r = SPORK_6_RECONSIDER_BLOCKS_DEFAULT;
        if (nSporkID == SPORK_7_INSTANTX) r = SPORK_7_INSTANTX_DEFAULT;
        if (r == -1 && fDebug) LogPrintf("GetSpork::Unknown Spork %d\n", nSporkID);
    }
    if (r == -1) r = 4070908800; //return 2099-1-1 by default

    return r < GetTime();
}

// grab the value of the spork on the network, or the default
long GetSporkValue(int nSporkID) {
    int r = 0;
    if (mapSporksActive.count(nSporkID)) {
        r = mapSporksActive[nSporkID].nValue;
    } else {
        if (nSporkID == SPORK_1_MASTERNODE_PAYMENTS_ENFORCEMENT) r = SPORK_1_MASTERNODE_PAYMENTS_ENFORCEMENT_DEFAULT;
        if (nSporkID == SPORK_2_MAX_VALUE) r = SPORK_2_MAX_VALUE_DEFAULT;
        if (nSporkID == SPORK_3_REPLAY_BLOCKS) r = SPORK_3_REPLAY_BLOCKS_DEFAULT;
        if (nSporkID == SPORK_5_REPLAY_BLOCKS) r = SPORK_5_REPLAY_BLOCKS_DEFAULT;
        if (nSporkID == SPORK_6_RECONSIDER_BLOCKS) r = SPORK_6_RECONSIDER_BLOCKS_DEFAULT;
        if (nSporkID == SPORK_7_INSTANTX) r = SPORK_7_INSTANTX_DEFAULT;
        if (r == 0 && fDebug) LogPrintf("GetSpork::Unknown Spork %d\n", nSporkID);
    }

    return r;
}

void ExecuteSpork(int nSporkID, int nValue) {

    //replay and process blocks (to sync to the longest chain after disabling sporks)
    if (nSporkID == SPORK_5_REPLAY_BLOCKS) {
        DisconnectBlocksAndReprocess(nValue);
    }
    //correct fork via spork technology
    if (nSporkID == SPORK_6_RECONSIDER_BLOCKS && nValue > 0) {
        LogPrintf("Spork::ExecuteSpork -- Reconsider Last %d Blocks\n", nValue);
        CBlockIndex* pindex = chainActive.Tip();
        int count = 0;

        for (int i = 1; pindex && pindex->nHeight > 0; i++) {
            count++;
            if (count >= nValue) return;

            CValidationState state;
            {
                LOCK(cs_main);

                LogPrintf("Spork::ExecuteSpork -- ReconsiderBlock %s\n", pindex->phashBlock->ToString());
                ReconsiderBlock(state, pindex);
            }

            if (state.IsValid()) {
                ActivateBestChain(state, Params(), nullptr);
            }

            if (pindex->pprev == NULL) {
                assert(pindex);
                break;
            }
            pindex = pindex->pprev;
        }
    }
}

bool CSporkManager::CheckSignature(CSporkMessage& spork) {
    //note: need to investigate why this is failing
    std::string strMessage = boost::lexical_cast<std::string>(spork.nSporkID) + boost::lexical_cast<std::string>(spork.nValue) + boost::lexical_cast<std::string>(spork.nTimeSigned);
    std::string strPubKey = strMainPubKey;
    CPubKey pubkey(ParseHex(strPubKey));

    std::string errorMessage = "";
    if (!darkSendSigner.VerifyMessage(pubkey, spork.vchSig, strMessage, errorMessage)) {
        return false;
    }

    return true;
}

bool CSporkManager::Sign(CSporkMessage& spork) {
    std::string strMessage = boost::lexical_cast<std::string>(spork.nSporkID) + boost::lexical_cast<std::string>(spork.nValue) + boost::lexical_cast<std::string>(spork.nTimeSigned);

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if (!darkSendSigner.SetKey(strMasterPrivKey, errorMessage, key2, pubkey2)) {
        LogPrintf("CMasternodePayments::Sign - ERROR: Invalid masternodeprivkey: '%s'\n", errorMessage.c_str());
        return false;
    }

    if (!darkSendSigner.SignMessage(strMessage, errorMessage, spork.vchSig, key2)) {
        LogPrintf("CMasternodePayments::Sign - Sign message failed");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubkey2, spork.vchSig, strMessage, errorMessage)) {
        LogPrintf("CMasternodePayments::Sign - Verify message failed");
        return false;
    }

    return true;
}

bool CSporkManager::UpdateSpork(int nSporkID, int64_t nValue) {

    CSporkMessage msg;
    msg.nSporkID = nSporkID;
    msg.nValue = nValue;
    msg.nTimeSigned = GetTime();

    if (Sign(msg)) {
        Relay(msg);
        mapSporks[msg.GetHash()] = msg;
        mapSporksActive[nSporkID] = msg;
        return true;
    }

    return false;
}

void CSporkManager::Relay(CSporkMessage& msg) {
    CInv inv(MSG_SPORK, msg.GetHash());

    vector <CInv> vInv;
    vInv.push_back(inv);
    LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (pnode)
            pnode->PushMessage("inv", vInv);
    }
}

bool CSporkManager::SetPrivKey(std::string strPrivKey) {
    CSporkMessage msg;

    // Test signing successful, proceed
    strMasterPrivKey = strPrivKey;

    Sign(msg);

    if (CheckSignature(msg)) {
        LogPrintf("CSporkManager::SetPrivKey - Successfully initialized as spork signer\n");
        return true;
    } else {
        return false;
    }
}

int CSporkManager::GetSporkIDByName(std::string strName) {
    if (strName == "SPORK_1_MASTERNODE_PAYMENTS_ENFORCEMENT") return SPORK_1_MASTERNODE_PAYMENTS_ENFORCEMENT;
    if (strName == "SPORK_2_MAX_VALUE") return SPORK_2_MAX_VALUE;
    if (strName == "SPORK_3_REPLAY_BLOCKS") return SPORK_3_REPLAY_BLOCKS;
    if (strName == "SPORK_5_REPLAY_BLOCKS") return SPORK_5_REPLAY_BLOCKS;
    if (strName == "SPORK_6_RECONSIDER_BLOCKS") return SPORK_6_RECONSIDER_BLOCKS;
    if (strName == "SPORK_7_INSTANTX") return SPORK_7_INSTANTX;
    return -1;
}

std::string CSporkManager::GetSporkNameByID(int id) {
    if (id == SPORK_1_MASTERNODE_PAYMENTS_ENFORCEMENT) return "SPORK_1_MASTERNODE_PAYMENTS_ENFORCEMENT";
    if (id == SPORK_2_MAX_VALUE) return "SPORK_2_MAX_VALUE";
    if (id == SPORK_3_REPLAY_BLOCKS) return "SPORK_3_REPLAY_BLOCKS";
    if (id == SPORK_5_REPLAY_BLOCKS) return "SPORK_5_REPLAY_BLOCKS";
    if (id == SPORK_6_RECONSIDER_BLOCKS) return "SPORK_6_RECONSIDER_BLOCKS";
    if (id == SPORK_7_INSTANTX) return "SPORK_7_INSTANTX";
    return "Unknown";
}
