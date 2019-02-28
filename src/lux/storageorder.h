#ifndef LUX_STORAGEORDER_H
#define LUX_STORAGEORDER_H

#include "amount.h"
#include "decryptionkeys.h"
#include "hash.h"
#include "protocol.h"
#include "serialize.h"
#include "streams.h"
#include "uint256.h"
#include "utilstrencodings.h"

class StorageOrder
{
public:
    std::time_t time;           // current timestamp
    std::time_t storageUntil;   // timestamp
    std::string filename;
    uint64_t fileSize;
    uint256 fileURI;
    CAmount maxRate;            // Lux * COIN / (sec * byte)
    int maxGap;                 // max number of blocks, which can be mined between proofs
    CService address;           // [!!!] global-wide sender address

    uint256 GetHash() const{
        CDataStream ss(SER_GETHASH, 0);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(time);
        READWRITE(storageUntil);
        READWRITE(filename);
        READWRITE(fileSize);
        READWRITE(fileURI);
        READWRITE(maxRate);
        READWRITE(maxGap);
        READWRITE(address);
    }
};

class StorageOrderDB
{
public:
    std::time_t storageUntil;
    std::string filename;
    uint64_t fileSize;
    uint256 fileURI;
    uint256 merkleRootHash;
    CAmount maxRate;
    int maxGap;
    DecryptionKeys keys;

    uint256 GetHash() const{
        CDataStream ss(SER_GETHASH, 0);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(storageUntil);
        READWRITE(filename);
        READWRITE(fileSize);
        READWRITE(fileURI);
        READWRITE(merkleRootHash);
        READWRITE(maxRate);
        READWRITE(maxGap);
        READWRITE(keys);
    }
};

class StorageProposal
{
public:
    std::time_t time;       // current timestamp
    uint256 orderHash;
    CAmount rate;           // Lux * COIN / (sec * byte)
    CService address;       // [!!!] global-wide sender address

    uint256 GetHash() const{
        CDataStream ss(SER_GETHASH, 0);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(time);
        READWRITE(orderHash);
        READWRITE(rate);
        READWRITE(address);
    }
};

class StorageHandshake
{
public:
    std::time_t time;
    uint256 orderHash;
    uint256 proposalHash;
    unsigned short port; // TODO: does not used (SS)

    uint256 GetHash() const{
        CDataStream ss(SER_GETHASH, 0);
        ss << *this;
        return Hash(ss.begin(), ss.end());
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(time);
        READWRITE(orderHash);
        READWRITE(proposalHash);
        READWRITE(port);
    }
};

#endif //LUX_STORAGEORDER_H
