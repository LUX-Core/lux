// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUX_BLOCKCHAINCLIENT_H
#define LUX_BLOCKCHAINCLIENT_H

#include <util.h>
#include <univalue/univalue.h>

#include <string>

class CClientConnectionError : public std::runtime_error {
public:
    explicit inline CClientConnectionError(const std::string& msg) : std::runtime_error(msg) {}
};

class CAbstractBlockchainClient {

public:
    CAbstractBlockchainClient(const BlockchainConfig& conf) : config(conf), ticker(conf.ticker) {}
    virtual ~CAbstractBlockchainClient() {};

    virtual bool CanConnect() = 0;
    virtual bool IsSwapSupported() = 0; // Mainly affected by BIP65 support
    virtual UniValue CallRPC(const std::string& strMethod, const UniValue& params) = 0;

    const std::string ticker;
    const BlockchainConfig config;
};

extern std::map<std::string, std::shared_ptr<CAbstractBlockchainClient>> blockchainClientPool;

class CBitcoinClient : public CAbstractBlockchainClient {
public:
    CBitcoinClient(const BlockchainConfig& conf) : CAbstractBlockchainClient(conf) {};
    virtual ~CBitcoinClient() {};

    virtual bool CanConnect() override;
    virtual bool IsSwapSupported() override;
    virtual UniValue CallRPC(const std::string& strMethod, const UniValue& params) override;

    const std::string ticker = "BTC";

private:
    const int nCLTVSupportedFrom = 100400;
};

#endif //LUX_BLOCKCHAINCLIENT_H
