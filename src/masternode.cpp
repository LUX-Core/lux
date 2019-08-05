// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "masternode.h"
#include "activemasternode.h"
#include "consensus/validation.h"
#include "darksend.h"
#include "primitives/transaction.h"
#include "main.h"
#include "util.h"
#include "addrman.h"
#include <boost/lexical_cast.hpp>
//tt

int CMasterNode::minProtoVersion = MIN_PROTO_VERSION;

CCriticalSection cs_masternodes;

/** The list of active masternodes */
std::vector<CMasterNode> vecMasternodes;
/** Object for who's going to get paid on which blocks */
CMasternodePayments masternodePayments;
// keep track of masternode votes I've seen
map<uint256, CMasternodePaymentWinner> mapSeenMasternodeVotes;
// keep track of the scanning errors I've seen
map<uint256, int> mapSeenMasternodeScanningErrors;
// who's asked for the masternode list and the last time
std::map<CNetAddr, int64_t> askedForMasternodeList;
// which masternodes we've asked for
std::map<COutPoint, int64_t> askedForMasternodeListEntry;
// cache block hashes as we calculate them
std::map<int64_t, uint256> mapCacheBlockHashes;

extern std::string SCVersion;

// manage the masternode connections
void ProcessMasternodeConnections() {
    //LOCK(cs_vNodes);

    for (CNode* pnode : vNodes) {
        if (!pnode) continue;
        //if it's our masternode, let it be
        if (darkSendPool.submittedToMasternode == pnode->addr) continue;

        if (pnode->fDarkSendMaster) {
            LogPrintf("Closing masternode connection %s \n", pnode->addr.ToString().c_str());
            pnode->CloseSocketDisconnect();
        }
    }
}

void ProcessMasternode(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isMasternodeCommand) {

    int nHeight = chainActive.Height() + 1;

    // Do not accept the peers having older versions when the fork happens
    if (nHeight >= nLuxProtocolSwitchHeight)
    {
        SCVersion = WORKING_VERSION;
    }

    // Reject the MN from older version
    if (pfrom && pfrom->strSubVer.compare(SCVersion) < 0)
    {
        LogPrintf("MASTERNODE: %s: Invalid MN %d, wrong wallet version %s\n", __func__, nHeight, pfrom->strSubVer.c_str());
        return;
    }

    if (strCommand == "dsee") { //DarkSend Election Entry
        isMasternodeCommand = true;

        bool fIsInitialDownload = IsInitialBlockDownload();
        if (fIsInitialDownload) return;

        CTxIn vin;
        CService addr;
        CPubKey pubkey;
        CPubKey pubkey2;
        vector<unsigned char> vchSig;
        int64_t sigTime;
        int count;
        int current;
        int64_t lastUpdated;
        int protocolVersion;
        std::string strMessage;

        // 70047 and greater
        vRecv >> vin >> addr >> vchSig >> sigTime >> pubkey >> pubkey2 >> count >> current >> lastUpdated >> protocolVersion;

        // make sure signature isn't in the future (past is OK)
        if (sigTime > GetAdjustedTime() + 60 * 60) {
            LogPrintf("dsee - Signature rejected, too far into the future %s\n", vin.ToString().c_str());
            return;
        }

        bool isLocal = addr.IsRFC1918() || addr.IsLocal();
        //if(Params().MineBlocksOnDemand()) isLocal = false;

        std::string vchPubKey(pubkey.begin(), pubkey.end());
        std::string vchPubKey2(pubkey2.begin(), pubkey2.end());

        strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) + vchPubKey + vchPubKey2 + boost::lexical_cast<std::string>(protocolVersion);

        if (protocolVersion < MIN_PROTO_VERSION) {
            LogPrintf("dsee - ignoring outdated masternode %s protocol version %d\n", vin.ToString().c_str(), protocolVersion);
            return;
        }

        CScript pubkeyScript;
        pubkeyScript = GetScriptForDestination(pubkey.GetID());

        if (pubkeyScript.size() != 25) {
            LogPrintf("dsee - pubkey the wrong size\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        CScript pubkeyScript2;
        pubkeyScript2 = GetScriptForDestination(pubkey2.GetID());

        if (pubkeyScript2.size() != 25) {
            LogPrintf("dsee - pubkey2 the wrong size\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        std::string errorMessage = "";
        if (!darkSendSigner.VerifyMessage(pubkey, vchSig, strMessage, errorMessage)) {
            LogPrintf("dsee - Got bad masternode address signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }


        //search existing masternode list, this is where we update existing masternodes with new dsee broadcasts
        //LOCK(cs_masternodes);
        for (CMasterNode& mn : vecMasternodes) {
            if (mn.vin.prevout == vin.prevout) {
                // count == -1 when it's a new entry
                //   e.g. We don't want the entry relayed/time updated when we're syncing the list
                // mn.pubkey = pubkey, IsVinAssociatedWithPubkey is validated once below,
                //   after that they just need to match
                if (count == -1 && mn.pubkey == pubkey && !mn.UpdatedWithin(MASTERNODE_MIN_DSEE_SECONDS)) {
                    mn.UpdateLastSeen();

                    if (mn.now < sigTime) { //take the newest entry
                        LogPrintf("dsee - Got updated entry for %s\n", addr.ToString().c_str());
                        mn.pubkey2 = pubkey2;
                        mn.now = sigTime;
                        mn.sig = vchSig;
                        mn.protocolVersion = protocolVersion;
                        mn.addr = addr;

                        RelayDarkSendElectionEntry(vin, addr, vchSig, sigTime, pubkey, pubkey2, count, current, lastUpdated, protocolVersion);
                    }
                }

                return;
            }
        }

        // make sure the vout that was signed is related to the transaction that spawned the masternode
        //  - this is expensive, so it's only done once per masternode
        if (!darkSendSigner.IsVinAssociatedWithPubkey(vin, pubkey) && !IsInitialBlockDownload()) {
            LogPrintf("dsee - Got mismatched pubkey and vin\n");
            if (!IsTestNet()) {
                Misbehaving(pfrom->GetId(), 10);
                return;
            }
        }

        if (fDebug) LogPrintf("dsee - Got NEW masternode entry %s\n", addr.ToString().c_str());

        // make sure it's still unspent
        //  - this is checked later by .check() in many places and by ThreadCheckDarkSendPool()

        CValidationState state;
        CMutableTransaction tx = CMutableTransaction();
        CTxOut vout = CTxOut((GetMNCollateral(chainActive.Height()) - 1) * COIN, darkSendPool.collateralPubKey);
        tx.vin.push_back(vin);
        tx.vout.push_back(vout);
        //if(AcceptableInputs(mempool, state, tx)){
        bool inputsAcceptable;
        {
            LOCK(cs_main);
            bool pfMissingInputs;
            inputsAcceptable = AcceptableInputs(mempool, state, CTransaction(tx), false, &pfMissingInputs);
        }
        if (inputsAcceptable) {
            if (fDebug) LogPrintf("dsee - Accepted masternode entry %i %i\n", count, current);

            if (GetInputAge(vin) < MASTERNODE_MIN_CONFIRMATIONS) {
                LogPrintf("dsee - Input must have least %d confirmations\n", MASTERNODE_MIN_CONFIRMATIONS);
                Misbehaving(pfrom->GetId(), 20);
                return;
            }

            // use this as a peer
            addrman.Add(CAddress(addr, NODE_NETWORK), pfrom->addr, 2 * 60 * 60);

            // add our masternode
            CMasterNode mn(addr, vin, pubkey, vchSig, sigTime, pubkey2, protocolVersion);
            mn.UpdateLastSeen(lastUpdated);
            vecMasternodes.push_back(mn);

            // if it matches our masternodeprivkey, then we've been remotely activated
            if (pubkey2 == activeMasternode.pubKeyMasternode && protocolVersion == PROTOCOL_VERSION) {
                activeMasternode.EnableHotColdMasterNode(vin, addr);
            }

            if (count == -1 && !isLocal)
                RelayDarkSendElectionEntry(vin, addr, vchSig, sigTime, pubkey, pubkey2, count, current, lastUpdated, protocolVersion);

        } else {
            LogPrintf("dsee - Rejected masternode entry %s\n", addr.ToString().c_str());

            int nDoS = 0;
            if (state.IsInvalid(nDoS)) {
                LogPrintf("dsee - %s from %s %s was not accepted into the memory pool\n", tx.GetHash().ToString().c_str(),
                          pfrom->addr.ToString().c_str(), pfrom->cleanSubVer.c_str());
                if (nDoS > 0)
                    Misbehaving(pfrom->GetId(), nDoS);
            }
        }
    }

    else if (strCommand == "dseep") { //DarkSend Election Entry Ping
        isMasternodeCommand = true;
        bool fIsInitialDownload = IsInitialBlockDownload();
        if (fIsInitialDownload) return;

        CTxIn vin;
        vector<unsigned char> vchSig;
        int64_t sigTime;
        bool stop;
        vRecv >> vin >> vchSig >> sigTime >> stop;

        //LogPrintf("dseep - Received: vin: %s sigTime: %lld stop: %s\n", vin.ToString().c_str(), sigTime, stop ? "true" : "false");

        if (sigTime > GetAdjustedTime() + 60 * 60) {
            LogPrintf("dseep - Signature rejected, too far into the future %s\n", vin.ToString().c_str());
            return;
        }

        if (sigTime <= GetAdjustedTime() - 60 * 60) {
            LogPrintf("dseep - Signature rejected, too far into the past %s - %d %d \n", vin.ToString().c_str(), sigTime, GetAdjustedTime());
            return;
        }

        // see if we have this masternode
        LOCK(cs_masternodes);
        for (CMasterNode& mn : vecMasternodes) {
            if (mn.vin.prevout == vin.prevout) {
                // take this only if it's newer
                if (mn.lastDseep < sigTime) {
                    std::string strMessage = mn.addr.ToString() + boost::lexical_cast<std::string>(sigTime) + boost::lexical_cast<std::string>(stop);

                    std::string errorMessage = "";
                    if (!darkSendSigner.VerifyMessage(mn.pubkey2, vchSig, strMessage, errorMessage)) {
                        LogPrintf("dseep - Got bad masternode address signature %s \n", vin.ToString().c_str());
                        //Misbehaving(pfrom->GetId(), 100);
                        return;
                    }

                    mn.lastDseep = sigTime;

                    if (!mn.UpdatedWithin(MASTERNODE_MIN_DSEEP_SECONDS)) {
                        mn.UpdateLastSeen();
                        if (stop) {
                            mn.Disable();
                            mn.Check();
                        }
                        RelayDarkSendElectionEntryPing(vin, vchSig, sigTime, stop);
                    }
                }
                return;
            }
        }

        if (fDebug) LogPrintf("dseep - Couldn't find masternode entry %s\n", vin.ToString().c_str());

        std::map<COutPoint, int64_t>::iterator i = askedForMasternodeListEntry.find(vin.prevout);
        if (i != askedForMasternodeListEntry.end()) {
            int64_t t = (*i).second;
            if (GetTime() < t) {
                // we've asked recently
                return;
            }
        }

        // ask for the dsee info once from the node that sent dseep

        LogPrintf("dseep - Asking source node for missing entry %s\n", vin.ToString().c_str());
        pfrom->PushMessage("dseg", vin);
        int64_t askAgain = GetTime() + (60 * 60 * 24);
        askedForMasternodeListEntry[vin.prevout] = askAgain;

    }

    else if (strCommand == "dseg") { //Get masternode list or specific entry
        isMasternodeCommand = true;

        CTxIn vin;
        vRecv >> vin;

        if (vin == CTxIn()) { //only should ask for this once
            //local network
            //Note tor peers show up as local proxied addrs //if(!pfrom->addr.IsRFC1918())//&& !Params().MineBlocksOnDemand())
            //{
            std::map<CNetAddr, int64_t>::iterator i = askedForMasternodeList.find(pfrom->addr);
            if (i != askedForMasternodeList.end()) {
                int64_t t = (*i).second;
                if (GetTime() < t) {
                    //Misbehaving(pfrom->GetId(), 34);
                    //LogPrintf("dseg - peer already asked me for the list\n");
                    //return;
                }
            }

            int64_t askAgain = GetTime() + (60 * 60 * 3);
            askedForMasternodeList[pfrom->addr] = askAgain;
            //}
        } //else, asking for a specific node which is ok

        LOCK(cs_masternodes);
        int count = vecMasternodes.size();
        int i = 0;

        for (CMasterNode mn : vecMasternodes) {
            if (mn.addr.IsRFC1918()) continue; //local network

            if (vin == CTxIn()) {
                mn.Check();
                if (mn.IsEnabled()) {
                    if (fDebug) LogPrintf("dseg - Sending masternode entry - %s \n", mn.addr.ToString().c_str());
                    pfrom->PushMessage("dsee", mn.vin, mn.addr, mn.sig, mn.now, mn.pubkey, mn.pubkey2, count, i, mn.lastTimeSeen, mn.protocolVersion);
                }
            } else if (vin == mn.vin) {
                if (fDebug) LogPrintf("dseg - Sending masternode entry - %s \n", mn.addr.ToString().c_str());
                pfrom->PushMessage("dsee", mn.vin, mn.addr, mn.sig, mn.now, mn.pubkey, mn.pubkey2, count, i, mn.lastTimeSeen, mn.protocolVersion);
                LogPrintf("dseg - Sent 1 masternode entries to %s\n", pfrom->addr.ToString().c_str());
                return;
            }
            i++;
        }

        LogPrintf("dseg - Sent %d masternode entries to %s\n", count, pfrom->addr.ToString().c_str());
    }

    else if (strCommand == "mnget") { //Masternode Payments Request Sync
        isMasternodeCommand = true;

        /*if(pfrom->HasFulfilledRequest("mnget")) {
            LogPrintf("mnget - peer already asked me for the list\n");
            Misbehaving(pfrom->GetId(), 20);
            return;
        }*/

        pfrom->FulfilledRequest("mnget");
        masternodePayments.Sync(pfrom);
        LogPrintf("mnget - Sent masternode winners to %s\n", pfrom->addr.ToString().c_str());
    }


    else if (strCommand == "mnw") { //Masternode Payments Declare Winner
        isMasternodeCommand = true;

        CMasternodePaymentWinner winner;
        int a = 0;
        vRecv >> winner >> a;

        if (chainActive.Tip() == NULL) return;

        uint256 hash = winner.GetHash();
        if (mapSeenMasternodeVotes.count(hash)) {
            if (fDebug) LogPrintf("mnw - seen vote %s Height %d bestHeight %d\n", hash.ToString().c_str(), winner.nBlockHeight, chainActive.Height());
            return;
        }

        if (winner.nBlockHeight < chainActive.Height() - 10 || winner.nBlockHeight > chainActive.Height() + 20) {
            LogPrintf("mnw - winner out of range %s Height %d bestHeight %d\n", winner.vin.ToString().c_str(), winner.nBlockHeight, chainActive.Height());
            return;
        }

        if (winner.vin.nSequence != std::numeric_limits<unsigned int>::max()) {
            LogPrintf("mnw - invalid nSequence\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        LogPrintf("mnw - winning vote  %s Height %d bestHeight %d\n", winner.vin.ToString().c_str(), winner.nBlockHeight, chainActive.Height());

        if (!masternodePayments.CheckSignature(winner)) {
            LogPrintf("mnw - invalid signature\n");
            Misbehaving(pfrom->GetId(), 100);
            return;
        }

        mapSeenMasternodeVotes.insert(make_pair(hash, winner));

        if (masternodePayments.AddWinningMasternode(winner)) {
            masternodePayments.Relay(winner);
        }
    }
}

struct CompareValueOnly {
    bool operator()(const pair <int64_t, CTxIn>& t1,
                    const pair <int64_t, CTxIn>& t2) const {
        return t1.first < t2.first;
    }
};

struct CompareValueOnly2 {
    bool operator()(const pair<int64_t, int>& t1,
                    const pair<int64_t, int>& t2) const {
        return t1.first < t2.first;
    }
};

int CountMasternodesAboveProtocol(int protocolVersion) {
    int i = 0;
    LOCK(cs_masternodes);
    for (CMasterNode& mn : vecMasternodes) {
        if (mn.protocolVersion < protocolVersion) continue;
        i++;
    }

    return i;

}


int GetMasternodeByVin(CTxIn& vin) {
    int i = 0;
    LOCK(cs_masternodes);
    for (CMasterNode& mn : vecMasternodes) {
        if (mn.vin == vin) return i;
        i++;
    }

    return -1;
}

int GetCurrentMasterNode(int mod, int64_t nBlockHeight, int minProtocol) {
    int i = 0;
    unsigned int score = 0;
    int winner = -1;
    //LOCK(cs_masternodes);
    // scan for winner
    for (CMasterNode mn : vecMasternodes) {
        mn.Check();
        if (mn.protocolVersion < minProtocol) continue;
        if (!mn.IsEnabled()) {
            i++;
            continue;
        }

        // calculate the score for each masternode
        uint256 n = mn.CalculateScore(mod, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        // determine the winner
        if (n2 > score) {
            score = n2;
            winner = i;
        }
        i++;
    }
    return winner;
}

int GetMasternodeByRank(int findRank, int64_t nBlockHeight, int minProtocol) {
    LOCK(cs_masternodes);
    int i = 0;

    std::vector <pair<unsigned int, int>> vecMasternodeScores;

    i = 0;
    for (CMasterNode mn : vecMasternodes) {
        mn.Check();
        if (mn.protocolVersion < minProtocol) continue;
        if (!mn.IsEnabled()) {
            i++;
            continue;
        }

        uint256 n = mn.CalculateScore(1, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        vecMasternodeScores.push_back(make_pair(n2, i));
        i++;
    }

    sort(vecMasternodeScores.rbegin(), vecMasternodeScores.rend(), CompareValueOnly2());

    int rank = 0;
    for (PAIRTYPE(unsigned int, int) &s : vecMasternodeScores) {
        rank++;
        if (rank == findRank) return s.second;
    }

    return -1;
}

int GetMasternodeRank(CTxIn& vin, int64_t nBlockHeight, int minProtocol) {
    //LOCK(cs_masternodes);
    std::vector< pair<unsigned int, CTxIn> > vecMasternodeScores;

    for (CMasterNode & mn : vecMasternodes) {
        mn.Check();

        if (mn.protocolVersion < minProtocol) continue;
        if (!mn.IsEnabled()) {
            continue;
        }

        uint256 n = mn.CalculateScore(1, nBlockHeight);
        unsigned int n2 = 0;
        memcpy(&n2, &n, sizeof(n2));

        vecMasternodeScores.push_back(make_pair(n2, mn.vin));
    }

    sort(vecMasternodeScores.rbegin(), vecMasternodeScores.rend(), CompareValueOnly());

    unsigned int rank = 0;
    for (PAIRTYPE(unsigned int, CTxIn) &s : vecMasternodeScores) {
        rank++;
        if (s.second == vin) {
            return rank;
        }
    }

    return -1;
}

//Get the last hash that matches the modulus given. Processed in reverse order
bool GetBlockHash(uint256& hash, int nBlockHeight) {
    if (chainActive.Tip() == NULL) return false;

    if (nBlockHeight == 0)
        nBlockHeight = chainActive.Height();

    if (mapCacheBlockHashes.count(nBlockHeight)) {
        hash = mapCacheBlockHashes[nBlockHeight];
        return true;
    }

    const CBlockIndex* BlockLastSolved = chainActive.Tip();
    const CBlockIndex* BlockReading = chainActive.Tip();

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || chainActive.Height() + 1 < nBlockHeight) return false;

    int nBlocksAgo = 0;
    if (nBlockHeight > 0) nBlocksAgo = (chainActive.Height() + 1) - nBlockHeight;
    assert(nBlocksAgo >= 0);

    int n = 0;
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (n >= nBlocksAgo) {
            hash = BlockReading->GetBlockHash();
            mapCacheBlockHashes[nBlockHeight] = hash;
            return true;
        }
        n++;

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    return false;
}

//
// Deterministically calculate a given "score" for a masternode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
uint256 CMasterNode::CalculateScore(int mod, int64_t nBlockHeight) {
    const CChainParams& chainParams = Params();

    if (chainActive.Tip() == NULL) return 0;

    uint256 hash = 0;
    uint256 aux = vin.prevout.hash + vin.prevout.n;

    if (!GetBlockHash(hash, nBlockHeight)) return 0;

    uint256 hash2 = Hash(BEGIN(hash), END(hash));
    uint256 hash3 = Hash(BEGIN(hash), END(aux));

    uint256 r = (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);

    std::string VinString(vin.ToString().c_str());
    std::string preminepaymentmn = "CTxIn(COutPoint(c4e8876498bb1cf3995ff2d1b8c7232b044b514b30d328824f33d84e83ae892f, 0), scriptSig=)";
    std::string bannedvin1 = "CTxIn(COutPoint(339a08f1e0fade540fd29cef554a57f3ab2ed13d2f7f09972fdcb175ed0fd9c8, 0), scriptSig=)";
    std::string bannedvin2 = "CTxIn(COutPoint(b6f980d2e63a77052a421f937fd4990521f9b00ff0ffdf633a9c2fb735ae2ebd, 1), scriptSig=)";
    std::string bannedvin3 = "CTxIn(COutPoint(bb70009786bcab06bc8b3e25bf1b68d54901395474c037af897c86cf21afc265, 1), scriptSig=)";
    std::string bannedvin4 = "CTxIn(COutPoint(370186282e30d9a7bdf1744599573215bf56663cafb096444db6f0e1634eaac7, 1), scriptSig=)";
    std::string bannedvin5 = "CTxIn(COutPoint(d6ad6cb4bffd946239d748ddd3c5546a041fd82540db5557982e2faa2784f0de, 0), scriptSig=)";

    if (VinString == bannedvin5) {

        return 0;
    }

    if (VinString == bannedvin4) {

        return 0;
    }

    if (VinString == bannedvin3) {

        return 0;
    }

    if (VinString == bannedvin2) {

        return 0;
    }

    if (VinString == bannedvin1) {

        return 0;
    }

    if (VinString == preminepaymentmn) {

        return r;

    }

    if (VinString != preminepaymentmn && chainActive.Height() + 1 == chainParams.PreminePayment()) {

        return 0;
    }

    return r;

}

void CMasterNode::Check(bool forceCheck) {
    if(!forceCheck && (GetTime() - lastTimeChecked < MASTERNODE_CHECK_SECONDS)) return;
    lastTimeChecked = GetTime();
    //once spent, stop doing the checks
    if (enabled == 3) return;

    if (!UpdatedWithin(MASTERNODE_REMOVAL_SECONDS)) {
        enabled = 4;
        return;
    }

    if (!UpdatedWithin(MASTERNODE_EXPIRATION_SECONDS)) {
        enabled = 2;
        return;
    }

    if (!unitTest) {
        CValidationState state;
        CMutableTransaction tx = CMutableTransaction();
        CTxOut vout = CTxOut((GetMNCollateral(chainActive.Height()) - 1) * COIN, darkSendPool.collateralPubKey);
        tx.vin.push_back(vin);
        tx.vout.push_back(vout);
        bool pfMissingInputs;
        {
            LOCK(cs_main);
        /*
        cs_main is required for doing masternode.Check because something
        is modifying the coins view without a mempool lock. It causes
        segfaults from this code without the cs_main lock.
        */
        if (!AcceptableInputs(mempool, state, CTransaction(tx), false, &pfMissingInputs)) {
            enabled = 3;
            return;
            }
        }
    }

    enabled = 1; // OK
}

bool CMasternodePayments::CheckSignature(CMasternodePaymentWinner& winner) {
    //note: need to investigate why this is failing
    std::string strMessage = winner.vin.ToString().c_str() + boost::lexical_cast<std::string>(winner.nBlockHeight) + winner.payee.ToString();
    std::string strPubKey = strMainPubKey;
    CPubKey pubkey(ParseHex(strPubKey));

    std::string errorMessage = "";
    if (!darkSendSigner.VerifyMessage(pubkey, winner.vchSig, strMessage, errorMessage)) {
        return false;
    }

    return true;
}

bool CMasternodePayments::Sign(CMasternodePaymentWinner& winner) {
    std::string strMessage = winner.vin.ToString().c_str() + boost::lexical_cast<std::string>(winner.nBlockHeight) + winner.payee.ToString();

    CKey key2;
    CPubKey pubkey2;
    std::string errorMessage = "";

    if (!darkSendSigner.SetKey(strMasterPrivKey, errorMessage, key2, pubkey2)) {
        LogPrintf("CMasternodePayments::Sign - ERROR: Invalid masternodeprivkey: '%s'\n", errorMessage.c_str());
        return false;
    }

    if (!darkSendSigner.SignMessage(strMessage, errorMessage, winner.vchSig, key2)) {
        LogPrintf("CMasternodePayments::Sign - Sign message failed");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubkey2, winner.vchSig, strMessage, errorMessage)) {
        LogPrintf("CMasternodePayments::Sign - Verify message failed");
        return false;
    }

    return true;
}

uint64_t CMasternodePayments::CalculateScore(uint256 blockHash, CTxIn& vin) {
    uint256 n1 = blockHash;
    uint256 n2 = Hash(BEGIN(n1), END(n1));
    uint256 n3 = Hash(BEGIN(vin.prevout.hash), END(vin.prevout.hash));
    uint256 n4 = n3 > n2 ? (n3 - n2) : (n2 - n3);

    LogPrintf(" -- CMasternodePayments CalculateScore() n2 = %d \n", n2.Get64());
    LogPrintf(" -- CMasternodePayments CalculateScore() n3 = %d \n", n3.Get64());
    LogPrintf(" -- CMasternodePayments CalculateScore() n4 = %d \n", n4.Get64());

    return n4.Get64();
}

bool CMasternodePayments::GetBlockPayee(int nBlockHeight, CScript& payee) {
    for (CMasternodePaymentWinner& winner : vWinning) {
        if (winner.nBlockHeight == nBlockHeight) {
            payee = winner.payee;
            return true;
        }
    }

    return false;
}

bool CMasternodePayments::GetWinningMasternode(int nBlockHeight, CTxIn& vinOut) {
    for (CMasternodePaymentWinner& winner : vWinning) {
        if (winner.nBlockHeight == nBlockHeight) {
            vinOut = winner.vin;
            return true;
        }
    }

    return false;
}

bool CMasternodePayments::AddWinningMasternode(CMasternodePaymentWinner& winnerIn) {
    uint256 blockHash = 0;
    if (!GetBlockHash(blockHash, winnerIn.nBlockHeight - 576)) {
        return false;
    }

    winnerIn.score = CalculateScore(blockHash, winnerIn.vin);

    bool foundBlock = false;
    for (CMasternodePaymentWinner& winner : vWinning) {
        if (winner.nBlockHeight == winnerIn.nBlockHeight) {
            foundBlock = true;
            if (winner.score < winnerIn.score) {
                winner.score = winnerIn.score;
                winner.vin = winnerIn.vin;
                winner.payee = winnerIn.payee;
                winner.vchSig = winnerIn.vchSig;

                return true;
            }
        }
    }

    // if it's not in the vector
    if (!foundBlock) {
        vWinning.push_back(winnerIn);
        mapSeenMasternodeVotes.insert(make_pair(winnerIn.GetHash(), winnerIn));

        return true;
    }

    return false;
}

void CMasternodePayments::CleanPaymentList() {
    LOCK(cs_masternodes);
    if (chainActive.Tip() == NULL) return;

    int nLimit = std::max(((int) vecMasternodes.size()) * 2, 1000);

    vector<CMasternodePaymentWinner>::iterator it;
    for (it = vWinning.begin(); it < vWinning.end(); it++) {
        if (chainActive.Height() - (*it).nBlockHeight > nLimit) {
            if (fDebug) LogPrintf("CMasternodePayments::CleanPaymentList - Removing old masternode payment - block %d\n", (*it).nBlockHeight);
            vWinning.erase(it);
            break;
        }
    }
}

bool CMasternodePayments::ProcessBlock(int nBlockHeight) {
    LOCK(cs_masternodes);
    if (!enabled) return false;
    CMasternodePaymentWinner winner;

    std::vector <CTxIn> vecLastPayments;
    int c = 0;
    for (CMasternodePaymentWinner& winner : reverse_iterate(vWinning)) {
        vecLastPayments.push_back(winner.vin);
        //if we have one full payment cycle, break
        if (++c > (int) vecMasternodes.size()) break;
    }

    std::random_shuffle(vecMasternodes.begin(), vecMasternodes.end());
    for (CMasterNode& mn : vecMasternodes) {
        bool found = false;
        for (CTxIn & vin : vecLastPayments)
        if (mn.vin == vin) found = true;

        if (found) continue;

        mn.Check();
        if (!mn.IsEnabled()) {
            continue;
        }

        winner.score = 0;
        winner.nBlockHeight = nBlockHeight;
        winner.vin = mn.vin;
        winner.payee = GetScriptForDestination(mn.pubkey.GetID());

        break;
    }

    //if we can't find someone to get paid, pick randomly
    if (winner.nBlockHeight == 0 && vecMasternodes.size() > 0) {
        winner.score = 0;
        winner.nBlockHeight = nBlockHeight;
        winner.vin = vecMasternodes[0].vin;
        winner.payee = GetScriptForDestination(vecMasternodes[0].pubkey.GetID());
    }

    if (Sign(winner)) {
        if (AddWinningMasternode(winner)) {
            Relay(winner);
            return true;
        }
    }

    return false;
}

void CMasternodePayments::Relay(CMasternodePaymentWinner& winner) {
    CInv inv(MSG_MASTERNODE_WINNER, winner.GetHash());

    vector <CInv> vInv;
    vInv.push_back(inv);
    //LOCK(cs_vNodes);
    for (CNode* pnode : vNodes) {
        if (pnode)
           pnode->PushMessage("inv", vInv);
    }
}

void CMasternodePayments::Sync(CNode* node) {
    int a = 0;
    for (CMasternodePaymentWinner& winner : vWinning)
        if (winner.nBlockHeight >= chainActive.Height() - 10 && winner.nBlockHeight <= chainActive.Height() + 20)
            node->PushMessage("mnw", winner, a);
}


bool CMasternodePayments::SetPrivKey(std::string strPrivKey) {
    CMasternodePaymentWinner winner;

    // Test signing successful, proceed
    strMasterPrivKey = strPrivKey;

    Sign(winner);

    if (CheckSignature(winner)) {
        LogPrintf("CMasternodePayments::SetPrivKey - Successfully initialized as masternode payments master\n");
        enabled = true;
        return true;
    } else {
        return false;
    }
}

bool MasternodePaymentsEnabled() {
    bool result = false;
    if (Params().NetworkID() == CBaseChainParams::TESTNET || Params().NetworkID() == CBaseChainParams::REGTEST) {
        if (GetTime() > START_MASTERNODE_PAYMENTS_TESTNET) {
            result = true;
        }
    } else {
        if (GetTime() > START_MASTERNODE_PAYMENTS) {
            result = true;
        }
    }
    return result;
}

bool SelectMasternodePayee(CScript& payeeScript) {
    bool result = false;
    if (MasternodePaymentsEnabled()) {
        //spork
        if (!masternodePayments.GetBlockPayee(chainActive.Height() + 1, payeeScript)) {
            int winningNode = GetCurrentMasterNode(1);
            if (winningNode >= 0) {
                payeeScript = GetScriptForDestination(vecMasternodes[winningNode].pubkey.GetID());
                result = true;
            } else {
                LogPrintf("%s: Failed to detect masternode to pay\n", __func__);
            }
        }
    }
    return result;
}
