// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/block.h"
#include "primitives/transaction.h"

#include "chain.h"
#include "hash.h"
#include "main.h"
#include "tinyformat.h"
#include "utilstrencodings.h"
#include "transaction.h"

#include <boost/foreach.hpp>

extern bool GetTransaction(const uint256 &hash, CTransaction &txOut, uint256 &hashBlock, bool fAllowSlow);

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString()/*.substr(0,10)*/, n);
}

std::string COutPoint::ToStringShort() const
{
    return strprintf("%s-%u", hash.ToString().substr(0,64), n);
}

uint256 COutPoint::GetHash()
{
    return Hash(BEGIN(hash), END(hash), BEGIN(n), END(n));
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        if(scriptSig.IsZerocoinSpend())
            str += strprintf(", zerocoinspend %s", HexStr(scriptSig));
        else
            str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", scriptSig.ToString().substr(0,24));
    if (nSequence != std::numeric_limits<unsigned int>::max())
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}

CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
    nRounds = -10;
}

bool COutPoint::IsMasternodeReward(const CTransaction* tx) const
{
    if(!tx->IsCoinStake())
        return false;

    return (n == tx->vout.size() - 1) && (tx->vout[1].scriptPubKey != tx->vout[n].scriptPubKey);
}

uint256 CTxOut::GetHash() const
{
    return SerializeHash(*this);
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s)", nValue / COIN, nValue % COIN, scriptPubKey.ToString().substr(0,30));
}

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : nVersion(tx.nVersion), vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime) {}

uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this);
}

std::string CMutableTransaction::ToString() const
{
    std::string str;
    str += strprintf("CMutableTransaction(ver=%d, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
        nVersion,
        vin.size(),
        vout.size(),
        nLockTime);
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    return str;
}

void CTransaction::UpdateHash() const
{
    *const_cast<uint256*>(&hash) = SerializeHash(*this);
}

CTransaction::CTransaction() : hash(), nVersion(CTransaction::CURRENT_VERSION), nTime(0), vin(), vout(), nLockTime(0) { }

CTransaction::CTransaction(const CMutableTransaction &tx) : nVersion(tx.nVersion), nTime(tx.nTime), vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime) {
    UpdateHash();
}

CTransaction& CTransaction::operator=(const CTransaction &tx) {
    *const_cast<int*>(&nVersion) = tx.nVersion;
    *const_cast<unsigned int*>(&nTime) = tx.nTime;
    *const_cast<std::vector<CTxIn>*>(&vin) = tx.vin;
    *const_cast<std::vector<CTxOut>*>(&vout) = tx.vout;
    *const_cast<unsigned int*>(&nLockTime) = tx.nLockTime;
    *const_cast<uint256*>(&hash) = tx.hash;
    return *this;
}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
    for (std::vector<CTxOut>::const_iterator it(vout.begin()); it != vout.end(); ++it)
    {
        // LUX: previously MoneyRange() was called here. This has been replaced with negative check and boundary wrap check.
        if (it->nValue < 0)
            throw std::runtime_error("CTransaction::GetValueOut() : value out of range : less than 0");

        if ((nValueOut + it->nValue) < nValueOut)
            throw std::runtime_error("CTransaction::GetValueOut() : value out of range : wraps the int64_t boundary");

        nValueOut += it->nValue;
    }
    return nValueOut;
}

CAmount CTransaction::GetZerocoinMinted() const
{
    for (const CTxOut txOut : vout) {
        if(!txOut.scriptPubKey.IsZerocoinMint())
            continue;

        return txOut.nValue;
    }

    return  CAmount(0);
}

bool CTransaction::UsesUTXO(const COutPoint out)
{
    for (const CTxIn in : vin) {
        if (in.prevout == out)
            return true;
    }

    return false;
}

std::list<COutPoint> CTransaction::GetOutPoints() const
{
    std::list<COutPoint> listOutPoints;
    uint256 txHash = GetHash();
    for (unsigned int i = 0; i < vout.size(); i++)
        listOutPoints.emplace_back(COutPoint(txHash, i));
    return listOutPoints;
}

CAmount CTransaction::GetZerocoinSpent() const
{
    if(!IsZerocoinSpend())
        return 0;

    CAmount nValueOut = 0;
    for (const CTxIn txin : vin) {
        if(!txin.scriptSig.IsZerocoinSpend())
            LogPrintf("%s is not zcspend\n", __func__);

        std::vector<char, zero_after_free_allocator<char> > dataTxIn;
        dataTxIn.insert(dataTxIn.end(), txin.scriptSig.begin() + 4, txin.scriptSig.end());

        CDataStream serializedCoinSpend(dataTxIn, SER_NETWORK, PROTOCOL_VERSION);
        libzerocoin::CoinSpend spend(Params().Zerocoin_Params(), serializedCoinSpend);
        nValueOut += libzerocoin::ZerocoinDenominationToAmount(spend.getDenomination());
    }

    return nValueOut;
}

int CTransaction::GetZerocoinMintCount() const
{
    int nCount = 0;
    for (const CTxOut out : vout) {
        if (out.scriptPubKey.IsZerocoinMint())
            nCount++;
    }
    return nCount;
}

double CTransaction::ComputePriority(double dPriorityInputs, unsigned int nTxSize) const
{
    nTxSize = CalculateModifiedSize(nTxSize);
    if (nTxSize == 0) return 0.0;

    return dPriorityInputs / nTxSize;
}

unsigned int CTransaction::CalculateModifiedSize(unsigned int nTxSize) const
{
    // In order to avoid disincentivizing cleaning up the UTXO set we don't count
    // the constant overhead for each txin and up to 110 bytes of scriptSig (which
    // is enough to cover a compressed pubkey p2sh redemption) for priority.
    // Providing any more cleanup incentive than making additional inputs free would
    // risk encouraging people to create junk outputs to redeem later.
    if (nTxSize == 0)
        nTxSize = ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
    for (std::vector<CTxIn>::const_iterator it(vin.begin()); it != vin.end(); ++it)
    {
        unsigned int offset = 41U + std::min(110U, (unsigned int)it->scriptSig.size());
        if (nTxSize > offset)
            nTxSize -= offset;
    }
    return nTxSize;
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, vin.size=%u, vout.size=%u, nLockTime=%u)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        vin.size(),
        vout.size(),
        nLockTime);
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    return str;
}
