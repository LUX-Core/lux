#ifndef LUX_LIB_DECRYPTIONKEYS_H
#define LUX_LIB_DECRYPTIONKEYS_H

#include "hash.h"
#include "utilstrencodings.h"
#include "serialize.h"
#include "streams.h"

#include <openssl/rsa.h>
#include <openssl/pem.h>

typedef unsigned char byte;
typedef std::vector<byte> AESKey;
typedef std::vector<byte> RSAKey;

static constexpr size_t nBlockSizeRSA = 128;
static constexpr size_t nBlockSizeAES = 16;

namespace {
#if OPENSSL_VERSION_NUMBER < 0x10100005L
    static void RSA_get0_key(const RSA *r, const BIGNUM **n, const BIGNUM **e, const BIGNUM **d)
    {
        if(n)
            *n = r->n;

        if(e)
            *e = r->e;

        if(d)
            *d = r->d;
    }
#endif
}

struct DecryptionKeys
{
    using bytes = std::vector<unsigned char>;
    RSAKey rsaKey;
    AESKey aesKey;

    static std::string ToString(bytes data)
    {
        if (data.size() == 0) {
            return "";
        }
        return std::string(data.begin(), data.end());
    }

    static bytes ToBytes(std::string data)
    {
        if (data.empty())
        {
            return {};
        }
        return bytes(data.begin(), data.end());
    }

    static BIGNUM *GetMinModulus()
    {
        BIGNUM *minModulus = BN_new();
        std::string minModulusHex;
        minModulusHex.reserve(nBlockSizeRSA * 2);
        minModulusHex += "0000";
        for (size_t i = 0; i < (nBlockSizeRSA - 2); i++)
            minModulusHex += "ff";

        BN_hex2bn(&minModulus, minModulusHex.c_str());
        return minModulus;
    }

    static RSA* CreatePublicRSA(const std::string &key)
    {
        RSA *rsa = NULL;
        BIO *keybio;
        const char* c_string = key.c_str();
        keybio = BIO_new_mem_buf((void*)c_string, -1);
        if (keybio==NULL) {
            return 0;
        }
        rsa = PEM_read_bio_RSAPublicKey(keybio, &rsa, 0, NULL);
        return rsa;
    }

    static DecryptionKeys GenerateKeys(RSA **rsa)
    {
        const BIGNUM *rsa_n;
        // search for rsa->n > 0x0000ff...126 bytes...ff
        {
            *rsa = RSA_generate_key(nBlockSizeRSA * 8, 3, nullptr, nullptr);
            BIGNUM *minModulus = GetMinModulus();
            RSA_get0_key(*rsa, &rsa_n, nullptr, nullptr);
            while (BN_ucmp(minModulus, rsa_n) >= 0) {
                RSA_free(*rsa);
                *rsa = RSA_generate_key(nBlockSizeRSA * 8, 3, nullptr, nullptr);
            }
            BN_free(minModulus);
        }
        const char chAESKey[] = "1234567890123456"; // TODO: generate unique 16 bytes (SS)
        const AESKey aesKey(chAESKey, chAESKey + sizeof(chAESKey)/sizeof(*chAESKey));

        BIO *pub = BIO_new(BIO_s_mem());
        PEM_write_bio_RSAPublicKey(pub, *rsa);
        size_t publicKeyLength = BIO_pending(pub);
        char *rsaPubKey = new char[publicKeyLength + 1];
        BIO_read(pub, rsaPubKey, publicKeyLength);
        rsaPubKey[publicKeyLength] = '\0';

        DecryptionKeys decryptionKeys = {DecryptionKeys::ToBytes(std::string(rsaPubKey)), aesKey};
        return decryptionKeys;
    }

    uint256 GetHash() const{
        CDataStream ss(SER_GETHASH, 0);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(rsaKey);
        READWRITE(aesKey);
    }
};

#endif //LUX_LIB_DECRYPTIONKEYS_H
