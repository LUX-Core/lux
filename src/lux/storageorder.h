#ifndef PROJECT_STORAGEORDER_H
#define PROJECT_STORAGEORDER_H

#include "uint256.h"
#include "amount.h"
#include "hash.h"
#include "utilstrencodings.h"
#include "serialize.h"

class StorageOrder
{
public:
    size_t time;            // current timestamp
    size_t storageUntil;    // timestamp
    uint64_t fileSize;
    uint256 fileURI;
    CAmount maxRate;        // Lux * COIN / (sec * byte)
    uint maxGap;            // max number of blocks, which can be mined between proofs
    CAddress address;       // [!!!] global-wide sender address

    uint256 GetHash(){
        uint256 n = Hash(BEGIN(time), END(maxGap));
        return n;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(time);
        READWRITE(storageUntil);
        READWRITE(fileSize);
        READWRITE(fileURI);
        READWRITE(maxRate);
        READWRITE(maxGap);
        READWRITE(address);
    }
};

#endif //PROJECT_STORAGEORDER_H
