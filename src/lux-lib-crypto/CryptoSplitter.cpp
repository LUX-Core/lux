#include "CryptoSplitter.h"

#include <cstring>
#include <cmath>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/rsa.h>

void handleErrors()
{
    ERR_print_errors_fp(stderr);
    abort();
}

int encrypt(const unsigned char *plaintext, int plaintext_len, AESKey key,
            const unsigned char *iv, unsigned char *ciphertext)
{
    EVP_CIPHER_CTX *ctx;

    int len;

    int ciphertext_len;

    /* Create and initialise the context */
    if(!(ctx = EVP_CIPHER_CTX_new())) handleErrors();

    /* Initialise the encryption operation. IMPORTANT - ensure you use a key
     * and IV size appropriate for your cipher
     * In this example we are using 256 bit AES (i.e. a 256 bit key). The
     * IV size for *most* modes is the same as the block size. For AES this
     * is 128 bits */
    if(1 != EVP_EncryptInit_ex(ctx, EVP_aes_128_ecb(), NULL, key.data(), iv))
        handleErrors();

    /* Provide the message to be encrypted, and obtain the encrypted output.
     * EVP_EncryptUpdate can be called multiple times if necessary
     */
    if(1 != EVP_EncryptUpdate(ctx, ciphertext, &len, plaintext, plaintext_len))
        handleErrors();
    ciphertext_len = len;

    /* Finalise the encryption. Further ciphertext bytes may be written at
     * this stage.
     */
    if(1 != EVP_EncryptFinal_ex(ctx, ciphertext + len, &len)) handleErrors();
    ciphertext_len += len;

    /* Clean up */
    EVP_CIPHER_CTX_free(ctx);

    return ciphertext_len;
}

BIGNUM *GetMinModulus()
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

uint64_t GetCryptoReplicaSize(size_t srcSize, size_t blockSizeRSA){
    return (uint64_t)ceil((double)srcSize / (blockSizeRSA - 2)) * blockSizeRSA;
}

void BuildSalt(uint64_t offset, const AESKey &aesKey, byte *salt)
{
    uint64_t clearSalt[nBlockSizeRSA / sizeof(uint64_t)];
    memset(clearSalt, 0, nBlockSizeRSA * sizeof(char));
    for (uint64_t i = 0; i < nBlockSizeRSA / nBlockSizeAES; ++i) {
        // if aes.blockSize is 16, then salt is [0, offset, 0, offset + 1, 0, offset + 2...]
        const size_t step = nBlockSizeAES / sizeof(uint64_t);
        clearSalt[(i + 1) * step - 1] = offset + i;
    }
    // encrypt salt
    if (encrypt((byte*) clearSalt, nBlockSizeRSA, aesKey, NULL, salt) != (nBlockSizeRSA + 16)) {
        handleErrors();
    }
}

void EncryptData(const byte *src, uint64_t offset, size_t srcSize, byte *cipherText,
                 const AESKey &aesKey, RSA *rsa)
{
    byte salt[nBlockSizeRSA + 16];
    byte temp[nBlockSizeRSA];
    for (size_t numChunk = 0, srcIndex = 0; srcIndex < srcSize; ++numChunk){
        BuildSalt(offset + numChunk * nBlockSizeRSA / nBlockSizeAES, aesKey, salt);
        memset(temp, 0, nBlockSizeRSA * sizeof(char));
        for (size_t i = 2; (i < nBlockSizeRSA) && (srcIndex < srcSize); ++i, ++srcIndex){
            temp[i] =  src[srcIndex] ^ salt[i];
        }
        if(RSA_private_encrypt(nBlockSizeRSA, temp, cipherText + numChunk * nBlockSizeRSA, rsa, RSA_NO_PADDING) == -1) {
            ERR_load_crypto_strings();
            handleErrors();
        }
    }
}

void DecryptData(const byte *src, uint64_t offset, size_t srcSize, byte *plainText,
                 const AESKey &aesKey, RSA *rsa)
{
    byte salt[nBlockSizeRSA + 16];
    byte temp[nBlockSizeRSA];
    for (size_t numChunk = 0, srcIndex = 0; srcIndex < srcSize; ++numChunk){
        if(RSA_public_decrypt(nBlockSizeRSA, src + numChunk * nBlockSizeRSA, temp, rsa, RSA_NO_PADDING) == -1) {
            ERR_load_crypto_strings();
            handleErrors();
        }
        BuildSalt(offset + numChunk * nBlockSizeRSA / nBlockSizeAES, aesKey, salt);
        for (size_t i = 2; (i < nBlockSizeRSA) && (srcIndex < srcSize); ++i, ++srcIndex){
            plainText[srcIndex] =  temp[i] ^ salt[i];
        }
    }
}

