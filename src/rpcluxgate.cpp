// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <atomicswap.h>
#include <base58.h>
#include <blockchainclient.h>
#include <core_io.h>
#include <luxgate.h>
#include <main.h>
#include <rpcserver.h>
#include <sync.h>
#include <timedata.h>
#include <univalue/univalue.h>
#include <util.h>
#include <utilstrencodings.h>
#include <wallet.h>

bool TickerIsActive(Ticker ticker) {
    
    if (ticker == "LUX")
        return true;
    
     for (auto const &c : blockchainClientPool) { 
         if (c.first == ticker && c.second->CanConnect() && c.second->IsSwapSupported()) {
            return true;
         }
     }

     return false;
}

bool OneOfTheTickersIsLUX(Ticker t1, Ticker t2) {
    return (t1 == "LUX" || t2 == "LUX") && (t1 != t2);
}

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

    Ticker base = params[0].get_str();
    Ticker rel = params[1].get_str();

    if (!OneOfTheTickersIsLUX(base, rel)) 
        throw JSONRPCError(RPC_LUXGATE_ERROR, "One of the tickers must be LUX");
    
    if (!TickerIsActive(base)) {
        throw JSONRPCError(RPC_LUXGATE_ERROR, "Ticker is not active: " + base);
    }
    if (!TickerIsActive(rel)) {
        throw JSONRPCError(RPC_LUXGATE_ERROR, "Ticker is not active: " + rel);
    }

    
    CAmount baseAmount = AmountFromValue(UniValue(UniValue::VNUM, params[2].get_str()));
    CAmount relAmount =  AmountFromValue(UniValue(UniValue::VNUM, params[3].get_str()));

    if (baseAmount <= 0 || relAmount <= 0)
        throw JSONRPCError(RPC_LUXGATE_ERROR, "Invalid amount");
    

    //LOCK(cs_vNodes);
    for (CNode *pnode : vNodes) {
        auto order = std::make_shared<COrder>(base, rel, baseAmount, relAmount);
        CAddress addr;
        if (GetLocal(addr, &pnode->addr))
            order->SetSender(addr);
            pnode->PushMessage("createorder", *order);
    }

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

UniValue listorderbook(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() > 0 ) {
         throw std::runtime_error(
             "listorderbook\n"
             "List orderbook\n");
    }

    UniValue result(UniValue::VOBJ);
    UniValue orders(UniValue::VARR);
    for (auto const &entry : orderbook) {
        auto o = entry.second;
        UniValue orderEntry(UniValue::VOBJ);
        orderEntry.push_back(Pair("base", o->Base()));
        orderEntry.push_back(Pair("rel", o->Rel()));
        orderEntry.push_back(Pair("base_amount", o->BaseAmount()));
        orderEntry.push_back(Pair("rel_amount", o->RelAmount()));
        orderEntry.push_back(Pair("sender", o->Sender().ToStringIPPort()));
        orders.push_back(orders);
    }
    result.push_back(Pair("orders", orders));

    return result;
}

UniValue listactiveorders(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() > 0 ) {
         throw std::runtime_error(
             "listactiveorders\n"
             "List active orders\n");
    }

    UniValue result(UniValue::VOBJ);
    UniValue orders(UniValue::VARR);
    for (auto const &entry : activeOrders) {
        auto o = entry.second;
        UniValue orderEntry(UniValue::VOBJ);
        orderEntry.push_back(Pair("base", o->Base()));
        orderEntry.push_back(Pair("rel", o->Rel()));
        orderEntry.push_back(Pair("base_amount", o->BaseAmount()));
        orderEntry.push_back(Pair("rel_amount", o->RelAmount()));
        orderEntry.push_back(Pair("sender", o->Sender().ToStringIPPort()));
        orders.push_back(orders);
    }
    result.push_back(Pair("orders", orders));

    return result;
}

UniValue createswaptransaction(const UniValue& params, bool fHelp) {
    if (fHelp || params.size() != 2) {
        throw std::runtime_error(
                "createswaptransaction\n"
                "Create atomic swap transaction\n"
                "1. address - receiver\n"
                "2. amount - amount of coins to send\n");
    }

    CTxDestination addr = DecodeDestination(params[0].get_str());
    if (addr.which() != 1) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Participant address is not P2PKH");
    }

    CAmount nAmount = AmountFromValue(UniValue(UniValue::VNUM, params[1].get_str()));
    if (nAmount <= 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    }

    CKeyID dest = boost::get<CKeyID>(addr);
    int64_t locktime = GetAdjustedTime() + 48 * 60 * 60;
    std::vector<unsigned char> secret, secretHash;
    CWalletTx wtx;
    CMutableTransaction refundTx;
    CAmount nFeeRequired;
    CAmount nRefundFeeRequired;
    CScript contractRedeemScript;
    std::string strError;

    EnsureWalletIsUnlocked();

    {
        LOCK2(cs_main, pwalletMain->cs_wallet);

        if (!CreateSwapTransaction(pwalletMain, dest, nAmount, locktime, wtx, contractRedeemScript, nFeeRequired, secret, secretHash, strError))
            throw JSONRPCError(RPC_WALLET_ERROR, strError);

        if (!CreateRefundTransaction(pwalletMain, contractRedeemScript, wtx, refundTx, nRefundFeeRequired, strError))
            throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    UniValue result(UniValue::VOBJ);
    result.push_back(Pair("redeemscripthex", HexStr(contractRedeemScript.begin(), contractRedeemScript.end())));
    result.push_back(Pair("redeemscript", (contractRedeemScript.ToString())));
    result.push_back(Pair("redeemtx", EncodeHexTx(wtx)));
    result.push_back(Pair("refundtx", EncodeHexTx(refundTx)));
    result.push_back(Pair("feerequired", ValueFromAmount(nFeeRequired)));
    result.push_back(Pair("refundfeerequired", ValueFromAmount(nRefundFeeRequired)));
    result.push_back(Pair("secret", HexStr(secret.begin(), secret.end())));
    result.push_back(Pair("secrethash", HexStr(secretHash.begin(), secretHash.end())));

    return result;
}


