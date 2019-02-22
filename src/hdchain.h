// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUX_HDCHAIN_H
#define LUX_HDCHAIN_H

#include "key.h"
#include "sync.h"

/* hd account data model */
class CHDAccount
{
public:
    uint32_t nExternalChainCounter;
    uint32_t nInternalChainCounter;

    CHDAccount() : nExternalChainCounter(0), nInternalChainCounter(0) {}

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(nExternalChainCounter);
        READWRITE(nInternalChainCounter);
    }
};

/* simple HD chain data model */
class CHDChain
{
private:
    static const int CURRENT_VERSION = 1;
    int nVersion;

    uint256 id;

    bool fCrypted;

    SecureVector vchSeed;
    SecureVector vchMnemonic;
    SecureVector vchMnemonicPassphrase;

    std::map<uint32_t, CHDAccount> mapAccounts;
    // critical section to protect mapAccounts
    mutable CCriticalSection cs_accounts;

public:

    CHDChain() : nVersion(CHDChain::CURRENT_VERSION) { SetNull(); }
    CHDChain(const CHDChain& other) :
            nVersion(other.nVersion),
            id(other.id),
            fCrypted(other.fCrypted),
            vchSeed(other.vchSeed),
            vchMnemonic(other.vchMnemonic),
            vchMnemonicPassphrase(other.vchMnemonicPassphrase),
            mapAccounts(other.mapAccounts)
    {}

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        LOCK(cs_accounts);
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(id);
        READWRITE(fCrypted);
        READWRITE(vchSeed);
        READWRITE(vchMnemonic);
        READWRITE(vchMnemonicPassphrase);
        READWRITE(mapAccounts);
    }

    void swap(CHDChain& first, CHDChain& second) // nothrow
    {
        // enable ADL (not necessary in our case, but good practice)
        using std::swap;

        // by swapping the members of two classes,
        // the two classes are effectively swapped
        swap(first.nVersion, second.nVersion);
        swap(first.id, second.id);
        swap(first.fCrypted, second.fCrypted);
        swap(first.vchSeed, second.vchSeed);
        swap(first.vchMnemonic, second.vchMnemonic);
        swap(first.vchMnemonicPassphrase, second.vchMnemonicPassphrase);
        swap(first.mapAccounts, second.mapAccounts);
    }
    CHDChain& operator=(CHDChain from)
    {
        swap(*this, from);
        return *this;
    }

    bool SetNull();
    bool IsNull() const;

    void SetCrypted(bool fCryptedIn);
    bool IsCrypted() const;

    void Debug(std::string strName) const;

    bool SetMnemonic(const SecureVector& vchMnemonic, const SecureVector& vchMnemonicPassphrase, bool fUpdateID);
    bool SetMnemonic(const SecureString& ssMnemonic, const SecureString& ssMnemonicPassphrase, bool fUpdateID);
    bool GetMnemonic(SecureVector& vchMnemonicRet, SecureVector& vchMnemonicPassphraseRet) const;
    bool GetMnemonic(SecureString& ssMnemonicRet, SecureString& ssMnemonicPassphraseRet) const;

    bool SetSeed(const SecureVector& vchSeedIn, bool fUpdateID);
    SecureVector GetSeed() const;

    uint256 GetID() const { return id; }

    uint256 GetSeedHash();
    void DeriveChildExtKey(uint32_t nAccountIndex, bool internal, uint32_t nChildIndex, CExtKey& extKeyRet);

    void AddAccount();
    bool GetAccount(uint32_t nAccountIndex, CHDAccount& hdAccountRet);
    bool SetAccount(uint32_t nAccountIndex, const CHDAccount& hdAccount);
    size_t CountAccounts();
};

/* hd pubkey data model */
class CHDPubKey
{
private:
    static const int CURRENT_VERSION = 1;
    int nVersion;

public:
    CExtPubKey extPubKey;
    uint256 hdchainID;
    uint32_t nAccountIndex;
    uint32_t nChangeIndex;

    CHDPubKey() : nVersion(CHDPubKey::CURRENT_VERSION), nAccountIndex(0), nChangeIndex(0) {}

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(this->nVersion);
        nVersion = this->nVersion;
        READWRITE(extPubKey);
        READWRITE(hdchainID);
        READWRITE(nAccountIndex);
        READWRITE(nChangeIndex);
    }

    std::string GetKeyPath() const;
};

#endif // LUX_HDCHAIN_H
