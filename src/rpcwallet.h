#ifndef RPCWALLET_H
#define RPCWALLET_H

#include "wallet.h"
#include "amount.h"
#include "base58.h"
#include "core_io.h"
#include "consensus/validation.h"
#include "init.h"
#include "net.h"
#include "netbase.h"
#include "rpcserver.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "walletdb.h"
#include "stake.h"
#include "coincontrol.h"
#include "consensus/validation.h"
#include "rbf.h"
#include <stdint.h>
#include <string>
#include "miner.h"

#include "libdevcore/CommonData.h"

#include "univalue/univalue.h"
#include "rpcutil.h"
#include <boost/assign/list_of.hpp>

struct CUpdatedBlock
{
    uint256 hash;
    int height;
};

static std::mutex cs_blockchange;
static std::condition_variable cond_blockchange;
static CUpdatedBlock latestblock;

static CCriticalSection cs_nWalletUnlockTime;

string AccountFromValue(const UniValue& value);
void WalletTxToJSON(const CWalletTx& wtx, UniValue& entry);
void EnsureWalletIsUnlocked();
std::string HelpRequiringPassphrase();
bool EnsureWalletIsAvailable(bool avoidException);
UniValue executionResultToJSON(const dev::eth::ExecutionResult& exRes);
UniValue transactionReceiptToJSON(const dev::eth::TransactionReceipt& txRec);
void transactionReceiptInfoToJSON(const TransactionReceiptInfo& resExec, UniValue& entry);
void assignJSON(UniValue& entry, const TransactionReceiptInfo& resExec);
void assignJSON(UniValue& logEntry, const dev::eth::LogEntry& log, bool includeAddress);

size_t parseUInt(const UniValue& val, size_t defaultVal);
size_t parseBlockHeight(const UniValue& val);
size_t parseBlockHeight(const UniValue& val, size_t defaultVal);
void parseParam(const UniValue& val, std::vector<dev::h160> &h160s);
void parseParam(const UniValue& val, std::set<dev::h160> &h160s);
void parseParam(const UniValue& val, std::vector<boost::optional<dev::h256>> &h256s);
dev::h160 parseParamH160(const UniValue& val);

UniValue sendtocontract(const UniValue& params, bool fHelp);

#endif
