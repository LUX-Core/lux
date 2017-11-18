// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "wallet.h"

#include "base58.h"
#include "coincontrol.h"
#include "kernel.h"
#include "net.h"
#include "timedata.h"
#include "txdb.h"
#include "ui_interface.h"
#include "walletdb.h"
#include "crypter.h"
#include "key.h"
#include "spork.h"
#include "darksend.h"
#include "instantx.h"
#include "masternode.h"
#include "chainparams.h"
#include "smessage.h"

#include <boost/algorithm/string/replace.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/numeric/ublas/matrix.hpp>

using namespace std;

// Settings
int64_t nTransactionFee = MIN_TX_FEE;
int64_t nReserveBalance = 0;
int64_t nMinimumInputValue = 0;

static unsigned int GetStakeSplitAge() { return 9 * 24 * 60 * 60; }
static int64_t GetStakeCombineThreshold() { return 100 * COIN; }

int64_t gcd(int64_t n,int64_t m) { return m == 0 ? n : gcd(m, n % m); }
static uint64_t CoinWeightCost(const COutput &out)
{
    int64_t nTimeWeight = (int64_t)GetTime() - (int64_t)out.tx->nTime;
    CBigNum bnCoinDayWeight = CBigNum(out.tx->vout[out.i].nValue) * nTimeWeight / (24 * 60 * 60);
    return bnCoinDayWeight.getuint64();
}


// mapWallet


struct CompareValueOnly
{
    bool operator()(const pair<int64_t, pair<const CWalletTx*, unsigned int> >& t1,
                    const pair<int64_t, pair<const CWalletTx*, unsigned int> >& t2) const
    {
        return t1.first < t2.first;
    }
};

CPubKey CWallet::GenerateNewKey()
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    bool fCompressed = CanSupportFeature(FEATURE_COMPRPUBKEY); // default to compressed public keys if we want 0.6.0 wallets

    RandAddSeedPerfmon();
    CKey secret;
    secret.MakeNewKey(fCompressed);

    // Compressed public keys were introduced in version 0.6.0
    if (fCompressed)
        SetMinVersion(FEATURE_COMPRPUBKEY);

    CPubKey pubkey = secret.GetPubKey();

    // Create new metadata
    int64_t nCreationTime = GetTime();
    mapKeyMetadata[pubkey.GetID()] = CKeyMetadata(nCreationTime);
    if (!nTimeFirstKey || nCreationTime < nTimeFirstKey)
        nTimeFirstKey = nCreationTime;

    if (!AddKeyPubKey(secret, pubkey))
        throw std::runtime_error("CWallet::GenerateNewKey() : AddKey failed");
    return pubkey;
}

bool CWallet::AddKeyPubKey(const CKey& secret, const CPubKey &pubkey)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (!CCryptoKeyStore::AddKeyPubKey(secret, pubkey))
        return false;
    if (!fFileBacked)
        return true;
    if (!IsCrypted()) {
        return CWalletDB(strWalletFile).WriteKey(pubkey, secret.GetPrivKey(), mapKeyMetadata[pubkey.GetID()]);
    }
    return true;
}

bool CWallet::AddCryptedKey(const CPubKey &vchPubKey, const vector<unsigned char> &vchCryptedSecret)
{
    if (!CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret))
        return false;
    if (!fFileBacked)
        return true;
    {
        LOCK(cs_wallet);
        if (pwalletdbEncryption)
            return pwalletdbEncryption->WriteCryptedKey(vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
        else
            return CWalletDB(strWalletFile).WriteCryptedKey(vchPubKey, vchCryptedSecret, mapKeyMetadata[vchPubKey.GetID()]);
    }
    return false;
}

bool CWallet::LoadKeyMetadata(const CPubKey &pubkey, const CKeyMetadata &meta)
{
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    if (meta.nCreateTime && (!nTimeFirstKey || meta.nCreateTime < nTimeFirstKey))
        nTimeFirstKey = meta.nCreateTime;

    mapKeyMetadata[pubkey.GetID()] = meta;
    return true;
}

bool CWallet::LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    return CCryptoKeyStore::AddCryptedKey(vchPubKey, vchCryptedSecret);
}

bool CWallet::AddCScript(const CScript& redeemScript)
{
    if (!CCryptoKeyStore::AddCScript(redeemScript))
        return false;
    if (!fFileBacked)
        return true;
    return CWalletDB(strWalletFile).WriteCScript(Hash160(redeemScript), redeemScript);
}

// optional setting to unlock wallet for staking only
// serves to disable the trivial sendmoney when OS account compromised
// provides no real security
bool fWalletUnlockStakingOnly = false;

bool CWallet::LoadCScript(const CScript& redeemScript)
{
    /* A sanity check was added in pull #3843 to avoid adding redeemScripts
     * that never can be redeemed. However, old wallets may still contain
     * these. Do not add them to the wallet and warn. */
    if (redeemScript.size() > MAX_SCRIPT_ELEMENT_SIZE)
    {
        std::string strAddr = CBitcoinAddress(redeemScript.GetID()).ToString();
        LogPrintf("%s: Warning: This wallet contains a redeemScript of size %u which exceeds maximum size %i thus can never be redeemed. Do not use address %s.\n",
            __func__, redeemScript.size(), MAX_SCRIPT_ELEMENT_SIZE, strAddr);
        return true;
    }

    return CCryptoKeyStore::AddCScript(redeemScript);
}

bool CWallet::Lock()
{
    if (IsLocked())
        return true;

    if (fDebug)
        printf("Locking wallet.\n");

    {
        LOCK(cs_wallet);
        CWalletDB wdb(strWalletFile);

        // -- load encrypted spend_secret of stealth addresses
        CStealthAddress sxAddrTemp;
        std::set<CStealthAddress>::iterator it;
        for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
        {
            if (it->scan_secret.size() < 32)
                continue; // stealth address is not owned
            // -- CStealthAddress are only sorted on spend_pubkey
            CStealthAddress &sxAddr = const_cast<CStealthAddress&>(*it);
            if (fDebug)
                printf("Recrypting stealth key %s\n", sxAddr.Encoded().c_str());

            sxAddrTemp.scan_pubkey = sxAddr.scan_pubkey;
            if (!wdb.ReadStealthAddress(sxAddrTemp))
            {
                printf("Error: Failed to read stealth key from db %s\n", sxAddr.Encoded().c_str());
                continue;
            }
            sxAddr.spend_secret = sxAddrTemp.spend_secret;
        };
    }
    return LockKeyStore();
};

bool CWallet::Unlock(const SecureString& strWalletPassphrase, bool anonymizeOnly)
{
    SecureString strWalletPassphraseFinal;

    if(!IsLocked())
    {
	fWalletUnlockAnonymizeOnly = anonymizeOnly;
	return true;
    }

    strWalletPassphraseFinal = strWalletPassphrase;

    CCrypter crypter;
    CKeyingMaterial vMasterKey;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strWalletPassphraseFinal, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (!CCryptoKeyStore::Unlock(vMasterKey))
                return false;
	    break;
        }

	fWalletUnlockAnonymizeOnly = anonymizeOnly;
	UnlockStealthAddresses(vMasterKey);
	return true;
    }
    return false;
}

bool CWallet::ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase)
{
    bool fWasLocked = IsLocked();

    SecureString strOldWalletPassphraseFinal;
    strOldWalletPassphraseFinal = strOldWalletPassphrase;

    {
        LOCK(cs_wallet);
        Lock();

        CCrypter crypter;
        CKeyingMaterial vMasterKey;
        BOOST_FOREACH(MasterKeyMap::value_type& pMasterKey, mapMasterKeys)
        {
            if(!crypter.SetKeyFromPassphrase(strOldWalletPassphraseFinal, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                return false;
            if (!crypter.Decrypt(pMasterKey.second.vchCryptedKey, vMasterKey))
                return false;
            if (CCryptoKeyStore::Unlock(vMasterKey)
		&& UnlockStealthAddresses(vMasterKey))
            {
                int64_t nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = pMasterKey.second.nDeriveIterations * (100 / ((double)(GetTimeMillis() - nStartTime)));

                nStartTime = GetTimeMillis();
                crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod);
                pMasterKey.second.nDeriveIterations = (pMasterKey.second.nDeriveIterations + pMasterKey.second.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

                if (pMasterKey.second.nDeriveIterations < 25000)
                    pMasterKey.second.nDeriveIterations = 25000;

                LogPrintf("Wallet passphrase changed to an nDeriveIterations of %i\n", pMasterKey.second.nDeriveIterations);

                if (!crypter.SetKeyFromPassphrase(strNewWalletPassphrase, pMasterKey.second.vchSalt, pMasterKey.second.nDeriveIterations, pMasterKey.second.nDerivationMethod))
                    return false;
                if (!crypter.Encrypt(vMasterKey, pMasterKey.second.vchCryptedKey))
                    return false;
                CWalletDB(strWalletFile).WriteMasterKey(pMasterKey.first, pMasterKey.second);
                if (fWasLocked)
                    Lock();

                return true;
            }
        }
    }

    return false;
}

void CWallet::SetBestChain(const CBlockLocator& loc)
{
    CWalletDB walletdb(strWalletFile);
    walletdb.WriteBestBlock(loc);
}

bool CWallet::SetMinVersion(enum WalletFeature nVersion, CWalletDB* pwalletdbIn, bool fExplicit)
{
    LOCK(cs_wallet); // nWalletVersion
    if (nWalletVersion >= nVersion)
        return true;

    // when doing an explicit upgrade, if we pass the max version permitted, upgrade all the way
    if (fExplicit && nVersion > nWalletMaxVersion)
            nVersion = FEATURE_LATEST;

    nWalletVersion = nVersion;

    if (nVersion > nWalletMaxVersion)
        nWalletMaxVersion = nVersion;

    if (fFileBacked)
    {
        CWalletDB* pwalletdb = pwalletdbIn ? pwalletdbIn : new CWalletDB(strWalletFile);
        if (nWalletVersion > 40000)
            pwalletdb->WriteMinVersion(nWalletVersion);
        if (!pwalletdbIn)
            delete pwalletdb;
    }

    return true;
}

bool CWallet::SetMaxVersion(int nVersion)
{
    LOCK(cs_wallet); // nWalletVersion, nWalletMaxVersion
    // cannot downgrade below current version
    if (nWalletVersion > nVersion)
        return false;

    nWalletMaxVersion = nVersion;

    return true;
}

bool CWallet::EncryptWallet(const SecureString& strWalletPassphrase)
{
    if (IsCrypted())
        return false;

    CKeyingMaterial vMasterKey;
    RandAddSeedPerfmon();

    vMasterKey.resize(WALLET_CRYPTO_KEY_SIZE);
    RAND_bytes(&vMasterKey[0], WALLET_CRYPTO_KEY_SIZE);

    CMasterKey kMasterKey(nDerivationMethodIndex);

    RandAddSeedPerfmon();
    kMasterKey.vchSalt.resize(WALLET_CRYPTO_SALT_SIZE);
    RAND_bytes(&kMasterKey.vchSalt[0], WALLET_CRYPTO_SALT_SIZE);

    CCrypter crypter;
    int64_t nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, 25000, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = 2500000 / ((double)(GetTimeMillis() - nStartTime));

    nStartTime = GetTimeMillis();
    crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod);
    kMasterKey.nDeriveIterations = (kMasterKey.nDeriveIterations + kMasterKey.nDeriveIterations * 100 / ((double)(GetTimeMillis() - nStartTime))) / 2;

    if (kMasterKey.nDeriveIterations < 25000)
        kMasterKey.nDeriveIterations = 25000;

    LogPrintf("Encrypting Wallet with an nDeriveIterations of %i\n", kMasterKey.nDeriveIterations);

    if (!crypter.SetKeyFromPassphrase(strWalletPassphrase, kMasterKey.vchSalt, kMasterKey.nDeriveIterations, kMasterKey.nDerivationMethod))
        return false;
    if (!crypter.Encrypt(vMasterKey, kMasterKey.vchCryptedKey))
        return false;

    {
        LOCK(cs_wallet);
        mapMasterKeys[++nMasterKeyMaxID] = kMasterKey;
        if (fFileBacked)
        {
            pwalletdbEncryption = new CWalletDB(strWalletFile);
            if (!pwalletdbEncryption->TxnBegin())
                return false;
            pwalletdbEncryption->WriteMasterKey(nMasterKeyMaxID, kMasterKey);
        }

        if (!EncryptKeys(vMasterKey))
        {
            if (fFileBacked)
                pwalletdbEncryption->TxnAbort();
            exit(1); //We now probably have half of our keys encrypted in memory, and half not...die and let the user reload their unencrypted wallet.
        }

        std::set<CStealthAddress>::iterator it;
        for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
        {
            if (it->scan_secret.size() < 32)
                continue; // stealth address is not owned
            // -- CStealthAddress is only sorted on spend_pubkey
            CStealthAddress &sxAddr = const_cast<CStealthAddress&>(*it);

            if (fDebug)
                printf("Encrypting stealth key %s\n", sxAddr.Encoded().c_str());

            std::vector<unsigned char> vchCryptedSecret;

            CSecret vchSecret;
            vchSecret.resize(32);
            memcpy(&vchSecret[0], &sxAddr.spend_secret[0], 32);

            uint256 iv = Hash(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end());
            if (!EncryptSecret(vMasterKey, vchSecret, iv, vchCryptedSecret))
            {
                printf("Error: Failed encrypting stealth key %s\n", sxAddr.Encoded().c_str());
                continue;
            };

            sxAddr.spend_secret = vchCryptedSecret;
            pwalletdbEncryption->WriteStealthAddress(sxAddr);
        };

        // Encryption was introduced in version 0.4.0
        SetMinVersion(FEATURE_WALLETCRYPT, pwalletdbEncryption, true);

        if (fFileBacked)
        {
            if (!pwalletdbEncryption->TxnCommit())
                exit(1); //We now have keys encrypted in memory, but no on disk...die to avoid confusion and let the user reload their unencrypted wallet.

            delete pwalletdbEncryption;
            pwalletdbEncryption = NULL;
        }

        Lock();
        Unlock(strWalletPassphrase);
        NewKeyPool();
        Lock();

        // Need to completely rewrite the wallet file; if we don't, bdb might keep
        // bits of the unencrypted private key in slack space in the database file.
        CDB::Rewrite(strWalletFile);

    }
    NotifyStatusChanged(this);

    return true;
}

int64_t CWallet::IncOrderPosNext(CWalletDB *pwalletdb)
{
    AssertLockHeld(cs_wallet); // nOrderPosNext
    int64_t nRet = nOrderPosNext++;
    if (pwalletdb) {
        pwalletdb->WriteOrderPosNext(nOrderPosNext);
    } else {
        CWalletDB(strWalletFile).WriteOrderPosNext(nOrderPosNext);
    }
    return nRet;
}

CWallet::TxItems CWallet::OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount)
{
    AssertLockHeld(cs_wallet); // mapWallet
    CWalletDB walletdb(strWalletFile);

    // First: get all CWalletTx and CAccountingEntry into a sorted-by-order multimap.
    TxItems txOrdered;

    // Note: maintaining indices in the database of (account,time) --> txid and (account, time) --> acentry
    // would make this much faster for applications that do this a lot.
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        CWalletTx* wtx = &((*it).second);
        txOrdered.insert(make_pair(wtx->nOrderPos, TxPair(wtx, (CAccountingEntry*)0)));
    }
    acentries.clear();
    walletdb.ListAccountCreditDebit(strAccount, acentries);
    BOOST_FOREACH(CAccountingEntry& entry, acentries)
    {
        txOrdered.insert(make_pair(entry.nOrderPos, TxPair((CWalletTx*)0, &entry)));
    }

    return txOrdered;
}

void CWallet::WalletUpdateSpent(const CTransaction &tx, bool fBlock)
{
    // Anytime a signature is successfully verified, it's proof the outpoint is spent.
    // Update the wallet spent flag if it doesn't know due to wallet.dat being
    // restored from backup or the user making copies of wallet.dat.
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(const CTxIn& txin, tx.vin)
        {
            map<uint256, CWalletTx>::iterator mi = mapWallet.find(txin.prevout.hash);
            if (mi != mapWallet.end())
            {
                CWalletTx& wtx = (*mi).second;
                if (txin.prevout.n >= wtx.vout.size())
                    LogPrintf("WalletUpdateSpent: bad wtx %s\n", wtx.GetHash().ToString());
                else if (!wtx.IsSpent(txin.prevout.n) && IsMine(wtx.vout[txin.prevout.n]))
                {
                    LogPrintf("WalletUpdateSpent found spent coin %s BC %s\n", FormatMoney(wtx.GetCredit()), wtx.GetHash().ToString());
                    wtx.MarkSpent(txin.prevout.n);
                    wtx.WriteToDisk();
                    NotifyTransactionChanged(this, txin.prevout.hash, CT_UPDATED);
                }
            }
        }

        if (fBlock)
        {
            uint256 hash = tx.GetHash();
            map<uint256, CWalletTx>::iterator mi = mapWallet.find(hash);
            CWalletTx& wtx = (*mi).second;

            BOOST_FOREACH(const CTxOut& txout, tx.vout)
            {
                if (IsMine(txout))
                {
                    wtx.MarkUnspent(&txout - &tx.vout[0]);
                    wtx.WriteToDisk();
                    NotifyTransactionChanged(this, hash, CT_UPDATED);
                }
            }
        }

    }
}

void CWallet::MarkDirty()
{
    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
            item.second.MarkDirty();
    }
}

bool CWallet::AddToWallet(const CWalletTx& wtxIn)
{
    uint256 hash = wtxIn.GetHash();
    {
        LOCK(cs_wallet);
        // Inserts only if not already there, returns tx inserted or tx found
        pair<map<uint256, CWalletTx>::iterator, bool> ret = mapWallet.insert(make_pair(hash, wtxIn));
        CWalletTx& wtx = (*ret.first).second;
        wtx.BindWallet(this);
        bool fInsertedNew = ret.second;
        if (fInsertedNew)
        {
            wtx.nTimeReceived = GetAdjustedTime();
            wtx.nOrderPos = IncOrderPosNext();

            wtx.nTimeSmart = wtx.nTimeReceived;
            if (wtxIn.hashBlock != 0)
            {
                if (mapBlockIndex.count(wtxIn.hashBlock))
                {
                    unsigned int latestNow = wtx.nTimeReceived;
                    unsigned int latestEntry = 0;
                    {
                        // Tolerate times up to the last timestamp in the wallet not more than 5 minutes into the future
                        int64_t latestTolerated = latestNow + 300;
                        std::list<CAccountingEntry> acentries;
                        TxItems txOrdered = OrderedTxItems(acentries);
                        for (TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it)
                        {
                            CWalletTx *const pwtx = (*it).second.first;
                            if (pwtx == &wtx)
                                continue;
                            CAccountingEntry *const pacentry = (*it).second.second;
                            int64_t nSmartTime;
                            if (pwtx)
                            {
                                nSmartTime = pwtx->nTimeSmart;
                                if (!nSmartTime)
                                    nSmartTime = pwtx->nTimeReceived;
                            }
                            else
                                nSmartTime = pacentry->nTime;
                            if (nSmartTime <= latestTolerated)
                            {
                                latestEntry = nSmartTime;
                                if (nSmartTime > latestNow)
                                    latestNow = nSmartTime;
                                break;
                            }
                        }
                    }

                    unsigned int& blocktime = mapBlockIndex[wtxIn.hashBlock]->nTime;
                    wtx.nTimeSmart = std::max(latestEntry, std::min(blocktime, latestNow));
                }
                else
                    LogPrintf("AddToWallet() : found %s in block %s not in index\n",
                             wtxIn.GetHash().ToString(),
                             wtxIn.hashBlock.ToString());
            }
        }

        bool fUpdated = false;
        if (!fInsertedNew)
        {
            // Merge
            if (wtxIn.hashBlock != 0 && wtxIn.hashBlock != wtx.hashBlock)
            {
                wtx.hashBlock = wtxIn.hashBlock;
                fUpdated = true;
            }
            if (wtxIn.nIndex != -1 && (wtxIn.vMerkleBranch != wtx.vMerkleBranch || wtxIn.nIndex != wtx.nIndex))
            {
                wtx.vMerkleBranch = wtxIn.vMerkleBranch;
                wtx.nIndex = wtxIn.nIndex;
                fUpdated = true;
            }
            if (wtxIn.fFromMe && wtxIn.fFromMe != wtx.fFromMe)
            {
                wtx.fFromMe = wtxIn.fFromMe;
                fUpdated = true;
            }
            fUpdated |= wtx.UpdateSpent(wtxIn.vfSpent);
        }

        //// debug print
        LogPrintf("AddToWallet %s  %s%s\n", wtxIn.GetHash().ToString(), (fInsertedNew ? "new" : ""), (fUpdated ? "update" : ""));

        // Write to disk
        if (fInsertedNew || fUpdated)
            if (!wtx.WriteToDisk())
                return false;

        if (!fHaveGUI) {
            // If default receiving address gets used, replace it with a new one
            if (vchDefaultKey.IsValid()) {
                CScript scriptDefaultKey;
                scriptDefaultKey.SetDestination(vchDefaultKey.GetID());
                BOOST_FOREACH(const CTxOut& txout, wtx.vout)
                {
                    if (txout.scriptPubKey == scriptDefaultKey)
                    {
                        CPubKey newDefaultKey;
                        if (GetKeyFromPool(newDefaultKey))
                        {
                            SetDefaultKey(newDefaultKey);
                            SetAddressBookName(vchDefaultKey.GetID(), "");
                        }
                    }
                }
            }
        }
        // since AddToWallet is called directly for self-originating transactions, check for consumption of own coins
        WalletUpdateSpent(wtx, (wtxIn.hashBlock != 0));

        // Notify UI of new or updated transaction
        NotifyTransactionChanged(this, hash, fInsertedNew ? CT_NEW : CT_UPDATED);

        // notify an external script when a wallet transaction comes in or is updated
        std::string strCmd = GetArg("-walletnotify", "");

        if ( !strCmd.empty())
        {
            boost::replace_all(strCmd, "%s", wtxIn.GetHash().GetHex());
            boost::thread t(runCommand, strCmd); // thread runs free
        }

    }
    return true;
}

// Add a transaction to the wallet, or update it.
// pblock is optional, but should be provided if the transaction is known to be in a block.
// If fUpdate is true, existing transactions will be updated.
bool CWallet::AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate)
{
    uint256 hash = tx.GetHash();
    {
        LOCK(cs_wallet);
        bool fExisted = mapWallet.count(hash);
        if (fExisted && !fUpdate) return false;

        mapValue_t mapNarr;
        FindStealthTransactions(tx, mapNarr);

        if (fExisted || IsMine(tx) || IsFromMe(tx))
        {
            CWalletTx wtx(this,tx);

            if (!mapNarr.empty())
                wtx.mapValue.insert(mapNarr.begin(), mapNarr.end());

            // Get merkle branch if transaction was found in a block
            if (pblock)
                wtx.SetMerkleBranch(pblock);
            return AddToWallet(wtx);
        }
        else
            WalletUpdateSpent(tx);
    }
    return false;
}

void CWallet::SyncTransaction(const CTransaction& tx, const CBlock* pblock, bool fConnect) {
    if (!fConnect)
    {
        // wallets need to refund inputs when disconnecting coinstake
        if (tx.IsCoinStake())
        {
            if (IsFromMe(tx))
                DisableTransaction(tx);
        }
        return;
    }

    AddToWalletIfInvolvingMe(tx, pblock, true);
}

void CWallet::EraseFromWallet(const uint256 &hash)
{
    if (!fFileBacked)
        return;
    {
        LOCK(cs_wallet);
        if (mapWallet.erase(hash))
            CWalletDB(strWalletFile).EraseTx(hash);
    }
    return;
}


bool CWallet::IsMine(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]))
                    return true;
        }
    }
    return false;
}

int64_t CWallet::GetDebit(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size())
                if (IsMine(prev.vout[txin.prevout.n]))
                    return prev.vout[txin.prevout.n].nValue;
        }
    }
    return 0;
}


bool CWallet::IsDenominated(const CTxIn &txin) const
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            const CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size()) return IsDenominatedAmount(prev.vout[txin.prevout.n].nValue);
        }
    }
    return false;
}

bool CWallet::IsDenominatedAmount(int64_t nInputAmount) const
{
    BOOST_FOREACH(int64_t d, darkSendDenominations)
        if(nInputAmount == d)
            return true;
    return false;
}


bool CWallet::IsChange(const CTxOut& txout) const
{
    CTxDestination address;

    // TODO: fix handling of 'change' outputs. The assumption is that any
    // payment to a TX_PUBKEYHASH that is mine but isn't in the address book
    // is change. That assumption is likely to break when we implement multisignature
    // wallets that return change back into a multi-signature-protected address;
    // a better way of identifying which outputs are 'the send' and which are
    // 'the change' will need to be implemented (maybe extend CWalletTx to remember
    // which output, if any, was change).
    if (ExtractDestination(txout.scriptPubKey, address) && ::IsMine(*this, address))
    {
        LOCK(cs_wallet);
        if (!mapAddressBook.count(address))
            return true;
    }
    return false;
}

int64_t CWalletTx::GetTxTime() const
{
    int64_t n = nTimeSmart;
    return n ? n : nTimeReceived;
}

int CWalletTx::GetRequestCount() const
{
    // Returns -1 if it wasn't being tracked
    int nRequests = -1;
    {
        LOCK(pwallet->cs_wallet);
        if (IsCoinBase() || IsCoinStake())
        {
            // Generated block
            if (hashBlock != 0)
            {
                map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                if (mi != pwallet->mapRequestCount.end())
                    nRequests = (*mi).second;
            }
        }
        else
        {
            // Did anyone request this transaction?
            map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(GetHash());
            if (mi != pwallet->mapRequestCount.end())
            {
                nRequests = (*mi).second;

                // How about the block it's in?
                if (nRequests == 0 && hashBlock != 0)
                {
                    map<uint256, int>::const_iterator mi = pwallet->mapRequestCount.find(hashBlock);
                    if (mi != pwallet->mapRequestCount.end())
                        nRequests = (*mi).second;
                    else
                        nRequests = 1; // If it's in someone else's block it must have got out
                }
            }
        }
    }
    return nRequests;
}

void CWalletTx::GetAmounts(list<pair<CTxDestination, int64_t> >& listReceived,
                           list<pair<CTxDestination, int64_t> >& listSent, int64_t& nFee, string& strSentAccount) const
{
    LOCK(pwallet->cs_wallet);
    nFee = 0;
    listReceived.clear();
    listSent.clear();
    strSentAccount = strFromAccount;

    // Compute fee:
    int64_t nDebit = GetDebit();
    if (nDebit > 0) // debit>0 means we signed/sent this transaction
    {
        int64_t nValueOut = GetValueOut();
        nFee = nDebit - nValueOut;
    }

    // Sent/received.
    BOOST_FOREACH(const CTxOut& txout, vout)
    {
        // Skip special stake out
        if (txout.scriptPubKey.empty())
            continue;

        bool fIsMine;
        // Only need to handle txouts if AT LEAST one of these is true:
        //   1) they debit from us (sent)
        //   2) the output is to us (received)
        if (nDebit > 0)
        {
            // Don't report 'change' txouts
	    // LuxNOTE: CoinControl possible fix related... with HD wallet we need to report change?
            //if (pwallet->IsChange(txout))
            //    continue;
            fIsMine = pwallet->IsMine(txout);
        }
        else if (!(fIsMine = pwallet->IsMine(txout)))
            continue;

        // In either case, we need to get the destination address
        CTxDestination address;
        if (!ExtractDestination(txout.scriptPubKey, address))
        {
            LogPrintf("CWalletTx::GetAmounts: Unknown transaction type found, txid %s\n",
                     this->GetHash().ToString());
            address = CNoDestination();
        }

        // If we are debited by the transaction, add the output as a "sent" entry
        if (nDebit > 0)
            listSent.push_back(make_pair(address, txout.nValue));

        // If we are receiving the output, add it as a "received" entry
        if (fIsMine)
            listReceived.push_back(make_pair(address, txout.nValue));
    }

}

void CWalletTx::GetAccountAmounts(const string& strAccount, int64_t& nReceived,
                                  int64_t& nSent, int64_t& nFee) const
{
    LOCK(pwallet->cs_wallet);
    nReceived = nSent = nFee = 0;

    int64_t allFee;
    string strSentAccount;
    list<pair<CTxDestination, int64_t> > listReceived;
    list<pair<CTxDestination, int64_t> > listSent;
    GetAmounts(listReceived, listSent, allFee, strSentAccount);

    if (strAccount == strSentAccount)
    {
        BOOST_FOREACH(const PAIRTYPE(CTxDestination,int64_t)& s, listSent)
            nSent += s.second;
        nFee = allFee;
    }
    {

        BOOST_FOREACH(const PAIRTYPE(CTxDestination,int64_t)& r, listReceived)
        {
            if (pwallet->mapAddressBook.count(r.first))
            {
                map<CTxDestination, string>::const_iterator mi = pwallet->mapAddressBook.find(r.first);
                if (mi != pwallet->mapAddressBook.end() && (*mi).second == strAccount)
                    nReceived += r.second;
            }
            else if (strAccount.empty())
            {
                nReceived += r.second;
            }
        }
    }
}

void CWalletTx::AddSupportingTransactions(CTxDB& txdb)
{
    vtxPrev.clear();

    const int COPY_DEPTH = 3;
    if (SetMerkleBranch() < COPY_DEPTH)
    {
        vector<uint256> vWorkQueue;
        BOOST_FOREACH(const CTxIn& txin, vin)
            vWorkQueue.push_back(txin.prevout.hash);

        // This critsect is OK because txdb is already open
        {
            LOCK(pwallet->cs_wallet);
            map<uint256, const CMerkleTx*> mapWalletPrev;
            set<uint256> setAlreadyDone;
            for (unsigned int i = 0; i < vWorkQueue.size(); i++)
            {
                uint256 hash = vWorkQueue[i];
                if (setAlreadyDone.count(hash))
                    continue;
                setAlreadyDone.insert(hash);

                CMerkleTx tx;
                map<uint256, CWalletTx>::const_iterator mi = pwallet->mapWallet.find(hash);
                if (mi != pwallet->mapWallet.end())
                {
                    tx = (*mi).second;
                    BOOST_FOREACH(const CMerkleTx& txWalletPrev, (*mi).second.vtxPrev)
                        mapWalletPrev[txWalletPrev.GetHash()] = &txWalletPrev;
                }
                else if (mapWalletPrev.count(hash))
                {
                    tx = *mapWalletPrev[hash];
                }
                else if (txdb.ReadDiskTx(hash, tx))
                {
                    ;
                }
                else
                {
                    LogPrintf("ERROR: AddSupportingTransactions() : unsupported transaction\n");
                    continue;
                }

                int nDepth = tx.SetMerkleBranch();
                vtxPrev.push_back(tx);

                if (nDepth < COPY_DEPTH)
                {
                    BOOST_FOREACH(const CTxIn& txin, tx.vin)
                        vWorkQueue.push_back(txin.prevout.hash);
                }
            }
        }
    }

    reverse(vtxPrev.begin(), vtxPrev.end());
}

bool CWalletTx::WriteToDisk()
{
    return CWalletDB(pwallet->strWalletFile).WriteTx(GetHash(), *this);
}

// Scan the block chain (starting in pindexStart) for transactions
// from or to us. If fUpdate is true, found transactions that already
// exist in the wallet will be updated.
int CWallet::ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate)
{
    int ret = 0;

    CBlockIndex* pindex = pindexStart;
    {
        LOCK2(cs_main, cs_wallet);
        while (pindex)
        {
            // no need to read and scan block, if block was created before
            // our wallet birthday (as adjusted for block time variability)
            if (nTimeFirstKey && (pindex->nTime < (nTimeFirstKey - 7200))) {
                pindex = pindex->pnext;
                continue;
            }

            CBlock block;
            block.ReadFromDisk(pindex, true);
            BOOST_FOREACH(CTransaction& tx, block.vtx)
            {
                if (AddToWalletIfInvolvingMe(tx, &block, fUpdate))
                    ret++;
            }
            pindex = pindex->pnext;
        }
    }
    return ret;
}

void CWallet::ReacceptWalletTransactions()
{
    CTxDB txdb("r");
    bool fRepeat = true;
    while (fRepeat)
    {
        LOCK2(cs_main, cs_wallet);
        fRepeat = false;
        vector<CDiskTxPos> vMissingTx;
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            if ((wtx.IsCoinBase() && wtx.IsSpent(0)) || (wtx.IsCoinStake() && wtx.IsSpent(1)))
                continue;

            CTxIndex txindex;
            bool fUpdated = false;
            if (txdb.ReadTxIndex(wtx.GetHash(), txindex))
            {
                // Update fSpent if a tx got spent somewhere else by a copy of wallet.dat
                if (txindex.vSpent.size() != wtx.vout.size())
                {
                    LogPrintf("ERROR: ReacceptWalletTransactions() : txindex.vSpent.size() %u != wtx.vout.size() %u\n", txindex.vSpent.size(), wtx.vout.size());
                    continue;
                }
                for (unsigned int i = 0; i < txindex.vSpent.size(); i++)
                {
                    if (wtx.IsSpent(i))
                        continue;
                    if (!txindex.vSpent[i].IsNull() && IsMine(wtx.vout[i]))
                    {
                        wtx.MarkSpent(i);
                        fUpdated = true;
                        vMissingTx.push_back(txindex.vSpent[i]);
                    }
                }
                if (fUpdated)
                {
                    LogPrintf("ReacceptWalletTransactions found spent coin %s BC %s\n", FormatMoney(wtx.GetCredit()), wtx.GetHash().ToString());
                    wtx.MarkDirty();
                    wtx.WriteToDisk();
                }
            }
            else
            {
                // Re-accept any txes of ours that aren't already in a block
                if (!(wtx.IsCoinBase() || wtx.IsCoinStake()))
                    wtx.AcceptWalletTransaction(txdb);
            }
        }
        if (!vMissingTx.empty())
        {
            // TODO: optimize this to scan just part of the block chain?
            if (ScanForWalletTransactions(pindexGenesisBlock))
                fRepeat = true;  // Found missing transactions: re-do re-accept.
        }
    }
}

void CWalletTx::RelayWalletTransaction(CTxDB& txdb)
{
    BOOST_FOREACH(const CMerkleTx& tx, vtxPrev)
    {
        if (!(tx.IsCoinBase() || tx.IsCoinStake()))
        {
            uint256 hash = tx.GetHash();
            if (!txdb.ContainsTx(hash))
                RelayTransaction((CTransaction)tx, hash);
        }
    }
    if (!(IsCoinBase() || IsCoinStake()))
    {
        uint256 hash = GetHash();
        if (!txdb.ContainsTx(hash))
        {
            LogPrintf("Relaying wtx %s\n", hash.ToString());
            RelayTransaction((CTransaction)*this, hash);
        }
    }
}

void CWalletTx::RelayWalletTransaction()
{
   CTxDB txdb("r");
   RelayWalletTransaction(txdb);
}

void CWallet::ResendWalletTransactions(bool fForce)
{
    if (!fForce)
    {
        // Do this infrequently and randomly to avoid giving away
        // that these are our transactions.
        static int64_t nNextTime;
        if (GetTime() < nNextTime)
            return;
        bool fFirst = (nNextTime == 0);
        nNextTime = GetTime() + GetRand(30 * 60);
        if (fFirst)
            return;

        // Only do it if there's been a new block since last time
        static int64_t nLastTime;
        if (nTimeBestReceived < nLastTime)
            return;
        nLastTime = GetTime();
    }

    // Rebroadcast any of our txes that aren't in a block yet
    LogPrintf("ResendWalletTransactions()\n");
    CTxDB txdb("r");
    {
        LOCK(cs_wallet);
        // Sort them in chronological order
        multimap<unsigned int, CWalletTx*> mapSorted;
        BOOST_FOREACH(PAIRTYPE(const uint256, CWalletTx)& item, mapWallet)
        {
            CWalletTx& wtx = item.second;
            // Don't rebroadcast until it's had plenty of time that
            // it should have gotten in already by now.
            if (fForce || nTimeBestReceived - (int64_t)wtx.nTimeReceived > 5 * 60)
                mapSorted.insert(make_pair(wtx.nTimeReceived, &wtx));
        }
        BOOST_FOREACH(PAIRTYPE(const unsigned int, CWalletTx*)& item, mapSorted)
        {
            CWalletTx& wtx = *item.second;
            if (wtx.CheckTransaction())
                wtx.RelayWalletTransaction(txdb);
            else
                LogPrintf("ResendWalletTransactions() : CheckTransaction failed for transaction %s\n", wtx.GetHash().ToString());
        }
    }
}






//////////////////////////////////////////////////////////////////////////////
//
// Actions
//


int64_t CWallet::GetBalance() const
{
    int64_t nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsTrusted())
                nTotal += pcoin->GetAvailableCredit();
        }
    }

    return nTotal;
}

int64_t CWallet::GetBalanceNoLocks() const
{
    int64_t nTotal = 0;
    {
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsTrusted())
                nTotal += pcoin->GetAvailableCredit();
        }
    }

    return nTotal;
}

CAmount CWallet::GetAnonymizedBalance() const
{
    int64_t nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            if (pcoin->IsTrusted())
            {
                int nDepth = pcoin->GetDepthInMainChain();

                for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
		//isminetype mine = IsMine(pcoin->vout[i]);
		bool mine = IsMine(pcoin->vout[i]);
                    //COutput out = COutput(pcoin, i, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO);
		COutput out = COutput(pcoin, i, nDepth, mine);
                    CTxIn vin = CTxIn(out.tx->GetHash(), out.i);

                    //if(IsSpent(out.tx->GetHash(), i) || !IsMine(pcoin->vout[i]) || !IsDenominated(vin)) continue;
		    if(pcoin->IsSpent(i) || !IsMine(pcoin->vout[i]) || !IsDenominated(vin)) continue;

                    int rounds = GetInputLuxsendRounds(vin);
                    if(rounds >= nLuxsendRounds){
                        nTotal += pcoin->vout[i].nValue;
                    }
                }
            }
        }
    }

    return nTotal;
}

double CWallet::GetAverageAnonymizedRounds() const
{
    double fTotal = 0;
    double fCount = 0;

    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            if (pcoin->IsTrusted())
            {
                int nDepth = pcoin->GetDepthInMainChain();

                for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
		//isminetype mine = IsMine(pcoin->vout[i]);
		    bool mine = IsMine(pcoin->vout[i]);
                    //COutput out = COutput(pcoin, i, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO);
		    COutput out = COutput(pcoin, i, nDepth, mine);
                    CTxIn vin = CTxIn(out.tx->GetHash(), out.i);

                    //if(IsSpent(out.tx->GetHash(), i) || !IsMine(pcoin->vout[i]) || !IsDenominated(vin)) continue;
		    if(pcoin->IsSpent(i) || !IsMine(pcoin->vout[i]) || !IsDenominated(vin)) continue;

                    int rounds = GetInputLuxsendRounds(vin);
                    fTotal += (float)rounds;
                    fCount += 1;
                }
            }
        }
    }

    if(fCount == 0) return 0;

    return fTotal/fCount;
}

CAmount CWallet::GetNormalizedAnonymizedBalance() const
{
    int64_t nTotal = 0;

    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            if (pcoin->IsTrusted())
            {
                int nDepth = pcoin->GetDepthInMainChain();

                for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
		    //isminetype mine = IsMine(pcoin->vout[i]);
		    bool mine = IsMine(pcoin->vout[i]);
                    //COutput out = COutput(pcoin, i, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO);
		    COutput out = COutput(pcoin, i, nDepth, mine);
                    CTxIn vin = CTxIn(out.tx->GetHash(), out.i);

                    //if(IsSpent(out.tx->GetHash(), i) || !IsMine(pcoin->vout[i]) || !IsDenominated(vin)) continue;
		    if(pcoin->IsSpent(i) || !IsMine(pcoin->vout[i]) || !IsDenominated(vin)) continue;

                    int rounds = GetInputLuxsendRounds(vin);
                    nTotal += pcoin->vout[i].nValue * rounds / nLuxsendRounds;
                }
            }
        }
    }

    return nTotal;
}

CAmount CWallet::GetDenominatedBalance(bool onlyDenom, bool onlyUnconfirmed) const
{
    int64_t nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            int nDepth = pcoin->GetDepthInMainChain();

            // skip conflicted
            if(nDepth < 0) continue;

            bool unconfirmed = (!IsFinalTx(*pcoin) || (!pcoin->IsTrusted() && nDepth == 0));
            if(onlyUnconfirmed != unconfirmed) continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
				//isminetype mine = IsMine(pcoin->vout[i]);
		//bool mine = IsMine(pcoin->vout[i]);
                //COutput out = COutput(pcoin, i, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO);
		//COutput out = COutput(pcoin, i, nDepth, mine);

                //if(IsSpent(out.tx->GetHash(), i)) continue;
		if(pcoin->IsSpent(i)) continue;
                if(!IsMine(pcoin->vout[i])) continue;
                if(onlyDenom != IsDenominatedAmount(pcoin->vout[i].nValue)) continue;

                nTotal += pcoin->vout[i].nValue;
            }
        }
    }



    return nTotal;
}


int64_t CWallet::GetUnconfirmedBalance() const
{
    int64_t nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (!IsFinalTx(*pcoin) || (!pcoin->IsTrusted() && pcoin->GetDepthInMainChain() == 0))
                nTotal += pcoin->GetAvailableCredit();
        }
    }
    return nTotal;
}

int64_t CWallet::GetImmatureBalance() const
{
    int64_t nTotal = 0;
    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx& pcoin = (*it).second;
            if (pcoin.IsCoinBase() && pcoin.GetBlocksToMaturity() > 0 && pcoin.IsInMainChain())
                nTotal += GetCredit(pcoin);
        }
    }
    return nTotal;
}


// populate vCoins with vector of spendable COutputs
void CWallet::AvailableCoins(vector<COutput>& vCoins, bool fOnlyConfirmed, const CCoinControl *coinControl, AvailableCoinsType coin_type, bool useIX) const
{
    vCoins.clear();

    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            if (!IsFinalTx(*pcoin))
                continue;

            if (fOnlyConfirmed && !pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            if(pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth <= 0) // LuxNOTE: coincontrol fix / ignore 0 confirm
                continue;

           /* for (unsigned int i = 0; i < pcoin->vout.size(); i++)
                if (!(pcoin->IsSpent(i)) && IsMine(pcoin->vout[i]) && pcoin->vout[i].nValue >= nMinimumInputValue &&
                (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                    vCoins.push_back(COutput(pcoin, i, nDepth));*/
            // do not use IX for inputs that have less then 6 blockchain confirmations
            if (useIX && nDepth < 6)
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                bool found = false;
                if(coin_type == ONLY_DENOMINATED) {
                    //should make this a vector

                    found = IsDenominatedAmount(pcoin->vout[i].nValue);
                } else if(coin_type == ONLY_NONDENOMINATED || coin_type == ONLY_NONDENOMINATED_NOTMN) {
                    found = true;
                    if (IsCollateralAmount(pcoin->vout[i].nValue)) continue; // do not use collateral amounts
                    found = !IsDenominatedAmount(pcoin->vout[i].nValue);
                    if(found && coin_type == ONLY_NONDENOMINATED_NOTMN) found = (pcoin->vout[i].nValue != GetMNCollateral(pindexBest->nHeight)*COIN); // do not use MN funds
                } else {
                    found = true;
                }
                if(!found) continue;

				//isminetype mine = IsMine(pcoin->vout[i]);
		bool mine = IsMine(pcoin->vout[i]);

                //if (!(IsSpent(wtxid, i)) && mine != ISMINE_NO &&
                //    !IsLockedCoin((*it).first, i) && pcoin->vout[i].nValue > 0 &&
                //    (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                //        vCoins.push_back(COutput(pcoin, i, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO));
		//if (!(IsSpent(wtxid, i)) && mine &&
		if (!(pcoin->IsSpent(i)) && mine &&
                    !IsLockedCoin((*it).first, i) && pcoin->vout[i].nValue > 0 &&
                    (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                        vCoins.push_back(COutput(pcoin, i, nDepth, mine));
            }
        }
    }
}

void CWallet::AvailableCoinsMN(vector<COutput>& vCoins, bool fOnlyConfirmed, const CCoinControl *coinControl, AvailableCoinsType coin_type, bool useIX) const
{
    vCoins.clear();

    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            if (!IsFinalTx(*pcoin))
                continue;

            if (fOnlyConfirmed && !pcoin->IsTrusted())
                continue;

            if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            if(pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth <= 0) // LuxNOTE: coincontrol fix / ignore 0 confirm
                continue;

           /* for (unsigned int i = 0; i < pcoin->vout.size(); i++)
                if (!(pcoin->IsSpent(i)) && IsMine(pcoin->vout[i]) && pcoin->vout[i].nValue >= nMinimumInputValue &&
                (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                    vCoins.push_back(COutput(pcoin, i, nDepth));*/
            // do not use IX for inputs that have less then 6 blockchain confirmations
            if (useIX && nDepth < 6)
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
                bool found = false;
                if(coin_type == ONLY_DENOMINATED) {
                    //should make this a vector

                    found = IsDenominatedAmount(pcoin->vout[i].nValue);
                } else if(coin_type == ONLY_NONDENOMINATED || coin_type == ONLY_NONDENOMINATED_NOTMN) {
                    found = true;
                    if (IsCollateralAmount(pcoin->vout[i].nValue)) continue; // do not use collateral amounts
                    found = !IsDenominatedAmount(pcoin->vout[i].nValue);
                     if(found && coin_type == ONLY_NONDENOMINATED_NOTMN) found = (pcoin->vout[i].nValue != GetMNCollateral(pindexBest->nHeight)*COIN); // do not use MN funds
                } else {
                    found = true;
                }
                if(!found) continue;

				//isminetype mine = IsMine(pcoin->vout[i]);
		bool mine = IsMine(pcoin->vout[i]);

                //if (!(IsSpent(wtxid, i)) && mine != ISMINE_NO &&
                //    !IsLockedCoin((*it).first, i) && pcoin->vout[i].nValue > 0 &&
                //    (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                //        vCoins.push_back(COutput(pcoin, i, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO));
		//if (!(IsSpent(wtxid, i)) && mine &&
		if (!(pcoin->IsSpent(i)) &&
                    !IsLockedCoin((*it).first, i) && pcoin->vout[i].nValue > 0 &&
                    (!coinControl || !coinControl->HasSelected() || coinControl->IsSelected((*it).first, i)))
                        vCoins.push_back(COutput(pcoin, i, nDepth, mine));
            }
        }
    }
}

void CWallet::AvailableCoinsForStaking(vector<COutput>& vCoins, unsigned int nSpendTime) const
{
    vCoins.clear();

    {
        LOCK2(cs_main, cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;

            // Filtering by tx timestamp instead of block timestamp may give false positives but never false negatives
            if (pcoin->nTime + nStakeMinAge > nSpendTime)
                continue;

            if (pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < 1)
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
                if (!(pcoin->IsSpent(i)) && IsMine(pcoin->vout[i]) && pcoin->vout[i].nValue >= nMinimumInputValue)
                    vCoins.push_back(COutput(pcoin, i, nDepth, true));
        }
    }
}

static void ApproximateBestSubset(vector<pair<int64_t, pair<const CWalletTx*,unsigned int> > >vValue, int64_t nTotalLower, int64_t nTargetValue,
                                  vector<char>& vfBest, int64_t& nBest, int iterations = 1000)
{
    vector<char> vfIncluded;

    vfBest.assign(vValue.size(), true);
    nBest = nTotalLower;

    seed_insecure_rand();

    for (int nRep = 0; nRep < iterations && nBest != nTargetValue; nRep++)
    {
        vfIncluded.assign(vValue.size(), false);
        int64_t nTotal = 0;
        bool fReachedTarget = false;
        for (int nPass = 0; nPass < 2 && !fReachedTarget; nPass++)
        {
            for (unsigned int i = 0; i < vValue.size(); i++)
            {
                //The solver here uses a randomized algorithm,
                //the randomness serves no real security purpose but is just
                //needed to prevent degenerate behavior and it is important
                //that the rng fast. We do not use a constant random sequence,
                //because there may be some privacy improvement by making
                //the selection random.
                if (nPass == 0 ? insecure_rand()&1 : !vfIncluded[i])
                {
                    nTotal += vValue[i].first;
                    vfIncluded[i] = true;
                    if (nTotal >= nTargetValue)
                    {
                        fReachedTarget = true;
                        if (nTotal < nBest)
                        {
                            nBest = nTotal;
                            vfBest = vfIncluded;
                        }
                        nTotal -= vValue[i].first;
                        vfIncluded[i] = false;
                    }
                }
            }
        }
    }
}

// ppcoin: total coins staked (non-spendable until maturity)
int64_t CWallet::GetStake() const
{
    int64_t nTotal = 0;
    LOCK2(cs_main, cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinStake() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin);
    }
    return nTotal;
}

int64_t CWallet::GetNewMint() const
{
    int64_t nTotal = 0;
    LOCK2(cs_main, cs_wallet);
    for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
    {
        const CWalletTx* pcoin = &(*it).second;
        if (pcoin->IsCoinBase() && pcoin->GetBlocksToMaturity() > 0 && pcoin->GetDepthInMainChain() > 0)
            nTotal += CWallet::GetCredit(*pcoin);
    }
    return nTotal;
}

struct LargerOrEqualThanThreshold
{
    int64_t threshold;
    LargerOrEqualThanThreshold(int64_t threshold) : threshold(threshold) {}
    bool operator()(pair<pair<int64_t,int64_t>,pair<const CWalletTx*,unsigned int> > const &v) const { return v.first.first >= threshold; }
};

bool CWallet::SelectCoinsMinConfByCoinAge(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    vector<pair<COutput, uint64_t> > mCoins;
    BOOST_FOREACH(const COutput& out, vCoins)
    {
        mCoins.push_back(std::make_pair(out, CoinWeightCost(out)));
    }

    // List of values less than target
    pair<pair<int64_t,int64_t>, pair<const CWalletTx*,unsigned int> > coinLowestLarger;
    coinLowestLarger.first.second = std::numeric_limits<int64_t>::max();
    coinLowestLarger.second.first = NULL;
    vector<pair<pair<int64_t,int64_t>,pair<const CWalletTx*,unsigned int> > > vValue;
    int64_t nTotalLower = 0;
    boost::sort(mCoins, boost::bind(&std::pair<COutput, uint64_t>::second, _1) < boost::bind(&std::pair<COutput, uint64_t>::second, _2));

    BOOST_FOREACH(const PAIRTYPE(COutput, uint64_t)& output, mCoins)
    {
        const CWalletTx *pcoin = output.first.tx;

        if (output.first.nDepth < (pcoin->IsFromMe() ? nConfMine : nConfTheirs))
            continue;

        int i = output.first.i;

        // Follow the timestamp rules
        if (pcoin->nTime > nSpendTime)
            continue;

        int64_t n = pcoin->vout[i].nValue;

        pair<pair<int64_t,int64_t>,pair<const CWalletTx*,unsigned int> > coin = make_pair(make_pair(n,output.second),make_pair(pcoin, i));

        if (n < nTargetValue + CENT)
        {
            vValue.push_back(coin);
            nTotalLower += n;
        }
        else if (output.second < (uint64_t)coinLowestLarger.first.second)
        {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower < nTargetValue)
    {
        if (coinLowestLarger.second.first == NULL)
            return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first.first;
        return true;
    }

    // Calculate dynamic programming matrix
    int64_t nTotalValue = vValue[0].first.first;
    int64_t nGCD = vValue[0].first.first;
    for (unsigned int i = 1; i < vValue.size(); ++i)
    {
        nGCD = gcd(vValue[i].first.first, nGCD);
        nTotalValue += vValue[i].first.first;
    }
    nGCD = gcd(nTargetValue, nGCD);
    int64_t denom = nGCD;
    const int64_t k = 25;
    const int64_t approx = int64_t(vValue.size() * (nTotalValue - nTargetValue)) / k;
    if (approx > nGCD)
    {
        denom = approx; // apply approximation
    }
    if (fDebug) cerr << "nGCD " << nGCD << " denom " << denom << " k " << k << endl;

    if (nTotalValue == nTargetValue)
    {
        for (unsigned int i = 0; i < vValue.size(); ++i)
        {
            setCoinsRet.insert(vValue[i].second);
        }
        nValueRet = nTotalValue;
        return true;
    }

    size_t nBeginBundles = vValue.size();
    size_t nTotalCoinValues = vValue.size();
    size_t nBeginCoinValues = 0;
    int64_t costsum = 0;
    vector<vector<pair<pair<int64_t,int64_t>,pair<const CWalletTx*,unsigned int> > >::iterator> vZeroValueBundles;
    if (denom != nGCD)
    {
        // All coin outputs that with zero value will always be added by the dynamic programming routine
        // So we collect them into bundles of value denom
        vector<pair<pair<int64_t,int64_t>,pair<const CWalletTx*,unsigned int> > >::iterator itZeroValue = std::stable_partition(vValue.begin(), vValue.end(), LargerOrEqualThanThreshold(denom));
        vZeroValueBundles.push_back(itZeroValue);
        pair<int64_t, int64_t> pBundle = make_pair(0, 0);
        nBeginBundles = itZeroValue - vValue.begin();
        nTotalCoinValues = nBeginBundles;
        while (itZeroValue != vValue.end())
        {
            pBundle.first += itZeroValue->first.first;
            pBundle.second += itZeroValue->first.second;
            itZeroValue++;
            if (pBundle.first >= denom)
            {
                vZeroValueBundles.push_back(itZeroValue);
                vValue[nTotalCoinValues].first = pBundle;
                pBundle = make_pair(0, 0);
                nTotalCoinValues++;
            }
        }
        // We need to recalculate the total coin value due to truncation of integer division
        nTotalValue = 0;
        for (unsigned int i = 0; i < nTotalCoinValues; ++i)
        {
            nTotalValue += vValue[i].first.first / denom;
        }
        // Check if dynamic programming is still applicable with the approximation
        if (nTargetValue/denom >= nTotalValue)
        {
            // We lose too much coin value through the approximation, i.e. the residual of the previous recalculation is too large
            // Since the partitioning of the previously sorted list is stable, we can just pick the first coin outputs in the list until we have a valid target value
            for (; nBeginCoinValues < nTotalCoinValues && (nTargetValue - nValueRet)/denom >= nTotalValue; ++nBeginCoinValues)
            {
                if (nBeginCoinValues >= nBeginBundles)
                {
                    if (fDebug) cerr << "prepick bundle item " << FormatMoney(vValue[nBeginCoinValues].first.first) << " normalized " << vValue[nBeginCoinValues].first.first / denom << " cost " << vValue[nBeginCoinValues].first.second << endl;
                    const size_t nBundle = nBeginCoinValues - nBeginBundles;
                    for (vector<pair<pair<int64_t,int64_t>,pair<const CWalletTx*,unsigned int> > >::iterator it = vZeroValueBundles[nBundle]; it != vZeroValueBundles[nBundle + 1]; ++it)
                    {
                        setCoinsRet.insert(it->second);
                    }
                }
                else
                {
                    if (fDebug) cerr << "prepicking " << FormatMoney(vValue[nBeginCoinValues].first.first) << " normalized " << vValue[nBeginCoinValues].first.first / denom << " cost " << vValue[nBeginCoinValues].first.second << endl;
                    setCoinsRet.insert(vValue[nBeginCoinValues].second);
                }
                nTotalValue -= vValue[nBeginCoinValues].first.first / denom;
                nValueRet += vValue[nBeginCoinValues].first.first;
                costsum += vValue[nBeginCoinValues].first.second;
            }
            if (nValueRet >= nTargetValue)
            {
                    if (fDebug) cerr << "Done without dynprog: " << "requested " << FormatMoney(nTargetValue) << "\tnormalized " << nTargetValue/denom + (nTargetValue % denom != 0 ? 1 : 0) << "\tgot " << FormatMoney(nValueRet) << "\tcost " << costsum << endl;
                    return true;
            }
        }
    }
    else
    {
        nTotalValue /= denom;
    }

    uint64_t nAppend = 1;
    if ((nTargetValue - nValueRet) % denom != 0)
    {
        // We need to decrease the capacity because of integer truncation
        nAppend--;
    }

    // The capacity (number of columns) corresponds to the amount of coin value we are allowed to discard
    boost::numeric::ublas::matrix<uint64_t> M((nTotalCoinValues - nBeginCoinValues) + 1, (nTotalValue - (nTargetValue - nValueRet)/denom) + nAppend, std::numeric_limits<int64_t>::max());
    boost::numeric::ublas::matrix<unsigned int> B((nTotalCoinValues - nBeginCoinValues) + 1, (nTotalValue - (nTargetValue - nValueRet)/denom) + nAppend);
    for (unsigned int j = 0; j < M.size2(); ++j)
    {
        M(0,j) = 0;
    }
    for (unsigned int i = 1; i < M.size1(); ++i)
    {
        uint64_t nWeight = vValue[nBeginCoinValues + i - 1].first.first / denom;
        uint64_t nValue = vValue[nBeginCoinValues + i - 1].first.second;
        //cerr << "Weight " << nWeight << " Value " << nValue << endl;
        for (unsigned int j = 0; j < M.size2(); ++j)
        {
            B(i, j) = j;
            if (nWeight <= j)
            {
                uint64_t nStep = M(i - 1, j - nWeight) + nValue;
                if (M(i - 1, j) >= nStep)
                {
                    M(i, j) = M(i - 1, j);
                }
                else
                {
                    M(i, j) = nStep;
                    B(i, j) = j - nWeight;
                }
            }
            else
            {
                M(i, j) = M(i - 1, j);
            }
        }
    }
    // Trace back optimal solution
    int64_t nPrev = M.size2() - 1;
    for (unsigned int i = M.size1() - 1; i > 0; --i)
    {
        //cerr << i - 1 << " " << vValue[i - 1].second.second << " " << vValue[i - 1].first.first << " " << vValue[i - 1].first.second << " " << nTargetValue << " " << nPrev << " " << (nPrev == B(i, nPrev) ? "XXXXXXXXXXXXXXX" : "") << endl;
        if (nPrev == B(i, nPrev))
        {
            const size_t nValue = nBeginCoinValues + i - 1;
            // Check if this is a bundle
            if (nValue >= nBeginBundles)
            {
                if (fDebug) cerr << "pick bundle item " << FormatMoney(vValue[nValue].first.first) << " normalized " << vValue[nValue].first.first / denom << " cost " << vValue[nValue].first.second << endl;
                const size_t nBundle = nValue - nBeginBundles;
                for (vector<pair<pair<int64_t,int64_t>,pair<const CWalletTx*,unsigned int> > >::iterator it = vZeroValueBundles[nBundle]; it != vZeroValueBundles[nBundle + 1]; ++it)
                {
                    setCoinsRet.insert(it->second);
                }
            }
            else
            {
                if (fDebug) cerr << "pick " << nValue << " value " << FormatMoney(vValue[nValue].first.first) << " normalized " << vValue[nValue].first.first / denom << " cost " << vValue[nValue].first.second << endl;
                setCoinsRet.insert(vValue[nValue].second);
            }
            nValueRet += vValue[nValue].first.first;
            costsum += vValue[nValue].first.second;
        }
        nPrev = B(i, nPrev);
    }
    if (nValueRet < nTargetValue && !vZeroValueBundles.empty())
    {
        // If we get here it means that there are either not sufficient funds to pay the transaction or that there are small coin outputs left that couldn't be bundled
        // We try to fulfill the request by adding these small coin outputs
        for (vector<pair<pair<int64_t,int64_t>,pair<const CWalletTx*,unsigned int> > >::iterator it = vZeroValueBundles.back(); it != vValue.end() && nValueRet < nTargetValue; ++it)
        {
             setCoinsRet.insert(it->second);
             nValueRet += it->first.first;
        }
    }
    if (fDebug) cerr << "requested " << FormatMoney(nTargetValue) << "\tnormalized " << nTargetValue/denom + (nTargetValue % denom != 0 ? 1 : 0) << "\tgot " << FormatMoney(nValueRet) << "\tcost " << costsum << endl;
    if (fDebug) cerr << "M " << M.size1() << "x" << M.size2() << "; vValue.size() = " << vValue.size() << endl;
    return true;
}

// TODO: find appropriate place for this sort function
// move denoms down
bool less_then_denom (const COutput& out1, const COutput& out2)
{
    const CWalletTx *pcoin1 = out1.tx;
    const CWalletTx *pcoin2 = out2.tx;

    bool found1 = false;
    bool found2 = false;
    BOOST_FOREACH(int64_t d, darkSendDenominations) // loop through predefined denoms
    {
        if(pcoin1->vout[out1.i].nValue == d) found1 = true;
        if(pcoin2->vout[out2.i].nValue == d) found2 = true;
    }
    return (!found1 && found2);
}

bool CWallet::SelectCoinsMinConf(int64_t nTargetValue, unsigned int nSpendTime, int nConfMine, int nConfTheirs, vector<COutput> vCoins, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const
{
    setCoinsRet.clear();
    nValueRet = 0;

    // List of values less than target
    pair<int64_t, pair<const CWalletTx*,unsigned int> > coinLowestLarger;
    coinLowestLarger.first = std::numeric_limits<int64_t>::max();
    coinLowestLarger.second.first = NULL;
    vector<pair<int64_t, pair<const CWalletTx*,unsigned int> > > vValue;
    int64_t nTotalLower = 0;

    random_shuffle(vCoins.begin(), vCoins.end(), GetRandInt);

    // move denoms down on the list
    sort(vCoins.begin(), vCoins.end(), less_then_denom);

    // try to find nondenom first to prevent unneeded spending of mixed coins
    for (unsigned int tryDenom = 0; tryDenom < 2; tryDenom++)
    {
        if (fDebug) LogPrint("selectcoins", "tryDenom: %d\n", tryDenom);
        vValue.clear();
        nTotalLower = 0;

    BOOST_FOREACH(COutput output, vCoins)
    {
        const CWalletTx *pcoin = output.tx;

        if (output.nDepth < (pcoin->IsFromMe() ? nConfMine : nConfTheirs))
            continue;

        int i = output.i;

        // Follow the timestamp rules
        if (pcoin->nTime > nSpendTime)
            continue;

        int64_t n = pcoin->vout[i].nValue;

        if (tryDenom == 0 && IsDenominatedAmount(n)) continue; // we don't want denom values on first run

        pair<int64_t,pair<const CWalletTx*,unsigned int> > coin = make_pair(n,make_pair(pcoin, i));

        if (n == nTargetValue)
        {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            return true;
        }
        else if (n < nTargetValue + CENT)
        {
            vValue.push_back(coin);
            nTotalLower += n;
        }
        else if (n < coinLowestLarger.first)
        {
            coinLowestLarger = coin;
        }
    }

    if (nTotalLower == nTargetValue)
    {
        for (unsigned int i = 0; i < vValue.size(); ++i)
        {
            setCoinsRet.insert(vValue[i].second);
            nValueRet += vValue[i].first;
        }
        return true;
    }

    if (nTotalLower < nTargetValue)
    {
        if (coinLowestLarger.second.first == NULL)
            return false;
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
        return true;
    }

    // Solve subset sum by stochastic approximation
    sort(vValue.rbegin(), vValue.rend(), CompareValueOnly());
    vector<char> vfBest;
    int64_t nBest;

    ApproximateBestSubset(vValue, nTotalLower, nTargetValue, vfBest, nBest, 1000);
    if (nBest != nTargetValue && nTotalLower >= nTargetValue + CENT)
        ApproximateBestSubset(vValue, nTotalLower, nTargetValue + CENT, vfBest, nBest, 1000);

    // If we have a bigger coin and (either the stochastic approximation didn't find a good solution,
    //                                   or the next bigger coin is closer), return the bigger coin
    if (coinLowestLarger.second.first &&
        ((nBest != nTargetValue && nBest < nTargetValue + CENT) || coinLowestLarger.first <= nBest))
    {
        setCoinsRet.insert(coinLowestLarger.second);
        nValueRet += coinLowestLarger.first;
    }
    else {
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
            {
                setCoinsRet.insert(vValue[i].second);
                nValueRet += vValue[i].first;
            }

        LogPrint("selectcoins", "SelectCoins() best subset: ");
        for (unsigned int i = 0; i < vValue.size(); i++)
            if (vfBest[i])
                LogPrint("selectcoins", "%s ", FormatMoney(vValue[i].first));
        LogPrint("selectcoins", "total %s\n", FormatMoney(nBest));
    }

    return true;
    }
    return false;
}

bool CWallet::SelectCoins(int64_t nTargetValue, unsigned int nSpendTime, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet, const CCoinControl* coinControl, AvailableCoinsType coin_type, bool useIX) const
{
    vector<COutput> vCoins;
    AvailableCoins(vCoins, true, coinControl);

    //if we're doing only denominated, we need to round up to the nearest .1Lux
    if(coin_type == ONLY_DENOMINATED){
        // Make outputs by looping through denominations, from large to small
        BOOST_FOREACH(int64_t v, darkSendDenominations)
        {
            int added = 0;
            BOOST_FOREACH(const COutput& out, vCoins)
            {
                if(out.tx->vout[out.i].nValue == v                                            //make sure it's the denom we're looking for
                    && nValueRet + out.tx->vout[out.i].nValue < nTargetValue + (0.1*COIN)+100 //round the amount up to .1Lux over
                    && added <= 100){                                                          //don't add more than 100 of one denom type
                        CTxIn vin = CTxIn(out.tx->GetHash(),out.i);
                        int rounds = GetInputLuxsendRounds(vin);
                        // make sure it's actually anonymized
                        if(rounds < nLuxsendRounds) continue;
                        nValueRet += out.tx->vout[out.i].nValue;
                        setCoinsRet.insert(make_pair(out.tx, out.i));
                        added++;
                }
            }
        }
        return (nValueRet >= nTargetValue);
    }

    // coin control -> return all selected outputs (we want all selected to go into the transaction for sure)
    if (coinControl && coinControl->HasSelected())
    {
        BOOST_FOREACH(const COutput& out, vCoins)
        {
            if(!out.fSpendable)
                continue;
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.insert(make_pair(out.tx, out.i));
        }
        return (nValueRet >= nTargetValue);
    }

    boost::function<bool (const CWallet*, int64_t, unsigned int, int, int, std::vector<COutput>, std::set<std::pair<const CWalletTx*,unsigned int> >&, int64_t&)> f = fMinimizeCoinAge ? &CWallet::SelectCoinsMinConfByCoinAge : &CWallet::SelectCoinsMinConf;

    return (f(this, nTargetValue, nSpendTime, 1, 10, vCoins, setCoinsRet, nValueRet) ||
            f(this, nTargetValue, nSpendTime, 1, 1, vCoins, setCoinsRet, nValueRet) ||
            f(this, nTargetValue, nSpendTime, 0, 1, vCoins, setCoinsRet, nValueRet));
}

// Select some coins without random shuffle or best subset approximation
bool CWallet::SelectCoinsForStaking(int64_t nTargetValue, unsigned int nSpendTime, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const
{
    vector<COutput> vCoins;
    AvailableCoinsForStaking(vCoins, nSpendTime);

    setCoinsRet.clear();
    nValueRet = 0;

    BOOST_FOREACH(COutput output, vCoins)
    {
        const CWalletTx *pcoin = output.tx;
        int i = output.i;

        // Stop if we've chosen enough inputs
        if (nValueRet >= nTargetValue)
            break;

        int64_t n = pcoin->vout[i].nValue;

        pair<int64_t,pair<const CWalletTx*,unsigned int> > coin = make_pair(n,make_pair(pcoin, i));

        if (n >= nTargetValue)
        {
            // If input value is greater or equal to target then simply insert
            //    it into the current subset and exit
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
            break;
        }
        else if (n < nTargetValue + CENT)
        {
            setCoinsRet.insert(coin.second);
            nValueRet += coin.first;
        }
    }

    return true;
}

struct CompareByPriority
{
    bool operator()(const COutput& t1,
                    const COutput& t2) const
    {
        return t1.Priority() > t2.Priority();
    }
};

bool CWallet::SelectCoinsByDenominations(int nDenom, int64_t nValueMin, int64_t nValueMax, std::vector<CTxIn>& setCoinsRet, vector<COutput>& setCoinsRet2, int64_t& nValueRet, int nLuxsendRoundsMin, int nLuxsendRoundsMax)
{
    setCoinsRet.clear();
    nValueRet = 0;

    setCoinsRet2.clear();
    vector<COutput> vCoins;
    AvailableCoins(vCoins);

    //order the array so fees are first, then denominated money, then the rest.
    std::random_shuffle(vCoins.rbegin(), vCoins.rend());

    //keep track of each denomination that we have
    bool fFound100000 = false;
    bool fFound10000 = false;
    bool fFound1000 = false;
    bool fFound100 = false;
    bool fFound10 = false;
    bool fFound1 = false;
    bool fFoundDot1 = false;

    //Check to see if any of the denomination are off, in that case mark them as fulfilled



    if(!(nDenom & (1 << 0))) fFound100000 = true;
    if(!(nDenom & (1 << 1))) fFound10000 = true;
    if(!(nDenom & (1 << 2))) fFound1000 = true;
    if(!(nDenom & (1 << 3))) fFound100 = true;
    if(!(nDenom & (1 << 4))) fFound10 = true;
    if(!(nDenom & (1 << 5))) fFound1 = true;
    if(!(nDenom & (1 << 6))) fFoundDot1 = true;

    BOOST_FOREACH(const COutput& out, vCoins)
    {
        //there's no reason to allow inputs less than 1 COIN into DS (other than denominations smaller than that amount)
        if(out.tx->vout[out.i].nValue < 1*COIN && out.tx->vout[out.i].nValue != (.1*COIN)+100) continue;
        if(fMasterNode && out.tx->vout[out.i].nValue == GetMNCollateral(pindexBest->nHeight)*COIN) continue; //masternode input
        if(nValueRet + out.tx->vout[out.i].nValue <= nValueMax){
            bool fAccepted = false;

            // Function returns as follows:
            //
            // bit 0 - 100Lux+1 ( bit on if present )
            // bit 1 - 10Lux+1
            // bit 2 - 1Lux+1
            // bit 3 - .1Lux+1

            CTxIn vin = CTxIn(out.tx->GetHash(),out.i);

            int rounds = GetInputLuxsendRounds(vin);
            if(rounds >= nLuxsendRoundsMax) continue;
            if(rounds < nLuxsendRoundsMin) continue;

            if(fFound100000 && fFound10000 && fFound1000 && fFound100 && fFound10 && fFound1 && fFoundDot1){ //if fulfilled
                //we can return this for submission
                if(nValueRet >= nValueMin){
                    //random reduce the max amount we'll submit for anonymity
                    nValueMax -= (rand() % (nValueMax/5));
                    //on average use 50% of the inputs or less
                    int r = (rand() % (int)vCoins.size());
                    if((int)setCoinsRet.size() > r) return true;
                }
                //Denomination criterion has been met, we can take any matching denominations
                if((nDenom & (1 << 0)) && out.tx->vout[out.i].nValue ==      ((100000*COIN)    +100000000)) {fAccepted = true;}
                else if((nDenom & (1 << 1)) && out.tx->vout[out.i].nValue == ((10000*COIN)     +10000000)) {fAccepted = true;}
                else if((nDenom & (1 << 2)) && out.tx->vout[out.i].nValue == ((1000*COIN)      +1000000)) {fAccepted = true;}
                else if((nDenom & (1 << 3)) && out.tx->vout[out.i].nValue == ((100*COIN)       +100000)) {fAccepted = true;}
                else if((nDenom & (1 << 4)) && out.tx->vout[out.i].nValue == ((10*COIN)        +10000)) {fAccepted = true;}
                else if((nDenom & (1 << 5)) && out.tx->vout[out.i].nValue == ((1*COIN)         +1000)) {fAccepted = true;}
                else if((nDenom & (1 << 6)) && out.tx->vout[out.i].nValue == ((.1*COIN)        +100)) {fAccepted = true;}
            } else {
                //Criterion has not been satisfied, we will only take 1 of each until it is.
                if((nDenom & (1 << 0)) && out.tx->vout[out.i].nValue ==      ((100000*COIN)    +100000000)) {fAccepted = true; fFound100000 = true;}
                else if((nDenom & (1 << 1)) && out.tx->vout[out.i].nValue == ((10000*COIN)     +10000000)) {fAccepted = true; fFound10000 = true;}
                else if((nDenom & (1 << 1)) && out.tx->vout[out.i].nValue == ((1000*COIN)      +1000000)) {fAccepted = true; fFound1000 = true;}
                else if((nDenom & (1 << 1)) && out.tx->vout[out.i].nValue == ((100*COIN)       +100000)) {fAccepted = true; fFound100 = true;}
                else if((nDenom & (1 << 1)) && out.tx->vout[out.i].nValue == ((10*COIN)        +10000)) {fAccepted = true; fFound10 = true;}
                else if((nDenom & (1 << 2)) && out.tx->vout[out.i].nValue == ((1*COIN)         +1000)) {fAccepted = true; fFound1 = true;}
                else if((nDenom & (1 << 3)) && out.tx->vout[out.i].nValue == ((.1*COIN)        +100)) {fAccepted = true; fFoundDot1 = true;}
            }
            if(!fAccepted) continue;

            vin.prevPubKey = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.push_back(vin);
            setCoinsRet2.push_back(out);
        }
    }

    return (nValueRet >= nValueMin && fFound100000 && fFound10000 && fFound1000 && fFound100 && fFound10 && fFound1 && fFoundDot1);
}

bool CWallet::SelectCoinsDark(int64_t nValueMin, int64_t nValueMax, std::vector<CTxIn>& setCoinsRet, int64_t& nValueRet, int nLuxsendRoundsMin, int nLuxsendRoundsMax) const
{
    CCoinControl *coinControl=NULL;

    setCoinsRet.clear();
    nValueRet = 0;

    vector<COutput> vCoins;
    AvailableCoins(vCoins, true, coinControl, nLuxsendRoundsMin < 0 ? ONLY_NONDENOMINATED_NOTMN : ONLY_DENOMINATED);

    set<pair<const CWalletTx*,unsigned int> > setCoinsRet2;

    //order the array so fees are first, then denominated money, then the rest.
    sort(vCoins.rbegin(), vCoins.rend(), CompareByPriority());

    //the first thing we get is a fee input, then we'll use as many denominated as possible. then the rest
    BOOST_FOREACH(const COutput& out, vCoins)
    {
        //there's no reason to allow inputs less than 1 COIN into DS (other than denominations smaller than that amount)
        if(out.tx->vout[out.i].nValue < 1*COIN && out.tx->vout[out.i].nValue != (.1*COIN)+100) continue;
        if(fMasterNode && out.tx->vout[out.i].nValue == GetMNCollateral(pindexBest->nHeight)*COIN) continue; //masternode input

        if(nValueRet + out.tx->vout[out.i].nValue <= nValueMax){
            CTxIn vin = CTxIn(out.tx->GetHash(),out.i);

            int rounds = GetInputLuxsendRounds(vin);
            if(rounds >= nLuxsendRoundsMax) continue;
            if(rounds < nLuxsendRoundsMin) continue;

            vin.prevPubKey = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.push_back(vin);
            setCoinsRet2.insert(make_pair(out.tx, out.i));
        }
    }

    // if it's more than min, we're good to return
    if(nValueRet >= nValueMin) return true;

    return false;
}

bool CWallet::SelectCoinsCollateral(std::vector<CTxIn>& setCoinsRet, int64_t& nValueRet) const
{
    vector<COutput> vCoins;

    //printf(" selecting coins for collateral\n");
    AvailableCoins(vCoins);

    //printf("found coins %d\n", (int)vCoins.size());

    set<pair<const CWalletTx*,unsigned int> > setCoinsRet2;

    BOOST_FOREACH(const COutput& out, vCoins)
    {
        // collateral inputs will always be a multiple of DARSEND_COLLATERAL, up to five
        if(IsCollateralAmount(out.tx->vout[out.i].nValue))
        {
            CTxIn vin = CTxIn(out.tx->GetHash(),out.i);

            vin.prevPubKey = out.tx->vout[out.i].scriptPubKey; // the inputs PubKey
            nValueRet += out.tx->vout[out.i].nValue;
            setCoinsRet.push_back(vin);
            setCoinsRet2.insert(make_pair(out.tx, out.i));
            return true;
        }
    }

    return false;
}

int CWallet::CountInputsWithAmount(int64_t nInputAmount)
{
    int64_t nTotal = 0;
    {
        LOCK(cs_wallet);
        for (map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        {
            const CWalletTx* pcoin = &(*it).second;
            if (pcoin->IsTrusted()){
                int nDepth = pcoin->GetDepthInMainChain();

                for (unsigned int i = 0; i < pcoin->vout.size(); i++) {
					//isminetype mine = IsMine(pcoin->vout[i]);
		    bool mine = IsMine(pcoin->vout[i]);
                    //COutput out = COutput(pcoin, i, nDepth, (mine & ISMINE_SPENDABLE) != ISMINE_NO);
		    COutput out = COutput(pcoin, i, nDepth, mine);
                    CTxIn vin = CTxIn(out.tx->GetHash(), out.i);

                    if(out.tx->vout[out.i].nValue != nInputAmount) continue;
                    if(!IsDenominatedAmount(pcoin->vout[i].nValue)) continue;
                    //if(IsSpent(out.tx->GetHash(), i) || !IsMine(pcoin->vout[i]) || !IsDenominated(vin)) continue;
		    if(pcoin->IsSpent(i) || !IsMine(pcoin->vout[i]) || !IsDenominated(vin)) continue;

                    nTotal++;
                }
            }
        }
    }

    return nTotal;
}

bool CWallet::HasCollateralInputs() const
{
    vector<COutput> vCoins;
    AvailableCoins(vCoins);

    int nFound = 0;
    BOOST_FOREACH(const COutput& out, vCoins)
        if(IsCollateralAmount(out.tx->vout[out.i].nValue)) nFound++;

    return nFound > 1; // should have more than one just in case
}

bool CWallet::IsCollateralAmount(int64_t nInputAmount) const
{
    return  nInputAmount == (DARKSEND_COLLATERAL * 5)+DARKSEND_FEE ||
            nInputAmount == (DARKSEND_COLLATERAL * 4)+DARKSEND_FEE ||
            nInputAmount == (DARKSEND_COLLATERAL * 3)+DARKSEND_FEE ||
            nInputAmount == (DARKSEND_COLLATERAL * 2)+DARKSEND_FEE ||
            nInputAmount == (DARKSEND_COLLATERAL * 1)+DARKSEND_FEE;
}

bool CWallet::SelectCoinsWithoutDenomination(int64_t nTargetValue, set<pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const
{
    CCoinControl *coinControl=NULL;

    vector<COutput> vCoins;
    AvailableCoins(vCoins, true, coinControl, ONLY_NONDENOMINATED);

    BOOST_FOREACH(const COutput& out, vCoins)
    {
        nValueRet += out.tx->vout[out.i].nValue;
        setCoinsRet.insert(make_pair(out.tx, out.i));
    }
    return (nValueRet >= nTargetValue);
}

bool CWallet::CreateCollateralTransaction(CTransaction& txCollateral, std::string strReason)
{
    /*
        To doublespend a collateral transaction, it will require a fee higher than this. So there's
        still a significant cost.
    */
    int64_t nFeeRet = 0.001*COIN;

    txCollateral.vin.clear();
    txCollateral.vout.clear();

    CReserveKey reservekey(this);
    int64_t nValueIn2 = 0;
    std::vector<CTxIn> vCoinsCollateral;

    if (!SelectCoinsCollateral(vCoinsCollateral, nValueIn2))
    {
        strReason = "Error: Luxsend requires a collateral transaction and could not locate an acceptable input!";
        return false;
    }

    // make our change address
    CScript scriptChange;
    CPubKey vchPubKey;
    assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked
    scriptChange =GetScriptForDestination(vchPubKey.GetID());
    reservekey.KeepKey();

    BOOST_FOREACH(CTxIn v, vCoinsCollateral)
        txCollateral.vin.push_back(v);

    if(nValueIn2 - DARKSEND_COLLATERAL - nFeeRet > 0) {
        //pay collateral charge in fees
        CTxOut vout3 = CTxOut(nValueIn2 - DARKSEND_COLLATERAL, scriptChange);
        txCollateral.vout.push_back(vout3);
    }

    int vinNumber = 0;
    BOOST_FOREACH(CTxIn v, txCollateral.vin) {
        if(!SignSignature(*this, v.prevPubKey, txCollateral, vinNumber, int(SIGHASH_ALL|SIGHASH_ANYONECANPAY))) {
            BOOST_FOREACH(CTxIn v, vCoinsCollateral)
                UnlockCoin(v.prevout);

            strReason = "CDarkSendPool::Sign - Unable to sign collateral transaction! \n";
            return false;
        }
        vinNumber++;
    }

    return true;
}

bool CWallet::ConvertList(std::vector<CTxIn> vCoins, std::vector<int64_t>& vecAmounts)
{
    BOOST_FOREACH(CTxIn i, vCoins){
        if (mapWallet.count(i.prevout.hash))
        {
            CWalletTx& wtx = mapWallet[i.prevout.hash];
            if(i.prevout.n < wtx.vout.size()){
                vecAmounts.push_back(wtx.vout[i.prevout.n].nValue);
            }
        } else {
            LogPrintf("ConvertList -- Couldn't find transaction\n");
        }
    }
    return true;
}


bool CWallet::CreateTransaction(const vector<pair<CScript, int64_t> >& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, int32_t& nChangePos, std::string& strFailReason, const CCoinControl* coinControl, AvailableCoinsType coin_type, bool useIX)
{
    int64_t nValue = 0;
    BOOST_FOREACH (const PAIRTYPE(CScript, int64_t)& s, vecSend)
    {
        if (nValue < 0)
            return false;
        nValue += s.second;
    }
    if (vecSend.empty() || nValue < 0)
        return false;

    wtxNew.BindWallet(this);
    CTransaction txNew;

    {
        LOCK2(cs_main, cs_wallet);
        // txdb must be opened before the mapWallet lock
        CTxDB txdb("r");
        {
            nFeeRet = nTransactionFee;
            while (true)
            {
                wtxNew.vin.clear();
                wtxNew.vout.clear();
                wtxNew.fFromMe = true;

                int64_t nTotalValue = nValue + nFeeRet;
                double dPriority = 0;
                // vouts to the payees
                BOOST_FOREACH (const PAIRTYPE(CScript, int64_t)& s, vecSend)
                    wtxNew.vout.push_back(CTxOut(s.second, s.first));

                // Choose coins to use
                set<pair<const CWalletTx*,unsigned int> > setCoins;
                int64_t nValueIn = 0;
                if (!SelectCoins(nTotalValue, wtxNew.nTime, setCoins, nValueIn, coinControl))
                {
                    if(coin_type == ALL_COINS) {
                        strFailReason = _("Insufficient funds.");
                    } else if (coin_type == ONLY_NONDENOMINATED) {
                        strFailReason = _("Unable to locate enough Luxsend non-denominated funds for this transaction.");
                    } else if (coin_type == ONLY_NONDENOMINATED_NOTMN) {
                        strFailReason = _("Unable to locate enough Luxsend non-denominated funds for this transaction that are not equal 1000 Lux.");
                    } else {
                        strFailReason = _("Unable to locate enough Luxsend denominated funds for this transaction.");
                        strFailReason += _("Luxsend uses exact denominated amounts to send funds, you might simply need to anonymize some more coins.");
                    }

                    if(useIX){
                        strFailReason += _("InstantX requires inputs with at least 6 confirmations, you might need to wait a few minutes and try again.");
                    }
                    return false;
                }
                BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
                {
                    int64_t nCredit = pcoin.first->vout[pcoin.second].nValue;
                    dPriority += (double)nCredit * pcoin.first->GetDepthInMainChain();
                }

                int64_t nChange = nValueIn - nValue - nFeeRet;

                if (nChange > 0)
                {
                    // Fill a vout to ourself
                    // TODO: pass in scriptChange instead of reservekey so
                    // change transaction isn't always pay-to-bitcoin-address
                    CScript scriptChange;

                    // coin control: send change to custom address
                    if (coinControl && !boost::get<CNoDestination>(&coinControl->destChange))
                        scriptChange.SetDestination(coinControl->destChange);

                    // no coin control: send change to newly generated address
                    else
                    {
                        // Note: We use a new key here to keep it from being obvious which side is the change.
                        //  The drawback is that by not reusing a previous key, the change may be lost if a
                        //  backup is restored, if the backup doesn't have the new private key for the change.
                        //  If we reused the old key, it would be possible to add code to look for and
                        //  rediscover unknown transactions that were written with keys of ours to recover
                        //  post-backup change.

                        // Reserve a new key pair from key pool
                        CPubKey vchPubKey;
                        assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked

                        scriptChange.SetDestination(vchPubKey.GetID());
                    }

                    // Insert change txn at random position:
                    vector<CTxOut>::iterator position = wtxNew.vout.begin()+GetRandInt(wtxNew.vout.size());

		    // -- don't put change output between value and narration outputs
                    if (position > wtxNew.vout.begin() && position < wtxNew.vout.end())
                    {
                        while (position > wtxNew.vout.begin())
                        {
                            if (position->nValue != 0)
                                break;
                            position--;
                        };
                    };

                    wtxNew.vout.insert(position, CTxOut(nChange, scriptChange));
		    nChangePos = std::distance(wtxNew.vout.begin(), position);
                }
                else
                    reservekey.ReturnKey();

                // Fill vin
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    wtxNew.vin.push_back(CTxIn(coin.first->GetHash(),coin.second));

                // Sign
                int nIn = 0;
                BOOST_FOREACH(const PAIRTYPE(const CWalletTx*,unsigned int)& coin, setCoins)
                    if (!SignSignature(*this, *coin.first, wtxNew, nIn++))
                        return false;

                // Limit size
                unsigned int nBytes = ::GetSerializeSize(*(CTransaction*)&wtxNew, SER_NETWORK, PROTOCOL_VERSION);
                if (nBytes >= MAX_STANDARD_TX_SIZE)
                    return false;
                dPriority /= nBytes;

                // Check that enough fee is included
                int64_t nPayFee = nTransactionFee * (1 + (int64_t)nBytes / 1000);
                int64_t nMinFee = GetMinFee(wtxNew, 1, GMF_SEND, nBytes);

                if (nFeeRet < max(nPayFee, nMinFee))
                {
                    nFeeRet = max(nPayFee, nMinFee);
                    continue;
                }

                // Fill vtxPrev by copying from previous transactions vtxPrev
                wtxNew.AddSupportingTransactions(txdb);
                wtxNew.fTimeReceivedIsTxTime = true;

                break;
            }
        }
    }
    return true;
}

bool CWallet::CreateTransaction(CScript scriptPubKey, int64_t nValue, std::string& sNarr, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl* coinControl)
{
    vector< pair<CScript, int64_t> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));

    if (sNarr.length() > 0)
    {
        std::vector<uint8_t> vNarr(sNarr.c_str(), sNarr.c_str() + sNarr.length());
        std::vector<uint8_t> vNDesc;

        vNDesc.resize(2);
        vNDesc[0] = 'n';
        vNDesc[1] = 'p';

        CScript scriptN = CScript() << OP_RETURN << vNDesc << OP_RETURN << vNarr;

        vecSend.push_back(make_pair(scriptN, 0));
    }

    // -- CreateTransaction won't place change between value and narr output.
    //    narration output will be for preceding output

    int nChangePos;
    std::string strFailReason;
    bool rv = CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, nChangePos, strFailReason, coinControl);

    // -- narration will be added to mapValue later in FindStealthTransactions From CommitTransaction
    return rv;
}


bool CWallet::NewStealthAddress(std::string& sError, std::string& sLabel, CStealthAddress& sxAddr)
{
    ec_secret scan_secret;
    ec_secret spend_secret;

    if (GenerateRandomSecret(scan_secret) != 0
        || GenerateRandomSecret(spend_secret) != 0)
    {
        sError = "GenerateRandomSecret failed.";
        printf("Error CWallet::NewStealthAddress - %s\n", sError.c_str());
        return false;
    };

    ec_point scan_pubkey, spend_pubkey;
    if (SecretToPublicKey(scan_secret, scan_pubkey) != 0)
    {
        sError = "Could not get scan public key.";
        printf("Error CWallet::NewStealthAddress - %s\n", sError.c_str());
        return false;
    };

    if (SecretToPublicKey(spend_secret, spend_pubkey) != 0)
    {
        sError = "Could not get spend public key.";
        printf("Error CWallet::NewStealthAddress - %s\n", sError.c_str());
        return false;
    };

    if (fDebug)
    {
        printf("getnewstealthaddress: ");
        printf("scan_pubkey ");
        for (uint32_t i = 0; i < scan_pubkey.size(); ++i)
          printf("%02x", scan_pubkey[i]);
        printf("\n");

        printf("spend_pubkey ");
        for (uint32_t i = 0; i < spend_pubkey.size(); ++i)
          printf("%02x", spend_pubkey[i]);
        printf("\n");
    };


    sxAddr.label = sLabel;
    sxAddr.scan_pubkey = scan_pubkey;
    sxAddr.spend_pubkey = spend_pubkey;

    sxAddr.scan_secret.resize(32);
    memcpy(&sxAddr.scan_secret[0], &scan_secret.e[0], 32);
    sxAddr.spend_secret.resize(32);
    memcpy(&sxAddr.spend_secret[0], &spend_secret.e[0], 32);

    return true;
}

bool CWallet::AddStealthAddress(CStealthAddress& sxAddr)
{
    LOCK(cs_wallet);

    // must add before changing spend_secret
    stealthAddresses.insert(sxAddr);

    bool fOwned = sxAddr.scan_secret.size() == ec_secret_size;



    if (fOwned)
    {
        // -- owned addresses can only be added when wallet is unlocked
        if (IsLocked())
        {
            printf("Error: CWallet::AddStealthAddress wallet must be unlocked.\n");
            stealthAddresses.erase(sxAddr);
            return false;
        };

        if (IsCrypted())
        {
            std::vector<unsigned char> vchCryptedSecret;
            CSecret vchSecret;
            vchSecret.resize(32);
            memcpy(&vchSecret[0], &sxAddr.spend_secret[0], 32);

            uint256 iv = Hash(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end());
            if (!EncryptSecret(vMasterKey, vchSecret, iv, vchCryptedSecret))
            {
                printf("Error: Failed encrypting stealth key %s\n", sxAddr.Encoded().c_str());
                stealthAddresses.erase(sxAddr);
                return false;
            };
            sxAddr.spend_secret = vchCryptedSecret;
        };
    };


    bool rv = CWalletDB(strWalletFile).WriteStealthAddress(sxAddr);

    if (rv)
        NotifyAddressBookChanged(this, sxAddr, sxAddr.label, fOwned, CT_NEW);

    return rv;
}

bool CWallet::UnlockStealthAddresses(const CKeyingMaterial& vMasterKeyIn)
{
    // -- decrypt spend_secret of stealth addresses
    std::set<CStealthAddress>::iterator it;
    for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
    {
        if (it->scan_secret.size() < 32)
            continue; // stealth address is not owned

        // -- CStealthAddress are only sorted on spend_pubkey
        CStealthAddress &sxAddr = const_cast<CStealthAddress&>(*it);

        if (fDebug)
            printf("Decrypting stealth key %s\n", sxAddr.Encoded().c_str());

        CSecret vchSecret;
        uint256 iv = Hash(sxAddr.spend_pubkey.begin(), sxAddr.spend_pubkey.end());
        if(!DecryptSecret(vMasterKeyIn, sxAddr.spend_secret, iv, vchSecret)
            || vchSecret.size() != 32)
        {
            printf("Error: Failed decrypting stealth key %s\n", sxAddr.Encoded().c_str());
            continue;
        };

        ec_secret testSecret;
        memcpy(&testSecret.e[0], &vchSecret[0], 32);
        ec_point pkSpendTest;

        if (SecretToPublicKey(testSecret, pkSpendTest) != 0
            || pkSpendTest != sxAddr.spend_pubkey)
        {
            printf("Error: Failed decrypting stealth key, public key mismatch %s\n", sxAddr.Encoded().c_str());
            continue;
        };

        sxAddr.spend_secret.resize(32);
        memcpy(&sxAddr.spend_secret[0], &vchSecret[0], 32);
    };

    CryptedKeyMap::iterator mi = mapCryptedKeys.begin();
    for (; mi != mapCryptedKeys.end(); ++mi)
    {
        CPubKey &pubKey = (*mi).second.first;
        std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
        if (vchCryptedSecret.size() != 0)
            continue;

        CKeyID ckid = pubKey.GetID();
        CBitcoinAddress addr(ckid);

        StealthKeyMetaMap::iterator mi = mapStealthKeyMeta.find(ckid);
        if (mi == mapStealthKeyMeta.end())
        {
            printf("Error: No metadata found to add secret for %s\n", addr.ToString().c_str());
            continue;
        };

        CStealthKeyMetadata& sxKeyMeta = mi->second;

        CStealthAddress sxFind;
        sxFind.scan_pubkey = sxKeyMeta.pkScan.Raw();

        std::set<CStealthAddress>::iterator si = stealthAddresses.find(sxFind);
        if (si == stealthAddresses.end())
        {
            printf("No stealth key found to add secret for %s\n", addr.ToString().c_str());
            continue;
        };

        if (fDebug)
            printf("Expanding secret for %s\n", addr.ToString().c_str());

        ec_secret sSpendR;
        ec_secret sSpend;
        ec_secret sScan;

        if (si->spend_secret.size() != ec_secret_size
            || si->scan_secret.size() != ec_secret_size)
        {
            printf("Stealth address has no secret key for %s\n", addr.ToString().c_str());
            continue;
        }
        memcpy(&sScan.e[0], &si->scan_secret[0], ec_secret_size);
        memcpy(&sSpend.e[0], &si->spend_secret[0], ec_secret_size);

        ec_point pkEphem = sxKeyMeta.pkEphem.Raw();
        if (StealthSecretSpend(sScan, pkEphem, sSpend, sSpendR) != 0)
        {
            printf("StealthSecretSpend() failed.\n");
            continue;
        };

        ec_point pkTestSpendR;
        if (SecretToPublicKey(sSpendR, pkTestSpendR) != 0)
        {
            printf("SecretToPublicKey() failed.\n");
            continue;
        };

        CSecret vchSecret;
        vchSecret.resize(ec_secret_size);

        memcpy(&vchSecret[0], &sSpendR.e[0], ec_secret_size);
        CKey ckey;

        try {
	    ckey.Set(vchSecret.begin(), vchSecret.end(), true);
            //ckey.SetSecret(vchSecret, true);
        } catch (std::exception& e) {
            printf("ckey.SetSecret() threw: %s.\n", e.what());
            continue;
        };

        CPubKey cpkT = ckey.GetPubKey();

        if (!cpkT.IsValid())
        {
            printf("cpkT is invalid.\n");
            continue;
        };

        if (cpkT != pubKey)
        {
            printf("Error: Generated secret does not match.\n");
            continue;
        };

        if (!ckey.IsValid())
        {
            printf("Reconstructed key is invalid.\n");
            continue;
        };

        if (fDebug)
        {
            CKeyID keyID = cpkT.GetID();
            CBitcoinAddress coinAddress(keyID);
            printf("Adding secret to key %s.\n", coinAddress.ToString().c_str());
        };

        if (!AddKey(ckey))
        {
            printf("AddKey failed.\n");
            continue;
        };

        if (!CWalletDB(strWalletFile).EraseStealthKeyMeta(ckid))
            printf("EraseStealthKeyMeta failed for %s\n", addr.ToString().c_str());
    };
    return true;
}

bool CWallet::UpdateStealthAddress(std::string &addr, std::string &label, bool addIfNotExist)
{
    if (fDebug)
        printf("UpdateStealthAddress %s\n", addr.c_str());


    CStealthAddress sxAddr;

    if (!sxAddr.SetEncoded(addr))
        return false;

    std::set<CStealthAddress>::iterator it;
    it = stealthAddresses.find(sxAddr);

    ChangeType nMode = CT_UPDATED;
    CStealthAddress sxFound;
    if (it == stealthAddresses.end())
    {
        if (addIfNotExist)
        {
            sxFound = sxAddr;
            sxFound.label = label;
            stealthAddresses.insert(sxFound);
            nMode = CT_NEW;
        } else
        {
            printf("UpdateStealthAddress %s, not in set\n", addr.c_str());
            return false;
        };
    } else
    {
        sxFound = const_cast<CStealthAddress&>(*it);

        if (sxFound.label == label)
        {
            // no change
            return true;
        };

        it->label = label; // update in .stealthAddresses

        if (sxFound.scan_secret.size() == ec_secret_size)
        {
            printf("UpdateStealthAddress: todo - update owned stealth address.\n");
            return false;
        };
    };

    sxFound.label = label;

    if (!CWalletDB(strWalletFile).WriteStealthAddress(sxFound))
    {
        printf("UpdateStealthAddress(%s) Write to db failed.\n", addr.c_str());
        return false;
    };

    bool fOwned = sxFound.scan_secret.size() == ec_secret_size;
    NotifyAddressBookChanged(this, sxFound, sxFound.label, fOwned, nMode);

    return true;
}

bool CWallet::CreateStealthTransaction(CScript scriptPubKey, int64_t nValue, std::vector<uint8_t>& P, std::vector<uint8_t>& narr, std::string& sNarr, CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, const CCoinControl* coinControl)
{
    vector< pair<CScript, int64_t> > vecSend;
    vecSend.push_back(make_pair(scriptPubKey, nValue));

    CScript scriptP = CScript() << OP_RETURN << P;
    if (narr.size() > 0)
        scriptP = scriptP << OP_RETURN << narr;

    vecSend.push_back(make_pair(scriptP, 1));

    // -- shuffle inputs, change output won't mix enough as it must be not fully random for plantext narrations
    std::random_shuffle(vecSend.begin(), vecSend.end());

    int nChangePos;
    std::string strFailReason;
    bool rv = CreateTransaction(vecSend, wtxNew, reservekey, nFeeRet, nChangePos, strFailReason, coinControl);

    // -- the change txn is inserted in a random pos, check here to match narr to output
    if (rv && narr.size() > 0)
    {
        for (unsigned int k = 0; k < wtxNew.vout.size(); ++k)
        {
            if (wtxNew.vout[k].scriptPubKey != scriptPubKey
                || wtxNew.vout[k].nValue != nValue)
                continue;

            char key[64];
            if (snprintf(key, sizeof(key), "n_%u", k) < 1)
            {
                printf("CreateStealthTransaction(): Error creating narration key.");
                break;
            };
            wtxNew.mapValue[key] = sNarr;
            break;
        };
    };

    return rv;
}

string CWallet::SendStealthMoney(CScript scriptPubKey, int64_t nValue, std::vector<uint8_t>& P, std::vector<uint8_t>& narr, std::string& sNarr, CWalletTx& wtxNew, bool fAskFee)
{
    CReserveKey reservekey(this);
    int64_t nFeeRequired;

    if (IsLocked())
    {
        string strError = _("Error: Wallet locked, unable to create transaction  ");
        printf("SendStealthMoney() : %s", strError.c_str());
        return strError;
    }
    if (fWalletUnlockStakingOnly)
    {
        string strError = _("Error: Wallet unlocked for staking only, unable to create transaction.");
        printf("SendStealthMoney() : %s", strError.c_str());
        return strError;
    }
    if (!CreateStealthTransaction(scriptPubKey, nValue, P, narr, sNarr, wtxNew, reservekey, nFeeRequired))
    {
        string strError;
        if (nValue + nFeeRequired > GetBalance())
            strError = strprintf(_("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds  "), FormatMoney(nFeeRequired).c_str());
        else
            strError = _("Error: Transaction creation failed  ");
        printf("SendStealthMoney() : %s", strError.c_str());
        return strError;
    }

    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired, _("Sending...")))
        return "ABORTED";

    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected.  This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");

    return "";
}

bool CWallet::SendStealthMoneyToDestination(CStealthAddress& sxAddress, int64_t nValue, std::string& sNarr, CWalletTx& wtxNew, std::string& sError, bool fAskFee)
{
    // -- Check amount
    if (nValue <= 0)
    {
        sError = "Invalid amount";
        return false;
    };
    if (nValue + nTransactionFee + (1) > GetBalance())
    {
        sError = "Insufficient funds";
        return false;
    };


    ec_secret ephem_secret;
    ec_secret secretShared;
    ec_point pkSendTo;
    ec_point ephem_pubkey;

    if (GenerateRandomSecret(ephem_secret) != 0)
    {
        sError = "GenerateRandomSecret failed.";
        return false;
    };

    if (StealthSecret(ephem_secret, sxAddress.scan_pubkey, sxAddress.spend_pubkey, secretShared, pkSendTo) != 0)
    {
        sError = "Could not generate receiving public key.";
        return false;
    };

    CPubKey cpkTo(pkSendTo);
    if (!cpkTo.IsValid())
    {
        sError = "Invalid public key generated.";
        return false;
    };

    CKeyID ckidTo = cpkTo.GetID();

    CBitcoinAddress addrTo(ckidTo);

    if (SecretToPublicKey(ephem_secret, ephem_pubkey) != 0)
    {
        sError = "Could not generate ephem public key.";
        return false;
    };

    if (fDebug)
    {
        printf("Stealth send to generated pubkey %"PRIszu": %s\n", pkSendTo.size(), HexStr(pkSendTo).c_str());
        printf("hash %s\n", addrTo.ToString().c_str());
        printf("ephem_pubkey %"PRIszu": %s\n", ephem_pubkey.size(), HexStr(ephem_pubkey).c_str());
    };

    std::vector<unsigned char> vchNarr;
    if (sNarr.length() > 0)
    {
        SecMsgCrypter crypter;
        crypter.SetKey(&secretShared.e[0], &ephem_pubkey[0]);

        if (!crypter.Encrypt((uint8_t*)&sNarr[0], sNarr.length(), vchNarr))
        {
            sError = "Narration encryption failed.";
            return false;
        };

        if (vchNarr.size() > 48)
        {
            sError = "Encrypted narration is too long.";
            return false;
        };
    };

    // -- Parse Bitcoin address
    CScript scriptPubKey;
    scriptPubKey.SetDestination(addrTo.Get());

    if ((sError = SendStealthMoney(scriptPubKey, nValue, ephem_pubkey, vchNarr, sNarr, wtxNew, fAskFee)) != "")
        return false;


    return true;
}

bool CWallet::FindStealthTransactions(const CTransaction& tx, mapValue_t& mapNarr)
{
    if (fDebug)
        LogPrintf("FindStealthTransactions() tx: %s\n", tx.GetHash().GetHex().c_str());

    mapNarr.clear();

    LOCK(cs_wallet);
    ec_secret sSpendR;
    ec_secret sSpend;
    ec_secret sScan;
    ec_secret sShared;

    ec_point pkExtracted;

    std::vector<uint8_t> vchEphemPK;
    std::vector<uint8_t> vchDataB;
    std::vector<uint8_t> vchENarr;
    opcodetype opCode;
    char cbuf[256];

    int32_t nOutputIdOuter = -1;
    BOOST_FOREACH(const CTxOut& txout, tx.vout)
    {
        nOutputIdOuter++;
        // -- for each OP_RETURN need to check all other valid outputs

        //printf("txout scriptPubKey %s\n",  txout.scriptPubKey.ToString().c_str());
        CScript::const_iterator itTxA = txout.scriptPubKey.begin();

        if (!txout.scriptPubKey.GetOp(itTxA, opCode, vchEphemPK)
            || opCode != OP_RETURN)
            continue;
        else
        if (!txout.scriptPubKey.GetOp(itTxA, opCode, vchEphemPK)
            || vchEphemPK.size() != 33)
        {
            // -- look for plaintext narrations
            if (vchEphemPK.size() > 1
                && vchEphemPK[0] == 'n'
                && vchEphemPK[1] == 'p')
            {
                if (txout.scriptPubKey.GetOp(itTxA, opCode, vchENarr)
                    && opCode == OP_RETURN
                    && txout.scriptPubKey.GetOp(itTxA, opCode, vchENarr)
                    && vchENarr.size() > 0)
                {
                    std::string sNarr = std::string(vchENarr.begin(), vchENarr.end());

                    snprintf(cbuf, sizeof(cbuf), "n_%d", nOutputIdOuter-1); // plaintext narration always matches preceding value output
                    mapNarr[cbuf] = sNarr;
                } else
                {
                    printf("Warning: FindStealthTransactions() tx: %s, Could not extract plaintext narration.\n", tx.GetHash().GetHex().c_str());
                };
            }

            continue;
        }

        int32_t nOutputId = -1;
        nStealth++;
        BOOST_FOREACH(const CTxOut& txoutB, tx.vout)
        {
            nOutputId++;

            if (&txoutB == &txout)
                continue;

            bool txnMatch = false; // only 1 txn will match an ephem pk
            //printf("txoutB scriptPubKey %s\n",  txoutB.scriptPubKey.ToString().c_str());

            CTxDestination address;
            if (!ExtractDestination(txoutB.scriptPubKey, address))
                continue;

            if (address.type() != typeid(CKeyID))
                continue;

            CKeyID ckidMatch = boost::get<CKeyID>(address);

            if (HaveKey(ckidMatch)) // no point checking if already have key
                continue;

            std::set<CStealthAddress>::iterator it;
            for (it = stealthAddresses.begin(); it != stealthAddresses.end(); ++it)
            {
                if (it->scan_secret.size() != ec_secret_size)
                    continue; // stealth address is not owned

                //printf("it->Encodeded() %s\n",  it->Encoded().c_str());
                memcpy(&sScan.e[0], &it->scan_secret[0], ec_secret_size);

                if (StealthSecret(sScan, vchEphemPK, it->spend_pubkey, sShared, pkExtracted) != 0)
                {
                    printf("StealthSecret failed.\n");
                    continue;
                };
                //printf("pkExtracted %"PRIszu": %s\n", pkExtracted.size(), HexStr(pkExtracted).c_str());

                CPubKey cpkE(pkExtracted);

                if (!cpkE.IsValid())
                    continue;
                CKeyID ckidE = cpkE.GetID();

                if (ckidMatch != ckidE)
                    continue;

                if (fDebug)
                    printf("Found stealth txn to address %s\n", it->Encoded().c_str());

                if (IsLocked())
                {
                    if (fDebug)
                        printf("Wallet is locked, adding key without secret.\n");

                    // -- add key without secret
                    std::vector<uint8_t> vchEmpty;
                    AddCryptedKey(cpkE, vchEmpty);
                    CKeyID keyId = cpkE.GetID();
                    CBitcoinAddress coinAddress(keyId);
                    std::string sLabel = it->Encoded();
                    SetAddressBookName(keyId, sLabel);

                    CPubKey cpkEphem(vchEphemPK);
                    CPubKey cpkScan(it->scan_pubkey);
                    CStealthKeyMetadata lockedSkMeta(cpkEphem, cpkScan);

                    if (!CWalletDB(strWalletFile).WriteStealthKeyMeta(keyId, lockedSkMeta))
                        printf("WriteStealthKeyMeta failed for %s\n", coinAddress.ToString().c_str());

                    mapStealthKeyMeta[keyId] = lockedSkMeta;
                    nFoundStealth++;
                } else
                {
                    if (it->spend_secret.size() != ec_secret_size)
                        continue;
                    memcpy(&sSpend.e[0], &it->spend_secret[0], ec_secret_size);


                    if (StealthSharedToSecretSpend(sShared, sSpend, sSpendR) != 0)
                    {
                        printf("StealthSharedToSecretSpend() failed.\n");
                        continue;
                    };

                    ec_point pkTestSpendR;
                    if (SecretToPublicKey(sSpendR, pkTestSpendR) != 0)
                    {
                        printf("SecretToPublicKey() failed.\n");
                        continue;
                    };

                    CSecret vchSecret;
                    vchSecret.resize(ec_secret_size);

                    memcpy(&vchSecret[0], &sSpendR.e[0], ec_secret_size);
                    CKey ckey;

                    try {
		 	ckey.Set(vchSecret.begin(), vchSecret.end(), true);
                        //ckey.SetSecret(vchSecret, true);
                    } catch (std::exception& e) {
                        printf("ckey.SetSecret() threw: %s.\n", e.what());
                        continue;
                    };

                    CPubKey cpkT = ckey.GetPubKey();
                    if (!cpkT.IsValid())
                    {
                        printf("cpkT is invalid.\n");
                        continue;
                    };

                    if (!ckey.IsValid())
                    {
                        printf("Reconstructed key is invalid.\n");
                        continue;
                    };

                    CKeyID keyID = cpkT.GetID();
                    if (fDebug)
                    {
                        CBitcoinAddress coinAddress(keyID);
                        printf("Adding key %s.\n", coinAddress.ToString().c_str());
                    };

                    if (!AddKey(ckey))
                    {
                        printf("AddKey failed.\n");
                        continue;
                    };

                    std::string sLabel = it->Encoded();
                    SetAddressBookName(keyID, sLabel);
                    nFoundStealth++;
                };

                if (txout.scriptPubKey.GetOp(itTxA, opCode, vchENarr)
                    && opCode == OP_RETURN
                    && txout.scriptPubKey.GetOp(itTxA, opCode, vchENarr)
                    && vchENarr.size() > 0)
                {
                    SecMsgCrypter crypter;
                    crypter.SetKey(&sShared.e[0], &vchEphemPK[0]);
                    std::vector<uint8_t> vchNarr;
                    if (!crypter.Decrypt(&vchENarr[0], vchENarr.size(), vchNarr))
                    {
                        printf("Decrypt narration failed.\n");
                        continue;
                    };
                    std::string sNarr = std::string(vchNarr.begin(), vchNarr.end());

                    snprintf(cbuf, sizeof(cbuf), "n_%d", nOutputId);
                    mapNarr[cbuf] = sNarr;
                };

                txnMatch = true;
                break;
            };
            if (txnMatch)
                break;
        };
    };

    return true;
};




uint64_t CWallet::GetStakeWeight() const
{
    // Choose coins to use
    int64_t nBalance = GetBalance();

    if (nBalance <= nReserveBalance)
        return 0;

    vector<const CWalletTx*> vwtxPrev;

    set<pair<const CWalletTx*,unsigned int> > setCoins;
    int64_t nValueIn = 0;

    if (!SelectCoinsForStaking(nBalance - nReserveBalance, GetTime(), setCoins, nValueIn))
        return 0;

    if (setCoins.empty())
        return 0;

    uint64_t nWeight = 0;

    int64_t nCurrentTime = GetTime();
    CTxDB txdb("r");

    LOCK2(cs_main, cs_wallet);
    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        CTxIndex txindex;
        if (!txdb.ReadTxIndex(pcoin.first->GetHash(), txindex))
            continue;

        if (nCurrentTime - pcoin.first->nTime > nStakeMinAge)
            nWeight += pcoin.first->vout[pcoin.second].nValue;
    }

    return nWeight;
}

bool CWallet::CreateCoinStake(const CKeyStore& keystore, unsigned int nBits, int64_t nSearchInterval, int64_t nFees, CTransaction& txNew, CKey& key)
{
    CBlockIndex* pindexPrev = pindexBest;
    CBigNum bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);

    txNew.vin.clear();
    txNew.vout.clear();

    // Mark coin stake transaction
    CScript scriptEmpty;
    scriptEmpty.clear();
    txNew.vout.push_back(CTxOut(0, scriptEmpty));

    // Choose coins to use
    int64_t nBalance = GetBalance();

    if (nBalance <= nReserveBalance)
        return false;

    vector<const CWalletTx*> vwtxPrev;

    set<pair<const CWalletTx*,unsigned int> > setCoins;
    int64_t nValueIn = 0;

    // Select coins with suitable depth
    if (!SelectCoinsForStaking(nBalance - nReserveBalance, txNew.nTime, setCoins, nValueIn))
        return false;

    if (setCoins.empty())
        return false;

    int64_t nCredit = 0;
    CScript scriptPubKeyKernel;
    CTxDB txdb("r");
    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        static int nMaxStakeSearchInterval = 60;
        bool fKernelFound = false;
        for (unsigned int n=0; n<min(nSearchInterval,(int64_t)nMaxStakeSearchInterval) && !fKernelFound && pindexPrev == pindexBest; n++)
        {
            boost::this_thread::interruption_point();
            // Search backward in time from the given txNew timestamp
            // Search nSearchInterval seconds back up to nMaxStakeSearchInterval
            COutPoint prevoutStake = COutPoint(pcoin.first->GetHash(), pcoin.second);
            int64_t nBlockTime;
            if (CheckKernel(pindexPrev, nBits, txNew.nTime - n, prevoutStake, &nBlockTime))
            {
                // Found a kernel
                LogPrint("coinstake", "CreateCoinStake : kernel found\n");
                vector<valtype> vSolutions;
                txnouttype whichType;
                CScript scriptPubKeyOut;
                scriptPubKeyKernel = pcoin.first->vout[pcoin.second].scriptPubKey;
                if (!Solver(scriptPubKeyKernel, whichType, vSolutions))
                {
                    LogPrint("coinstake", "CreateCoinStake : failed to parse kernel\n");
                    break;
                }
                LogPrint("coinstake", "CreateCoinStake : parsed kernel type=%d\n", whichType);
                if (whichType != TX_PUBKEY && whichType != TX_PUBKEYHASH)
                {
                    LogPrint("coinstake", "CreateCoinStake : no support for kernel type=%d\n", whichType);
                    break;  // only support pay to public key and pay to address
                }
                if (whichType == TX_PUBKEYHASH) // pay to address type
                {
                    // convert to pay to public key type
                    if (!keystore.GetKey(uint160(vSolutions[0]), key))
                    {
                        LogPrint("coinstake", "CreateCoinStake : failed to get key for kernel type=%d\n", whichType);
                        break;  // unable to find corresponding public key
                    }
                    scriptPubKeyOut << key.GetPubKey() << OP_CHECKSIG;
                }
                if (whichType == TX_PUBKEY)
                {
                    valtype& vchPubKey = vSolutions[0];
                    if (!keystore.GetKey(Hash160(vchPubKey), key))
                    {
                        LogPrint("coinstake", "CreateCoinStake : failed to get key for kernel type=%d\n", whichType);
                        break;  // unable to find corresponding public key
                    }

                    if (key.GetPubKey() != vchPubKey)
                    {
                        LogPrint("coinstake", "CreateCoinStake : invalid key for kernel type=%d\n", whichType);
                        break; // keys mismatch
                    }

                    scriptPubKeyOut = scriptPubKeyKernel;
                }

                txNew.nTime -= n;
                txNew.vin.push_back(CTxIn(pcoin.first->GetHash(), pcoin.second));
                nCredit += pcoin.first->vout[pcoin.second].nValue;
                vwtxPrev.push_back(pcoin.first);
                txNew.vout.push_back(CTxOut(0, scriptPubKeyOut));

                if(nCredit > 100 * COIN)
                    txNew.vout.push_back(CTxOut(0, scriptPubKeyOut)); //split stake
                LogPrint("coinstake", "CreateCoinStake : added kernel type=%d\n", whichType);
                fKernelFound = true;
                break;
            }
        }

        if (fKernelFound)
            break; // if kernel is found stop searching
    }

    if (nCredit == 0 || nCredit > nBalance - nReserveBalance)
        return false;

    BOOST_FOREACH(PAIRTYPE(const CWalletTx*, unsigned int) pcoin, setCoins)
    {
        // Attempt to add more inputs
        // Only add coins of the same key/address as kernel
        if (txNew.vout.size() == 2 && ((pcoin.first->vout[pcoin.second].scriptPubKey == scriptPubKeyKernel || pcoin.first->vout[pcoin.second].scriptPubKey == txNew.vout[1].scriptPubKey))
            && pcoin.first->GetHash() != txNew.vin[0].prevout.hash)
        {
            int64_t nTimeWeight = GetWeight((int64_t)pcoin.first->nTime, (int64_t)txNew.nTime);

            // Stop adding more inputs if already too many inputs
            if (txNew.vin.size() >= 100)
                break;
            // Stop adding more inputs if value is already pretty significant
            if (nCredit >= GetStakeCombineThreshold())
                break;
            // Stop adding inputs if reached reserve limit
            if (nCredit + pcoin.first->vout[pcoin.second].nValue > nBalance - nReserveBalance)
                break;
            // Do not add additional significant input
            if (pcoin.first->vout[pcoin.second].nValue >= GetStakeCombineThreshold())
                continue;
            // Do not add input that is still too young
            if (nTimeWeight < nStakeMinAge)
                continue;

            txNew.vin.push_back(CTxIn(pcoin.first->GetHash(), pcoin.second));
            nCredit += pcoin.first->vout[pcoin.second].nValue;
            vwtxPrev.push_back(pcoin.first);
        }
    }

    // Calculate coin age reward
    int64_t nReward;
    {
        uint64_t nCoinAge;
        CTxDB txdb("r");
        if (!txNew.GetCoinAge(txdb, nCoinAge))
            return error("CreateCoinStake : failed to calculate coin age");

        nReward = GetProofOfStakeReward(pindexPrev->nHeight + 1, nCoinAge, nFees);
        if (nReward <= 0)
            return false;

        nCredit += nReward;
    }


    // Masternode Payments
    int payments = 1;
    // start masternode payments
    bool bMasterNodePayment = true; // note was false, set true to test

    if ( Params().NetworkID() == CChainParams::TESTNET ){
        if (GetTime() > START_MASTERNODE_PAYMENTS_TESTNET ){
            bMasterNodePayment = true;
        }
    }else{
        if (GetTime() > START_MASTERNODE_PAYMENTS){
            bMasterNodePayment = true;
        }
    }

    CScript payee;
    bool hasPayment = true;
    if(bMasterNodePayment) {
        //spork
        if(!masternodePayments.GetBlockPayee(pindexPrev->nHeight+1, payee)){
            int winningNode = GetCurrentMasterNode(1);
                if(winningNode >= 0){
                    payee =GetScriptForDestination(vecMasternodes[winningNode].pubkey.GetID());
                } else {
                    LogPrintf("CreateCoinStake: Failed to detect masternode to pay\n");
                    hasPayment = false;
                }
        }
    }

    if(hasPayment){
        payments = txNew.vout.size() + 1;
        txNew.vout.resize(payments);

        txNew.vout[payments-1].scriptPubKey = payee;
        txNew.vout[payments-1].nValue = 0;

        CTxDestination address1;
        ExtractDestination(payee, address1);
        CBitcoinAddress address2(address1);

        LogPrintf("Masternode payment to %s\n", address2.ToString().c_str());
    }

    int64_t blockValue = nCredit;
    int64_t masternodePayment = GetMasternodePayment(pindexPrev->nHeight+1, nReward);


    // Set output amount
    if (!hasPayment && txNew.vout.size() == 3) // 2 stake outputs, stake was split, no masternode payment
    {
        txNew.vout[1].nValue = (blockValue / 2 / CENT) * CENT;
        txNew.vout[2].nValue = blockValue - txNew.vout[1].nValue;
    }
    else if(hasPayment && txNew.vout.size() == 4) // 2 stake outputs, stake was split, plus a masternode payment
    {
        txNew.vout[payments-1].nValue = masternodePayment;
        blockValue -= masternodePayment;
        txNew.vout[1].nValue = (blockValue / 2 / CENT) * CENT;
        txNew.vout[2].nValue = blockValue - txNew.vout[1].nValue;
    }
    else if(!hasPayment && txNew.vout.size() == 2) // only 1 stake output, was not split, no masternode payment
        txNew.vout[1].nValue = blockValue;
    else if(hasPayment && txNew.vout.size() == 3) // only 1 stake output, was not split, plus a masternode payment
    {
        txNew.vout[payments-1].nValue = masternodePayment;
        blockValue -= masternodePayment;
        txNew.vout[1].nValue = blockValue;
    }

    // Sign
    int nIn = 0;
    BOOST_FOREACH(const CWalletTx* pcoin, vwtxPrev)
    {
        if (!SignSignature(*this, *pcoin, txNew, nIn++))
            return error("CreateCoinStake : failed to sign coinstake");
    }

    // Limit size
    unsigned int nBytes = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);
    if (nBytes >= MAX_BLOCK_SIZE_GEN/5)
        return error("CreateCoinStake : exceeded coinstake size limit");

    // Successfully generated coinstake
    return true;
}


// Call after CreateTransaction unless you want to abort
bool CWallet::CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey)
{
    mapValue_t mapNarr;
    FindStealthTransactions(wtxNew, mapNarr);

    if (!mapNarr.empty())
    {
        BOOST_FOREACH(const PAIRTYPE(string,string)& item, mapNarr)
            wtxNew.mapValue[item.first] = item.second;
    };

    {
        LOCK2(cs_main, cs_wallet);
        LogPrintf("CommitTransaction:\n%s", wtxNew.ToString());
        {
            // This is only to keep the database open to defeat the auto-flush for the
            // duration of this scope.  This is the only place where this optimization
            // maybe makes sense; please don't do it anywhere else.
            CWalletDB* pwalletdb = fFileBacked ? new CWalletDB(strWalletFile,"r") : NULL;

            // Take key pair from key pool so it won't be used again
            reservekey.KeepKey();

            // Add tx to wallet, because if it has change it's also ours,
            // otherwise just for transaction history.
            AddToWallet(wtxNew);

            // Mark old coins as spent
            set<CWalletTx*> setCoins;
            BOOST_FOREACH(const CTxIn& txin, wtxNew.vin)
            {
                CWalletTx &coin = mapWallet[txin.prevout.hash];
                coin.BindWallet(this);
                coin.MarkSpent(txin.prevout.n);
                coin.WriteToDisk();
                NotifyTransactionChanged(this, coin.GetHash(), CT_UPDATED);
            }

            if (fFileBacked)
                delete pwalletdb;
        }

        // Track how many getdata requests our transaction gets
        mapRequestCount[wtxNew.GetHash()] = 0;

        // Broadcast
        if (!wtxNew.AcceptToMemoryPool(true))
        {
            // This must not fail. The transaction has already been signed and recorded.
            LogPrintf("CommitTransaction() : Error: Transaction not valid\n");
            return false;
        }
        wtxNew.RelayWalletTransaction();
    }
    return true;
}




string CWallet::SendMoney(CScript scriptPubKey, int64_t nValue, std::string& sNarr, CWalletTx& wtxNew, bool fAskFee)
{
    CReserveKey reservekey(this);
    int64_t nFeeRequired;

    if (IsLocked())
    {
        string strError = _("Error: Wallet locked, unable to create transaction!");
        LogPrintf("SendMoney() : %s", strError);
        return strError;
    }
    if (fWalletUnlockStakingOnly)
    {
        string strError = _("Error: Wallet unlocked for staking only, unable to create transaction.");
        LogPrintf("SendMoney() : %s", strError);
        return strError;
    }
    if (!CreateTransaction(scriptPubKey, nValue, sNarr, wtxNew, reservekey, nFeeRequired))
    {
        string strError;
        if (nValue + nFeeRequired > GetBalance())
            strError = strprintf(_("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!"), FormatMoney(nFeeRequired));
        else
            strError = _("Error: Transaction creation failed!");
        LogPrintf("SendMoney() : %s\n", strError);
        return strError;
    }

    if (fAskFee && !uiInterface.ThreadSafeAskFee(nFeeRequired, _("Sending...")))
        return "ABORTED";

    if (!CommitTransaction(wtxNew, reservekey))
        return _("Error: The transaction was rejected! This might happen if some of the coins in your wallet were already spent, such as if you used a copy of wallet.dat and coins were spent in the copy but not marked as spent here.");

    return "";
}



string CWallet::SendMoneyToDestination(const CTxDestination& address, int64_t nValue, std::string& sNarr, CWalletTx& wtxNew, bool fAskFee)
{
    // Check amount
    if (nValue <= 0)
        return _("Invalid amount");
    if (nValue + nTransactionFee > GetBalance())
        return _("Insufficient funds");

    if (sNarr.length() > 24)
        return _("Narration must be 24 characters or less.");

    // Parse Bitcoin address
    CScript scriptPubKey;
    scriptPubKey.SetDestination(address);

    return SendMoney(scriptPubKey, nValue, sNarr, wtxNew, fAskFee);
}


string CWallet::PrepareLuxsendDenominate(int minRounds, int maxRounds)
{
    if (IsLocked())
        return _("Error: Wallet locked, unable to create transaction!");

    if(darkSendPool.GetState() != POOL_STATUS_ERROR && darkSendPool.GetState() != POOL_STATUS_SUCCESS)
        if(darkSendPool.GetMyTransactionCount() > 0)
            return _("Error: You already have pending entries in the Luxsend pool");

    // ** find the coins we'll use
    std::vector<CTxIn> vCoins;
    std::vector<COutput> vCoins2;
    int64_t nValueIn = 0;
    CReserveKey reservekey(this);

    /*
        Select the coins we'll use
        if minRounds >= 0 it means only denominated inputs are going in and coming out
    */
    if(minRounds >= 0){
        if (!SelectCoinsByDenominations(darkSendPool.sessionDenom, 0.1*COIN, DARKSEND_POOL_MAX, vCoins, vCoins2, nValueIn, minRounds, maxRounds))
            return _("Insufficient funds");
    }

    // calculate total value out
    int64_t nTotalValue = GetTotalValue(vCoins);
    LogPrintf("PrepareLuxsendDenominate - preparing darksend denominate . Got: %d \n", nTotalValue);

    //--------------
    BOOST_FOREACH(CTxIn v, vCoins)
        LockCoin(v.prevout);

    // denominate our funds
    int64_t nValueLeft = nTotalValue;
    std::vector<CTxOut> vOut;
    std::vector<int64_t> vDenoms;

    /*
        TODO: Front load with needed denominations (e.g. .1, 1 )
    */

    /*
        Add all denominations once
        The beginning of the list is front loaded with each possible
        denomination in random order. This means we'll at least get 1
        of each that is required as outputs.
    */
    BOOST_FOREACH(int64_t d, darkSendDenominations){
        vDenoms.push_back(d);
        vDenoms.push_back(d);
    }

    //randomize the order of these denominations
    std::random_shuffle (vDenoms.begin(), vDenoms.end());

    /*
        Build a long list of denominations
        Next we'll build a long random list of denominations to add.
        Eventually as the algorithm goes through these it'll find the ones
        it nees to get exact change.
    */
    for(int i = 0; i <= 500; i++)
        BOOST_FOREACH(int64_t d, darkSendDenominations)
            vDenoms.push_back(d);

    //randomize the order of inputs we get back
    std::random_shuffle (vDenoms.begin() + (int)darkSendDenominations.size() + 1, vDenoms.end());

    // Make outputs by looping through denominations randomly
    BOOST_REVERSE_FOREACH(int64_t v, vDenoms){
        //only use the ones that are approved
        bool fAccepted = false;
        if((darkSendPool.sessionDenom & (1 << 0))      && v == ((100000*COIN) +100000000)) {fAccepted = true;}
        else if((darkSendPool.sessionDenom & (1 << 1)) && v == ((10000*COIN)  +10000000)) {fAccepted = true;}
        else if((darkSendPool.sessionDenom & (1 << 2)) && v == ((1000*COIN)   +1000000)) {fAccepted = true;}
        else if((darkSendPool.sessionDenom & (1 << 3)) && v == ((100*COIN)    +100000)) {fAccepted = true;}
        else if((darkSendPool.sessionDenom & (1 << 4)) && v == ((10*COIN)     +10000)) {fAccepted = true;}
        else if((darkSendPool.sessionDenom & (1 << 5)) && v == ((1*COIN)      +1000)) {fAccepted = true;}
        else if((darkSendPool.sessionDenom & (1 << 6)) && v == ((.1*COIN)     +100)) {fAccepted = true;}
        if(!fAccepted) continue;

        int nOutputs = 0;

        // add each output up to 10 times until it can't be added again
        if(nValueLeft - v >= 0 && nOutputs <= 10) {
            CScript scriptChange;
            CPubKey vchPubKey;
            //use a unique change address
            assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked
            scriptChange =GetScriptForDestination(vchPubKey.GetID());
            reservekey.KeepKey();

            CTxOut o(v, scriptChange);
            vOut.push_back(o);

            //increment outputs and subtract denomination amount
            nOutputs++;
            nValueLeft -= v;
        }

        if(nValueLeft == 0) break;
    }

    //back up mode , incase we couldn't successfully make the outputs for some reason
    if(vOut.size() > 40 || darkSendPool.GetDenominations(vOut) != darkSendPool.sessionDenom || nValueLeft != 0){
        vOut.clear();
        nValueLeft = nTotalValue;

        // Make outputs by looping through denominations, from small to large

        BOOST_FOREACH(const COutput& out, vCoins2){
            CScript scriptChange;
            CPubKey vchPubKey;
            //use a unique change address
            assert(reservekey.GetReservedKey(vchPubKey)); // should never fail, as we just unlocked
            scriptChange =GetScriptForDestination(vchPubKey.GetID());
            reservekey.KeepKey();

            CTxOut o(out.tx->vout[out.i].nValue, scriptChange);
            vOut.push_back(o);

            //increment outputs and subtract denomination amount
            nValueLeft -= out.tx->vout[out.i].nValue;

            if(nValueLeft == 0) break;
        }

    }

    if(darkSendPool.GetDenominations(vOut) != darkSendPool.sessionDenom)
        return "Error: can't make current denominated outputs";

    // we don't support change at all
    if(nValueLeft != 0)
        return "Error: change left-over in pool. Must use denominations only";


    //randomize the output order
    std::random_shuffle (vOut.begin(), vOut.end());

    darkSendPool.SendLuxsendDenominate(vCoins, vOut, nValueIn);

    return "";
}

int64_t CWallet::GetTotalValue(std::vector<CTxIn> vCoins) {
    int64_t nTotalValue = 0;
    CWalletTx wtx;
    BOOST_FOREACH(CTxIn i, vCoins){
        if (mapWallet.count(i.prevout.hash))
        {
            CWalletTx& wtx = mapWallet[i.prevout.hash];
            if(i.prevout.n < wtx.vout.size()){
                nTotalValue += wtx.vout[i.prevout.n].nValue;
            }
        } else {
            LogPrintf("GetTotalValue -- Couldn't find transaction\n");
        }
    }
    return nTotalValue;
}


DBErrors CWallet::LoadWallet(bool& fFirstRunRet)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    fFirstRunRet = false;
    DBErrors nLoadWalletRet = CWalletDB(strWalletFile,"cr+").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE)
    {
        if (CDB::Rewrite(strWalletFile, "\x04pool"))
        {
            LOCK(cs_wallet);
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // the requires a new key.
        }
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;
    fFirstRunRet = !vchDefaultKey.IsValid();

    return DB_LOAD_OK;
}


bool CWallet::SetAddressBookName(const CTxDestination& address, const string& strName)
{
    bool fUpdated = false;
    {
        LOCK(cs_wallet); // mapAddressBook
        std::map<CTxDestination, std::string>::iterator mi = mapAddressBook.find(address);
        fUpdated = mi != mapAddressBook.end();
        mapAddressBook[address] = strName;
    }
    NotifyAddressBookChanged(this, address, strName, ::IsMine(*this, address),
                             (fUpdated ? CT_UPDATED : CT_NEW) );
    if (!fFileBacked)
        return false;
    return CWalletDB(strWalletFile).WriteName(CBitcoinAddress(address).ToString(), strName);
}

bool CWallet::DelAddressBookName(const CTxDestination& address)
{
    {
        LOCK(cs_wallet); // mapAddressBook

        mapAddressBook.erase(address);
    }

    NotifyAddressBookChanged(this, address, "", ::IsMine(*this, address), CT_DELETED);

    if (!fFileBacked)
        return false;
    return CWalletDB(strWalletFile).EraseName(CBitcoinAddress(address).ToString());
}

bool CWallet::SetDefaultKey(const CPubKey &vchPubKey)
{
    if (fFileBacked)
    {
        if (!CWalletDB(strWalletFile).WriteDefaultKey(vchPubKey))
            return false;
    }
    vchDefaultKey = vchPubKey;
    return true;
}

//
// Mark old keypool keys as used,
// and generate all new keys
//
bool CWallet::NewKeyPool()
{
    {
        LOCK(cs_wallet);
        CWalletDB walletdb(strWalletFile);
        BOOST_FOREACH(int64_t nIndex, setKeyPool)
            walletdb.ErasePool(nIndex);
        setKeyPool.clear();

        if (IsLocked())
            return false;

        int64_t nKeys = max(GetArg("-keypool", 100), (int64_t)0);
        for (int i = 0; i < nKeys; i++)
        {
            int64_t nIndex = i+1;
            walletdb.WritePool(nIndex, CKeyPool(GenerateNewKey()));
            setKeyPool.insert(nIndex);
        }
        LogPrintf("CWallet::NewKeyPool wrote %d new keys\n", nKeys);
    }
    return true;
}

bool CWallet::TopUpKeyPool(unsigned int nSize)
{
    {
        LOCK(cs_wallet);

        if (IsLocked())
            return false;

        CWalletDB walletdb(strWalletFile);

        // Top up key pool
        unsigned int nTargetSize;
        if (nSize > 0)
            nTargetSize = nSize;
        else
            nTargetSize = max(GetArg("-keypool", 100), (int64_t)0);

        while (setKeyPool.size() < (nTargetSize + 1))
        {
            int64_t nEnd = 1;
            if (!setKeyPool.empty())
                nEnd = *(--setKeyPool.end()) + 1;
            if (!walletdb.WritePool(nEnd, CKeyPool(GenerateNewKey())))
                throw runtime_error("TopUpKeyPool() : writing generated key failed");
            setKeyPool.insert(nEnd);
            LogPrintf("keypool added key %d, size=%u\n", nEnd, setKeyPool.size());
        }
    }
    return true;
}

void CWallet::ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool)
{
    nIndex = -1;
    keypool.vchPubKey = CPubKey();
    {
        LOCK(cs_wallet);

        if (!IsLocked())
            TopUpKeyPool();

        // Get the oldest key
        if(setKeyPool.empty())
            return;

        CWalletDB walletdb(strWalletFile);

        nIndex = *(setKeyPool.begin());
        setKeyPool.erase(setKeyPool.begin());
        if (!walletdb.ReadPool(nIndex, keypool))
            throw runtime_error("ReserveKeyFromKeyPool() : read failed");
        if (!HaveKey(keypool.vchPubKey.GetID()))
            throw runtime_error("ReserveKeyFromKeyPool() : unknown key in key pool");
        assert(keypool.vchPubKey.IsValid());
        LogPrintf("keypool reserve %d\n", nIndex);
    }
}

int64_t CWallet::AddReserveKey(const CKeyPool& keypool)
{
    {
        LOCK2(cs_main, cs_wallet);
        CWalletDB walletdb(strWalletFile);

        int64_t nIndex = 1 + *(--setKeyPool.end());
        if (!walletdb.WritePool(nIndex, keypool))
            throw runtime_error("AddReserveKey() : writing added key failed");
        setKeyPool.insert(nIndex);
        return nIndex;
    }
    return -1;
}

void CWallet::KeepKey(int64_t nIndex)
{
    // Remove from key pool
    if (fFileBacked)
    {
        CWalletDB walletdb(strWalletFile);
        walletdb.ErasePool(nIndex);
    }
    LogPrintf("keypool keep %d\n", nIndex);
}

void CWallet::ReturnKey(int64_t nIndex)
{
    // Return to key pool
    {
        LOCK(cs_wallet);
        setKeyPool.insert(nIndex);
    }
    LogPrintf("keypool return %d\n", nIndex);
}

bool CWallet::GetKeyFromPool(CPubKey& result)
{
    int64_t nIndex = 0;
    CKeyPool keypool;
    {
        LOCK(cs_wallet);
        ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex == -1)
        {
            if (IsLocked()) return false;
            result = GenerateNewKey();
            return true;
        }
        KeepKey(nIndex);
        result = keypool.vchPubKey;
    }
    return true;
}

int64_t CWallet::GetOldestKeyPoolTime()
{
    int64_t nIndex = 0;
    CKeyPool keypool;
    ReserveKeyFromKeyPool(nIndex, keypool);
    if (nIndex == -1)
        return GetTime();
    ReturnKey(nIndex);
    return keypool.nTime;
}

std::map<CTxDestination, int64_t> CWallet::GetAddressBalances()
{
    map<CTxDestination, int64_t> balances;

    {
        LOCK(cs_wallet);
        BOOST_FOREACH(PAIRTYPE(uint256, CWalletTx) walletEntry, mapWallet)
        {
            CWalletTx *pcoin = &walletEntry.second;

            if (!IsFinalTx(*pcoin) || !pcoin->IsTrusted())
                continue;

            if ((pcoin->IsCoinBase() || pcoin->IsCoinStake()) && pcoin->GetBlocksToMaturity() > 0)
                continue;

            int nDepth = pcoin->GetDepthInMainChain();
            if (nDepth < (pcoin->IsFromMe() ? 0 : 1))
                continue;

            for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            {
                CTxDestination addr;
                if (!IsMine(pcoin->vout[i]))
                    continue;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, addr))
                    continue;

                int64_t n = pcoin->IsSpent(i) ? 0 : pcoin->vout[i].nValue;

                if (!balances.count(addr))
                    balances[addr] = 0;
                balances[addr] += n;
            }
        }
    }

    return balances;
}

set< set<CTxDestination> > CWallet::GetAddressGroupings()
{
    AssertLockHeld(cs_wallet); // mapWallet
    set< set<CTxDestination> > groupings;
    set<CTxDestination> grouping;

    BOOST_FOREACH(PAIRTYPE(uint256, CWalletTx) walletEntry, mapWallet)
    {
        CWalletTx *pcoin = &walletEntry.second;

        if (pcoin->vin.size() > 0 && IsMine(pcoin->vin[0]))
        {
            // group all input addresses with each other
            BOOST_FOREACH(CTxIn txin, pcoin->vin)
            {
                CTxDestination address;
                if(!ExtractDestination(mapWallet[txin.prevout.hash].vout[txin.prevout.n].scriptPubKey, address))
                    continue;
                grouping.insert(address);
            }

            // group change with input addresses
            BOOST_FOREACH(CTxOut txout, pcoin->vout)
                if (IsChange(txout))
                {
                    CWalletTx tx = mapWallet[pcoin->vin[0].prevout.hash];
                    CTxDestination txoutAddr;
                    if(!ExtractDestination(txout.scriptPubKey, txoutAddr))
                        continue;
                    grouping.insert(txoutAddr);
                }
            groupings.insert(grouping);
            grouping.clear();
        }

        // group lone addrs by themselves
        for (unsigned int i = 0; i < pcoin->vout.size(); i++)
            if (IsMine(pcoin->vout[i]))
            {
                CTxDestination address;
                if(!ExtractDestination(pcoin->vout[i].scriptPubKey, address))
                    continue;
                grouping.insert(address);
                groupings.insert(grouping);
                grouping.clear();
            }
    }

    set< set<CTxDestination>* > uniqueGroupings; // a set of pointers to groups of addresses
    map< CTxDestination, set<CTxDestination>* > setmap;  // map addresses to the unique group containing it
    BOOST_FOREACH(set<CTxDestination> grouping, groupings)
    {
        // make a set of all the groups hit by this new group
        set< set<CTxDestination>* > hits;
        map< CTxDestination, set<CTxDestination>* >::iterator it;
        BOOST_FOREACH(CTxDestination address, grouping)
            if ((it = setmap.find(address)) != setmap.end())
                hits.insert((*it).second);

        // merge all hit groups into a new single group and delete old groups
        set<CTxDestination>* merged = new set<CTxDestination>(grouping);
        BOOST_FOREACH(set<CTxDestination>* hit, hits)
        {
            merged->insert(hit->begin(), hit->end());
            uniqueGroupings.erase(hit);
            delete hit;
        }
        uniqueGroupings.insert(merged);

        // update setmap
        BOOST_FOREACH(CTxDestination element, *merged)
            setmap[element] = merged;
    }

    set< set<CTxDestination> > ret;
    BOOST_FOREACH(set<CTxDestination>* uniqueGrouping, uniqueGroupings)
    {
        ret.insert(*uniqueGrouping);
        delete uniqueGrouping;
    }

    return ret;
}

// ppcoin: check 'spent' consistency between wallet and txindex
// ppcoin: fix wallet spent state according to txindex
void CWallet::FixSpentCoins(int& nMismatchFound, int64_t& nBalanceInQuestion, bool fCheckOnly)
{
    nMismatchFound = 0;
    nBalanceInQuestion = 0;

    LOCK(cs_wallet);
    vector<CWalletTx*> vCoins;
    vCoins.reserve(mapWallet.size());
    for (map<uint256, CWalletTx>::iterator it = mapWallet.begin(); it != mapWallet.end(); ++it)
        vCoins.push_back(&(*it).second);

    CTxDB txdb("r");
    BOOST_FOREACH(CWalletTx* pcoin, vCoins)
    {
        // Find the corresponding transaction index
        CTxIndex txindex;
        if (!txdb.ReadTxIndex(pcoin->GetHash(), txindex))
            continue;
        for (unsigned int n=0; n < pcoin->vout.size(); n++)
        {
            if (IsMine(pcoin->vout[n]) && pcoin->IsSpent(n) && (txindex.vSpent.size() <= n || txindex.vSpent[n].IsNull()))
            {
                LogPrintf("FixSpentCoins found lost coin %s BC %s[%d], %s\n",
                    FormatMoney(pcoin->vout[n].nValue), pcoin->GetHash().ToString(), n, fCheckOnly? "repair not attempted" : "repairing");
                nMismatchFound++;
                nBalanceInQuestion += pcoin->vout[n].nValue;
                if (!fCheckOnly)
                {
                    pcoin->MarkUnspent(n);
                    pcoin->WriteToDisk();
                }
            }
            else if (IsMine(pcoin->vout[n]) && !pcoin->IsSpent(n) && (txindex.vSpent.size() > n && !txindex.vSpent[n].IsNull()))
            {
                LogPrintf("FixSpentCoins found spent coin %s BC %s[%d], %s\n",
                    FormatMoney(pcoin->vout[n].nValue), pcoin->GetHash().ToString(), n, fCheckOnly? "repair not attempted" : "repairing");
                nMismatchFound++;
                nBalanceInQuestion += pcoin->vout[n].nValue;
                if (!fCheckOnly)
                {
                    pcoin->MarkSpent(n);
                    pcoin->WriteToDisk();
                }
            }
        }
    }
}

// ppcoin: disable transaction (only for coinstake)
void CWallet::DisableTransaction(const CTransaction &tx)
{
    if (!tx.IsCoinStake() || !IsFromMe(tx))
        return; // only disconnecting coinstake requires marking input unspent

    LOCK(cs_wallet);
    BOOST_FOREACH(const CTxIn& txin, tx.vin)
    {
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(txin.prevout.hash);
        if (mi != mapWallet.end())
        {
            CWalletTx& prev = (*mi).second;
            if (txin.prevout.n < prev.vout.size() && IsMine(prev.vout[txin.prevout.n]))
            {
                prev.MarkUnspent(txin.prevout.n);
                prev.WriteToDisk();
            }
        }
    }
}

bool CReserveKey::GetReservedKey(CPubKey& pubkey)
{
    if (nIndex == -1)
    {
        CKeyPool keypool;
        pwallet->ReserveKeyFromKeyPool(nIndex, keypool);
        if (nIndex != -1)
            vchPubKey = keypool.vchPubKey;
        else {
            if (pwallet->vchDefaultKey.IsValid()) {
                LogPrintf("CReserveKey::GetReservedKey(): Warning: Using default key instead of a new key, top up your keypool!");
                vchPubKey = pwallet->vchDefaultKey;
            } else
                return false;
        }
    }
    assert(vchPubKey.IsValid());
    pubkey = vchPubKey;
    return true;
}

void CReserveKey::KeepKey()
{
    if (nIndex != -1)
        pwallet->KeepKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CReserveKey::ReturnKey()
{
    if (nIndex != -1)
        pwallet->ReturnKey(nIndex);
    nIndex = -1;
    vchPubKey = CPubKey();
}

void CWallet::GetAllReserveKeys(set<CKeyID>& setAddress) const
{
    setAddress.clear();

    CWalletDB walletdb(strWalletFile);

    LOCK2(cs_main, cs_wallet);
    BOOST_FOREACH(const int64_t& id, setKeyPool)
    {
        CKeyPool keypool;
        if (!walletdb.ReadPool(id, keypool))
            throw runtime_error("GetAllReserveKeyHashes() : read failed");
        assert(keypool.vchPubKey.IsValid());
        CKeyID keyID = keypool.vchPubKey.GetID();
        if (!HaveKey(keyID))
            throw runtime_error("GetAllReserveKeyHashes() : unknown key in key pool");
        setAddress.insert(keyID);
    }
}

void CWallet::UpdatedTransaction(const uint256 &hashTx)
{
    {
        LOCK(cs_wallet);
        // Only notify UI if this transaction is in this wallet
        map<uint256, CWalletTx>::const_iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
            NotifyTransactionChanged(this, hashTx, CT_UPDATED);
    }
}


void CWallet::LockCoin(COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.insert(output);
}

void CWallet::UnlockCoin(COutPoint& output)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.erase(output);
}

void CWallet::UnlockAllCoins()
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    setLockedCoins.clear();
}

bool CWallet::IsLockedCoin(uint256 hash, unsigned int n) const
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    COutPoint outpt(hash, n);

    return (setLockedCoins.count(outpt) > 0);
}

void CWallet::ListLockedCoins(std::vector<COutPoint>& vOutpts)
{
    AssertLockHeld(cs_wallet); // setLockedCoins
    for (std::set<COutPoint>::iterator it = setLockedCoins.begin();
         it != setLockedCoins.end(); it++) {
        COutPoint outpt = (*it);
        vOutpts.push_back(outpt);
    }
}


void CWallet::GetKeyBirthTimes(std::map<CKeyID, int64_t> &mapKeyBirth) const {
    AssertLockHeld(cs_wallet); // mapKeyMetadata
    mapKeyBirth.clear();

    // get birth times for keys with metadata
    for (std::map<CKeyID, CKeyMetadata>::const_iterator it = mapKeyMetadata.begin(); it != mapKeyMetadata.end(); it++)
        if (it->second.nCreateTime)
            mapKeyBirth[it->first] = it->second.nCreateTime;

    // map in which we'll infer heights of other keys
    CBlockIndex *pindexMax = FindBlockByHeight(std::max(0, nBestHeight - 144)); // the tip can be reorganised; use a 144-block safety margin
    std::map<CKeyID, CBlockIndex*> mapKeyFirstBlock;
    std::set<CKeyID> setKeys;
    GetKeys(setKeys);
    BOOST_FOREACH(const CKeyID &keyid, setKeys) {
        if (mapKeyBirth.count(keyid) == 0)
            mapKeyFirstBlock[keyid] = pindexMax;
    }
    setKeys.clear();

    // if there are no such keys, we're done
    if (mapKeyFirstBlock.empty())
        return;

    // find first block that affects those keys, if there are any left
    std::vector<CKeyID> vAffected;
    for (std::map<uint256, CWalletTx>::const_iterator it = mapWallet.begin(); it != mapWallet.end(); it++) {
        // iterate over all wallet transactions...
        const CWalletTx &wtx = (*it).second;
        std::map<uint256, CBlockIndex*>::const_iterator blit = mapBlockIndex.find(wtx.hashBlock);
        if (blit != mapBlockIndex.end() && blit->second->IsInMainChain()) {
            // ... which are already in a block
            int nHeight = blit->second->nHeight;
            BOOST_FOREACH(const CTxOut &txout, wtx.vout) {
                // iterate over all their outputs
                ::ExtractAffectedKeys(*this, txout.scriptPubKey, vAffected);
                BOOST_FOREACH(const CKeyID &keyid, vAffected) {
                    // ... and all their affected keys
                    std::map<CKeyID, CBlockIndex*>::iterator rit = mapKeyFirstBlock.find(keyid);
                    if (rit != mapKeyFirstBlock.end() && nHeight < rit->second->nHeight)
                        rit->second = blit->second;
                }
                vAffected.clear();
            }
        }
    }

    // Extract block timestamps for those keys
    for (std::map<CKeyID, CBlockIndex*>::const_iterator it = mapKeyFirstBlock.begin(); it != mapKeyFirstBlock.end(); it++)
        mapKeyBirth[it->first] = it->second->nTime - 7200; // block times can be 2h off
}

bool CWallet::GetTransaction(const uint256 &hashTx, CWalletTx& wtx)
{
    {
        LOCK(cs_wallet);
        map<uint256, CWalletTx>::iterator mi = mapWallet.find(hashTx);
        if (mi != mapWallet.end())
        {
            wtx = (*mi).second;
            return true;
        }
    }
    return false;
}

int CMerkleTx::GetTransactionLockSignatures() const
{
    if(!IsSporkActive(SPORK_1_MASTERNODE_PAYMENTS_ENFORCEMENT)) return -3;
    if(nInstantXDepth == 0) return -1;

    //compile consessus vote
    std::map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(GetHash());
    if (i != mapTxLocks.end()){
        return (*i).second.CountSignatures();
    }

    return -1;
}

bool CMerkleTx::IsTransactionLockTimedOut() const
{
    if(nInstantXDepth == 0) return 0;

    //compile consessus vote
    std::map<uint256, CTransactionLock>::iterator i = mapTxLocks.find(GetHash());
    if (i != mapTxLocks.end()){
        return GetTime() > (*i).second.nTimeout;
    }

    return false;
}

bool CWallet::AddAdrenalineNodeConfig(CAdrenalineNodeConfig nodeConfig)
{
    bool rv = CWalletDB(strWalletFile).WriteAdrenalineNodeConfig(nodeConfig.sAlias, nodeConfig);
    if(rv)
	uiInterface.NotifyAdrenalineNodeChanged(nodeConfig);

    return rv;
}
