#include "rpcserver.h"

#include "core_io.h"
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
    if (fHelp || params.size() != 4)
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
                        "2. \"orders hash\":hash    (string, required) The hash of announces order\n"
                        "3. \"proposal hash\":hash  (string, required) The hash of proposal\n"
                        "4. \"address\": P2PKH      (string, required)\n"
                        "\nResult:\n"
                        "\"hex\"             (string) The transaction hash in hex\n"
        );

#ifdef ENABLE_WALLET
//    LOCK2(cs_main, wallet->cs_wallet);
#else
    LOCK(cs_main);
#endif

    RPCTypeCheck(params, boost::assign::list_of(UniValue::VARR)(UniValue::VOBJ), true);

    uint256 orderHash = uint256{params[1].get_str()};

    const StorageOrder *order = storageController->GetAnnounce(orderHash);

    if (!order) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameters, order not found");
    }

    uint256 proposalHash = uint256{params[2].get_str()};

    auto metadata = storageController->GetMetadata(orderHash, proposalHash);

    std::string destBase58 = params[3].get_str();
    CTxDestination dest = DecodeDestination(destBase58);

    DecryptionKeys keys = metadata.second;
    uint256 merkleRootHash = metadata.first;
    CDataStream ss(STORAGE_TX_PREFIX, SER_NETWORK, PROTOCOL_VERSION);
    ss << static_cast<unsigned char>(StorageTxTypes::Announce)
       << order->fileURI
       << order->filename
       << order->fileSize
       << keys
       << merkleRootHash
       << order->maxGap
       << order->maxRate
       << order->storageUntil;

    CScript scriptMarkerOut = CScript() << OP_RETURN << ToByteVector(ss);
    CScript scriptMerkleProofOut = CScript() << ToByteVector(merkleRootHash) << OP_MERKLE_PATH;

    CAmount amount = std::difftime(order->storageUntil, std::time(nullptr)) * order->maxRate;
    UniValue vouts(UniValue::VOBJ);
    vouts.push_back(Pair(EncodeDestination(scriptMarkerOut), ValueFromAmount(0)));
    vouts.push_back(Pair(EncodeDestination(scriptMerkleProofOut), ValueFromAmount(amount)));


    UniValue newparams(UniValue::VARR);
    newparams.push_back(params[0]);
    newparams.push_back(vouts);

    return AssemblyNewTx(newparams);
}
