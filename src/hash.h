// Copyright (c) 2009-2010 Satoshi Nakamoto                     -*- c++ -*-
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HASH_H
#define BITCOIN_HASH_H

#include "crypto/ripemd160.h"
#include "crypto/sha256.h"
#include "serialize.h"
#include "uint256.h"
#include "version.h"

// Initilised PHI
#include "crypto/sph_skein.h"
#include "crypto/sph_jh.h"
#include "crypto/sph_cubehash.h"
#include "crypto/sph_fugue.h"
#include "crypto/sph_gost.h"
#include "crypto/sph_echo.h"
#include "crypto/lyra2/Lyra2.h"

#include "crypto/common.h"

#include <iomanip>
#include <openssl/sha.h>
#include <sstream>
#include <vector>

#define PHI2_ALGO_ENABLE

using namespace std;

/** A hasher class for Bitcoin's 256-bit hash (double SHA-256). */
class CHash256
{
private:
    CSHA256 sha;

public:
    static const size_t OUTPUT_SIZE = CSHA256::OUTPUT_SIZE;

    void Finalize(unsigned char hash[OUTPUT_SIZE])
    {
        unsigned char buf[sha.OUTPUT_SIZE];
        sha.Finalize(buf);
        sha.Reset().Write(buf, sha.OUTPUT_SIZE).Finalize(hash);
    }

    CHash256& Write(const unsigned char* data, size_t len)
    {
        sha.Write(data, len);
        return *this;
    }

    CHash256& Reset()
    {
        sha.Reset();
        return *this;
    }
};

#ifdef GLOBALDEFINED
#define GLOBAL
#else
#define GLOBAL extern
#endif

GLOBAL sph_skein512_context     z_skein;
GLOBAL sph_jh512_context        z_jh;
GLOBAL sph_cubehash512_context  z_cubehash;
GLOBAL sph_fugue512_context     z_fugue;
GLOBAL sph_gost512_context      z_gost;
GLOBAL sph_echo512_context      z_echo;

#define fillz() do { \
    sph_skein512_init(&z_skein); \
    sph_jh512_init(&z_jh); \
    sph_cubehash512_init(&z_cubehash); \
    sph_fugue512_init(&z_fugue); \
    sph_gost512_init(&z_gost); \
    sph_echo512_init(&z_echo); \
} while (0)

#define ZSKEIN (memcpy(&ctx_skein, &z_skein, sizeof(z_skein)))
#define ZJH (memcpy(&ctx_jh, &z_jh, sizeof(z_jh)))
#define ZFUGUE (memcpy(&ctx_fugue, &z_fugue, sizeof(z_fugue)))
#define ZGOST (memcpy(&ctx_gost, &z_gost, sizeof(z_gost)))

/* ----------- Bitcoin Hash ------------------------------------------------- */
/** A hasher class for Bitcoin's 160-bit hash (SHA-256 + RIPEMD-160). */
class CHash160
{
private:
    CSHA256 sha;

public:
    static const size_t OUTPUT_SIZE = CRIPEMD160::OUTPUT_SIZE;

    void Finalize(unsigned char hash[OUTPUT_SIZE])
    {
        unsigned char buf[sha.OUTPUT_SIZE];
        sha.Finalize(buf);
        CRIPEMD160().Write(buf, sha.OUTPUT_SIZE).Finalize(hash);
    }

    CHash160& Write(const unsigned char* data, size_t len)
    {
        sha.Write(data, len);
        return *this;
    }

    CHash160& Reset()
    {
        sha.Reset();
        return *this;
    }
};

/** Compute the 256-bit hash of a std::string */
inline std::string Hash(std::string input)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, input.c_str(), input.size());
    SHA256_Final(hash, &sha256);
    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

/** Compute the 256-bit hash of a void pointer */
inline void Hash(void* in, unsigned int len, unsigned char* out)
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, in, len);
    SHA256_Final(out, &sha256);
}

/** Compute the 256-bit hash of an object. */
template <typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0])).Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 256-bit hash of the concatenation of two objects. */
template <typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end, const T2 p2begin, const T2 p2end)
{
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0])).Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0])).Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 256-bit hash of the concatenation of three objects. */
template <typename T1, typename T2, typename T3>
inline uint256 Hash(const T1 p1begin, const T1 p1end, const T2 p2begin, const T2 p2end, const T3 p3begin, const T3 p3end)
{
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0])).Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0])).Write(p3begin == p3end ? pblank : (const unsigned char*)&p3begin[0], (p3end - p3begin) * sizeof(p3begin[0])).Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 256-bit hash of the concatenation of three objects. */
template <typename T1, typename T2, typename T3, typename T4>
inline uint256 Hash(const T1 p1begin, const T1 p1end, const T2 p2begin, const T2 p2end, const T3 p3begin, const T3 p3end, const T4 p4begin, const T4 p4end)
{
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0])).Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0])).Write(p3begin == p3end ? pblank : (const unsigned char*)&p3begin[0], (p3end - p3begin) * sizeof(p3begin[0])).Write(p4begin == p4end ? pblank : (const unsigned char*)&p4begin[0], (p4end - p4begin) * sizeof(p4begin[0])).Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 256-bit hash of the concatenation of three objects. */
template <typename T1, typename T2, typename T3, typename T4, typename T5>
inline uint256 Hash(const T1 p1begin, const T1 p1end, const T2 p2begin, const T2 p2end, const T3 p3begin, const T3 p3end, const T4 p4begin, const T4 p4end, const T5 p5begin, const T5 p5end)
{
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0])).Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0])).Write(p3begin == p3end ? pblank : (const unsigned char*)&p3begin[0], (p3end - p3begin) * sizeof(p3begin[0])).Write(p4begin == p4end ? pblank : (const unsigned char*)&p4begin[0], (p4end - p4begin) * sizeof(p4begin[0])).Write(p5begin == p5end ? pblank : (const unsigned char*)&p5begin[0], (p5end - p5begin) * sizeof(p5begin[0])).Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 256-bit hash of the concatenation of three objects. */
template <typename T1, typename T2, typename T3, typename T4, typename T5, typename T6>
inline uint256 Hash(const T1 p1begin, const T1 p1end, const T2 p2begin, const T2 p2end, const T3 p3begin, const T3 p3end, const T4 p4begin, const T4 p4end, const T5 p5begin, const T5 p5end, const T6 p6begin, const T6 p6end)
{
    static const unsigned char pblank[1] = {};
    uint256 result;
    CHash256().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0])).Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0])).Write(p3begin == p3end ? pblank : (const unsigned char*)&p3begin[0], (p3end - p3begin) * sizeof(p3begin[0])).Write(p4begin == p4end ? pblank : (const unsigned char*)&p4begin[0], (p4end - p4begin) * sizeof(p4begin[0])).Write(p5begin == p5end ? pblank : (const unsigned char*)&p5begin[0], (p5end - p5begin) * sizeof(p5begin[0])).Write(p6begin == p6end ? pblank : (const unsigned char*)&p6begin[0], (p6end - p6begin) * sizeof(p6begin[0])).Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 160-bit hash an object. */
template <typename T1>
inline uint160 Hash160(const T1 pbegin, const T1 pend)
{
    static unsigned char pblank[1] = {};
    uint160 result;
    CHash160().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0])).Finalize((unsigned char*)&result);
    return result;
}

/** Compute the 160-bit hash of a vector. */
inline uint160 Hash160(const std::vector<unsigned char>& vch)
{
    return Hash160(vch.begin(), vch.end());
}

/** A writer stream (for serialization) that computes a 256-bit hash. */
class CHashWriter
{
private:
    CHash256 ctx;

public:
    int nType;
    int nVersion;

    CHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn) {}

    int GetType() const { return nType; }
    int GetVersion() const { return nVersion; }

    CHashWriter& write(const char* pch, size_t size)
    {
        ctx.Write((const unsigned char*)pch, size);
        return (*this);
    }

    // invalidates the object
    uint256 GetHash()
    {
        uint256 result;
        ctx.Finalize((unsigned char*)&result);
        return result;
    }

    template <typename T>
    CHashWriter& operator<<(const T& obj)
    {
        // Serialize to this stream
        ::Serialize(*this, obj, nType, nVersion);
        return (*this);
    }
};

/** Compute the 256-bit hash of an object's serialization. */
template <typename T>
uint256 SerializeHash(const T& obj, int nType = SER_GETHASH, int nVersion = PROTOCOL_VERSION)
{
    CHashWriter ss(nType, nVersion);
    ss << obj;
    return ss.GetHash();
}

unsigned int MurmurHash3(unsigned int nHashSeed, const std::vector<unsigned char>& vDataToHash);

void BIP32Hash(const unsigned char chainCode[32], unsigned int nChild, unsigned char header, const unsigned char data[32], unsigned char output[64]);

//int HMAC_SHA512_Init(HMAC_SHA512_CTX *pctx, const void *pkey, size_t len);
//int HMAC_SHA512_Update(HMAC_SHA512_CTX *pctx, const void *pdata, size_t len);
//int HMAC_SHA512_Final(unsigned char *pmd, HMAC_SHA512_CTX *pctx);

/* ----------- Phi1612 Hash ------------------------------------------------ */

template<typename T1>
inline uint256 phi2_hash(const T1 pbegin, const T1 pend)
{
    unsigned char hash[128] = { 0 };
    unsigned char hashA[64] = { 0 };
    unsigned char hashB[64] = { 0 };
    static unsigned char pblank[1];
    uint512 output;

    sph_cubehash512_context ctx_cubehash;
    sph_jh512_context ctx_jh;
    sph_gost512_context ctx_gost;
    sph_echo512_context ctx_echo;
    sph_skein512_context ctx_skein;

    sph_cubehash512_init(&ctx_cubehash);
    sph_cubehash512(&ctx_cubehash, (pbegin == pend ? pblank : static_cast<const void*>(&pbegin[0])), 80);
    sph_cubehash512_close(&ctx_cubehash, (void*)hashB);

    LYRA2(&hashA[ 0], 32, &hashB[ 0], 32, &hashB[ 0], 32, 1, 8, 8);
    LYRA2(&hashA[32], 32, &hashB[32], 32, &hashB[32], 32, 1, 8, 8);

    sph_jh512_init(&ctx_jh);
    sph_jh512(&ctx_jh, (const void*)hashA, 64);
    sph_jh512_close(&ctx_jh, (void*)hash);

    if (hash[0] & 1) {
        sph_gost512_init(&ctx_gost);
        sph_gost512(&ctx_gost, (const void*)hash, 64);
        sph_gost512_close(&ctx_gost, (void*)hash);
    } else {
        sph_echo512_init(&ctx_echo);
        sph_echo512(&ctx_echo, (const void*)hash, 64);
        sph_echo512_close(&ctx_echo, (void*)hash);

        sph_echo512_init(&ctx_echo);
        sph_echo512(&ctx_echo, (const void*)hash, 64);
        sph_echo512_close(&ctx_echo, (void*)hash);
    }
    sph_skein512_init(&ctx_skein);
    sph_skein512(&ctx_skein, (const void*)hash, 64);
    sph_skein512_close(&ctx_skein, (void*)hash);

    for (int i=0; i<32; i++)
        hash[i] ^= hash[i+32];

    memcpy((void *) &output, hash, 32);
    return output.trim256();
}

template<typename T1>
inline uint256 Phi1612(const T1 pbegin, const T1 pend)
{
    sph_skein512_context     ctx_skein;
    sph_jh512_context ctx_jh;
    sph_cubehash512_context   ctx_cubehash;
    sph_fugue512_context      ctx_fugue;
    sph_gost512_context      ctx_gost;
    sph_echo512_context ctx_echo;
    static unsigned char pblank[1];

    uint512 hash[17];

    sph_skein512_init(&ctx_skein);
    sph_skein512 (&ctx_skein, (pbegin == pend ? pblank : static_cast<const void*>(&pbegin[0])), (pend - pbegin) * sizeof(pbegin[0]));
    sph_skein512_close(&ctx_skein, static_cast<void*>(&hash[0]));

    sph_jh512_init(&ctx_jh);
    sph_jh512 (&ctx_jh, static_cast<const void*>(&hash[0]), 64);
    sph_jh512_close(&ctx_jh, static_cast<void*>(&hash[1]));

    sph_cubehash512_init(&ctx_cubehash);
    sph_cubehash512 (&ctx_cubehash, static_cast<const void*>(&hash[1]), 64);
    sph_cubehash512_close(&ctx_cubehash, static_cast<void*>(&hash[2]));

    sph_fugue512_init(&ctx_fugue);
    sph_fugue512 (&ctx_fugue, static_cast<const void*>(&hash[2]), 64);
    sph_fugue512_close(&ctx_fugue, static_cast<void*>(&hash[3]));

    sph_gost512_init(&ctx_gost);
    sph_gost512 (&ctx_gost, static_cast<const void*>(&hash[3]), 64);
    sph_gost512_close(&ctx_gost, static_cast<void*>(&hash[4]));

    sph_echo512_init(&ctx_echo);
    sph_echo512 (&ctx_echo, static_cast<const void*>(&hash[4]), 64);
    sph_echo512_close(&ctx_echo, static_cast<void*>(&hash[5]));

    return hash[5].trim256();
}


void scrypt_hash(const char* pass, unsigned int pLen, const char* salt, unsigned int sLen, char* output, unsigned int N, unsigned int r, unsigned int p, unsigned int dkLen);

/** Optimized SipHash-2-4 implementation for uint256.
 *
 *  It is identical to:
 *    SipHasher(k0, k1)
 *      .Write(val.GetUint64(0))
 *      .Write(val.GetUint64(1))
 *      .Write(val.GetUint64(2))
 *      .Write(val.GetUint64(3))
 *      .Finalize()
 */
uint64_t SipHashUint256(uint64_t k0, uint64_t k1, const uint256& val);
uint64_t SipHashUint256Extra(uint64_t k0, uint64_t k1, const uint256& val, uint32_t extra);

#endif // BITCOIN_HASH_H
