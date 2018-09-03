// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUX_BLOCKCHAINCLIENT_H
#define LUX_BLOCKCHAINCLIENT_H

#include <util.h>
#include <univalue/univalue.h>

#include <string>

/**
 * @class CClientConnectionError
 * @brief Thrown when bitcoin client cannot connect to the RPC server
 * @see CBitcoinClient
 */
class CClientConnectionError : public std::runtime_error {
public:
    explicit inline CClientConnectionError(const std::string& msg) : std::runtime_error(msg) {}
};

/**
 * @class CAbstractBlockchainClient
 * @brief Abstract blockchain client for performing RPC requests
 */
class CAbstractBlockchainClient {

public:
    /**
     * @brief CAbstractBlockchainClient ctor
     * @param conf abstract blockchain configuration
     * @see ReadLuxGateConfigFile()
     */
    CAbstractBlockchainClient(const BlockchainConfig& conf) : ticker(conf.ticker), config(conf) {}
    virtual ~CAbstractBlockchainClient() {};

    /**
     * @brief Abstract connectivity checking
     * @return true if can connect to blockchain via RPC using credentials provided in BlockchainConfig
     */
    virtual bool CanConnect() = 0;

    /**
     * @brief Abstract swap support checking
     *
     * Swap support is mainly defined by supporting CHECKLOCKTIMEVERIFY opcode, but non-bitcoin derivatives
     * can provide their own swap implementation and swap support checking
     *
     * @return true if atomic swap is supported
     */
    virtual bool IsSwapSupported() = 0; // Mainly affected by BIP65 support

    /**
     * @brief Call abstract blockchain RPC and return response
     * @param strMethod RPC method name
     * @param params UniValue representing JSON object of RPC method's parameters
     * @return UniValue with JSON representation of response
     */
    virtual UniValue CallRPC(const std::string& strMethod, const UniValue& params) = 0;

    const std::string ticker;
    const BlockchainConfig config;

    /**
     * Should contain errors from the last CallRPC call, if any
     */
    std::vector<std::string> errors;
};

/**
 * Map of blockchain clients, where key is a ticker
 */
extern std::map<std::string, std::shared_ptr<CAbstractBlockchainClient>> blockchainClientPool;

/**
 * @class CBitcoinClient
 * @brief Bitcoin RPC client implementation
 *
 * This class provides functions for calling Bitcoin RPC methods and
 * checking atomic swap support on a locally running node
 */
class CBitcoinClient : public CAbstractBlockchainClient {
public:
    CBitcoinClient(const BlockchainConfig& conf) : CAbstractBlockchainClient(conf) {};
    virtual ~CBitcoinClient() {};

    /**
     * @brief Checks if it is possible to connect to local bitcoin node
     * @return true if can connect to blockchain via RPC using credentials provided in BlockchainConfig
     *
     * It checks connectivity by trying to call help RPC method on a node
     *
     * @note All possible exceptions CallRPC may throw are hanlded
     */
    virtual bool CanConnect() override;

    /**
     * @brief checks if locally running bitcoin node supports atomic swaps
     * @return true if atomic swaps are supported
     *
     * It checks whether atomic swaps are supported by checking node's client version.
     * CLTV was introduced in bitcoin 0.10.4, so this method checks if locally running
     * bitcoin node is running equal or higher version
     *
     * @note All possible exceptions CallRPC may throw are hanlded
     */
    virtual bool IsSwapSupported() override;

    /**
     * @brief Call Bitcoin RPC method
     * @param strMethod Bitcoin RPC method name
     * @param params UniValue representing JSON object of RPC method's parameters
     * @return UniValue representing JSON object of RPC response
     * @throws CClientConnectionError if it cannot connect to RPC server
     * @throws runtime_error if response is not authorized
     * @throws runtime_error if method exists, it wasn't internal server error and request is valid, but error code >= 400 is returned
     * @throws runtime_error if response is empty
     * @throws runtime_error if failed to parse JSON response
     * @throws runtime_error if JSON response is parsed but the root object is empty
     */
    virtual UniValue CallRPC(const std::string& strMethod, const UniValue& params) override;

    const std::string ticker = "BTC";

private:
    /**
     * CLTV is supported from 0.10.4
     * Client version number if calculated by the following formula:
     *
     * 1000000 * CLIENT_VERSION_MAJOR +
     * 10000   * CLIENT_VERSION_MINOR +
     * 100     * CLIENT_VERSION_REVISION +
     * 1       * CLIENT_VERSION_BUILD
     */
    const int nCLTVSupportedFrom = 100400;
};

#endif //LUX_BLOCKCHAINCLIENT_H
