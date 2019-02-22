// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPCPROTOCOL_H
#define BITCOIN_RPCPROTOCOL_H

#include <list>
#include <map>
#include <stdint.h>
#include <string>
#include <boost/filesystem.hpp>

#include "univalue/univalue.h"

//! HTTP status codes
enum HTTPStatusCode {
    HTTP_OK                    = 200,
    HTTP_BAD_REQUEST           = 400,
    HTTP_UNAUTHORIZED          = 401,
    HTTP_FORBIDDEN             = 403,
    HTTP_NOT_FOUND             = 404,
    HTTP_BAD_METHOD            = 405,
    HTTP_INTERNAL_SERVER_ERROR = 500,
    HTTP_SERVICE_UNAVAILABLE   = 503,
};

//! LUX RPC error codes
enum RPCErrorCode {
    //! Standard JSON-RPC 2.0 errors
    RPC_INVALID_REQUEST        = -32600,
    RPC_METHOD_NOT_FOUND       = -32601,
    RPC_INVALID_PARAMS         = -32602,
    RPC_INTERNAL_ERROR         = -32603,
    RPC_PARSE_ERROR            = -32700,

    //! General application defined errors
    RPC_MISC_ERROR                  = -1,        //! std::exception thrown in command handling
    RPC_FORBIDDEN_BY_SAFE_MODE      = -2,        //! Server is in safe mode, and command is not allowed in safe mode
    RPC_TYPE_ERROR                  = -3,        //! Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY      = -5,        //! Invalid address or key
    RPC_OUT_OF_MEMORY               = -7,        //! Ran out of memory during operation
    RPC_INVALID_PARAMETER           = -8,        //! Invalid, missing or duplicate parameter
    RPC_DATABASE_ERROR              = -20,       //! Database error
    RPC_DESERIALIZATION_ERROR       = -22,       //! Error parsing or validating structure in raw format
    RPC_VERIFY_ERROR                = -25,       //! General error during transaction or block submission
    RPC_VERIFY_REJECTED             = -26,       //! Transaction or block was rejected by network rules
    RPC_VERIFY_ALREADY_IN_CHAIN     = -27,       //! Transaction already in chain
    RPC_IN_WARMUP                   = -28,       //! Client still warming up

    //! Aliases for backward compatibility
    RPC_TRANSACTION_ERROR = RPC_VERIFY_ERROR,
    RPC_TRANSACTION_REJECTED = RPC_VERIFY_REJECTED,
    RPC_TRANSACTION_ALREADY_IN_CHAIN = RPC_VERIFY_ALREADY_IN_CHAIN,

    //! P2P client errors
    RPC_CLIENT_NOT_CONNECTED        = -9,        //! LUX is not connected
    RPC_CLIENT_IN_INITIAL_DOWNLOAD  = -10,       //! Still downloading initial blocks
    RPC_CLIENT_NODE_ALREADY_ADDED   = -23,       //! Node is already added
    RPC_CLIENT_NODE_NOT_ADDED       = -24,       //! Node has not been added before
    RPC_CLIENT_NODE_NOT_CONNECTED   = -29, //! Node to disconnect not found in connected nodes
    RPC_CLIENT_INVALID_IP_OR_SUBNET = -30, //! Invalid IP/Subnet

    //! Wallet errors
    RPC_WALLET_ERROR                = -4,        //! Unspecified problem with wallet (key not found etc.)
    RPC_WALLET_INSUFFICIENT_FUNDS   = -6,        //! Not enough funds in wallet or account
    RPC_WALLET_INVALID_ACCOUNT_NAME = -11,       //! Invalid account name
    RPC_WALLET_KEYPOOL_RAN_OUT      = -12,       //! Keypool ran out, call keypoolrefill first
    RPC_WALLET_UNLOCK_NEEDED        = -13,       //! Enter the wallet passphrase with walletpassphrase first
    RPC_WALLET_PASSPHRASE_INCORRECT = -14,       //! The wallet passphrase entered was incorrect
    RPC_WALLET_WRONG_ENC_STATE      = -15,       //! Command given in wrong wallet encryption state (encrypting an encrypted wallet etc.)
    RPC_WALLET_ENCRYPTION_FAILED    = -16,       //! Failed to encrypt the wallet
    RPC_WALLET_ALREADY_UNLOCKED     = -17,       //! Wallet is already unlocked
};

std::string JSONRPCRequest(const std::string& strMethod, const UniValue& params, const UniValue& id);
UniValue JSONRPCReplyObj(const UniValue& result, const UniValue& error, const UniValue& id);
std::string JSONRPCReply(const UniValue& result, const UniValue& error, const UniValue& id);
UniValue JSONRPCError(int code, const std::string& message);

/** Get name of RPC authentication cookie file */
boost::filesystem::path GetAuthCookieFile();
/** Generate a new RPC authentication cookie and write it to disk */
bool GenerateAuthCookie(std::string *cookie_out);
/** Read the RPC authentication cookie from disk */
bool GetAuthCookie(std::string *cookie_out);
/** Delete RPC authentication cookie from disk */
void DeleteAuthCookie();

#endif // BITCOIN_RPCPROTOCOL_H
