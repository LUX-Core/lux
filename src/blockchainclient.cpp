// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <blockchainclient.h>

#include <rpcprotocol.h>
#include <utilstrencodings.h>

#include <boost/algorithm/string.hpp>

std::map<std::string, std::shared_ptr<CAbstractBlockchainClient>> blockchainClientPool;

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
        UniValue response = CallRPC("getnetworkinfo", NullUniValue);
        UniValue networkInfo = find_value(response, "result");
        if (networkInfo.isNull() || networkInfo.empty())
            return false;

        int version = find_value(networkInfo, "version").get_int();

        response = CallRPC("getmininginfo", NullUniValue);
        UniValue miningInfo = find_value(response, "result");
        if (networkInfo.isNull() || networkInfo.empty())
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
    // Connect to localhost
    bool fUseSSL = GetBoolArg("-rpcssl", false);
    boost::asio::io_service io_service;
    boost::asio::ssl::context context(io_service, boost::asio::ssl::context::sslv23);
    context.set_options(boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::no_sslv3);
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> sslStream(io_service, context);
    SSLIOStreamDevice<boost::asio::ip::tcp> d(sslStream, fUseSSL);
    boost::iostreams::stream<SSLIOStreamDevice<boost::asio::ip::tcp> > stream(d);

    const bool fConnected = d.connect(config.host, itostr(config.port));
    if (!fConnected)
        throw CClientConnectionError("couldn't connect to server");

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

    if (nStatus == HTTP_UNAUTHORIZED)
        throw std::runtime_error("incorrect rpcuser or rpcpassword (authorization failed)");
    else if (nStatus >= 400 && nStatus != HTTP_BAD_REQUEST && nStatus != HTTP_NOT_FOUND && nStatus != HTTP_INTERNAL_SERVER_ERROR)
        throw std::runtime_error(strprintf("server returned HTTP error %d", nStatus));
    else if (strReply.empty())
        throw std::runtime_error("no response from server");

    // Parse reply
    UniValue valReply(UniValue::VSTR);
    if (!valReply.read(strReply))
        throw std::runtime_error("couldn't parse reply from server");

    const UniValue& reply = valReply.get_obj();
    if (reply.empty())
        throw std::runtime_error("expected reply to have result, error and id properties");

    return reply;
}