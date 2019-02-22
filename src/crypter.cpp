// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypter.h"

#include "script/script.h"
#include "script/standard.h"
#include "util.h"

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <string>
#include <vector>

bool CCrypter::SetKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod)
{
    if (nRounds < 1 || chSalt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    int i = 0;
    if (nDerivationMethod == 0)
        i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), &chSalt[0],
            (unsigned char*)&strKeyData[0], strKeyData.size(), nRounds, chKey, chIV);

    if (i != (int)WALLET_CRYPTO_KEY_SIZE) {
        memory_cleanse(chKey, sizeof(chKey));
        memory_cleanse(chIV, sizeof(chIV));
        return false;
    }

    fKeySet = true;
    return true;
}

bool CCrypter::SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV)
{
    if (chNewKey.size() != WALLET_CRYPTO_KEY_SIZE || chNewIV.size() != WALLET_CRYPTO_KEY_SIZE)
        return false;

    memcpy(&chKey[0], &chNewKey[0], sizeof chKey);
    memcpy(&chIV[0], &chNewIV[0], sizeof chIV);

    fKeySet = true;
    return true;
}

bool CCrypter::Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char>& vchCiphertext)
{
    if (!fKeySet)
        return false;

    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCK_SIZE - 1 bytes
    int nLen = vchPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    vchCiphertext = std::vector<unsigned char>(nCLen);

    bool fOk = true;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (fOk) fOk = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKey, chIV) != 0;
    if (fOk) fOk = EVP_EncryptUpdate(ctx, &vchCiphertext[0], &nCLen, &vchPlaintext[0], nLen) != 0;
    if (fOk) fOk = EVP_EncryptFinal_ex(ctx, (&vchCiphertext[0]) + nCLen, &nFLen) != 0;
    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    vchCiphertext.resize(nCLen + nFLen);
    return true;
}

bool CCrypter::Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext)
{
    if (!fKeySet)
        return false;

    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = vchCiphertext.size();
    int nPLen = nLen, nFLen = 0;

    vchPlaintext = CKeyingMaterial(nPLen);

    bool fOk = true;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (fOk) fOk = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKey, chIV) != 0;
    if (fOk) fOk = EVP_DecryptUpdate(ctx, &vchPlaintext[0], &nPLen, &vchCiphertext[0], nLen) != 0;
    if (fOk) fOk = EVP_DecryptFinal_ex(ctx, (&vchPlaintext[0]) + nPLen, &nFLen) != 0;
    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    vchPlaintext.resize(nPLen + nFLen);
    return true;
}


bool EncryptSecret(const CKeyingMaterial& vMasterKey, const CKeyingMaterial& vchPlaintext, const uint256& nIV, std::vector<unsigned char>& vchCiphertext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if (!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Encrypt(*((const CKeyingMaterial*)&vchPlaintext), vchCiphertext);
}


// General secure AES 256 CBC encryption routine
bool EncryptAES256(const SecureString& sKey, const SecureString& sPlaintext, const std::string& sIV, std::string& sCiphertext)
{
    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCK_SIZE - 1 bytes
    int nLen = sPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE;
    int nFLen = 0;

    // Verify key sizes
    if (sKey.size() != 32 || sIV.size() != AES_BLOCK_SIZE) {
        LogPrintf("crypter EncryptAES256 - Invalid key or block size: Key: %d sIV:%d\n", sKey.size(), sIV.size());
        return false;
    }

    // Prepare output buffer
    sCiphertext.resize(nCLen);

    // Perform the encryption
    EVP_CIPHER_CTX* ctx;

    bool fOk = true;

    ctx = EVP_CIPHER_CTX_new();
    if (fOk) fOk = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*)&sKey[0], (const unsigned char*)&sIV[0]);
    if (fOk) fOk = EVP_EncryptUpdate(ctx, (unsigned char*)&sCiphertext[0], &nCLen, (const unsigned char*)&sPlaintext[0], nLen);
    if (fOk) fOk = EVP_EncryptFinal_ex(ctx, (unsigned char*)(&sCiphertext[0]) + nCLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    sCiphertext.resize(nCLen + nFLen);
    return true;
}


bool DecryptSecret(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCiphertext, const uint256& nIV, CKeyingMaterial& vchPlaintext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if (!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Decrypt(vchCiphertext, *((CKeyingMaterial*)&vchPlaintext));
}

bool DecryptAES256(const SecureString& sKey, const std::string& sCiphertext, const std::string& sIV, SecureString& sPlaintext)
{
    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = sCiphertext.size();
    int nPLen = nLen, nFLen = 0;

    // Verify key sizes
    if (sKey.size() != 32 || sIV.size() != AES_BLOCK_SIZE) {
        LogPrintf("crypter DecryptAES256 - Invalid key or block size\n");
        return false;
    }

    sPlaintext.resize(nPLen);

    EVP_CIPHER_CTX* ctx;

    bool fOk = true;

    ctx = EVP_CIPHER_CTX_new();
    if (fOk) fOk = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*)&sKey[0], (const unsigned char*)&sIV[0]);
    if (fOk) fOk = EVP_DecryptUpdate(ctx, (unsigned char*)&sPlaintext[0], &nPLen, (const unsigned char*)&sCiphertext[0], nLen);
    if (fOk) fOk = EVP_DecryptFinal_ex(ctx, (unsigned char*)(&sPlaintext[0]) + nPLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    sPlaintext.resize(nPLen + nFLen);
    return true;
}

bool CCryptoKeyStore::SetCrypted()
{
    LOCK(cs_KeyStore);
    if (fUseCrypto)
        return true;
    if (!mapKeys.empty())
        return false;
    fUseCrypto = true;
    return true;
}

bool CCryptoKeyStore::IsLocked() const
{
    if (!IsCrypted()) {
        return false;
    }
    LOCK(cs_KeyStore);
    return vMasterKey.empty();
}

bool CCryptoKeyStore::Lock()
{
    if (!SetCrypted())
        return false;

    {
        LOCK(cs_KeyStore);
        vMasterKey.clear();
    }

    NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::Unlock(const CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        bool keyPass = false;
        bool keyFail = false;
        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        for (; mi != mapCryptedKeys.end(); ++mi) {
            const CPubKey& vchPubKey = (*mi).second.first;
            const std::vector<unsigned char>& vchCryptedSecret = (*mi).second.second;
            CKeyingMaterial vchSecret;
            if (!DecryptSecret(vMasterKeyIn, vchCryptedSecret, vchPubKey.GetHash(), vchSecret)) {
                keyFail = true;
                break;
            }
            if (vchSecret.size() != 32) {
                keyFail = true;
                break;
            }
            CKey key;
            key.Set(vchSecret.begin(), vchSecret.end(), vchPubKey.IsCompressed());
            if (key.GetPubKey() != vchPubKey) {
                keyFail = true;
                break;
            }
            keyPass = true;
            if (fDecryptionThoroughlyChecked)
                break;
        }
        if (keyPass && keyFail) {
            LogPrintf("The wallet is probably corrupted: Some keys decrypt but not all.");
            //assert(false);
        }
        if (keyFail || (!keyPass && cryptedHDChain.IsNull()))
            return false;
        vMasterKey = vMasterKeyIn;
        if(!cryptedHDChain.IsNull()) {
            bool chainPass = false;
            // try to decrypt seed and make sure it matches
            CHDChain hdChainTmp;
            if (DecryptHDChain(hdChainTmp)) {
                // make sure seed matches this chain
                chainPass = cryptedHDChain.GetID() == hdChainTmp.GetSeedHash();
            }
            if (!chainPass) {
                vMasterKey.clear();
                return false;
            }
        }
        fDecryptionThoroughlyChecked = true;
    }
    NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::AddKeyPubKey(const CKey& key, const CPubKey& pubkey)
{
    LOCK(cs_KeyStore);
    if (!IsCrypted())
        return CBasicKeyStore::AddKeyPubKey(key, pubkey);

    if (IsLocked())
        return false;

    std::vector<unsigned char> vchCryptedSecret;
    CKeyingMaterial vchSecret(key.begin(), key.end());
    if (!EncryptSecret(vMasterKey, vchSecret, pubkey.GetHash(), vchCryptedSecret))
        return false;

    if (!AddCryptedKey(pubkey, vchCryptedSecret))
        return false;
    return true;
}


bool CCryptoKeyStore::AddCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret)
{
    LOCK(cs_KeyStore);
    if (!SetCrypted())
        return false;

    mapCryptedKeys[vchPubKey.GetID()] = make_pair(vchPubKey, vchCryptedSecret);
    ImplicitlyLearnRelatedKeyScripts(vchPubKey);
    return true;
}

bool CCryptoKeyStore::HaveKey(const CKeyID& address) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted()) {
        return CBasicKeyStore::HaveKey(address);
    }
    return mapCryptedKeys.count(address) > 0;
}

bool CCryptoKeyStore::GetKey(const CKeyID& address, CKey& keyOut) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted())
        return CBasicKeyStore::GetKey(address, keyOut);

    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end()) {
        const CPubKey& vchPubKey = (*mi).second.first;
        const std::vector<unsigned char>& vchCryptedSecret = (*mi).second.second;
        CKeyingMaterial vchSecret;
        if (!DecryptSecret(vMasterKey, vchCryptedSecret, vchPubKey.GetHash(), vchSecret))
            return false;
        if (vchSecret.size() != 32)
            return false;
        keyOut.Set(vchSecret.begin(), vchSecret.end(), vchPubKey.IsCompressed());
        return true;
    }
    return false;
}

bool CCryptoKeyStore::GetPubKey(const CKeyID& address, CPubKey& vchPubKeyOut) const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted())
        return CBasicKeyStore::GetPubKey(address, vchPubKeyOut);

    CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
    if (mi != mapCryptedKeys.end()) {
        vchPubKeyOut = (*mi).second.first;
        return true;
    }
    // Check for watch-only pubkeys
    return CBasicKeyStore::GetPubKey(address, vchPubKeyOut);
}
#if 0
std::set<CKeyID> CCryptoKeyStore::GetKeys() const
{
    LOCK(cs_KeyStore);
    if (!IsCrypted()) {
        return CBasicKeyStore::GetKeys();
    }
    std::set<CKeyID> set_address;
    for (const auto& mi : mapCryptedKeys) {
        set_address.insert(mi.first);
    }
    return set_address;
}
#endif
bool CCryptoKeyStore::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    LOCK(cs_KeyStore);
    if (!mapCryptedKeys.empty() || IsCrypted())
        return false;

    fUseCrypto = true;
    for (KeyMap::value_type& mKey : mapKeys) {
        const CKey& key = mKey.second;
        CPubKey vchPubKey = key.GetPubKey();
        CKeyingMaterial vchSecret(key.begin(), key.end());
        std::vector<unsigned char> vchCryptedSecret;
        if (!EncryptSecret(vMasterKeyIn, vchSecret, vchPubKey.GetHash(), vchCryptedSecret))
            return false;
        if (!AddCryptedKey(vchPubKey, vchCryptedSecret))
            return false;
    }
    mapKeys.clear();
    return true;
}
bool CCryptoKeyStore::EncryptHDChain(const CKeyingMaterial& vMasterKeyIn)
{
    // should call EncryptKeys first
    if (!IsCrypted())
        return false;

    if (!cryptedHDChain.IsNull())
        return true;

    if (cryptedHDChain.IsCrypted())
        return true;

    // make sure seed matches this chain
    if (hdChain.GetID() != hdChain.GetSeedHash())
        return false;

    std::vector<unsigned char> vchCryptedSeed;
    if (!EncryptSecret(vMasterKeyIn, hdChain.GetSeed(), hdChain.GetID(), vchCryptedSeed))
        return false;

    hdChain.Debug(__func__);
    cryptedHDChain = hdChain;
    cryptedHDChain.SetCrypted(true);

    SecureVector vchSecureCryptedSeed(vchCryptedSeed.begin(), vchCryptedSeed.end());
    if (!cryptedHDChain.SetSeed(vchSecureCryptedSeed, false))
        return false;

    SecureVector vchMnemonic;
    SecureVector vchMnemonicPassphrase;

    // it's ok to have no mnemonic if wallet was initialized via hdseed
    if (hdChain.GetMnemonic(vchMnemonic, vchMnemonicPassphrase)) {
        std::vector<unsigned char> vchCryptedMnemonic;
        std::vector<unsigned char> vchCryptedMnemonicPassphrase;

        if (!vchMnemonic.empty() && !EncryptSecret(vMasterKeyIn, vchMnemonic, hdChain.GetID(), vchCryptedMnemonic))
            return false;
        if (!vchMnemonicPassphrase.empty() && !EncryptSecret(vMasterKeyIn, vchMnemonicPassphrase, hdChain.GetID(), vchCryptedMnemonicPassphrase))
            return false;

        SecureVector vchSecureCryptedMnemonic(vchCryptedMnemonic.begin(), vchCryptedMnemonic.end());
        SecureVector vchSecureCryptedMnemonicPassphrase(vchCryptedMnemonicPassphrase.begin(), vchCryptedMnemonicPassphrase.end());
        if (!cryptedHDChain.SetMnemonic(vchSecureCryptedMnemonic, vchSecureCryptedMnemonicPassphrase, false))
            return false;
    }

    if (!hdChain.SetNull())
        return false;

    return true;
}

bool CCryptoKeyStore::DecryptHDChain(CHDChain& hdChainRet) const
{
    if (!IsCrypted())
        return true;

    if (cryptedHDChain.IsNull())
        return false;

    if (!cryptedHDChain.IsCrypted())
        return false;

    SecureVector vchSecureSeed;
    SecureVector vchSecureCryptedSeed = cryptedHDChain.GetSeed();
    std::vector<unsigned char> vchCryptedSeed(vchSecureCryptedSeed.begin(), vchSecureCryptedSeed.end());
    if (!DecryptSecret(vMasterKey, vchCryptedSeed, cryptedHDChain.GetID(), vchSecureSeed))
        return false;

    hdChainRet = cryptedHDChain;
    if (!hdChainRet.SetSeed(vchSecureSeed, false))
        return false;

    // hash of decrypted seed must match chain id
    if (hdChainRet.GetSeedHash() != cryptedHDChain.GetID())
        return false;

    SecureVector vchSecureCryptedMnemonic;
    SecureVector vchSecureCryptedMnemonicPassphrase;

    // it's ok to have no mnemonic if wallet was initialized via hdseed
    if (cryptedHDChain.GetMnemonic(vchSecureCryptedMnemonic, vchSecureCryptedMnemonicPassphrase)) {
        SecureVector vchSecureMnemonic;
        SecureVector vchSecureMnemonicPassphrase;

        std::vector<unsigned char> vchCryptedMnemonic(vchSecureCryptedMnemonic.begin(), vchSecureCryptedMnemonic.end());
        std::vector<unsigned char> vchCryptedMnemonicPassphrase(vchSecureCryptedMnemonicPassphrase.begin(), vchSecureCryptedMnemonicPassphrase.end());

        if (!vchCryptedMnemonic.empty() && !DecryptSecret(vMasterKey, vchCryptedMnemonic, cryptedHDChain.GetID(), vchSecureMnemonic))
            return false;
        if (!vchCryptedMnemonicPassphrase.empty() && !DecryptSecret(vMasterKey, vchCryptedMnemonicPassphrase, cryptedHDChain.GetID(), vchSecureMnemonicPassphrase))
            return false;

        if (!hdChainRet.SetMnemonic(vchSecureMnemonic, vchSecureMnemonicPassphrase, false))
            return false;
    }

    hdChainRet.SetCrypted(false);
    hdChainRet.Debug(__func__);

    return true;
}

bool CCryptoKeyStore::SetHDChain(const CHDChain& chain)
{
    if (IsCrypted())
        return false;

    if (chain.IsCrypted())
        return false;

    hdChain = chain;
    return true;
}

bool CCryptoKeyStore::SetCryptedHDChain(const CHDChain& chain)
{
    if (!SetCrypted())
        return false;

    if (!chain.IsCrypted())
        return false;

    cryptedHDChain = chain;
    return true;
}

bool CCryptoKeyStore::GetHDChain(CHDChain& hdChainRet) const
{
    if(IsCrypted()) {
        hdChainRet = cryptedHDChain;
        return !cryptedHDChain.IsNull();
    }

    hdChainRet = hdChain;
    return !hdChain.IsNull();
}
