// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchainclient.h>
#include <main.h>
#include <rpcserver.h>
#include <sync.h>
#include <univalue/univalue.h>
#include <util.h>


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