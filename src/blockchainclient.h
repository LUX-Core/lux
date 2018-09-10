// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUX_BLOCKCHAINCLIENT_H
#define LUX_BLOCKCHAINCLIENT_H

#include <amount.h>
#include <clientversion.h>
#include <util.h>
#include <univalue/univalue.h>
#include <version.h>

#include <string>

class CClientConnectionError;
class CAbstractBlockchainClient;
class CBitcoinClient;
class CPubKey;

typedef std::string Ticker;
typedef std::shared_ptr<CAbstractBlockchainClient> ClientPtr;


/**
 * Map of blockchain clients, where key is a ticker
 */
extern std::map<Ticker, ClientPtr> blockchainClientPool;

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
 * @brief Represents transactions parameter in createrawtransaction RPC call
 * This is like COutPoint, but with a string as a tx id instead of uin256 for more compatibility
 */
struct TransactionInputs {
    /** Hash of the input transaction */
    std::string txid;
    /** Index of the transaction output to spent*/
    int vout;
};

/**
 * @brief Parameters for the createrawtransaction RPC call
 */
struct CreateTransactionParams {
    /** List of transactions to spend */
    std::vector<TransactionInputs> transactions;
    /**
     * Where to send and how much
     *
     * key - blockchain address
     *
     * value - amount of coins to send
     */
    std::map<std::string, CAmount> addresses;
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

    /**
     * @brief Retrieves blockchain client version as int
     * @return Blockchain client version
     */
    virtual int GetClientVersion() = 0;

    /**
     * @brief Retrieves blockchain protocol version
     * @return Blockchain protocol version
     */
    virtual int GetProtocolVersion() = 0;

    /**
     * @brief Returns current block count
     * @return Current block count
     */
    virtual int GetBlockCount() = 0;

    /**
     * @brief createrawtransaction RPC call wrapper
     * @param params Parameters for the RPC call
     * @return Hex encoded raw transaction
     */
    virtual std::string CreateRawTransaction(const CreateTransactionParams& params) = 0;

    /**
     * @brief Sign raw transaction
     * @param txHex Hex encoded raw transaction
     * @return Hex encoded signed raw transaction
     */
    virtual std::string SignRawTransaction(std::string txHex) = 0;

    /**
     * @brief Send signed raw transaction
     * @param txHex Hex encoded signed raw transaction
     * @return Hash of the created transaction
     */
    virtual std::string SendRawTransaction(std::string txHex) = 0;

    /**
     * @brief Decodes raw
     * @param txHex Hex encoded transaction
     * @return Decoded transaction in the UniValue format
     */
    virtual UniValue DecodeRawTransaction(std::string txHex) = 0;

    /**
     * @brief Send coins to the address
     * @param addr Blockchain address where to send coins
     * @param nAmount Amount of coins to send
     * @return Hash of the created transaction
     */
    virtual std::string SendToAddress(std::string addr, CAmount nAmount) = 0;

    /**
     * @brief Generates new address
     * @return newly generated address
     */
    virtual std::string GetNewAddress() = 0;

    /**
     * @brief Validates blockchain address by performing validateaddress RPC call
     * @param addr Address to validate
     * @return Is address valid for this blockchain or not
     */
    virtual bool IsValidAddress(std::string addr) = 0;

    /**
     * @brief Get hash of the address's public key
     * @param addr address to get pubkeyhash from
     * @return
     */
    virtual CPubKey GetPubKeyForAddress(std::string addr) = 0;

    const Ticker ticker;
    const BlockchainConfig config;

    /**
     * Should contain errors from the last CallRPC call, if any
     */
    std::vector<std::string> errors;
};



/**
 * @class CBitcoinClient
 * @brief Bitcoin RPC client implementation
 *
 * This class provides functions for calling Bitcoin RPC methods and
 * checking atomic swap support on a locally running node
 *
 * @note Every function except for CanConnect and IsSwapSupported may throw std::runtime_exception or CClientConnectionError.
 * See CallRPC() for more information.
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
     * @note All possible exceptions CallRPC may throw are handled
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
     * @note All possible exceptions CallRPC may throw are handled
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

    virtual int GetClientVersion() override;

    virtual int GetProtocolVersion() override;

    virtual int GetBlockCount() override;

    virtual std::string CreateRawTransaction(const CreateTransactionParams& params) override;

    virtual std::string SignRawTransaction(std::string txHex) override;

    virtual std::string SendRawTransaction(std::string txHex) override;

    virtual UniValue DecodeRawTransaction(std::string txHex) override;

    virtual std::string SendToAddress(std::string addr, CAmount nAmount) override;

    virtual bool IsValidAddress(std::string addr) override;

    /**
     * @brief Generates new address
     * The generated address is always legacy (after segwit) or default (before segwit)
     * @return newly generated address
     */
    virtual std::string GetNewAddress() override;

    virtual CPubKey GetPubKeyForAddress(std::string addr) override;

    const Ticker ticker = "BTC";

    int nCurrentClientVersion;

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

/**
 * @class CLuxClient
 * @brief Lux client implementation
 *
 * This class provides CAbstractBlockchainClient interface, but uses core function calls instead of RPC calls.
 * @note Every function except for CanConnect and IsSwapSupported may throw std::runtime_exception or CClientConnectionError from
 * RPC call handler.
 */
class CLuxClient : public CAbstractBlockchainClient {
public:
    CLuxClient(const BlockchainConfig& conf) : CAbstractBlockchainClient(conf) {};
    virtual ~CLuxClient() {};

    /**
     * @brief Checks if it is possible to connect to local Lux node
     * @return true
     */
    virtual bool CanConnect() override { return true; }

    /**
     * @brief checks if locally running bitcoin node supports atomic swaps
     * @return true
     */
    virtual bool IsSwapSupported() override { return true; }

    virtual UniValue CallRPC(const std::string& strMethod, const UniValue& params) override { return NullUniValue; }

    virtual int GetClientVersion() override { return CLIENT_VERSION; }

    virtual int GetProtocolVersion() override { return PROTOCOL_VERSION; }

    virtual int GetBlockCount() override;

    virtual std::string CreateRawTransaction(const CreateTransactionParams& params) override;

    virtual std::string SignRawTransaction(std::string txHex) override;

    virtual std::string SendRawTransaction(std::string txHex) override;

    virtual UniValue DecodeRawTransaction(std::string txHex) override;

    virtual std::string SendToAddress(std::string addr, CAmount nAmount) override;

    /**
     * @brief Generates new address
     * The generated address is always legacy (after segwit) or default (before segwit)
     * @return newly generated address
     */
    virtual std::string GetNewAddress() override;

    virtual bool IsValidAddress(std::string addr) override;

    virtual CPubKey GetPubKeyForAddress(std::string addr) override;

    const Ticker ticker = "LUX";
};

#endif //LUX_BLOCKCHAINCLIENT_H
