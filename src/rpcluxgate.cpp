// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <atomicswap.h>
#include <base58.h>
#include <blockchainclient.h>
#include <main.h>
#include <rpcserver.h>
#include <sync.h>
#include <timedata.h>
#include <univalue/univalue.h>
#include <util.h>
#include <utilstrencodings.h>
#include <wallet.h>


UniValue createorder(const UniValue& params, bool fHelp) 
{
    if (fHelp ||  params.size() != 4 ) {
         throw std::runtime_error(
             "createorder base_coin rel_coin base_amount rel_amount\n"
             "Create order for atomic swap\n"
              "\nArguments:\n"
              "1. Coin ticker to sell"
              "2. Coin ticker to receive"
              "3. Amount of base coins"
              "4. Amount of rel coins");
    }
    
    LOCK(cs_main);
    UniValue result(UniValue::VOBJ);
    return result;
}

UniValue getactivecoins(const UniValue& params, bool fHelp)
{
    if (fHelp || params.size() > 0 ) {
         throw std::runtime_error(
             "getactivecoins\n"
             "Get active coins for atomic swap\n");
    }

    LOCK(cs_main);

    UniValue result(UniValue::VOBJ);

    UniValue coins(UniValue::VARR);
    for (auto const &c : blockchainClientPool) {
        UniValue coinInfo(UniValue::VOBJ);
        coinInfo.push_back(Pair("ticker", c.first));
        coinInfo.push_back(Pair("active", c.second->CanConnect()));
        coinInfo.push_back(Pair("swap_supported", c.second->IsSwapSupported()));
        auto config = c.second->config;
        coinInfo.push_back(Pair("rpchost", config.host));
        coinInfo.push_back(Pair("rpcport", config.port));
        UniValue errors(UniValue::VARR);
        for (std::string error : c.second->errors) {
            errors.push_back(error);
        }
        coinInfo.push_back(Pair("errors", errors));
        coins.push_back(coinInfo);
    }
    result.push_back(Pair("coins", coins));

    return result;
}

//TODO: for testing, remove later
UniValue createredeemscript(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() == 0 || params.size() > 1) {
        throw std::runtime_error(
                "createredeemscript\n"
                "Create atomic swap redeem script\n");
    }

    EnsureWalletIsUnlocked();

    int64_t locktime = GetAdjustedTime() + 48 * 60 * 60;
    std::vector<unsigned char> secret, secretHash;
    GenerateSecret(secret, secretHash);

    CTxDestination addr = DecodeDestination(params[0].get_str());
    CKeyID dest = boost::get<CKeyID>(addr);
    if (!pwalletMain->IsLocked())
        pwalletMain->TopUpKeyPool();

    // Generate a new key that is added to wallet
    CPubKey newKey;
    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    pwalletMain->LearnRelatedScripts(newKey, OUTPUT_TYPE_LEGACY);
    CTxDestination refundDest = GetDestinationForKey(newKey, OUTPUT_TYPE_LEGACY);
    CKeyID refundAddress = boost::get<CKeyID>(refundDest);

    CScript contractRedeemscript = CreateAtomicSwapRedeemscript(refundAddress, dest, locktime, secretHash);

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("hex", HexStr(contractRedeemscript.begin(), contractRedeemscript.end())));
    result.push_back(Pair("script", contractRedeemscript.ToString()));
    result.push_back(Pair("secret", HexStr(secret.begin(), secret.end())));
    result.push_back(Pair("secrethhash", HexStr(secretHash.begin(), secretHash.end())));
    return result;
}

//TODO: add swap redeem sig and test script interpretation