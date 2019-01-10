#include "../lux/replicabuilder.h"

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <boost/test/unit_test.hpp>
#include <iostream>

#if OPENSSL_VERSION_NUMBER < 0x10100005L
static void RSA_get0_key(const RSA *r, const BIGNUM **n, const BIGNUM **e, const BIGNUM **d)
{
    if(n != NULL)
        *n = r->n;

    if(e != NULL)
        *e = r->e;

    if(d != NULL)
        *d = r->d;
}
#endif

BOOST_AUTO_TEST_SUITE(crypto_splitter_tests)

BOOST_AUTO_TEST_CASE(get_crypto_replica_size)
{
    const int srcSize = 128;

    uint64_t replicaSize = GetCryptoReplicaSize(srcSize);

    BOOST_CHECK_EQUAL(replicaSize, 256);
}

BOOST_AUTO_TEST_CASE(encrypt_1bytes)
{
    RSA *rsa;
    const BIGNUM *rsa_n;

    // search for rsa->n > 0x0000ff...126 bytes...ff
    {
        rsa = RSA_generate_key(nBlockSizeRSA * 8, 3, nullptr, nullptr);
        BIGNUM *minModulus = GetMinModulus();
        RSA_get0_key(rsa, &rsa_n, NULL, NULL);
        while (BN_ucmp(minModulus, rsa_n) >= 0) {
            RSA_free(rsa);
            rsa = RSA_generate_key(nBlockSizeRSA * 8, 3, nullptr, nullptr);
        }
        BN_free(minModulus);
    }
    const byte src[] = "1"; // length 1
    const size_t sizeOfSrc = 1;
    const char chAESKey[] = "1234567890123456"; // length 16
    const AESKey aesKey(chAESKey, chAESKey + sizeof(chAESKey)/sizeof(*chAESKey));
    const uint64_t offset = 1;  // some int
    byte cipherText[256];       // GetShuffleReplicaSize(sizeof(src), 128) == 256
    byte plainText[1];        // sizeof(src) == 1

    EncryptData(src, offset, sizeOfSrc, cipherText, aesKey, rsa);
    DecryptData(cipherText, offset, sizeOfSrc, plainText, aesKey, rsa);

    BOOST_CHECK_EQUAL(memcmp(src, plainText, sizeOfSrc * sizeof(char)), 0);
    RSA_free(rsa);
}

BOOST_AUTO_TEST_CASE(encrypt_126bytes)
{
    RSA *rsa;
    const BIGNUM *rsa_n;
    // search for rsa->n > 0x0000ff...126 bytes...ff
    {
        rsa = RSA_generate_key(nBlockSizeRSA * 8, 3, nullptr, nullptr);
        BIGNUM *minModulus = GetMinModulus();
        RSA_get0_key(rsa, &rsa_n, nullptr, nullptr);
        while (BN_ucmp(minModulus, rsa_n) >= 0) {
            RSA_free(rsa);
            rsa = RSA_generate_key(nBlockSizeRSA * 8, 3, nullptr, nullptr);
        }
        BN_free(minModulus);
    }
    const byte src[] = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456"; // length 126
    const size_t sizeOfSrc = 126;
    const char chAESKey[] = "1234567890123456"; // length 16
    const AESKey aesKey(chAESKey, chAESKey + sizeof(chAESKey)/sizeof(*chAESKey));
    const uint64_t offset = 1;  // some int
    byte cipherText[128];       // GetShuffleReplicaSize(sizeof(src), 128) == 128
    byte plainText[126];        // sizeof(src) == 126

    EncryptData(src, offset, sizeOfSrc, cipherText, aesKey, rsa);
    DecryptData(cipherText, offset, sizeOfSrc, plainText, aesKey, rsa);

    BOOST_CHECK_EQUAL(memcmp(src, plainText, sizeOfSrc * sizeof(char)), 0);
    RSA_free(rsa);
}

BOOST_AUTO_TEST_CASE(encrypt_252bytes)
{
    RSA *rsa;
    const BIGNUM *rsa_n;
    // search for rsa->n > 0x0000ff...126 bytes...ff
    {
        rsa = RSA_generate_key(nBlockSizeRSA * 8, 3, nullptr, nullptr);
        BIGNUM *minModulus = GetMinModulus();
        RSA_get0_key(rsa, &rsa_n, nullptr, nullptr);
        while (BN_ucmp(minModulus, rsa_n) >= 0) {
            RSA_free(rsa);
            rsa = RSA_generate_key(nBlockSizeRSA * 8, 3, nullptr, nullptr);
        }
        BN_free(minModulus);
    }
    const byte src[] = "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012"; // length 252
    const size_t sizeOfSrc = 252;
    const char chAESKey[] = "1234567890123456"; // length 16
    const AESKey aesKey(chAESKey, chAESKey + sizeof(chAESKey)/sizeof(*chAESKey));
    const uint64_t offset = 1;  // some int
    byte cipherText[256];       // GetShuffleReplicaSize(sizeof(src), 128) == 256
    byte plainText[252];        // sizeof(src) == 252

    EncryptData(src, offset, sizeOfSrc, cipherText, aesKey, rsa);
    DecryptData(cipherText, offset, sizeOfSrc, plainText, aesKey, rsa);

    BOOST_CHECK_EQUAL(memcmp(src, plainText, sizeOfSrc * sizeof(char)), 0);
    RSA_free(rsa);
}

BOOST_AUTO_TEST_SUITE_END()
