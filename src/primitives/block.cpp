// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"

#include "hash.h"
#include "script/standard.h"
#include "script/sign.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "util.h"
#include "chainparams.h"
#include "versionbits.h"

uint256 CBlockHeader::GetHash(bool phi2block) const {
    if (phi2block && (nVersion & (1 << 30)))
        return phi2_hash(BEGIN(nVersion), END(hashUTXORoot));
    else if (nVersion > VERSIONBITS_LAST_OLD_BLOCK_VERSION && phi2block) {
        return phi2_hash(BEGIN(nVersion), END(nNonce));
    } else {
        return Phi1612(BEGIN(nVersion), END(nNonce));
    }
}

uint256 CBlock::BuildMerkleTree(bool* fMutated) const
{
    /* WARNING! If you're reading this because you're learning about crypto
       and/or designing a new system that will use merkle trees, keep in mind
       that the following merkle tree algorithm has a serious flaw related to
       duplicate txids, resulting in a vulnerability (CVE-2012-2459).

       The reason is that if the number of hashes in the list at a given time
       is odd, the last one is duplicated before computing the next level (which
       is unusual in Merkle trees). This results in certain sequences of
       transactions leading to the same merkle root. For example, these two
       trees:

                    A               A
                  /  \            /   \
                B     C         B       C
               / \    |        / \     / \
              D   E   F       D   E   F   F
             / \ / \ / \     / \ / \ / \ / \
             1 2 3 4 5 6     1 2 3 4 5 6 5 6

       for transaction lists [1,2,3,4,5,6] and [1,2,3,4,5,6,5,6] (where 5 and
       6 are repeated) result in the same root hash A (because the hash of both
       of (F) and (F,F) is C).

       The vulnerability results from being able to send a block with such a
       transaction list, with the same merkle root, and the same block hash as
       the original without duplication, resulting in failed validation. If the
       receiving node proceeds to mark that block as permanently invalid
       however, it will fail to accept further unmodified (and thus potentially
       valid) versions of the same block. We defend against this by detecting
       the case where we would hash two identical hashes at the end of the list
       together, and treating that identically to the block having an invalid
       merkle root. Assuming no double-SHA256 collisions, this will detect all
       known ways of changing the transactions without affecting the merkle
       root.
    */
    vMerkleTree.clear();
    vMerkleTree.reserve(vtx.size() * 2 + 16); // Safe upper bound for the number of total nodes.
    for (std::vector<CTransaction>::const_iterator it(vtx.begin()); it != vtx.end(); ++it)
        vMerkleTree.push_back(it->GetHash());
    int j = 0;
    bool mutated = false;
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        for (int i = 0; i < nSize; i += 2)
        {
            int i2 = std::min(i+1, nSize-1);
            if (i2 == i + 1 && i2 + 1 == nSize && vMerkleTree[j+i] == vMerkleTree[j+i2]) {
                // Two identical hashes at the end of the list at a particular level.
                mutated = true;
            }
            vMerkleTree.push_back(Hash(BEGIN(vMerkleTree[j+i]),  END(vMerkleTree[j+i]),
                                       BEGIN(vMerkleTree[j+i2]), END(vMerkleTree[j+i2])));
        }
        j += nSize;
    }
    if (fMutated) {
        *fMutated = mutated;
    }
    return (vMerkleTree.empty() ? 0 : vMerkleTree.back());
}

std::vector<uint256> CBlock::GetMerkleBranch(int nIndex) const
{
    if (vMerkleTree.empty())
        BuildMerkleTree();
    std::vector<uint256> vMerkleBranch;
    int j = 0;
    for (int nSize = vtx.size(); nSize > 1; nSize = (nSize + 1) / 2)
    {
        int i = std::min(nIndex^1, nSize-1);
        vMerkleBranch.push_back(vMerkleTree[j+i]);
        nIndex >>= 1;
        j += nSize;
    }
    return vMerkleBranch;
}

uint256 CBlock::CheckMerkleBranch(uint256 hash, const std::vector<uint256>& vMerkleBranch, int nIndex)
{
    if (nIndex == -1)
        return 0;
    for (std::vector<uint256>::const_iterator it(vMerkleBranch.begin()); it != vMerkleBranch.end(); ++it)
    {
        if (nIndex & 1)
            hash = Hash(BEGIN(*it), END(*it), BEGIN(hash), END(hash));
        else
            hash = Hash(BEGIN(hash), END(hash), BEGIN(*it), END(*it));
        nIndex >>= 1;
    }
    return hash;
}

std::string CBlock::ToString() const
{
    std::stringstream s;
    s << strprintf("CBlock(hash=%s, ver=%d, hashPrevBlock=%s, hashMerkleRoot=%s, nTime=%u, nBits=%08x, nNonce=%u, hashStateRoot=%s, hashUTXORoot=%s, vtx=%u)\n",
        GetHash().ToString(),
        nVersion,
        hashPrevBlock.ToString(),
        hashMerkleRoot.ToString(),
        nTime, nBits, nNonce,
        hashStateRoot.ToString(), // lux
        hashUTXORoot.ToString(), // lux
        vtx.size());
    for (unsigned int i = 0; i < vtx.size(); i++)
    {
        s << "  " << vtx[i].ToString() << "\n";
    }
    s << "  vMerkleTree: ";
    for (unsigned int i = 0; i < vMerkleTree.size(); i++)
        s << " " << vMerkleTree[i].ToString();
    s << "\n";
    return s.str();
}

void CBlock::print() const
{
    LogPrintf("%s", ToString());
}

typedef std::vector<unsigned char> valtype;
// ppcoin: sign block
bool CBlock::SignBlock(const CKeyStore& keystore)
{
    std::vector<valtype> vSolutions;
    txnouttype whichType;

    if(!IsProofOfStake()) {
        // if we are trying to sign
        //    a complete proof-of-stake block
        for (unsigned int i = 0; i < vtx[0].vout.size(); i++) {
            const CTxOut& txout = vtx[0].vout[i];

            if (!Solver(txout.scriptPubKey, whichType, vSolutions))
                continue;

            if (whichType == TX_PUBKEY) {
                // Sign
                CKeyID keyID;
                keyID = CKeyID(uint160(vSolutions[0]));

                CKey key;
                if (!keystore.GetKey(keyID, key))
                    return false;

                if (!key.Sign(GetHash(), vchBlockSig))
                    return false;

                return true;
            }
        }
    } else {
        for (unsigned int j = 0; j < vtx[1].vout.size(); j++) {
            const CTxOut &txout = vtx[1].vout[j];

            if (!Solver(txout.scriptPubKey, whichType, vSolutions))
                continue;

            if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH) {
                break;
            } else if (whichType == TX_PUBKEYHASH) {
                // Sign
                CKeyID keyID;
                keyID = CKeyID(uint160(vSolutions[0]));

                CKey key;
                if (!keystore.GetKey(keyID, key))
                    return false;

                if (!key.Sign(GetHash(), vchBlockSig))
                    return false;

                return true;
            } else if (whichType == TX_PUBKEY) {
                // Sign
                const valtype& vchPubKey = vSolutions[0];
                CKey key;
                CPubKey pubKey(vchPubKey);
                if (!keystore.GetKey(pubKey.GetID(), key))
                    return false;
                if (key.GetPubKey() != pubKey)
                    return false;
                return key.Sign(GetHash(), vchBlockSig, 0);
            }
            return false;
            }
        }
    LogPrintf("Sign failed\n");
    return false;
}

bool CBlock::CheckBlockSignature() const
{
    if (IsProofOfWork())
        return vchBlockSig.empty();

    if (IsProofOfStake()) {
        // if we are trying to sign
        //    a complete proof-of-stake block
        return vtx[0].vout[0].IsEmpty();
    } else {

    std::vector<valtype> vSolutions;
    txnouttype whichType;

    const CTxOut& txout = vtx[1].vout[1];

    if (!Solver(txout.scriptPubKey, whichType, vSolutions))
        return false;

    if (whichType == TX_PUBKEY)
    {
        valtype& vchPubKey = vSolutions[0];
        CPubKey pubkey(vchPubKey);
        if (!pubkey.IsValid())
          return false;

        if (vchBlockSig.empty())
            return false;

        return pubkey.Verify(GetHash(), vchBlockSig);
    }
    else if(whichType == TX_PUBKEYHASH)
    {
        valtype& vchPubKey = vSolutions[0];
        CKeyID keyID;
        keyID = CKeyID(uint160(vchPubKey));
        CPubKey pubkey(vchPubKey);

        if (!pubkey.IsValid())
          return false;

        if (vchBlockSig.empty())
            return false;

        return pubkey.Verify(GetHash(), vchBlockSig);

        }
    }

    return false;
}

int64_t GetBlockCost(const CBlock& block)
{
    // This implements the cost = (stripped_size * 4) + witness_size formula,
    // using only serialization with and without witness data. As witness_size
    // is equal to total_size - stripped_size, this formula is identical to:
    // cost = (stripped_size * 3) + total_size.
    return ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS) * (WITNESS_SCALE_FACTOR - 1) + ::GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION);
}
