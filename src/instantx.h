// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef INSTANTX_H
#define INSTANTX_H

#include "uint256.h"
#include "sync.h"
#include "net.h"
#include "key.h"
#include "primitives/transaction.h"
#include "util.h"
#include "script/script.h"
//#include "script.h"
#include "base58.h"
#include "main.h"

using namespace std;
using namespace boost;

#define INSTANTX_SIGNATURES_REQUIRED           20
#define INSTANTX_SIGNATURES_TOTAL              30

class CConsensusVote;
class CTransaction;
class CTransactionLock;

extern map<uint256, CTransaction> mapTxLockReq;
extern map<uint256, CTransaction> mapTxLockReqRejected;
extern map<uint256, CConsensusVote> mapTxLockVote;
extern map<uint256, CTransactionLock> mapTxLocks;
extern std::map<COutPoint, uint256> mapLockedInputs;
extern int nCompleteTXLocks;


int64_t CreateNewLock(CTransaction tx);

bool IsIXTXValid(const CTransaction& txCollateral);

// if two conflicting locks are approved by the network, they will cancel out
bool CheckForConflictingLocks(CTransaction& tx);

void ProcessInstantX(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool &isInstantXCommand);

//check if we need to vote on this transaction
void DoConsensusVote(CTransaction& tx, int64_t nBlockHeight);

//process consensus vote message
bool ProcessConsensusVote(CConsensusVote& ctx);

// keep transaction locks in memory for an hour
void CleanTransactionLocksList();

int64_t GetAverageVoteTime();

class CConsensusVote
{
public:
    CTxIn vinMasternode;
    uint256 txHash;
    int nBlockHeight;
    std::vector<unsigned char> vchMasterNodeSignature;

    uint256 GetHash() const;

    bool SignatureValid();
    bool Sign();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(txHash);
        READWRITE(vinMasternode);
        READWRITE(vchMasterNodeSignature);
        READWRITE(nBlockHeight);
    }
};

class CTransactionLock
{
public:
    int nBlockHeight;
    uint256 txHash;
    std::vector<CConsensusVote> vecConsensusVotes;
    int nExpiration;
    int nTimeout;

    bool SignaturesValid();
    int CountSignatures();
    void AddSignature(CConsensusVote& cv);

    uint256 GetHash()
    {
        return txHash;
    }
};


#endif
