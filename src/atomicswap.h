// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUX_ATOMICSWAP_H
#define LUX_ATOMICSWAP_H

#include <amount.h>
#include <blockchainclient.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <serialize.h>
#include <univalue/univalue.h>

#include <vector>

class CKeyID;
class CKeyStore;
class CScript;
class CWallet;
class CWalletTx;
enum OutputType : int;

/**
 * @class CSwapContract
 * @brief Contains data for finding swap transaction in blockchain
 */
class CSwapContract {
public:
    std::string ticker; /** ticker for choosing blockchain in which to search for transaction*/
    uint256 txid;       /** transaction id with atomic swap script */
    int nVout;          /** transaction vout index */

    CSwapContract() {}
    CSwapContract(std::string tickerIn, uint256& txidIn, int nVoutIn) : ticker(tickerIn), txid(txidIn), nVout(nVoutIn) {}
    virtual ~CSwapContract() {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(ticker);
        READWRITE(txid);
        READWRITE(nVout);
    }

    UniValue ToUniValue() const;

};

/**
 * @brief Generates atomic swap secret
 * @param secret Atomic swap secret
 * @param secretHash Atomic swap secret's hash
 */
void GenerateSecret(std::vector<unsigned char>& secret, std::vector<unsigned char>& secretHash);

/**
 * @brief Creates native Lux script for atomic swap
 * @param initiator The initiator of the swap, whho creates this script
 * @param redeemer The redeemer of the swap, who will receive coins from this script
 * @param locktime The timestamp of the time when the coins should be unlocked to refund
 * @param secretHash The hash of atomic secret
 * @return script for redeeming and refunding coins (depends on unlocking script) in this blockchain
 */
CScript CreateAtomicSwapRedeemScript(CKeyID initiator, CKeyID redeemer, int64_t locktime, std::vector<unsigned char> secretHash);

/**
 * @namespace LuxSwap
 * @brief Lux specific utility functions for swaps
 */
namespace LuxSwap {

/**
 * @brief Creates unlocking script for swap redemption and checks it's validity
 * @param pwallet Wallet to use for creating signature
 * @param redeemTx Transaction to sign
 * @param amount Amount of coins the signed transaction sends
 * @param address Hash of the recepient's public key
 * @param swapRedeemScript Atomic swap redeem script
 * @param secret Atomic swap secret
 * @param redeemContractPubKey P2SH script of atomic swap redeem script
 * @param [out] redeemScriptSig Redemption tx signature
 * @return Is script valid
 */
bool CreateAtomicSwapRedeemSigScript(CWallet* pwallet, const CMutableTransaction& redeemTx, CAmount amount,
                                     const CKeyID& address, const CScript& swapRedeemScript, const std::vector<unsigned char>& secret,
                                     const CScript redeemContractPubKey, CScript& redeemScriptSig);

/**
 * @brief Creates unlocking script for swap refunding and checks it's validity
 * @param pwallet Wallet to use for creating signature
 * @param refundTx Transaction to sign
 * @param amount Amount of coins the signed transaction sends
 * @param address Hash of the recepient's public key
 * @param swapRedeemScript Atomic swap redeem script
 * @param redeemContractPubKey P2SH script of atomic swap redeem script
 * @param [out] refundScriptSig Refunding tx signature
 * @return Is script valid
 */
bool CreateAtomicSwapRefundSigScript(CWallet* pwallet, const CMutableTransaction& refundTx, CAmount amount,
                                     const CKeyID& address, const CScript& swapRedeemScript,
                                     const CScript redeemContractPubKey, CScript& refundScriptSig);

/**
 * @brief Create address for refunding swap transaction
 * @param pwallet Wallet to create address from
 * @param outputType Refund address type
 * @return Created refund address in a form of CTxDestination
 */
CTxDestination GetRawChangeAddress(CWallet* pwallet, OutputType outputType);

/**
 * @brief Creates atomic swap transaction
 * @param pwallet Wallet to use for creating tx
 * @param redeemer Address of the swap redeemer
 * @param nAmount Amount of coins to swap
 * @param lockTime Timestamp to lock coins for refund until this time
 * @param [out] redeemTx Created swap transaction
 * @param [out] redeemScript Created redeem script
 * @param [out] nFeeRequired Minimum fee required for the swap transaction
 * @param [out] secret Generated atomic secret
 * @param [out] secretHash Hash of the generated atomic secret
 * @param [out] strError Error messages if any
 * @return Transaction creation success flag
 */
bool CreateSwapTransaction(CWallet *pwallet, const CKeyID &redeemer, CAmount nAmount, uint64_t lockTime,
                           CWalletTx &redeemTx, CScript &redeemScript, CAmount &nFeeRequired,
                           std::vector<unsigned char> &secret, std::vector<unsigned char> &secretHash, std::string &strError);

/**
 * @brief Create refund transaction for atomic swap transaction
 * @param pwallet Wallet to use for creating tx
 * @param redeemScript Redemption script
 * @param contractTx Swap redemption transaction
 * @param [out] refundTx Created swap refund transaction
 * @param [out] refundFee Minimum fee required for the refund transaction
 * @param [out] strError Error messages if any
 * @return Refund transaction creation success flag
 */
bool CreateRefundTransaction(CWallet *pwallet, const CScript &redeemScript, const CMutableTransaction &contractTx,
                             CMutableTransaction &refundTx, CAmount &refundFee, std::string &strError);

} //namespace LuxSwap

/**
 * @namespace BitcoinSwap
 * @brief Bitcoin specific utility functions for swaps
 */
namespace BitcoinSwap {

class COutPoint
{
public:
    uint256 hash;
    uint32_t n;

    COutPoint(): n((uint32_t) -1) { }
    COutPoint(const uint256& hashIn, uint32_t nIn): hash(hashIn), n(nIn) { }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(hash);
        READWRITE(n);
    }

    void SetNull() { hash.SetNull(); n = (uint32_t) -1; }
    bool IsNull() const { return (hash.IsNull() && n == (uint32_t) -1); }
};

struct CScriptWitness
{
    // Note that this encodes the data elements being pushed, rather than
    // encoding them as a CScript that pushes them.
    std::vector<std::vector<unsigned char> > stack;

    // Some compilers complain without a default constructor
    CScriptWitness() { }

    bool IsNull() const { return stack.empty(); }

    void SetNull() { stack.clear(); stack.shrink_to_fit(); }
};

class CTxIn
{
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;
    CScriptWitness scriptWitness; //! Only serialized through CTransaction

    /* Setting nSequence to this value for every input in a transaction
     * disables nLockTime. */
    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    CTxIn() { nSequence = SEQUENCE_FINAL; }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL) {
        prevout = prevoutIn;
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }
    CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn=CScript(), uint32_t nSequenceIn=SEQUENCE_FINAL) {
        prevout = COutPoint(hashPrevTx, nOut);
        scriptSig = scriptSigIn;
        nSequence = nSequenceIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(prevout);
        READWRITE(scriptSig);
        READWRITE(nSequence);
    }
};

class CTxOut
{
public:
    CAmount nValue;
    CScript scriptPubKey;

    CTxOut() { SetNull(); }

    CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn) {
        nValue = nValueIn;
        scriptPubKey = scriptPubKeyIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nValue);
        READWRITE(scriptPubKey);
    }

    void SetNull() {
        nValue = -1;
        scriptPubKey.clear();
    }

    bool IsNull() const {
        return (nValue == -1);
    }

    friend bool operator==(const CTxOut& a, const CTxOut& b) {
        return (a.nValue       == b.nValue &&
                a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut& a, const CTxOut& b) {
        return !(a == b);
    }

};

struct CMutableTransaction;

template<typename Stream, typename Operation, typename TxType>
inline void SerializeTransaction(TxType& tx, Stream& s, Operation ser_action, int nType, int nVersion) {
    READWRITE(*const_cast<int32_t*>(&tx.nVersion));
    unsigned char flags = 0;
    if (ser_action.ForRead()) {
        const_cast<std::vector<CTxIn>*>(&tx.vin)->clear();
        const_cast<std::vector<CTxOut>*>(&tx.vout)->clear();
        /* Try to read the vin. In case the dummy is there, this will be read as an empty vector. */
        READWRITE(*const_cast<std::vector<CTxIn>*>(&tx.vin));
        if (tx.vin.size() == 0 && !(nVersion & SERIALIZE_TRANSACTION_NO_WITNESS)) {
            /* We read a dummy or an empty vin. */
            READWRITE(flags);
            if (flags != 0) {
                READWRITE(*const_cast<std::vector<CTxIn>*>(&tx.vin));
                READWRITE(*const_cast<std::vector<CTxOut>*>(&tx.vout));
            }
        } else {
            /* We read a non-empty vin. Assume a normal vout follows. */
            READWRITE(*const_cast<std::vector<CTxOut>*>(&tx.vout));
        }
        if ((flags & 1) && !(nVersion & SERIALIZE_TRANSACTION_NO_WITNESS)) {
            /* The witness flag is present, and we support witnesses. */
            flags ^= 1;
            for (size_t i = 0; i < tx.vin.size(); i++) {
                READWRITE(tx.vin[i].scriptWitness.stack);
            }
        }
        if (flags) {
            /* Unknown flag in the serialization */
            throw std::ios_base::failure("Unknown transaction optional data");
        }
    } else {
        if (!(nVersion & SERIALIZE_TRANSACTION_NO_WITNESS)) {
            /* Check whether witnesses need to be serialized. */
            if (tx.HasWitness()) {
                flags |= 1;
            }
        }
        if (flags) {
            /* Use extended format in case witnesses are to be serialized. */
            std::vector<CTxIn> vinDummy;
            READWRITE(vinDummy);
            READWRITE(flags);
        }
        READWRITE(*const_cast<std::vector<CTxIn>*>(&tx.vin));
        READWRITE(*const_cast<std::vector<CTxOut>*>(&tx.vout));
        if (flags & 1) {
            for (size_t i = 0; i < tx.vin.size(); i++) {
                READWRITE(tx.vin[i].scriptWitness.stack);
            }
        }
    }
    READWRITE(*const_cast<uint32_t*>(&tx.nLockTime));
}

class CTransaction
{
public:
    static const int32_t CURRENT_VERSION=2;
    static const int32_t MAX_STANDARD_VERSION=2;

    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const int32_t nVersion;
    const uint32_t nLockTime;

public:
    CTransaction();

    /** Convert a CMutableTransaction into a CTransaction. */
    CTransaction(const CMutableTransaction &tx);
    CTransaction(CMutableTransaction &&tx);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        BitcoinSwap::SerializeTransaction(*this, s, ser_action, nType, nVersion);
    }

    bool IsNull() const {
        return vin.empty() && vout.empty();
    }

    bool HasWitness() const {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }
};

/** A mutable version of CTransaction. */
struct CMutableTransaction
{
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    int32_t nVersion;
    uint32_t nLockTime;

    CMutableTransaction();
    CMutableTransaction(const CTransaction& tx);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        BitcoinSwap::SerializeTransaction(*this, s, ser_action, nType, nVersion);
    }

    bool HasWitness() const {
        for (size_t i = 0; i < vin.size(); i++) {
            if (!vin[i].scriptWitness.IsNull()) {
                return true;
            }
        }
        return false;
    }
};

/**
 * @brief Creates unlocking script for swap redemption
 * @param client Bitcoin RPC client
 * @param redeemTxHex Raw transaction to sign
 * @param swapRedeemScript Atomic swap redeem script
 * @param secret Atomic swap secret
 * @param [out] redeemScriptSig Redemption tx signature
 * @return Hex encoded signed tx
 */
std::string CreateAtomicSwapRedeemSigScript(std::shared_ptr<CBitcoinClient> client, const std::string redeemTxHex,
                                            const CScript& swapRedeemScript, const std::vector<unsigned char>& secret,
                                            CScript& redeemScriptSig);

/**
 * @brief Creates unlocking script for swap refunding
 * @param client Bitcoin RPC client
 * @param refundTx Transaction to sign
 * @param swapRedeemScript Atomic swap redeem script
 * @param [out] refundScriptSig Refunding tx signature
 * @return Hex encoded signed tx
 */
std::string CreateAtomicSwapRefundSigScript(std::shared_ptr<CBitcoinClient> client, const std::string refundTx,
                                            const CScript& swapRedeemScript, CScript& refundScriptSig);
}

#endif //LUX_ATOMICSWAP_H
