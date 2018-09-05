// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUX_ATOMICSWAP_H
#define LUX_ATOMICSWAP_H

#include <amount.h>
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
 * @brief Creates native Bitcoin script for atomic swap
 * @param initiator The initiator of the swap, whho creates this script
 * @param redeemer The redeemer of the swap, who will receive coins from this script
 * @param locktime The timestamp of the time when the coins should be unlocked to refund
 * @param secretHash The hash of atomic secret
 * @return script for redeeming and refunding coins (depends on unlocking script) in this blockchain
 */
CScript CreateAtomicSwapRedeemScript(CKeyID initiator, CKeyID redeemer, int64_t locktime, std::vector<unsigned char> secretHash);

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
 * @param secret Atomic swap secret
 * @param redeemContractPubKey P2SH script of atomic swap redeem script
 * @param [out] refundScriptSig Refunding tx signature
 * @return Is script valid
 */
bool CreateAtomicSwapRefundSigScript(CWallet* pwallet, const CMutableTransaction& refundTx, CAmount amount,
                                     const CKeyID& address, const CScript& swapRedeemScript,
                                     const CScript redeemContractPubKey, CScript& refundScriptSig);

/**
 * @brief Generates atomic swap secret
 * @param secret Atomic swap secret
 * @param secretHash Atomic swap secret's hash
 */
void GenerateSecret(std::vector<unsigned char>& secret, std::vector<unsigned char>& secretHash);

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
bool CreateSwapTransaction(CWallet* pwallet, const CKeyID& redeemer, CAmount nAmount, uint64_t lockTime, CWalletTx& redeemTx,
                           CScript& redeemScript, CAmount& nFeeRequired, std::vector<unsigned char>& secret, std::vector<unsigned char>& secretHash, std::string& strError);

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
bool CreateRefundTransaction(CWallet* pwallet, const CScript& redeemScript, const CMutableTransaction& contractTx, CMutableTransaction& refundTx, CAmount& refundFee, std::string& strError);


#endif //LUX_ATOMICSWAP_H
