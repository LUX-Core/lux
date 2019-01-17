#ifndef LUX_STORAGEORDER_H
#define LUX_STORAGEORDER_H

#include "uint256.h"
#include "amount.h"
#include "hash.h"
#include "utilstrencodings.h"
#include "serialize.h"
#include "protocol.h"

class StorageOrder
{
public:
    std::time_t time;           // current timestamp
    std::time_t storageUntil;   // timestamp
    std::string filename;
    uint64_t fileSize;
    uint256 fileURI;
    CAmount maxRate;            // Lux * COIN / (sec * byte)
    size_t maxGap;              // max number of blocks, which can be mined between proofs
    CService address;           // [!!!] global-wide sender address

    uint256 GetHash() const{
        return Hash(BEGIN(time), END(maxGap));
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

class StorageProposal
{
public:
    std::time_t time;       // current timestamp
    uint256 orderHash;
    CAmount rate;           // Lux * COIN / (sec * byte)
    CService address;       // [!!!] global-wide sender address

    uint256 GetHash() const{
        return Hash(BEGIN(time), END(rate));
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
    unsigned short port;

    uint256 GetHash() const{
        return Hash(BEGIN(time), END(port));
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
