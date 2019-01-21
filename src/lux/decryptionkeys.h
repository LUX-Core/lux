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

struct DecryptionKeys
{
    using bytes = std::vector<unsigned char>;
    RSAKey rsaKey;
    AESKey aesKey;

    static std::string ToString(bytes data)
    {
        return std::string(data.begin(), data.end());
    }

    static bytes ToBytes(std::string data)
    {
        return bytes(data.begin(), data.end());
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
