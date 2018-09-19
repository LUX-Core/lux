// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SPENTINDEX_H
#define BITCOIN_SPENTINDEX_H

#include <crypto/common.h>

#include <uint256.h>
#include <amount.h>
#include <script/script.h>

/*
 * Lowest-level serialization and conversion.
 */
template<typename Stream> static inline uint8_t ser_readdata8(Stream& is) {
    uint8_t x;
    READDATA(is, x);
    return x;
}
template<typename Stream> static inline uint16_t ser_readdata16(Stream& is) {
    uint16_t x;
    READDATA(is, x);
    return x;
}
template<typename Stream> static inline uint32_t ser_readdata32(Stream& is) {
    uint32_t x;
    READDATA(is, x);
    return x;
}
template<typename Stream> static inline uint32_t ser_readdata32be(Stream& is) {
    uint32_t x;
    READDATA(is, x);
    return be32toh(x);
}

template<typename Stream> static inline void ser_writedata8(Stream &s, uint8_t obj) {
    s.write((char*)&obj, 1);
}
template<typename Stream> static inline void ser_writedata16(Stream &s, uint16_t obj) {
    s.write((char*)&obj, 2);
}
template<typename Stream> static inline void ser_writedata32(Stream &s, uint32_t obj) {
    obj = htole32(obj);
    s.write((char*)&obj, 4);
}
template<typename Stream> static inline void ser_writedata32be(Stream &s, uint32_t obj) {
    obj = htobe32(obj);
    s.write((char*)&obj, 4);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

struct CSpentIndexKey {
    uint256 txhash;
    uint32_t outputIndex;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(txhash);
        READWRITE(outputIndex);
    }

    CSpentIndexKey(uint256 txh, unsigned int i) {
        txhash = txh;
        outputIndex = i;
    }

    CSpentIndexKey() {
        SetNull();
    }

    void SetNull() {
        txhash.SetNull();
        outputIndex = 0;
    }

};

struct CSpentIndexValue {
    int32_t blockHeight;
    uint256 txhash;
    uint32_t inputIndex;
    CAmount satoshis;
    uint160 hashBytes;
    uint8_t hashType;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(blockHeight);
        READWRITE(txhash);
        READWRITE(inputIndex);
        READWRITE(satoshis);
        READWRITE(hashType);
        READWRITE(hashBytes);
    }

    CSpentIndexValue(uint256 txh, unsigned int i, int h, CAmount s, int type, uint160 addr) {
        blockHeight = (int32_t) h;
        txhash = txh;
        inputIndex = i;
        satoshis = s;
        hashType = (uint8_t) type;
        hashBytes = addr;
    }

    CSpentIndexValue() {
        SetNull();
    }

    void SetNull() {
        blockHeight = 0;
        txhash.SetNull();
        inputIndex = 0;
        satoshis = 0;
        hashType = 0;
        hashBytes.SetNull();
    }

    bool IsNull() const {
        return txhash.IsNull();
    }
};

struct CSpentIndexKeyCompare
{
    bool operator()(const CSpentIndexKey& a, const CSpentIndexKey& b) const {
        if (a.txhash == b.txhash) {
            return a.outputIndex < b.outputIndex;
        } else {
            return a.txhash < b.txhash;
        }
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

struct CAddressUnspentKey {
    uint256 txhash;
    uint160 hashBytes;
    uint16_t hashType;
    uint16_t indexout;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 56;
    }

    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata16(s, hashType);
        hashBytes.Serialize(s, nType, nVersion);
        txhash.Serialize(s, nType, nVersion);
        ser_writedata16(s, indexout);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        hashType = ser_readdata16(s);
        hashBytes.Unserialize(s, nType, nVersion);
        txhash.Unserialize(s, nType, nVersion);
        indexout = ser_readdata16(s);
    }

    CAddressUnspentKey(unsigned addrType, uint160 addrHash, uint256 txh, size_t indexValue) {
        hashType = (uint16_t) addrType;
        hashBytes = addrHash;
        txhash = txh;
        indexout = (uint16_t) indexValue;
    }

    CAddressUnspentKey() {
        SetNull();
    }

    void SetNull() {
        hashType = 0;
        hashBytes.SetNull();
        txhash.SetNull();
        indexout = 0;
    }
};

struct CAddressUnspentValue {
    CAmount satoshis;
    CScript script;
    int32_t blockHeight;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(satoshis);
        READWRITE(*(CScriptBase*)(&script));
        READWRITE(blockHeight);
    }

    CAddressUnspentValue(CAmount sats, CScript scriptPubKey, int height) {
        satoshis = sats;
        script = scriptPubKey;
        blockHeight = (int32_t) height;
    }

    CAddressUnspentValue() {
        SetNull();
    }

    void SetNull() {
        satoshis = -1;
        script.clear();
        blockHeight = 0;
    }

    bool IsNull() const {
        return (satoshis == -1);
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

#define ANDX_IS_SPENT 1
#define ANDX_IS_STAKE 2
#define ANDX_TO_SAME  4
#define ANDX_ORPHANED 128

struct CAddressIndexKey {
    uint160 hashBytes;
    uint8_t hashType;
    uint8_t spentFlags;
    int32_t blockHeight;
    uint32_t blockIndex;
    uint8_t  indexType;
    uint16_t indexInOut;
    uint256 txhash;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 64;
    }

    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata8(s, hashType);
        hashBytes.Serialize(s, nType, nVersion);
        // Heights are stored big-endian for key sorting in LevelDB
        ser_writedata32be(s, blockHeight);
        ser_writedata32be(s, (blockIndex << 4) | indexType);
        ser_writedata16(s, indexInOut);
        ser_writedata8(s, spentFlags);
        txhash.Serialize(s, nType, nVersion);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        hashType = ser_readdata8(s);
        hashBytes.Unserialize(s, nType, nVersion);
        blockHeight = ser_readdata32be(s);
        blockIndex = ser_readdata32be(s);
        indexType  = blockIndex & 0xF;
        blockIndex = blockIndex >> 4;
        indexInOut = ser_readdata16(s);
        spentFlags = ser_readdata8(s);
        txhash.Unserialize(s, nType, nVersion);
    }

    CAddressIndexKey(unsigned int addressType, uint160 addressHash, int height, int blockindex,
                     uint256 txh, size_t indexVout, uint8_t flags) {
        hashType = (uint8_t) addressType;
        hashBytes = addressHash;
        blockHeight = (int32_t) height;
        blockIndex = (uint32_t) blockindex; // could be reduced to uint16_t, but kept for big endian
        indexInOut = (uint16_t) indexVout;
        indexType = ((flags & ANDX_IS_SPENT) == 0) ? 1 : 0; // outputs after inputs
        spentFlags = flags;
        txhash = txh;
    }

    CAddressIndexKey() {
        SetNull();
    }

    void SetNull() {
        hashType = 0;
        hashBytes.SetNull();
        spentFlags = 0;
        blockHeight = 0;
        blockIndex = 0;
        indexType = 0;
        indexInOut = 0;
        txhash.SetNull();
    }
};

// for DB_ADDRESSINDEX and DB_ADDRESSUNSPENTINDEX
struct CAddressIndexIteratorKey {
    uint160 hashBytes;
    uint8_t hashType;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 21;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata8(s, hashType);
        hashBytes.Serialize(s, nType, nVersion);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        hashType = ser_readdata8(s);
        hashBytes.Unserialize(s, nType, nVersion);
    }

    CAddressIndexIteratorKey(unsigned int addressType, uint160 addressHash) {
        hashBytes = addressHash;
        hashType = (uint8_t) addressType;
    }

    CAddressIndexIteratorKey() {
        SetNull();
    }

    void SetNull() {
        hashType = 0;
        hashBytes.SetNull();
    }
};

struct CAddressIndexIteratorHeightKey {
    int32_t blockHeight;
    uint160 hashBytes;
    uint8_t hashType;

    size_t GetSerializeSize(int nType, int nVersion) const {
        return 25;
    }
    template<typename Stream>
    void Serialize(Stream& s, int nType, int nVersion) const {
        ser_writedata32be(s, blockHeight);
        ser_writedata8(s, hashType);
        hashBytes.Serialize(s, nType, nVersion);
    }
    template<typename Stream>
    void Unserialize(Stream& s, int nType, int nVersion) {
        blockHeight = ser_readdata32be(s);
        hashType = ser_readdata8(s);
        hashBytes.Unserialize(s, nType, nVersion);
    }

    CAddressIndexIteratorHeightKey(unsigned int addrType, uint160 addrHash, int height) {
        blockHeight = (int32_t) height;
        hashBytes = addrHash;
        hashType = (uint8_t) addrType;
    }

    CAddressIndexIteratorHeightKey() {
        SetNull();
    }

    void SetNull() {
        blockHeight = 0;
        hashType = 0;
        hashBytes.SetNull();
    }
};


#endif // BITCOIN_SPENTINDEX_H
