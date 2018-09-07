// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchainclient.h>

#include <core_io.h>
#include <rpcprotocol.h>
#include <rpcserver.h>
#include <utilstrencodings.h>

#include <boost/algorithm/string.hpp>

// This blockchainclient used for luxgate, and its important.
// Do not adjust if you don't understand the consequences.
std::map<std::string, ClientPtr> blockchainClientPool;

bool CBitcoinClient::CanConnect() {
    try {
        CallRPC("help", NullUniValue);
    } catch (CClientConnectionError) {
        return false;
    } catch (const std::runtime_error& e) {
        // We connected to the server, but something failed after that
        // So basically we can connect, but warn about problems after connection
        LogPrintf("CBitcoinClient::CanConnect(): warning: %s", e.what());
        return true;
    }

    return true;
}

bool CBitcoinClient::IsSwapSupported() {
    try {
        int version = GetClientVersion();

        UniValue response = CallRPC("getmininginfo", NullUniValue);
        UniValue miningInfo = find_value(response, "result");
        if (miningInfo.isNull() || miningInfo.empty())
            return version >= nCLTVSupportedFrom;

        UniValue testnet = find_value(miningInfo, "testnet"); //TODO: only testnet for now
        bool isTestnet = testnet.empty() ? false : testnet.get_bool();
        std::string networkName = find_value(miningInfo.get_obj(), "chain").get_str();
        bool testNetworkName = boost::algorithm::to_lower_copy(networkName).find("test") != std::string::npos;

        return version >= nCLTVSupportedFrom && (isTestnet || testNetworkName);
    } catch (const std::runtime_error& e) {
        return false;
    }
}

UniValue CBitcoinClient::CallRPC(const std::string& strMethod, const UniValue& params)
{
    errors.clear();
    // Connect to localhost
    bool fUseSSL = GetBoolArg("-rpcssl", false);
    boost::asio::io_service io_service;
    boost::asio::ssl::context context(io_service, boost::asio::ssl::context::sslv23);
    context.set_options(boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::no_sslv3);
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> sslStream(io_service, context);
    SSLIOStreamDevice<boost::asio::ip::tcp> d(sslStream, fUseSSL);
    boost::iostreams::stream<SSLIOStreamDevice<boost::asio::ip::tcp> > stream(d);

    const bool fConnected = d.connect(config.host, itostr(config.port));
    if (!fConnected) {
        errors.push_back("couldn't connect to server");
        throw CClientConnectionError("couldn't connect to server");
    }

    std::string strRPCUserColonPass;
    if (config.rpcpassword != "") {
        strRPCUserColonPass = config.rpcuser + ":" + config.rpcpassword;
    }

    // HTTP basic authentication
    std::map<std::string, std::string> mapRequestHeaders;
    mapRequestHeaders["Authorization"] = std::string("Basic ") + EncodeBase64(strRPCUserColonPass);

    // Send request
    std:: string strRequest = JSONRPCRequest(strMethod, params, 1);
    std::string strPost = HTTPPost(strRequest, mapRequestHeaders);
    stream << strPost << std::flush;

    // Receive HTTP reply status
    int nProto = 0;
    int nStatus = ReadHTTPStatus(stream, nProto);

    // Receive HTTP reply message headers and body
    std::map<std::string, std::string> mapHeaders;
    std::string strReply;
    ReadHTTPMessage(stream, mapHeaders, strReply, nProto, std::numeric_limits<size_t>::max());

    if (nStatus == HTTP_UNAUTHORIZED) {
        errors.push_back("incorrect rpcuser or rpcpassword (authorization failed)");
        throw std::runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    }
    else if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR) {
        errors.push_back(strprintf("server returned HTTP error %d", nStatus));
        throw std::runtime_error(strprintf("server returned HTTP error %d", nStatus));
    } else if (strReply.empty()) {
        errors.push_back("no response from server");
        throw std::runtime_error("no response from server");
    }

    // Parse reply
    UniValue valReply(UniValue::VSTR);
    if (!valReply.read(strReply)) {
        errors.push_back("couldn't parse reply from server");
        throw std::runtime_error("couldn't parse reply from server");
    }

    try {
        const UniValue& reply = valReply.get_obj();
        if (reply.empty()) {
            errors.push_back("expected reply to have result, error and id properties");
            throw std::runtime_error("expected reply to have result, error and id properties");
        }

        return reply;
    } catch (std::runtime_error) {
        throw std::runtime_error("expected reply to have result, error and id properties");
    }
}

int CBitcoinClient::GetClientVersion() {
    nCurrentClientVersion = 0;
    UniValue response = CallRPC("getnetworkinfo", NullUniValue);
    UniValue networkInfo = find_value(response, "result");
    if (networkInfo.isNull() || networkInfo.empty())
        return nCurrentClientVersion;

    nCurrentClientVersion = find_value(networkInfo, "version").get_int();
    return nCurrentClientVersion;
}

int CBitcoinClient::GetBlockCount() {
    UniValue response = CallRPC("getblockcount", NullUniValue);
    return response.get_int();
}

std::string CBitcoinClient::CreateRawTransaction(const CreateTransactionParams& params) {
    UniValue rpcParams(UniValue::VARR);
    UniValue transactions(UniValue::VARR);
    for (auto transaction : params.transactions) {
        UniValue tr(UniValue::VOBJ);
        tr.push_back(Pair("txid", transaction.txid));
        tr.push_back(Pair("vout", transaction.vout));
        transactions.push_back(tr);
    }
    rpcParams.push_back(transactions);

    UniValue sendTo(UniValue::VOBJ);
    for (auto sendPair : params.addresses) {
        sendTo.push_back(Pair(sendPair.first, ValueFromAmount(sendPair.second)));
    }

    rpcParams.push_back(sendTo);

    UniValue response = CallRPC("createrawtransaction", rpcParams);
    return response.get_str();
}

std::string CBitcoinClient::SignRawTransaction(std::string txHex) {
    UniValue param(UniValue::VSTR);
    param.push_back(txHex);

    UniValue response = CallRPC("signrawtransaction", param);
    return find_value(response, "hex").get_str();
}

std::string CBitcoinClient::SendRawTransaction(std::string txHex) {
    UniValue param(UniValue::VSTR);
    param.setStr(txHex);

    UniValue response = CallRPC("sendrawtransaction", param);
    return response.get_str();
};

std::string CBitcoinClient::SendToAddress(std::string addr, CAmount nAmount) {
    UniValue params(UniValue::VARR);
    params.push_back(addr);
    params.push_back(ValueFromAmount(nAmount));

    UniValue response = CallRPC("sendtoaddress", params);
    return response.get_str();
}

std::string CBitcoinClient::GetNewAddress() {
    int version = 0;
    if (nCurrentClientVersion == 0)
        version = GetClientVersion();

    UniValue params(UniValue::VARR);
    params.push_back(""); //default account
    // From btc 0.16 getnewaddress has an optional address type parameter, but it defaults to p2sh-segwit
    if (version >= 160000) {
        // Force use legacy
        params.push_back("legacy"); //address type
    }

    UniValue response = CallRPC("getnewaddress", params);
    return response.get_str();
}

bool CBitcoinClient::IsValidAddress(std::string addr) {
    UniValue params(UniValue::VARR);
    params.push_back(addr);

    UniValue response = CallRPC("validateaddress", params);
    return find_value(response, "isvalid").get_bool();
}

int CLuxClient::GetBlockCount() {
    UniValue response = getblockcount(NullUniValue, false);
    return response.get_int();
}

std::string CLuxClient::CreateRawTransaction(const CreateTransactionParams& params) {
    UniValue rpcParams(UniValue::VARR);
    UniValue transactions(UniValue::VARR);
    for (auto transaction : params.transactions) {
        UniValue tr(UniValue::VOBJ);
        tr.push_back(Pair("txid", transaction.txid));
        tr.push_back(Pair("vout", transaction.vout));
        transactions.push_back(tr);
    }
    rpcParams.push_back(transactions);

    UniValue sendTo(UniValue::VOBJ);
    for (auto sendPair : params.addresses) {
        sendTo.push_back(Pair(sendPair.first, ValueFromAmount(sendPair.second)));
    }

    rpcParams.push_back(sendTo);

    UniValue response = createrawtransaction(rpcParams, false);
    return response.get_str();
}

std::string CLuxClient::SignRawTransaction(std::string txHex) {
    UniValue params(UniValue::VARR);
    params.push_back(txHex);

    // Direct RPC handler call
    UniValue response = signrawtransaction(params, false);
    return find_value(response, "hex").get_str();
}

std::string CLuxClient::SendRawTransaction(std::string txHex) {
    UniValue param(UniValue::VSTR);
    param.push_back(txHex);

    UniValue response = sendrawtransaction(param, false);
    return response.get_str();
};

std::string CLuxClient::SendToAddress(std::string addr, CAmount nAmount) {
    UniValue params(UniValue::VARR);
    params.push_back(addr);
    params.push_back(ValueFromAmount(nAmount));

    UniValue response = sendtoaddress(params, false);
    return response.get_str();
}

std::string CLuxClient::GetNewAddress() {
    UniValue params(UniValue::VARR);
    params.push_back("");
    params.push_back("legacy");

    UniValue response = getnewaddress(params, false);
    return response.get_str();
}

bool CLuxClient::IsValidAddress(std::string addr) {
    UniValue params(UniValue::VARR);
    params.push_back(addr);

    // we can use IsValidDestination here, but let's keep it consistent
    UniValue response = validateaddress(params, false);
    return find_value(response, "isvalid").get_bool();
}
