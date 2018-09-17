// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <atomicswap.h>
#include <base58.h>
#include <coincontrol.h>
#include <core_io.h>
#include <crypto/sha256.h>
#include <main.h>
#include <policy/policy.h>
#include <pubkey.h>
#include <random.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/sign.h>
#include <timedata.h>
#include <wallet.h>

#define ATOMIC_SECRET_SIZE 32

UniValue CSwapContract::ToUniValue() const {
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("ticker", ticker);
    obj.pushKV("txid", txid.GetHex());
    obj.pushKV("ticker", nVout);

    return obj;
}

static const int redeemAtomicSwapSigScriptSize = 1 + 73 + 1 + 33 + 1 + 32 + 1;
static const int refundAtomicSwapSigScriptSize = 1 + 73 + 1 + 33 + 1;

int CalcInputSize(int scriptSigSize)
{
    return 32 + 4 + ::GetSerializeSize(VARINT(scriptSigSize), 0, 0) + scriptSigSize + 4;
}

void GenerateSecret(std::vector<unsigned char>& secret, std::vector<unsigned char>& secretHash) {
    secret.assign(ATOMIC_SECRET_SIZE, 0);
    secretHash.assign(CSHA256::OUTPUT_SIZE, 0);

    GetRandBytes(secret.data(), secret.size());
    CSHA256 sha;
    sha.Write(secret.data(), secret.size());
    sha.Finalize(secretHash.data());
}

bool TryDecodeAtomicSwapScript(const CScript& swapRedeemScript,
                               std::vector<unsigned char>& secretHash, CKeyID& recipient, CKeyID& refund, int64_t& locktime, int64_t& secretSize) {
    CScript::const_iterator pc = swapRedeemScript.begin();
    opcodetype opcode;
    std::vector<unsigned char> secretSizeData;
    std::vector<unsigned char> locktimeData;
    std::vector<unsigned char> recipientHash;
    std::vector<unsigned char> refundHash;

    if ((swapRedeemScript.GetOp(pc, opcode) && opcode == OP_IF) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_SIZE) &&
        (swapRedeemScript.GetOp(pc, opcode, secretSizeData)) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_EQUALVERIFY) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_SHA256) &&
        (swapRedeemScript.GetOp(pc, opcode, secretHash) && opcode == 0x20) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_EQUALVERIFY) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_DUP) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_HASH160) &&
        (swapRedeemScript.GetOp(pc, opcode, recipientHash) && opcode == 0x14) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_ELSE) &&
        (swapRedeemScript.GetOp(pc, opcode, locktimeData)) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_CHECKLOCKTIMEVERIFY) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_DROP) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_DUP) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_HASH160) &&
        (swapRedeemScript.GetOp(pc, opcode, refundHash) && opcode == 0x14) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_ENDIF) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_EQUALVERIFY) &&
        (swapRedeemScript.GetOp(pc, opcode) && opcode == OP_CHECKSIG)
            ) {
        secretSize = CScriptNum(secretSizeData, true).getint();
        locktime = CScriptNum(locktimeData, true).getint();
        recipient = CKeyID(uint160(recipientHash));
        refund = CKeyID(uint160(refundHash));
        return true;
    }

    return false;
}

CScript CreateAtomicSwapRedeemScript(CKeyID initiator, CKeyID redeemer, int64_t locktime, std::vector<unsigned char> secretHash) {
    CScript script;
    script = CScript() << OP_IF;
    script << OP_SIZE << ATOMIC_SECRET_SIZE << OP_EQUALVERIFY << OP_SHA256 << secretHash << OP_EQUALVERIFY << OP_DUP << OP_HASH160 << ToByteVector(redeemer);
    script << OP_ELSE;
    script << locktime << OP_CHECKLOCKTIMEVERIFY << OP_DROP << OP_DUP << OP_HASH160 << ToByteVector(initiator);
    script << OP_ENDIF;
    script << OP_EQUALVERIFY << OP_CHECKSIG;
    return script;
}

namespace LuxSwap {


CKeyID DecodeAddress(const std::string luxaddress)
{
    CTxDestination dest = DecodeDestination(luxaddress);
    return *boost::get<CKeyID>(&dest);
}

bool CreateSwapTx(const std::string redeemerLuxAddress, const CAmount amount, std::string &txid)
{
    CWalletTx redeemTx;
    CScript redeemScript;
    CAmount feeRequired;
    dev::bytes secret;
    dev::bytes secretHash;
    std::string error;
    if (CreateSwapTransaction(pwalletMain, 
                              DecodeAddress(redeemerLuxAddress), 
                              amount, 
                              GetAdjustedTime() + 48 * 60 * 60, 
                              redeemTx, redeemScript, feeRequired, secret, secretHash, error)) {
        RelayTransaction(redeemTx);
        txid = redeemTx.GetHash().ToString();
        return true;
    } else {
        LogPrintf("Error while creating swap transaction: %s\n", error);
        return false;
    }
}

int EstimateRefundTxSerializeSize(const CScript &contractRedeemscript, std::vector <CTxOut> txOuts) {
    int outputsSerializeSize = 0;
    for (const CTxOut &txout : txOuts) {
        outputsSerializeSize += ::GetSerializeSize(txout, 0, 0);
    }
    return 4 + 4 + 4 + ::GetSerializeSize(VARINT(1), 0, 0) + ::GetSerializeSize(VARINT(txOuts.size()), 0, 0) +
           CalcInputSize(refundAtomicSwapSigScriptSize + contractRedeemscript.size()) + outputsSerializeSize;
}

int EstimateRedeemTxSerializeSize(const CScript &contractRedeemscript, std::vector <CTxOut> txOuts) {
    int outputsSerializeSize = 0;
    for (const CTxOut &txout : txOuts) {
        outputsSerializeSize += ::GetSerializeSize(txout, 0, 0);
    }
    return 4 + 4 + 4 + ::GetSerializeSize(VARINT(1), 0, 0) + ::GetSerializeSize(VARINT(txOuts.size()), 0, 0) +
           CalcInputSize(redeemAtomicSwapSigScriptSize + contractRedeemscript.size()) + outputsSerializeSize;
}

bool CreateAtomicSwapRedeemSigScript(CWallet *pwallet, const CMutableTransaction &redeemTx, CAmount amount,
                                     const CKeyID &address, const CScript &swapRedeemScript,
                                     const std::vector<unsigned char> &secret,
                                     const CScript redeemContractPubKey, CScript &redeemScriptSig) {
    MutableTransactionSignatureCreator creator(pwallet, &redeemTx, 0, amount, SIGHASH_ALL);
    std::vector<unsigned char> vch;
    CPubKey pubKey;
    pwallet->GetPubKey(address, pubKey);
    if (!creator.CreateSig(vch, address, swapRedeemScript, SIGVERSION_BASE)) {
        return false;
    }

    redeemScriptSig = CScript() << vch << ToByteVector(pubKey) << secret << int64_t(1)
                                << std::vector<unsigned char>(swapRedeemScript.begin(), swapRedeemScript.end());
    return VerifyScript(redeemScriptSig, redeemContractPubKey, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS,
                        creator.Checker());
}

bool CreateAtomicSwapRefundSigScript(CWallet *pwallet, const CMutableTransaction &redeemTx, CAmount amount,
                                     const CKeyID &address, const CScript &swapRedeemScript,
                                     const CScript redeemContractPubKey, CScript &refundScriptSig) {
    MutableTransactionSignatureCreator creator(pwallet, &redeemTx, 0, amount, SIGHASH_ALL);
    std::vector<unsigned char> vch;
    CPubKey pubKey;
    pwallet->GetPubKey(address, pubKey);
    if (!creator.CreateSig(vch, address, swapRedeemScript, SIGVERSION_BASE)) {
        return false;
    }

    refundScriptSig = CScript() << vch << ToByteVector(pubKey) << int64_t(0)
                                << std::vector<unsigned char>(swapRedeemScript.begin(), swapRedeemScript.end());
    return VerifyScript(refundScriptSig, redeemContractPubKey, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS,
                        creator.Checker());
}

CTxDestination GetRawChangeAddress(CWallet *pwallet, OutputType outputType) {
    CReserveKey reservekey(pwallet);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey, true))
        return CNoDestination();

    reservekey.KeepKey();

    pwallet->LearnRelatedScripts(vchPubKey, outputType);
    return GetDestinationForKey(vchPubKey, outputType);
}

bool CreateSwapTransaction(CWallet *pwallet, const CKeyID &redeemer, CAmount nAmount, uint64_t lockTime,
                           CWalletTx &redeemTx,
                           CScript &redeemScript, CAmount &nFeeRequired, std::vector<unsigned char> &secret,
                           std::vector<unsigned char> &secretHash, std::string &strError) {

    AssertLockHeld(cs_main);
    AssertLockHeld(pwallet->cs_wallet);

    GenerateSecret(secret, secretHash);

    CTxDestination refundDest = GetRawChangeAddress(pwallet, OUTPUT_TYPE_LEGACY);
    CKeyID refundAddr = boost::get<CKeyID>(refundDest);

    redeemScript = CreateAtomicSwapRedeemScript(refundAddr, redeemer, lockTime, secretHash);
    CScriptID swapContractAddr = CScriptID(redeemScript);
    CScript contractPubKeyScript = GetScriptForDestination(swapContractAddr);

    CReserveKey reservekey(pwalletMain);
    std::vector <std::pair<CScript, CAmount>> vecSend;
    vecSend.push_back(std::make_pair(contractPubKeyScript, nAmount));
    CCoinControl coinControl;
    coinControl.destChange = refundDest;
    int nChangePos;

    return pwallet->CreateTransaction(vecSend, redeemTx, reservekey, nFeeRequired, nChangePos, strError, &coinControl);
}

bool CreateRefundTransaction(CWallet *pwallet, const CScript &redeemScript, const CMutableTransaction &contractTx,
                             CMutableTransaction &refundTx, CAmount &refundFee, std::string &strError) {
    AssertLockHeld(cs_main);
    AssertLockHeld(pwallet->cs_wallet);

    std::vector<unsigned char> secretHash;
    CKeyID recipient;
    CKeyID refundAddr;
    int64_t locktime;
    int64_t secretSize;

    if (!TryDecodeAtomicSwapScript(redeemScript, secretHash, recipient, refundAddr, locktime, secretSize)) {
        strError = "Invalid atomic swap script";
        return false;
    }

    CScriptID swapContractAddr = CScriptID(redeemScript);
    CScript contractPubKeyScript = GetScriptForDestination(swapContractAddr);

    COutPoint contractOutPoint;
    contractOutPoint.hash = contractTx.GetHash();
    for (size_t i = 0; i < contractTx.vout.size(); i++) {
        if (contractTx.vout[i].scriptPubKey.size() == contractPubKeyScript.size() &&
            std::equal(contractPubKeyScript.begin(), contractPubKeyScript.end(), contractTx.vout[i].scriptPubKey.begin())) {
            contractOutPoint.n = i;
            break;
        }
    }

    if (contractOutPoint.n == (uint32_t) -1) {
        strError = "Contract tx does not contain a P2SH contract payment";
        return false;
    }

    //create refund transaction
    CTxDestination refundAddress = GetRawChangeAddress(pwallet, OUTPUT_TYPE_LEGACY);
    CScript refundPubkeyScript = GetScriptForDestination(refundAddress);

    refundTx.nLockTime = locktime;
    refundTx.vout.push_back(CTxOut(0, refundPubkeyScript));

    int refundTxSize = EstimateRefundTxSerializeSize(redeemScript, refundTx.vout);
    CCoinControl coinControl;
    refundFee = pwallet->GetMinimumFee(refundTxSize, 1, mempool);
    refundTx.vout[0].nValue = contractTx.vout[contractOutPoint.n].nValue - refundFee;
    if (refundTx.vout[0].IsDust(::minRelayTxFee)) {
        strError = strprintf("Redeem output value of %d is dust", refundTx.vout[0].nValue);
        return false;
    }

    CTxIn txIn(contractOutPoint);
    txIn.nSequence = 0;
    refundTx.vin.push_back(txIn);

    CScript refundSigScript;
    if (!CreateAtomicSwapRefundSigScript(pwallet, refundTx, contractTx.vout[contractOutPoint.n].nValue, refundAddr, redeemScript, contractPubKeyScript, refundSigScript)) {
        strError = "Failed to create refund script signature";
        return false;
    }

    refundTx.vin[0].scriptSig = refundSigScript;
    return true;
}

} //namespace LuxSwap


namespace BitcoinSwap {

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), nLockTime(tx.nLockTime) {}

CTransaction::CTransaction() : vin(), vout(), nVersion(CTransaction::CURRENT_VERSION), nLockTime(0) {}
CTransaction::CTransaction(const CMutableTransaction &tx) : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion), nLockTime(tx.nLockTime) {}
CTransaction::CTransaction(CMutableTransaction &&tx) : vin(std::move(tx.vin)), vout(std::move(tx.vout)), nVersion(tx.nVersion), nLockTime(tx.nLockTime) {}

std::string CreateAtomicSwapRedeemSigScript(std::shared_ptr<CBitcoinClient> client, const std::string redeemTx, const CScript &swapRedeemScript,
                                     const std::vector<unsigned char> &secret, CScript &redeemScriptSig) {

    std::string result;
    // Try to sign the redeemTx transaction. The signing is expected to fail, but it will return generated signature and pubkey.
    // Those should be fully valid, but because we haven't provided secret and 1 for OP_IF branching to the redemption, script verification will fail.
    // So we take generated scriptSig and write it to the input's scriptSig, then append secret and 1 to it, which makes
    // verification to be able to evaluate scriptSig to true. Then we hex encode this signed transaction and return it.
    if (!client->SignRawTransaction(redeemTx, result)) {
        int protocolVersion = client->GetProtocolVersion();
        std::vector<unsigned char> txData(ParseHex(redeemTx));

        CDataStream ssData(txData, SER_NETWORK, protocolVersion | SERIALIZE_TRANSACTION_NO_WITNESS);
        BitcoinSwap::CMutableTransaction mtx;
        ssData >> mtx;
        // Result is took from vin.hex, so it already has both <sig> and <pubkey>

        redeemScriptSig = CScript() << ToByteVector(result) << secret << int64_t(1) << std::vector<unsigned char>(swapRedeemScript.begin(), swapRedeemScript.end());
        mtx.vin[0].scriptSig = redeemScriptSig;

        CDataStream ssTx(SER_NETWORK, protocolVersion);
        ssTx << mtx;
        return HexStr(ssTx.begin(), ssTx.end());
    } else
        return result;

}

std::string CreateAtomicSwapRefundSigScript(std::shared_ptr<CBitcoinClient> client, const std::string refundTx,
                                     const CScript& swapRedeemScript, CScript& refundScriptSig) {
    std::string result;
    // Try to sign the refundTx transaction. The signing is expected to fail, but it will return generated signature and pubkey.
    // Those should be fully valid, but because we haven't provided 0 for OP_IF branching to the refund, script verification will fail.
    // So we take generated scriptSig and write it to the input's scriptSig, then append  0 to it, which makes
    // verification to be able to evaluate scriptSig to true. Then we hex encode this signed transaction and return it.
    if (!client->SignRawTransaction(refundTx, result)) {
        int protocolVersion = client->GetProtocolVersion();
        std::vector<unsigned char> txData(ParseHex(refundTx));

        CDataStream ssData(txData, SER_NETWORK, protocolVersion | SERIALIZE_TRANSACTION_NO_WITNESS);
        BitcoinSwap::CMutableTransaction mtx;
        ssData >> mtx;
        // Result is took from vin.hex, so it already has both <sig> and <pubkey>
        refundScriptSig = CScript() << ToByteVector(result) << int64_t(0) << std::vector<unsigned char>(swapRedeemScript.begin(), swapRedeemScript.end());
        mtx.vin[0].scriptSig = refundScriptSig;

        CDataStream ssTx(SER_NETWORK, protocolVersion);
        ssTx << mtx;
        return HexStr(ssTx.begin(), ssTx.end());
    } else
        return result;

}

} //namespace BitcoinSwap