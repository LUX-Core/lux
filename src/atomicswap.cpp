// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <atomicswap.h>

#include <crypto/sha256.h>
#include <policy/policy.h>
#include <pubkey.h>
#include <random.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/sign.h>

#define ATOMIC_SECRET_SIZE 32

UniValue CSwapContract::ToUniValue() const {
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("ticker", ticker);
    obj.pushKV("txid", txid.GetHex());
    obj.pushKV("ticker", nVout);

    return obj;
}

bool TryDecodeAtomicSwapScript(const CScript& swapRedeemScript,
                               std::vector<unsigned char>& secretHash, CKeyID& recipient, CKeyID& refund, int64_t& locktime, int64_t& secretSize)
{
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

CScript CreateAtomicSwapRedeemscript(CKeyID initiator, CKeyID redeemer, int64_t locktime, std::vector<unsigned char> secretHash)
{
    CScript script;
    script = CScript() << OP_IF;
    script << OP_SIZE << ATOMIC_SECRET_SIZE << OP_EQUALVERIFY << OP_SHA256 << secretHash << OP_EQUALVERIFY << OP_DUP << OP_HASH160 << ToByteVector(redeemer);
    script << OP_ELSE;
    script << locktime << OP_CHECKLOCKTIMEVERIFY << OP_DROP << OP_DUP << OP_HASH160 << ToByteVector(initiator);
    script << OP_ENDIF;
    script << OP_EQUALVERIFY << OP_CHECKSIG;
    return script;
}

bool CreateAtomicSwapRedeemSigScript(const CKeyStore* keystore, const CMutableTransaction& redeemTx, CAmount amount,
                                     const CKeyID& address, const CScript& swapRedeemScript, const std::vector<unsigned char>& secret,
                                     const CScript redeemContractPubKey, CScript& redeemScriptSig) {
    MutableTransactionSignatureCreator creator(keystore, &redeemTx, 0, amount, SIGHASH_ALL);
    std::vector<unsigned char> vch;
    CPubKey pubKey;
    keystore->GetPubKey(address, pubKey);
    if (!creator.CreateSig(vch, address, swapRedeemScript, SIGVERSION_BASE)) {
        return false;
    }

    redeemScriptSig = CScript() << vch << ToByteVector(pubKey) << secret << int64_t(1) << std::vector<unsigned char>(swapRedeemScript.begin(), swapRedeemScript.end());
    return VerifyScript(redeemScriptSig, redeemContractPubKey, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, creator.Checker());
}

bool CreateAtomicSwapRefundSigScript(const CKeyStore* keystore, const CMutableTransaction& redeemTx, CAmount amount,
                                     const CKeyID& address, const CScript& swapRedeemScript, const CScript redeemContractPubKey, CScript& refundScriptSig) {
    MutableTransactionSignatureCreator creator(keystore, &redeemTx, 0, amount, SIGHASH_ALL);
    std::vector<unsigned char> vch;
    CPubKey pubKey;
    keystore->GetPubKey(address, pubKey);
    if (!creator.CreateSig(vch, address, swapRedeemScript, SIGVERSION_BASE)) {
        return false;
    }

    refundScriptSig = CScript() << vch << ToByteVector(pubKey) << int64_t(0) << std::vector<unsigned char>(swapRedeemScript.begin(), swapRedeemScript.end());
    return VerifyScript(refundScriptSig, redeemContractPubKey, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, creator.Checker());
}

void GenerateSecret(std::vector<unsigned char>& secret, std::vector<unsigned char>& secretHash)
{
    secret.assign(ATOMIC_SECRET_SIZE, 0);
    secretHash.assign(CSHA256::OUTPUT_SIZE, 0);

    GetRandBytes(secret.data(), secret.size());
    CSHA256 sha;
    sha.Write(secret.data(), secret.size());
    sha.Finalize(secretHash.data());
}
