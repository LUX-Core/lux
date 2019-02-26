#include "rpcserver.h"

#include "lux/storagecontroller.h"
//#include "script/script_error.h"
//#include "script/sign.h"
//#include "version.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <boost/assign/list_of.hpp>

extern UniValue createrawtransaction(UniValue const & params, bool fHelp);
extern UniValue fundrawtransaction(UniValue const & params, bool fHelp);
extern UniValue signrawtransaction(UniValue const & params, bool fHelp);
extern UniValue sendrawtransaction(UniValue const & params, bool fHelp);

UniValue AssemblyNewTx(UniValue params, CKeyID const & changeAddress = CKeyID())
{
    UniValue created = createrawtransaction(params, false);

    UniValue fundsparams = fundrawtransaction(created, false);

    UniValue signparams(UniValue::VARR);
    signparams.push_back(fundsparams["hex"]);
    UniValue signedTxObj = signrawtransaction(signparams, false);

    UniValue sendparams(UniValue::VARR);
    sendparams.push_back(signedTxObj["hex"]);
    UniValue sent = sendrawtransaction(sendparams, false);
    return sent;
}

UniValue dfscreaterawordertx(UniValue const & params, bool fHelp)
{
    if (fHelp || params.size() != 2)
        throw std::runtime_error(
                "dfscreaterawordertx [{\"txid\":\"id\",\"vout\":n},...] {address, ...}\n"
                        "\nArguments:\n"
                        "1. \"transactions\"        (string, required) A json array of json objects\n"
                        "     [\n"
                        "       {\n"
                        "         \"txid\":\"id\",  (string, required) The transaction id\n"
                        "         \"vout\":n        (numeric, required) The output number\n"
                        "         \"sequence\":n    (numeric, optional) The sequence number\n"
                        "       }\n"
                        "       ,...\n"
                        "     ]\n"
                        "address\": P2PKH           (string, required)\n"
                        "\nResult:\n"
                        "\"hex\"             (string) The transaction hash in hex\n"
        );

#ifdef ENABLE_WALLET
//    LOCK2(cs_main, wallet->cs_wallet);
#else
    LOCK(cs_main);
#endif

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VARR)(UniValue::VOBJ), true);
    if (params[0].isNull() || params[1].isNull())
    {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameters, arguments 1 and 2 must be non-null");
    }
    UniValue obj = params[1].get_obj();


    std::string destBase58 = params[0].get_str();
    CTxDestination dest = DecodeDestination(destBase58);

    StorageOrder order;
    CDataStream ss(STORAGE_TX_PREFIX, SER_NETWORK, PROTOCOL_VERSION);
    ss << static_cast<unsigned char>(StorageTxTypes::Announce)
       << order.GetHash()
       << ToByteVector(GetScriptForDestination(dest));

    CScript script;
    script << OP_RETURN << ToByteVector(ss);

    CMutableTransaction orderTx;
    orderTx.vin.resize(1);
    orderTx.vin[0].prevout.SetNull();
    orderTx.vin[0].scriptSig = script;
    orderTx.vout.resize(1);
    orderTx.vout[0].nValue = 0;

    UniValue newparams(UniValue::VARR);
    newparams.push_back(params[0]);
    //newparams.push_back(vouts);

    return AssemblyNewTx(newparams);
}
