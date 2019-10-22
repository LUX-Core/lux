// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/lux-config.h"
#endif

#include "init.h"

#include "activemasternode.h"
#include "addrman.h"
#include "amount.h"
#include "chain.h"
#include "chainparams.h"
#include "checkpoints.h"
#include "compat/sanity.h"
#include "consensus/validation.h"
#include "httpserver.h"
#include "httprpc.h"
#include "key.h"
#include "luxcontrol.h"
#include "main.h"
#include "stake.h"
#include "masternodeconfig.h"
#include "masternode.h"
#include "miner.h"
#include "net.h"
#include "policy/policy.h"
#include "rpcserver.h"
#include "script/standard.h"
#include "script/sigcache.h"
#include "scheduler.h"
#include "spork.h"
#include "txdb.h"
#include "script/sigcache.h"
#include "txmempool.h"
#include "ui_interface.h"
#include "util.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"
#include "validationinterface.h"
#include "random.h"
#ifdef ENABLE_WALLET
#include "db.h"
#include "wallet.h"
#include "walletdb.h"
#include "miner.h"
#endif

#include <fstream>
#include <stdint.h>
#include <stdio.h>

#ifndef WIN32
#include <signal.h>
#endif

#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/function.hpp>
#include <boost/interprocess/sync/file_lock.hpp>
#include <boost/thread.hpp>
#include <openssl/crypto.h>

#if ENABLE_ZMQ
#include "zmq/zmqnotificationinterface.h"
#endif

using namespace boost;
using namespace std;

extern void ThreadSendAlert();
#ifdef ENABLE_WALLET
CWallet* pwalletMain = NULL;
//int nWalletBackups = 10;
#endif
bool fFeeEstimatesInitialized = false;
std::atomic<bool> fRestartRequested(false); // true: restart false: shutdown
unsigned int nMinerSleep;

static const bool DEFAULT_REST_ENABLE = false;

#if ENABLE_ZMQ
static CZMQNotificationInterface* pzmqNotificationInterface = NULL;
#endif

#ifdef WIN32
// Win32 LevelDB doesn't use filedescriptors, and the ones used for
// accessing block files, don't count towards to fd_set size limit
// anyway.
#define MIN_CORE_FILEDESCRIPTORS 0
#else
#define MIN_CORE_FILEDESCRIPTORS 150
#endif

/** Used to pass flags to the Bind() function */
enum BindFlags {
    BF_NONE = 0,
    BF_EXPLICIT = (1U << 0),
    BF_REPORT_ERROR = (1U << 1),
    BF_WHITELIST = (1U << 2),
};

static const char* FEE_ESTIMATES_FILENAME = "fee_estimates.dat";
CClientUIInterface uiInterface;

//////////////////////////////////////////////////////////////////////////////
//
// Shutdown
//

//
// Thread management and startup/shutdown:
//
// The network-processing threads are all part of a thread group
// created by AppInit() or the Qt main() function.
//
// A clean exit happens when StartShutdown() or the SIGTERM
// signal handler sets fRequestShutdown, which triggers
// the DetectShutdownThread(), which interrupts the main thread group.
// DetectShutdownThread() then exits, which causes AppInit() to
// continue (it .joins the shutdown thread).
// Shutdown() is then
// called to clean up database connections, and stop other
// threads that should only be stopped after the main network-processing
// threads have exited.
//
// Note that if running -daemon the parent process returns from AppInit2
// before adding any threads to the threadGroup, so .join_all() returns
// immediately and the parent exits from main().
//
// Shutdown for Qt is very similar, only it uses a QTimer to detect
// fRequestShutdown getting set, and then does the normal Qt
// shutdown thing.
//

std::atomic<bool> fRequestShutdown(false);
std::atomic<bool> fRequestRestart(false);

void StartShutdown()
{
    fRequestShutdown = true;
}
void StartRestart()
{
    fRequestShutdown = fRequestRestart = true;
}
bool ShutdownRequested()
{
    return fRequestShutdown;
}

class CCoinsViewErrorCatcher : public CCoinsViewBacked
{
public:
    CCoinsViewErrorCatcher(CCoinsView* view) : CCoinsViewBacked(view) {}
    bool GetCoins(const uint256& txid, CCoins& coins) const
    {
        try {
            return CCoinsViewBacked::GetCoins(txid, coins);
        } catch (const std::runtime_error& e) {
            uiInterface.ThreadSafeMessageBox(_("Error reading from database, shutting down."), "", CClientUIInterface::MSG_ERROR);
            LogPrintf("Error reading from database: %s\n", e.what());
            // Starting the shutdown sequence and returning false to the caller would be
            // interpreted as 'entry not found' (as opposed to unable to read data), and
            // could lead to invalid interpration. Just exit immediately, as we can't
            // continue anyway, and all writes should be atomic.
            abort();
        }
    }
    // Writes do not need similar protection, as failure to write is handled by the caller.
};

static CCoinsViewDB* pcoinsdbview = NULL;
static CCoinsViewErrorCatcher* pcoinscatcher = NULL;
static boost::scoped_ptr<ECCVerifyHandle> globalVerifyHandle;
static boost::thread_group threadGroup;
static CScheduler scheduler;

void Interrupt()
{
    InterruptHTTPServer();
    InterruptHTTPRPC();
    InterruptRPC();
    InterruptREST();
}

/** Preparing steps before shutting down or restarting the wallet */
void PrepareShutdown()
{
    LogPrintf("%s: In progress...\n", __func__);
    static CCriticalSection cs_Shutdown;
    TRY_LOCK(cs_Shutdown, lockShutdown);
    if (!lockShutdown)
        return;

    /// Note: Shutdown() must be able to handle cases in which AppInit2() failed part of the way,
    /// for example if the data directory was found to be locked.
    /// Be sure that anything that writes files or flushes caches only does this if the respective
    /// module was initialized.
    RenameThread("lux-shutoff");
    mempool.AddTransactionsUpdated(1);

    StopHTTPRPC();
    StopREST();
    StopRPC();
    StopHTTPServer();
#ifdef ENABLE_WALLET
    if (pwalletMain)
        bitdb.Flush(false);
//    GenerateBitcoins(NULL, 0);
#endif
    StopNode();
    UnregisterNodeSignals(GetNodeSignals());

    // After everything has been shut down, but before things get flushed, stop the
    // CScheduler/checkqueue threadGroup
    threadGroup.interrupt_all();
    threadGroup.join_all();

    if (fFeeEstimatesInitialized) {
        boost::filesystem::path est_path = GetDataDir() / FEE_ESTIMATES_FILENAME;
        CAutoFile est_fileout(fopen(est_path.string().c_str(), "wb"), SER_DISK, CLIENT_VERSION);
        if (!est_fileout.IsNull())
            mempool.WriteFeeEstimates(est_fileout);
        else
            LogPrintf("%s: Failed to write fee estimates to %s\n", __func__, est_path.string());
        fFeeEstimatesInitialized = false;
    }

    {
        LOCK(cs_main);
        if (pcoinsTip != NULL) {
            FlushStateToDisk();

            //record that client took the proper shutdown procedure
            pblocktree->WriteFlag("shutdown", true);
        }
        delete pcoinsTip;
        pcoinsTip = NULL;
        delete pcoinscatcher;
        pcoinscatcher = NULL;
        delete pcoinsdbview;
        pcoinsdbview = NULL;
        delete pblocktree;
        pblocktree = NULL;
        if(pstorageresult != nullptr) {
            delete pstorageresult;
            pstorageresult = nullptr;
        }
        delete globalState.release();
        globalSealEngine.reset();
    }
#ifdef ENABLE_WALLET
    if (pwalletMain)
        bitdb.Flush(true);
#endif

#if ENABLE_ZMQ
    if (pzmqNotificationInterface) {
        UnregisterValidationInterface(pzmqNotificationInterface);
        delete pzmqNotificationInterface;
        pzmqNotificationInterface = NULL;
    }
#endif

#ifndef WIN32
    try {
        boost::filesystem::remove(GetPidFile());
    } catch (const boost::filesystem::filesystem_error& e) {
        LogPrintf("%s: Unable to remove pidfile: %s\n", __func__, e.what());
    }
#endif
    UnregisterAllValidationInterfaces();
}

/**
* Shutdown is split into 2 parts:
* Part 1: shut down everything but the main wallet instance (done in PrepareShutdown() )
* Part 2: delete wallet instance
*
* In case of a restart PrepareShutdown() was already called before, but this method here gets
* called implicitly when the parent object is deleted. In this case we have to skip the
* PrepareShutdown() part because it was already executed and just delete the wallet instance.
*/
void Shutdown()
{
    // Shutdown part 1: prepare shutdown
    if(!fRequestRestart) {
        PrepareShutdown();
    }

// Shutdown part 2: delete wallet instance
    StopLuxControl();
#ifdef ENABLE_WALLET
    delete pwalletMain;
    pwalletMain = NULL;
#endif
    globalVerifyHandle.reset();
    ECC_Stop();
    LogPrintf("%s: done\n", __func__);
}

/**
 * Signal handlers are very limited in what they are allowed to do, so:
 */
#ifndef WIN32
void HandleSIGTERM(int)
{
    fRequestShutdown = true;
}

void HandleSIGHUP(int)
{
    fReopenDebugLog = true;
}
#else
static BOOL WINAPI consoleCtrlHandler(DWORD dwCtrlType)
{
    fRequestShutdown = true;
    Sleep(INFINITE);
    return true;
}
#endif

bool static InitError(const std::string& str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_ERROR);
    return false;
}

bool static InitWarning(const std::string& str)
{
    uiInterface.ThreadSafeMessageBox(str, "", CClientUIInterface::MSG_WARNING);
    return true;
}

bool static Bind(const CService& addr, unsigned int flags)
{
    if (!(flags & BF_EXPLICIT) && IsLimited(addr))
        return false;
    std::string strError;
    if (!BindListenPort(addr, strError, (flags & BF_WHITELIST) != 0)) {
        if (flags & BF_REPORT_ERROR)
            return InitError(strError);
        return false;
    }
    return true;
}

void OnRPCStopped()
{
    cvBlockChange.notify_all();
    LogPrint("rpc", "RPC stopped.\n");
}

void OnRPCPreCommand(const CRPCCommand& cmd)
{
#ifdef ENABLE_WALLET
    if (cmd.reqWallet && !pwalletMain)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (disabled)");
#endif

    // Observe safe mode
    string strWarning = GetWarnings("rpc");
    if (strWarning != "" && !GetBoolArg("-disablesafemode", false) &&
        !cmd.okSafeMode)
        throw JSONRPCError(RPC_FORBIDDEN_BY_SAFE_MODE, string("Safe mode: ") + strWarning);
}

std::string HelpMessage(HelpMessageMode mode)
{
    // When adding new options to the categories, please keep and ensure alphabetical ordering.
    string strUsage = HelpMessageGroup(_("Options:"));
    strUsage += HelpMessageOpt("-?", _("This help message"));
    strUsage += HelpMessageOpt("-alertnotify=<cmd>", _("Execute command when a relevant alert is received or we see a really long fork (%s in cmd is replaced by message)"));
    strUsage += HelpMessageOpt("-alerts", strprintf(_("Receive and display P2P network alerts (default: %u)"), DEFAULT_ALERTS));
    strUsage += HelpMessageOpt("-blocknotify=<cmd>", _("Execute command when the best block changes (%s in cmd is replaced by block hash)"));
    strUsage += HelpMessageOpt("-checkblocks=<n>", strprintf(_("How many blocks to check at startup (default: %u, 0 = all)"), 500));
    strUsage += HelpMessageOpt("-checklevel=<n>", strprintf(_("How thorough the block verification of -checkblocks is (0-4, default: %u)"), 3));
    strUsage += HelpMessageOpt("-conf=<file>", strprintf(_("Specify configuration file (default: %s)"), "lux.conf"));
    if (mode == HMM_BITCOIND) {
#if !defined(WIN32)
        strUsage += HelpMessageOpt("-daemon", _("Run in the background as a daemon and accept commands"));
#endif
    }
    strUsage += HelpMessageOpt("-datadir=<dir>", _("Specify data directory"));
    strUsage += HelpMessageOpt("-dbcache=<n>", strprintf(_("Set database cache size in megabytes (%d to %d, default: %d)"), nMinDbCache, nMaxDbCache, nDefaultDbCache));
    strUsage += HelpMessageOpt("-nlogfile=<n>", _("Set number of debug log files"));
    strUsage += HelpMessageOpt("-loadblock=<file>", _("Imports blocks from external blk000??.dat file") + " " + _("on startup"));
    strUsage += HelpMessageOpt("-maxorphantx=<n>", strprintf(_("Keep at most <n> unconnectable transactions in memory (default: %u)"), DEFAULT_MAX_ORPHAN_TRANSACTIONS));
    strUsage += HelpMessageOpt("-maxmempool=<n>", strprintf(_("Keep the transaction memory pool below <n> megabytes (default: %u)"), DEFAULT_MAX_MEMPOOL_SIZE));
    strUsage += HelpMessageOpt("-par=<n>", strprintf(_("Set the number of script verification threads (1 to %d, 0 = auto, <0 = leave that many cores free, default: %d)"), (int)boost::thread::hardware_concurrency(), DEFAULT_SCRIPTCHECK_THREADS));
#ifndef WIN32
    strUsage += HelpMessageOpt("-pid=<file>", strprintf(_("Specify pid file (default: %s)"), "luxd.pid"));
#endif
    strUsage += HelpMessageOpt("-record-log-opcodes", _("Logs all EVM LOG opcode operations to the file vmExecLogs.json"));
    //Temporarily disabled until our chain doesn't grow in size
    //strUsage += HelpMessageOpt("-prune=<n>", _("Reduce storage requirements by pruning (deleting) old blocks. This mode disables wallet support and is incompatible with -txindex.") + " " + _("Warning: Reverting this setting requires re-downloading the entire blockchain.") + " " + _("(default: 0 = disable pruning blocks,") + " " + strprintf(_(">%u = target size in MiB to use for block files)"), MIN_DISK_SPACE_FOR_BLOCK_FILES / 1024 / 1024));
    strUsage += HelpMessageOpt("-reindex-chainstate", _("Rebuild chain state from the currently indexed blocks"));
    strUsage += HelpMessageOpt("-reindex", _("Rebuild block chain index from current blk000??.dat files") + " " + _("on startup"));
#if !defined(WIN32)
    strUsage += HelpMessageOpt("-sysperms", _("Create new files with system default permissions, instead of umask 077 (only effective with disabled wallet functionality)"));
#endif
    strUsage += HelpMessageOpt("-txindex", strprintf(_("Maintain a full transaction index, used by the getrawtransaction rpc call (default: %u)"), DEFAULT_TXINDEX));
    strUsage += HelpMessageOpt("-addressindex", strprintf(_("Maintain a full address index, used to query for the balance, txids and unspent outputs for addresses (default: %u)"), DEFAULT_ADDRESSINDEX));
    strUsage += HelpMessageOpt("-spentindex", strprintf(_("Maintain a full spent index, used to query the spending txid and input index for an outpoint (default: %u)"), DEFAULT_SPENTINDEX));
    strUsage += HelpMessageOpt("-logevents", strprintf(_("Maintain a full EVM log index, used by searchlogs and gettransactionreceipt rpc calls (default: %u)"), false));

    strUsage += HelpMessageGroup(_("Connection options:"));
    strUsage += HelpMessageOpt("-addnode=<ip>", _("Add a node to connect to and attempt to keep the connection open"));
    strUsage += HelpMessageOpt("-peerlimit=<n>", _("limit the number of peers used while syncing to increase syncing speed. (limit removed after wallet is synced)"));
    strUsage += HelpMessageOpt("-banscore=<n>", strprintf(_("Threshold for disconnecting misbehaving peers (default: %u)"), 100));
    strUsage += HelpMessageOpt("-bantime=<n>", strprintf(_("Number of seconds to keep misbehaving peers from reconnecting (default: %u)"), 86400));
    strUsage += HelpMessageOpt("-bind=<addr>", _("Bind to given address and always listen on it. Use [host]:port notation for IPv6"));
    strUsage += HelpMessageOpt("-connect=<ip>", _("Connect only to the specified node(s)"));
    strUsage += HelpMessageOpt("-discover", _("Discover own IP address (default: 1 when listening and no -externalip)"));
    strUsage += HelpMessageOpt("-dns", _("Allow DNS lookups for -addnode, -seednode and -connect") + " " + _("(default: 1)"));
    strUsage += HelpMessageOpt("-dnsseed", _("Query for peer addresses via DNS lookup, if low on addresses (default: 1 unless -connect)"));
    strUsage += HelpMessageOpt("-externalip=<ip>", _("Specify your own public address"));
    strUsage += HelpMessageOpt("-forcednsseed", strprintf(_("Always query for peer addresses via DNS lookup (default: %u)"), 0));
    strUsage += HelpMessageOpt("-listen", _("Accept connections from outside (default: 1 if no -proxy or -connect)"));
    strUsage += HelpMessageOpt("-maxconnections=<n>", strprintf(_("Maintain at most <n> connections to peers (default: %u)"), 125));
    strUsage += HelpMessageOpt("-maxreceivebuffer=<n>", strprintf(_("Maximum per-connection receive buffer, <n>*1000 bytes (default: %u)"), 5000));
    strUsage += HelpMessageOpt("-maxsendbuffer=<n>", strprintf(_("Maximum per-connection send buffer, <n>*1000 bytes (default: %u)"), 1000));
    strUsage += HelpMessageOpt("-onion=<ip:port>", strprintf(_("Use separate SOCKS5 proxy to reach peers via Tor hidden services (default: %s)"), "-proxy"));
    strUsage += HelpMessageOpt("-onlynet=<net>", _("Only connect to nodes in network <net> (ipv4, ipv6 or onion)"));
    strUsage += HelpMessageOpt("-permitbaremultisig", strprintf(_("Relay non-P2SH multisig (default: %u)"), 1));
    strUsage += HelpMessageOpt("-port=<port>", strprintf(_("Listen for connections on <port> (default: %u or testnet: %u)"), 26969, 28333));
    strUsage += HelpMessageOpt("-proxy=<ip:port>", _("Connect through SOCKS5 proxy"));
    strUsage += HelpMessageOpt("-seednode=<ip>", _("Connect to a node to retrieve peer addresses, and disconnect"));
    strUsage += HelpMessageOpt("-timeout=<n>", strprintf(_("Specify connection timeout in milliseconds (minimum: 1, default: %d)"), DEFAULT_CONNECT_TIMEOUT));
#ifdef USE_UPNP
#if USE_UPNP
    strUsage += HelpMessageOpt("-upnp", _("Use UPnP to map the listening port (default: 1 when listening)"));
#else
    strUsage += HelpMessageOpt("-upnp", strprintf(_("Use UPnP to map the listening port (default: %u)"), 0));
#endif
#endif
    strUsage += HelpMessageOpt("-whitebind=<addr>", _("Bind to given address and whitelist peers connecting to it. Use [host]:port notation for IPv6"));
    strUsage += HelpMessageOpt("-whitelist=<netmask>", _("Whitelist peers connecting from the given netmask or IP address. Can be specified multiple times."));
    strUsage += HelpMessageOpt("", _("Whitelisted peers cannot be DoS banned and their transactions are always relayed, even if they are already in the mempool, useful e.g. for a gateway"));

#ifdef ENABLE_WALLET
    strUsage += HelpMessageGroup(_("Wallet options:"));
    strUsage += HelpMessageOpt("-createwalletbackups=<n>", _("Number of automatic wallet backups (default: 10)"));
    strUsage += HelpMessageOpt("-disablewallet", _("Do not load the wallet and disable wallet RPC calls"));
    strUsage += HelpMessageOpt("-keypool=<n>", strprintf(_("Set key pool size to <n> (default: %u)"), 100));
    if (GetBoolArg("-help-debug", false))
    strUsage += HelpMessageOpt("-mintxfee=<amt>", strprintf(_("Fees (in LUX/Kb) smaller than this are considered zero fee for transaction creation (default: %s)"), FormatMoney(CWallet::minTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-paytxfee=<amt>", strprintf(_("Fee (in LUX/kB) to add to transactions you send (default: %s)"), FormatMoney(payTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-rescan", _("Rescan the block chain for missing wallet transactions") + " " + _("on startup"));
    strUsage += HelpMessageOpt("-salvagewallet", _("Attempt to recover private keys from a corrupt wallet.dat") + " " + _("on startup"));
    strUsage += HelpMessageOpt("-sendfreetransactions", strprintf(_("Send transactions as zero-fee transactions if possible (default: %u)"), 0));
    strUsage += HelpMessageOpt("-spendzeroconfchange", strprintf(_("Spend unconfirmed change when sending transactions (default: %u)"), 1));
    strUsage += HelpMessageOpt("-staking", strprintf(_("Stake your coins to support network and gain reward (default: %s)"), DEFAULT_STAKE ? "true":"false"));
    strUsage += HelpMessageOpt("-txconfirmtarget=<n>", strprintf(_("If paytxfee is not set, include enough fee so transactions begin confirmation on average within n blocks (default: %u)"), 1));
    strUsage += HelpMessageOpt("-maxtxfee=<amt>", strprintf(_("Maximum total fees to use in a single wallet transaction, setting too low may abort large transactions (default: %s)"), FormatMoney(maxTxFee)));
    strUsage += HelpMessageOpt("-upgradewallet", _("Upgrade wallet to latest format") + " " + _("on startup"));
    strUsage += HelpMessageOpt("-wallet=<file>", _("Specify wallet file (within data directory)") + " " + strprintf(_("(default: %s)"), "wallet.dat"));
    strUsage += HelpMessageOpt("-walletbroadcast", _("Make the wallet broadcast transactions") + " " + strprintf(_("(default: %u)"), true));
    strUsage += HelpMessageOpt("-walletnotify=<cmd>", _("Execute command when a wallet transaction changes (%s in cmd is replaced by TxID)"));
    strUsage += HelpMessageOpt("-usehd", _("Use hierarchical deterministic key generation (HD) after bip32. Only has effect during wallet creation/first start") + " " + strprintf(_("(default: %u)"), DEFAULT_USE_HD_WALLET));
    strUsage += HelpMessageOpt("-mnemonic", _("User defined mnemonic for HD wallet (bip39). Only has effect during wallet creation/first start (default: randomly generated)"));
    strUsage += HelpMessageOpt("-mnemonicpassphrase", _("User defined memonic passphrase for HD wallet (bip39). Only has effect during wallet creation/first start (default: randomly generated)"));
    strUsage += HelpMessageOpt("-hdseed", _("User defined seed for HD wallet (should be in hex). Only has effect during wallet creation/first start (default: randomly generated)"));
    if (mode == HMM_BITCOIN_QT)
    strUsage += HelpMessageOpt("-windowtitle=<name>", _("Wallet window title"));
    strUsage += HelpMessageOpt("-zapwallettxes=<mode>", _("Delete all wallet transactions and only recover those parts of the blockchain through -rescan on startup"));
    strUsage += HelpMessageOpt(" ", _("(1 = keep tx meta data e.g. account owner and payment request information, 2 = drop tx meta data)"));
#endif

#if ENABLE_ZMQ
    strUsage += HelpMessageGroup(_("ZeroMQ notification options:"));
    strUsage += HelpMessageOpt("-zmqpubhashblock=<address>", _("Enable publish hash block in <address>"));
    strUsage += HelpMessageOpt("-zmqpubhashtx=<address>", _("Enable publish hash transaction in <address>"));
    strUsage += HelpMessageOpt("-zmqpubhashtxlock=<address>", _("Enable publish hash transaction (locked via InstanTX) in <address>"));
    strUsage += HelpMessageOpt("-zmqpubrawblock=<address>", _("Enable publish raw block in <address>"));
    strUsage += HelpMessageOpt("-zmqpubrawtx=<address>", _("Enable publish raw transaction in <address>"));
    strUsage += HelpMessageOpt("-zmqpubrawtxlock=<address>", _("Enable publish raw transaction (locked via InstanTX) in <address>"));
#endif

    strUsage += HelpMessageGroup(_("Debugging/Testing options:"));
    if (GetBoolArg("-help-debug", false)) {
        strUsage += HelpMessageOpt("-checkpoints", strprintf(_("Only accept block chain matching built-in checkpoints (default: %u)"), 1));
        strUsage += HelpMessageOpt("-dblogsize=<n>", strprintf(_("Flush database activity from memory pool to disk log every <n> megabytes (default: %u)"), 100));
        strUsage += HelpMessageOpt("-disablesafemode", strprintf(_("Disable safemode, override a real safe mode event (default: %u)"), 0));
        strUsage += HelpMessageOpt("-testsafemode", strprintf(_("Force safe mode (default: %u)"), 0));
        strUsage += HelpMessageOpt("-dropmessagestest=<n>", _("Randomly drop 1 of every <n> network messages"));
        strUsage += HelpMessageOpt("-fuzzmessagestest=<n>", _("Randomly fuzz 1 of every <n> network messages"));
        strUsage += HelpMessageOpt("-flushwallet", strprintf(_("Run a thread to flush wallet periodically (default: %u)"), 1));
        strUsage += HelpMessageOpt("-stopafterblockimport", strprintf(_("Stop running after importing blocks from disk (default: %u)"), 0));
    }
    string debugCategories ="addrman, alert, bench, coindb, db, lock, rand, rpc, selectcoins, mempool, net"; // Don't translate these and qt below
    if (mode == HMM_BITCOIN_QT)
        debugCategories +=", qt";
    strUsage += HelpMessageOpt("-debug=<category>", strprintf(_("Output debugging information (default: %u, supplying <category> is optional)"), 0) + ". " + _("If <category> is not supplied, output all debugging information.") + _("<category> can be:") + " " + debugCategories + ".");
#ifdef ENABLE_CPUMINER
    strUsage += HelpMessageOpt("-gen", strprintf(_("Generate coins (default: %u)"), 0));
    strUsage += HelpMessageOpt("-genproclimit=<n>", strprintf(_("Set the number of threads for coin generation if enabled (-1 = all cores, default: %d)"), 1));
#endif
    strUsage += HelpMessageOpt("-help-debug", _("Show all debugging options (usage: --help -help-debug)"));
    strUsage += HelpMessageOpt("-logips", strprintf(_("Include IP addresses in debug output (default: %u)"), 0));
    strUsage += HelpMessageOpt("-logtimestamps", strprintf(_("Prepend debug output with timestamp (default: %u)"), 1));
    if (GetBoolArg("-help-debug", false)) {
    strUsage += HelpMessageOpt("-limitfreerelay=<n>", strprintf(_("Continuously rate-limit free transactions to <n>*1000 bytes per minute (default:%u)"), 15));
    strUsage += HelpMessageOpt("-relaypriority", strprintf(_("Require high priority for relaying free or low-fee transactions (default:%u)"), 1));
    strUsage += HelpMessageOpt("-maxsigcachesize=<n>", strprintf(_("Limit size of signature cache to <n> entries (default: %u)"), 50000));
    }
    strUsage += HelpMessageOpt("-minrelaytxfee=<amt>", strprintf(_("Fees (in LUX/Kb) smaller than this are considered zero fee for relaying (default: %s)"), FormatMoney(::minRelayTxFee.GetFeePerK())));
    strUsage += HelpMessageOpt("-printtoconsole", strprintf(_("Send trace/debug info to console instead of debug.log file (default: %u)"), 0));
    if (GetBoolArg("-help-debug", false)) {
    strUsage += HelpMessageOpt("-printpriority", strprintf(_("Log transaction priority and fee per kB when mining blocks (default: %u)"), 0));
    strUsage += HelpMessageOpt("-privdb", strprintf(_("Sets the DB_PRIVATE flag in the wallet db environment (default: %u)"), 1));
    strUsage += HelpMessageOpt("-regtest", _("Enter regression test mode, which uses a special chain in which blocks can be solved instantly."));
    strUsage += HelpMessageOpt("", _("This is intended for regression testing tools and app development."));
    strUsage += HelpMessageOpt("", _("In this mode -genproclimit controls how many blocks are generated immediately."));
    }
    strUsage += HelpMessageOpt("-shrinkdebugfile", _("Shrink debug.log file on client startup (default: 1 when no -debug)"));
    strUsage += HelpMessageOpt("-testnet", _("Use the test network"));
    strUsage += HelpMessageOpt("-segwittest", _("Use the SegWit test network"));

    strUsage += HelpMessageGroup(_("Masternode options:"));
    strUsage += HelpMessageOpt("-masternode=<n>", strprintf(_("Enable the client to act as a masternode (0-1, default: %u)"), 0));
    strUsage += HelpMessageOpt("-mnconf=<file>", strprintf(_("Specify masternode configuration file (default: %s)"), "masternode.conf"));
    strUsage += HelpMessageOpt("-mnconflock=<n>", strprintf(_("Lock masternodes from masternode configuration file (default: %u)"), 1));
    strUsage += HelpMessageOpt("-masternodeprivkey=<n>", _("Set the masternode private key"));
    strUsage += HelpMessageOpt("-masternodeaddr=<n>", strprintf(_("Set external address:port to get to this masternode (example: %s)"), "128.127.106.235:51472"));
    strUsage += HelpMessageOpt("-budgetvotemode=<mode>", _("Change automatic finalized budget voting behavior. mode=auto: Vote for only exact finalized budget match to my generated budget. (string, default: auto)"));

    strUsage += HelpMessageGroup(_("Darksend options:"));
    strUsage += HelpMessageOpt("-enabledarksend=<n>", strprintf(_("Enable use of automated darksend for funds stored in this wallet (0-1, default: %u)"), 0));
    strUsage += HelpMessageOpt("-darksendrounds=<n>", strprintf(_("Use N separate masternodes to anonymize funds  (2-8, default: %u)"), 2));
    strUsage += HelpMessageOpt("-anonymizeluxamount=<n>", strprintf(_("Keep N LUX anonymized (default: %u)"), 0));
    strUsage += HelpMessageOpt("-liquidityprovider=<n>", strprintf(_("Provide liquidity to Darksend by infrequently mixing coins on a continual basis (0-100, default: %u, 1=very frequent, high fees, 100=very infrequent, low fees)"), 0));

    strUsage += HelpMessageGroup(_("InstanTX options:"));
    strUsage += HelpMessageOpt("-enableinstantx=<n>", strprintf(_("Enable instantx, show confirmations for locked transactions (bool, default: %s)"), "true"));
    strUsage += HelpMessageOpt("-instantxdepth=<n>", strprintf(_("Show N confirmations for a successfully locked transaction (0-9999, default: %u)"), nInstanTXDepth));

    strUsage += HelpMessageGroup(_("Node relay options:"));
    if (GetBoolArg("-help-debug", false))
        strUsage += HelpMessageOpt("-acceptnonstandardtxn", strprintf("Relay and mine \"non-standard\" transactions (%sdefault: %u)", "testnet/regtest only; ", !Params(CBaseChainParams::TESTNET).RequireStandard()));
    strUsage += HelpMessageOpt("-datacarrier", strprintf(_("Relay and mine data carrier transactions (default: %u)"), 1));
    strUsage += HelpMessageOpt("-datacarriersize", strprintf(_("Maximum size of data in data carrier transactions we relay and mine (default: %u)"), MAX_OP_RETURN_RELAY));

    strUsage += HelpMessageGroup(_("Block creation options:"));
    strUsage += HelpMessageOpt("-blockminsize=<n>", strprintf(_("Set minimum block size in bytes (default: %u)"), 0));
    strUsage += HelpMessageOpt("-blockmaxsize=<n>", strprintf(_("Set maximum block size in bytes (default: %d)"), DEFAULT_BLOCK_MAX_SIZE));
    strUsage += HelpMessageOpt("-blockprioritysize=<n>", strprintf(_("Set maximum size of high-priority/low-fee transactions in bytes (default: %d)"), DEFAULT_BLOCK_PRIORITY_SIZE));

    strUsage += HelpMessageGroup(_("RPC server options:"));
    strUsage += HelpMessageOpt("-server", _("Accept command line and JSON-RPC commands"));
    strUsage += HelpMessageOpt("-rest", strprintf(_("Accept public REST requests (default: %u)"), 0));
    strUsage += HelpMessageOpt("-rpcbind=<addr>", _("Bind to given address to listen for JSON-RPC connections. Use [host]:port notation for IPv6. This option can be specified multiple times (default: bind to all interfaces)"));
    strUsage += HelpMessageOpt("-rpcuser=<user>", _("Username for JSON-RPC connections"));
    strUsage += HelpMessageOpt("-rpcpassword=<pw>", _("Password for JSON-RPC connections"));
    strUsage += HelpMessageOpt("-rpcport=<port>", strprintf(_("Listen for JSON-RPC connections on <port> (default: %u or testnet: %u)"), 9888, 9777));
    strUsage += HelpMessageOpt("-rpcallowip=<ip>", _("Allow JSON-RPC connections from specified source. Valid for <ip> are a single IP (e.g. 1.2.3.4), a network/netmask (e.g. 1.2.3.4/255.255.255.0) or a network/CIDR (e.g. 1.2.3.4/24). This option can be specified multiple times"));
    strUsage += HelpMessageOpt("-rpcthreads=<n>", strprintf(_("Set the number of threads to service RPC calls (default: %d)"), 4));
    strUsage += HelpMessageOpt("-rpckeepalive", strprintf(_("RPC support for HTTP persistent connections (default: %d)"), 1));

    strUsage += HelpMessageGroup(_("RPC SSL options: (see the Bitcoin Wiki for SSL setup instructions)"));
    strUsage += HelpMessageOpt("-rpcssl", _("Use OpenSSL (https) for JSON-RPC connections"));
    strUsage += HelpMessageOpt("-rpcsslcertificatechainfile=<file.cert>", strprintf(_("Server certificate file (default: %s)"), "server.cert"));
    strUsage += HelpMessageOpt("-rpcsslprivatekeyfile=<file.pem>", strprintf(_("Server private key (default: %s)"), "server.pem"));
    strUsage += HelpMessageOpt("-rpcsslciphers=<ciphers>", strprintf(_("Acceptable ciphers (default: %s)"), "TLSv1.2+HIGH:TLSv1+HIGH:!SSLv2:!aNULL:!eNULL:!3DES:@STRENGTH"));

    return strUsage;
}

std::string LicenseInfo()
{
    return FormatParagraph(strprintf(_("Copyright (C) 2009-%i The Bitcoin Core Developers"), COPYRIGHT_YEAR)) + "\n" +
           "\n" +
           FormatParagraph(strprintf(_("Copyright (C) 2014-%i The Dash Core Developers"), COPYRIGHT_YEAR)) + "\n" +
           "\n" +
           FormatParagraph(strprintf(_("Copyright (C) 2017-%i The LUX-Core Developers"), COPYRIGHT_YEAR)) + "\n" +
           "\n" +
           FormatParagraph(_("This is experimental software.")) + "\n" +
           "\n" +
           FormatParagraph(_("Distributed under the MIT software license, see the accompanying file COPYING or <http://www.opensource.org/licenses/mit-license.php>.")) + "\n" +
           "\n" +
           FormatParagraph(_("This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit <https://www.openssl.org/> and cryptographic software written by Eric Young and UPnP software written by Thomas Bernard.")) +
           "\n";
}

static void BlockNotifyCallback(bool initialSync, const CBlockIndex *pBlockIndex)
{
    if (initialSync || !pBlockIndex)
        return;
    std::string strCmd = GetArg("-blocknotify", "");

    boost::replace_all(strCmd, "%s", pBlockIndex->GetBlockHash().GetHex());
    boost::thread t(runCommand, strCmd); // thread runs free
}

struct CImportingNow {
    CImportingNow()
    {
        assert(fImporting == false);
        fImporting = true;
    }

    ~CImportingNow()
    {
        assert(fImporting == true);
        fImporting = false;
    }
};

class Read;

void DeleteAllBlockFiles()
{
    if (boost::filesystem::exists(GetBlockPosFilename(CDiskBlockPos(0, 0), "blk")))
        return;

    LogPrintf("Removing all blk?????.dat and rev?????.dat files for -reindex with -prune\n");
    boost::filesystem::path blocksdir = GetDataDir() / "blocks";
    for (boost::filesystem::directory_iterator it(blocksdir); it != boost::filesystem::directory_iterator(); it++) {
        if (is_regular_file(*it)) {
            if ((it->path().filename().string().length() == 12) &&
                (it->path().filename().string().substr(8,4) == ".dat") &&
                ((it->path().filename().string().substr(0,3) == "blk") ||
                 (it->path().filename().string().substr(0,3) == "rev")))
                boost::filesystem::remove(it->path());
        }
    }
}

void ThreadImport(std::vector<boost::filesystem::path> vImportFiles)
{
    RenameThread("lux-loadblk");
    const CChainParams& chainparams = Params();
    // -reindex
    if (fReindex) {
        CImportingNow imp;
        int nFile = 0;
        while (true) {
            CDiskBlockPos pos(nFile, 0);
            if (!boost::filesystem::exists(GetBlockPosFilename(pos, "blk")))
                break; // No block files left to reindex
            FILE* file = OpenBlockFile(pos, true);
            if (!file)
                break; // This error is logged in OpenBlockFile
            LogPrintf("Reindexing block file blk%05u.dat...\n", (unsigned int)nFile);
            LoadExternalBlockFile(chainparams, file, &pos);
            nFile++;
        }
        pblocktree->WriteReindexing(false);
        fReindex = false;
        LogPrintf("Reindexing finished\n");
        // To avoid ending up in a situation without genesis block, re-try initializing (no-op if reindexing worked):
        InitBlockIndex(chainparams);
    }

    // hardcoded $DATADIR/bootstrap.dat
    filesystem::path pathBootstrap = GetDataDir() / "bootstrap.dat";
    if (filesystem::exists(pathBootstrap)) {
        FILE* file = fopen(pathBootstrap.string().c_str(), "rb");
        if (file) {
            CImportingNow imp;
            filesystem::path pathBootstrapOld = GetDataDir() / "bootstrap.dat.old";
            LogPrintf("Importing bootstrap.dat...\n");
            LoadExternalBlockFile(chainparams, file);
            RenameOver(pathBootstrap, pathBootstrapOld);
        } else {
            LogPrintf("Warning: Could not open bootstrap file %s\n", pathBootstrap.string());
        }
    }

    // -loadblock=
    for (boost::filesystem::path& path : vImportFiles) {
        FILE* file = fopen(path.string().c_str(), "rb");
        if (file) {
            CImportingNow imp;
            LogPrintf("Importing blocks file %s...\n", path.string());
            LoadExternalBlockFile(chainparams, file);
        } else {
            LogPrintf("Warning: Could not open blocks file %s\n", path.string());
        }
    }

    if (GetBoolArg("-stopafterblockimport", false)) {
        LogPrintf("Stopping after block import\n");
        StartShutdown();
    }
}

static bool LockDataDirectory(bool probeOnly)
{
    std::string strDataDir = GetDataDir().string();

    // Make sure only a single Bitcoin process is using the data directory.
    boost::filesystem::path pathLockFile = GetDataDir() / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);

    try {
        static boost::interprocess::file_lock lock(pathLockFile.string().c_str());
        if (!lock.try_lock())
            return InitError(strprintf(_("Cannot obtain a lock on data directory %s. %s is probably already running."), strDataDir, _(PACKAGE_NAME)));
    } catch(const boost::interprocess::interprocess_exception& e) {
        return InitError(strprintf(_("Cannot obtain a lock on data directory %s. %s is probably already running.") + " %s.", strDataDir, _(PACKAGE_NAME), e.what()));
    }
    return true;
}

/** Sanity checks
 *  Ensure that LUX is running in a usable environment with all
 *  necessary library support.
 */
bool InitSanityCheck(void)
{
    if (!ECC_InitSanityCheck()) {
        InitError("OpenSSL appears to lack support for elliptic curve cryptography. For more "
                  "information, visit https://en.bitcoin.it/wiki/OpenSSL_and_EC_Libraries");
        return false;
    }
    if (!glibc_sanity_test() || !glibcxx_sanity_test())
        return false;

    return true;
}

bool AppInitServers()
{
    RPCServer::OnStopped(&OnRPCStopped);
    RPCServer::OnPreCommand(&OnRPCPreCommand);
    if (!InitHTTPServer())
        return false;
    if (!StartRPC())
        return false;
    if (!StartHTTPRPC())
        return false;
    if (GetBoolArg("-rest", DEFAULT_REST_ENABLE) && !StartREST())
        return false;
    if (!StartHTTPServer())
        return false;
    return true;
}

/** Initialize lux.
 *  @pre Parameters should be parsed and config file should be read.
 */
bool AppInit2()
{
// ********************************************************* Step 1: setup
#ifdef WIN32
#ifdef _MSC_VER
    // Turn off Microsoft heap dump noise
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, CreateFileA("NUL", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, 0));
#endif
#if _MSC_VER >= 1400
    // Disable confusing "helpful" text message on abort, Ctrl-C
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
// Enable Data Execution Prevention (DEP)
// Minimum supported OS versions: WinXP SP3, WinVista >= SP1, Win Server 2008
// A failure is non-critical and needs no further attention!
#ifndef PROCESS_DEP_ENABLE
// We define this here, because GCCs winbase.h limits this to _WIN32_WINNT >= 0x0601 (Windows 7),
// which is not correct. Can be removed, when GCCs winbase.h is fixed!
#define PROCESS_DEP_ENABLE 0x00000001
#endif
    typedef BOOL(WINAPI * PSETPROCDEPPOL)(DWORD);
    PSETPROCDEPPOL setProcDEPPol = (PSETPROCDEPPOL)GetProcAddress(GetModuleHandleA("Kernel32.dll"), "SetProcessDEPPolicy");
    if (setProcDEPPol != NULL) setProcDEPPol(PROCESS_DEP_ENABLE);

    // Initialize Windows Sockets
    WSADATA wsadata;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (ret != NO_ERROR || LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wVersion) != 2) {
        return InitError(strprintf("Error: Winsock library failed to start (WSAStartup returned error %d)", ret));
    }

    SetConsoleCtrlHandler(consoleCtrlHandler, true);

#else /* WIN32 */

    if (GetBoolArg("-sysperms", false)) {
#ifdef ENABLE_WALLET
        if (!GetBoolArg("-disablewallet", false))
            return InitError("Error: -sysperms is not allowed in combination with enabled wallet functionality");
#endif
    } else {
        umask(077);
    }

    //TODO: registerSignalHandler
    // Clean shutdown on SIGTERM
    struct sigaction sa;
    sa.sa_handler = HandleSIGTERM;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // Reopen debug.log on SIGHUP
    struct sigaction sa_hup;
    sa_hup.sa_handler = HandleSIGHUP;
    sigemptyset(&sa_hup.sa_mask);
    sa_hup.sa_flags = 0;
    sigaction(SIGHUP, &sa_hup, NULL);

    // Ignore SIGPIPE, otherwise it will bring the daemon down if the client closes unexpectedly
    signal(SIGPIPE, SIG_IGN);
#endif

    // ********************************************************* Step 2: parameter interactions
    const CChainParams& chainparams = Params();

    // Set this early so that parameter interactions go to console
    nMinerSleep = GetArg("-minersleep", 500);
    fPrintToConsole = GetBoolArg("-printtoconsole", false);
    fLogTimestamps = GetBoolArg("-logtimestamps", true);
    fLogIPs = GetBoolArg("-logips", false);
    //Temporarily disabled until our chain doesn't grow in size
    //int nIndexNum;

    if (mapArgs.count("-bind") || mapArgs.count("-whitebind")) {
        // when specifying an explicit binding address, you want to listen on it
        // even when -connect or -proxy is specified
        if (SoftSetBoolArg("-listen", true))
            LogPrintf("AppInit2 : parameter interaction: -bind or -whitebind set -> setting -listen=1\n");
    }

    if (mapArgs.count("-connect") && mapMultiArgs["-connect"].size() > 0) {
        // when only connecting to trusted nodes, do not seed via DNS, or listen by default
        if (SoftSetBoolArg("-dnsseed", false))
            LogPrintf("AppInit2 : parameter interaction: -connect set -> setting -dnsseed=0\n");
        if (SoftSetBoolArg("-listen", false))
            LogPrintf("AppInit2 : parameter interaction: -connect set -> setting -listen=0\n");
    }

    if (mapArgs.count("-proxy")) {
        // to protect privacy, do not listen by default if a default proxy server is specified
        if (SoftSetBoolArg("-listen", false))
            LogPrintf("%s: parameter interaction: -proxy set -> setting -listen=0\n", __func__);
        // to protect privacy, do not use UPNP when a proxy is set. The user may still specify -listen=1
        // to listen locally, so don't rely on this happening through -listen below.
        if (SoftSetBoolArg("-upnp", false))
            LogPrintf("%s: parameter interaction: -proxy set -> setting -upnp=0\n", __func__);
        // to protect privacy, do not discover addresses by default
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("AppInit2 : parameter interaction: -proxy set -> setting -discover=0\n");
    }

    if (!GetBoolArg("-listen", true)) {
        // do not map ports or try to retrieve public IP when not listening (pointless)
        if (SoftSetBoolArg("-upnp", false))
            LogPrintf("AppInit2 : parameter interaction: -listen=0 -> setting -upnp=0\n");
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("AppInit2 : parameter interaction: -listen=0 -> setting -discover=0\n");
    }

    if (mapArgs.count("-externalip")) {
        // if an explicit public IP is specified, do not try to find others
        if (SoftSetBoolArg("-discover", false))
            LogPrintf("AppInit2 : parameter interaction: -externalip set -> setting -discover=0\n");
    }

    if (GetBoolArg("-salvagewallet", false)) {
        // Rewrite just private keys: rescan to find transactions
        if (SoftSetBoolArg("-rescan", true))
            LogPrintf("AppInit2 : parameter interaction: -salvagewallet=1 -> setting -rescan=1\n");
    }

    // -zapwallettx implies a rescan
    if (GetBoolArg("-zapwallettxes", false)) {
        if (SoftSetBoolArg("-rescan", true))
            LogPrintf("AppInit2 : parameter interaction: -zapwallettxes=<mode> -> setting -rescan=1\n");
    }

    if (mapArgs.count("-hdseed") && IsHex(GetArg("-hdseed", "not hex")) && (mapArgs.count("-mnemonic") || mapArgs.count("-mnemonicpassphrase"))) {
        mapArgs.erase("-mnemonic");
        mapArgs.erase("-mnemonicpassphrase");
        LogPrintf("%s: parameter interaction: can't use -hdseed and -mnemonic/-mnemonicpassphrase together, will prefer -seed\n", __func__);
    }

    if (!GetBoolArg("-enableinstantx", fEnableInstanTX)) {
        if (SoftSetArg("-instantxdepth", 0))
            LogPrintf("AppInit2 : parameter interaction: -enableinstantx=false -> setting -nInstanTXDepth=0\n");
    }

    if (mapArgs.count("-reservebalance")) {
        CAmount amount = 0;
        if (ParseMoney(mapArgs["-reservebalance"], amount)) {
            stake->ReserveBalance(amount);
        } else {
            InitError(_("Invalid amount for -reservebalance=<amount>"));
            return false;
        }
    }

    // Make sure additional indexes are recalculated correctly in VerifyDB
    // (we must reconnect blocks whenever we disconnect them for these indexes to work)
    bool fAdditionalIndexes =
        GetBoolArg("-addressindex", DEFAULT_ADDRESSINDEX) ||
        GetBoolArg("-spentindex", DEFAULT_SPENTINDEX);

    if (fAdditionalIndexes && GetArg("-checklevel", DEFAULT_CHECKLEVEL) < 4) {
        ForceSetArg("-checklevel", "4");
        LogPrintf("%s: parameter interaction: additional indexes -> setting -checklevel=4\n", __func__);
    }

    // Make sure enough file descriptors are available
    int nBind = std::max((int)mapArgs.count("-bind") + (int)mapArgs.count("-whitebind"), 1);
    nMaxConnections = GetArg("-maxconnections", 125);
    nMaxConnections = std::max(std::min(nMaxConnections, (int)(FD_SETSIZE - nBind - MIN_CORE_FILEDESCRIPTORS)), 0);
    int nFD = RaiseFileDescriptorLimit(nMaxConnections + MIN_CORE_FILEDESCRIPTORS);
    if (nFD < MIN_CORE_FILEDESCRIPTORS)
        return InitError(_("Not enough file descriptors available."));
    if (nFD - MIN_CORE_FILEDESCRIPTORS < nMaxConnections)
        nMaxConnections = nFD - MIN_CORE_FILEDESCRIPTORS;
    //Temporarily disabled until our chain doesn't grow in size
    /*if (GetArg("-prune", 0)) {
        std::string strLoadError;
        if (GetBoolArg("-txindex", DEFAULT_TXINDEX)) {
            strLoadError = _("You need to add txindex=0 to start pruning");
            bool fRetPrune = uiInterface.ThreadSafeQuestion(
                strLoadError + ".\n\n" + _("Do you want to add automatically add it to the configuration file and close the wallet?"),
                strLoadError + ".\nPlease restart with txindex=0 to prune.",
                "", CClientUIInterface::MSG_ERROR | CClientUIInterface::BTN_ABORT);
            if (fRetPrune){
                nIndexNum = 0;
                WriteConfigToFile("txindex", std::to_string(nIndexNum));
                fReindex = true;
                fRequestRestart = true;  
            } else {
                LogPrintf("Aborted configuration changes. Exiting.\n");
                return false;
            }
        }
    }
    */
    if (fRequestRestart) {
        LogPrintf("Restart requested. Exiting.\n");
        return false;
    }

    // ********************************************************* Step 3: parameter-to-internal-flags

    fDebug = !mapMultiArgs["-debug"].empty();
    // Special-case: if -debug=0/-nodebug is set, turn off debugging messages
    const vector<string>& categories = mapMultiArgs["-debug"];
    if (GetBoolArg("-nodebug", false) || find(categories.begin(), categories.end(), string("0")) != categories.end())
        fDebug = false;

    // check for -mnsec
    fDebugMnSecurity = GetBoolArg("-mnsec", false);

    // Check for -debugnet
    if (GetBoolArg("-debugnet", false))
        InitWarning(_("Warning: Unsupported argument -debugnet ignored, use -debug=net."));
    // Check for -socks - as this is a privacy risk to continue, exit here
    if (mapArgs.count("-socks"))
        return InitError(_("Error: Unsupported argument -socks found. Setting SOCKS version isn't possible anymore, only SOCKS5 proxies are supported."));
    // Check for -tor - as this is a privacy risk to continue, exit here
    if (GetBoolArg("-tor", false))
        return InitError(_("Error: Unsupported argument -tor found, use -onion."));

    if (GetBoolArg("-benchmark", false))
        InitWarning(_("Warning: Unsupported argument -benchmark ignored, use -debug=bench."));

    // Checkmempool and checkblockindex default to true in regtest mode
    mempool.setSanityCheck(GetBoolArg("-checkmempool", chainparams.DefaultConsistencyChecks()));
    fCheckBlockIndex = GetBoolArg("-checkblockindex", chainparams.DefaultConsistencyChecks());
    Checkpoints::fEnabled = GetBoolArg("-checkpoints", true);

    // Mempool size limit
    int64_t maxMempoolSize = GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000;
    if (maxMempoolSize < 0)
        return InitError(_("Error: -maxmempool must be at least %d MB"));

    // -par=0 means autodetect, but nScriptCheckThreads==0 means no concurrency
    nScriptCheckThreads = GetArg("-par", DEFAULT_SCRIPTCHECK_THREADS);
    if (nScriptCheckThreads <= 0)
        nScriptCheckThreads += boost::thread::hardware_concurrency();
    if (nScriptCheckThreads <= 0)
        nScriptCheckThreads = 1;
    else if (nScriptCheckThreads > MAX_SCRIPTCHECK_THREADS)
        nScriptCheckThreads = MAX_SCRIPTCHECK_THREADS;

    fServer = GetBoolArg("-server", false);
    setvbuf(stdout, NULL, _IOLBF, 0); /// ***TODO*** do we still need this after -printtoconsole is gone?

    int64_t nSignedPruneTarget = GetArg("-prune", 0) * 1024 * 1024;
    if (nSignedPruneTarget < 0) {
        return InitError(_("Prune cannot be configured with a negative value."));
    }
    nPruneTarget = (uint64_t) nSignedPruneTarget;
    if (nPruneTarget) {
        if (nPruneTarget < MIN_DISK_SPACE_FOR_BLOCK_FILES) {
            return InitError(strprintf(_("Prune configured below the minimum of %d MB.  Please use a higher number."), MIN_DISK_SPACE_FOR_BLOCK_FILES / 1024 / 1024));
        }
        LogPrintf("Prune configured to target %uMiB on disk for block and undo files.\n", nPruneTarget / 1024 / 1024);
        fPruneMode = true;
    }

#ifdef ENABLE_WALLET
    bool fDisableWallet = GetBoolArg("-disablewallet", false);
#endif

    nConnectTimeout = GetArg("-timeout", DEFAULT_CONNECT_TIMEOUT);
    if (nConnectTimeout <= 0)
        nConnectTimeout = DEFAULT_CONNECT_TIMEOUT;

    // Fee-per-kilobyte amount considered the same as "free"
    // If you are mining, be careful setting this:
    // if you set it to zero then
    // a transaction spammer can cheaply fill blocks using
    // 1-satoshi-fee transactions. It should be set above the real
    // cost to you of processing a transaction.
    if (mapArgs.count("-minrelaytxfee")) {
        CAmount n = 0;
        if (ParseMoney(mapArgs["-minrelaytxfee"], n) && n > 0)
            ::minRelayTxFee = CFeeRate(n);
        else
            return InitError(strprintf(_("Invalid amount for -minrelaytxfee=<amount>: '%s'"), mapArgs["-minrelaytxfee"]));
    }

    fRequireStandard = !GetBoolArg("-acceptnonstandardtxn", !Params().RequireStandard());
    if (Params().RequireStandard() && !fRequireStandard)
        return InitError(strprintf("acceptnonstandardtxn is not currently supported for %s chain", chainparams.NetworkIDString()));


#ifdef ENABLE_WALLET
    if (mapArgs.count("-mintxfee")) {
        CAmount n = 0;
        if (ParseMoney(mapArgs["-mintxfee"], n) && n >= 0)
            CWallet::minTxFee = CFeeRate(n);
        else
            return InitError(strprintf(_("Invalid amount for -mintxfee=<amount>: '%s'"), mapArgs["-mintxfee"]));
    }
    if (mapArgs.count("-paytxfee")) {
        CAmount nFeePerK = 0;
        if (!ParseMoney(mapArgs["-paytxfee"], nFeePerK))
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s'"), mapArgs["-paytxfee"]));
        if (nFeePerK > nHighTransactionFeeWarning)
            InitWarning(_("Warning: -paytxfee is set very high! This is the transaction fee you will pay if you send a transaction."));
        payTxFee = CFeeRate(nFeePerK, 1000);
        if (payTxFee < ::minRelayTxFee) {
            return InitError(strprintf(_("Invalid amount for -paytxfee=<amount>: '%s' (must be at least %s)"),
                mapArgs["-paytxfee"], ::minRelayTxFee.ToString()));
        }
    }
    if (mapArgs.count("-maxtxfee")) {
        CAmount nMaxFee = 0;
        if (!ParseMoney(mapArgs["-maxtxfee"], nMaxFee))
            return InitError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s'"), mapArgs["-maxtxfee"]));
        if (nMaxFee > nHighTransactionMaxFeeWarning)
            InitWarning(_("Warning: -maxtxfee is set very high! Fees this large could be paid on a single transaction."));
        maxTxFee = nMaxFee;
        if (CFeeRate(maxTxFee, 1000) < ::minRelayTxFee) {
            return InitError(strprintf(_("Invalid amount for -maxtxfee=<amount>: '%s' (must be at least the minrelay fee of %s to prevent stuck transactions)"),
                mapArgs["-maxtxfee"], ::minRelayTxFee.ToString()));
        }
    }
    nTxConfirmTarget = GetArg("-txconfirmtarget", 1);
    bSpendZeroConfChange = GetArg("-spendzeroconfchange", true);
    fSendFreeTransactions = GetArg("-sendfreetransactions", false);

    g_address_type = ParseOutputType(GetArg("-addresstype", ""));
    if (g_address_type == OUTPUT_TYPE_NONE) {
        return InitError(strprintf(_("Unknown address type '%s'"), GetArg("-addresstype", "")));
    }

    // If changetype is set in config file or parameter, check that it's valid.
    // Default to OUTPUT_TYPE_NONE if not set.
    g_change_type = ParseOutputType(GetArg("-changetype", ""), OUTPUT_TYPE_NONE);
    if (g_change_type == OUTPUT_TYPE_NONE && !GetArg("-changetype", "").empty()) {
        return InitError(strprintf(_("Unknown change type '%s'"), GetArg("-changetype", "")));
    }

    std::string strWalletFile = GetArg("-wallet", "wallet.dat");
#endif // ENABLE_WALLET

    fIsBareMultisigStd = GetArg("-permitbaremultisig", true) != 0;
    nMaxDatacarrierBytes = GetArg("-datacarriersize", nMaxDatacarrierBytes);

    fAlerts = GetBoolArg("-alerts", DEFAULT_ALERTS);


    if (GetBoolArg("-peerbloomfilters", false))
        nLocalServices = ServiceFlags(nLocalServices | NODE_BLOOM);

    // ********************************************************* Step 4: application initialization: dir lock, daemonize, pidfile, debug log
    // Initialize elliptic curve code
    std::string sha256_algo = SHA256AutoDetect();
    ECC_Start();
    globalVerifyHandle.reset(new ECCVerifyHandle());

    // Sanity check
    if (!InitSanityCheck())
        return InitError(_("Initialization sanity check failed. Luxcore is shutting down."));

    std::string strDataDir = GetDataDir().string();
#ifdef ENABLE_WALLET
    // Wallet file must be a plain filename without a directory
    if (strWalletFile != boost::filesystem::basename(strWalletFile) + boost::filesystem::extension(strWalletFile))
        return InitError(strprintf(_("Wallet %s resides outside data directory %s"), strWalletFile, strDataDir));
#endif
    // Make sure only a single LUX process is using the data directory.
    boost::filesystem::path pathLockFile = GetDataDir() / ".lock";
    FILE* file = fopen(pathLockFile.string().c_str(), "a"); // empty lock file; created if it doesn't exist.
    if (file) fclose(file);
    static boost::interprocess::file_lock lock(pathLockFile.string().c_str());

    // Wait maximum 10 seconds if an old wallet is still running. Avoids lockup during restart
    if (!lock.timed_lock(boost::get_system_time() + boost::posix_time::seconds(10)))
        return InitError(strprintf(_("Cannot obtain a lock on data directory %s. Luxcore is probably already running."), strDataDir));

#ifndef WIN32
    CreatePidFile(GetPidFile(), getpid());
#endif
    if (GetBoolArg("-shrinkdebugfile", !fDebug))
        ShrinkDebugFile();
    LogPrintf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
    LogPrintf("LUX version %s (%s)\n", FormatFullVersion(), CLIENT_DATE);
    LogPrintf("Using OpenSSL version %s\n", SSLeay_version(SSLEAY_VERSION));
    LogPrintf("Using the %s SHA256 implementation\n", sha256_algo.c_str());
#ifdef ENABLE_WALLET
    LogPrintf("Using BerkeleyDB version %s\n", DbEnv::version(0, 0, 0));
#endif

    ////////////////////////////////////////////////////////////////////// // lux
    dev::g_logPost = [&](std::string const& s, char const* c){ LogPrintStr(s + '\n', true); };
    dev::g_logPost(std::string("\n\n\n\n\n\n\n\n\n\n"), NULL);
    //////////////////////////////////////////////////////////////////////

    if (!fLogTimestamps)
        LogPrintf("Startup time: %s\n", DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()));
    LogPrintf("Default data directory %s\n", GetDefaultDataDir().string());
    LogPrintf("Using data directory %s\n", strDataDir);
    LogPrintf("Using config file %s\n", GetConfigFile().string());
    LogPrintf("Using at most %i connections (%i file descriptors available)\n", nMaxConnections, nFD);
    std::ostringstream strErrors;

    InitSignatureCache();

    LogPrintf("Using %u threads for script verification\n", nScriptCheckThreads);
    if (nScriptCheckThreads) {
        for (int i = 0; i < nScriptCheckThreads - 1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
    }

    if (mapArgs.count("-sporkkey")) // spork priv key
    {
        if (!sporkManager.SetPrivKey(GetArg("-sporkkey", "")))
            return InitError(_("Unable to sign spork message, wrong key?"));
    }

    // Start the lightweight task scheduler thread
    CScheduler::Function serviceLoop = boost::bind(&CScheduler::serviceQueue, &scheduler);
    threadGroup.create_thread(boost::bind(&TraceThread<CScheduler::Function>, "scheduler", serviceLoop));

    /* Start the RPC server already.  It will be started in "warmup" mode
     * and not really process calls already (but it will signify connections
     * that the server is there and will be ready later).  Warmup mode will
     * be disabled when initialisation is finished.
     */
    if (fServer) {
        uiInterface.InitMessage.connect(SetRPCWarmupStatus);
        if (!AppInitServers())
            return InitError(_("Unable to start HTTP server. See debug log for details."));
    }

    if (mapArgs.count("-masternodepaymentskey")) // masternode payments priv key
    {
        if (!masternodePayments.SetPrivKey(GetArg("-masternodepaymentskey", "")))
            return InitError(_("Unable to sign masternode payment winner, wrong key?"));
        if (!sporkManager.SetPrivKey(GetArg("-masternodepaymentskey", "")))
            return InitError(_("Unable to sign spork message, wrong key?"));
    }

    //ignore masternodes below protocol version
    CMasterNode::minProtoVersion = GetArg("-masternodeminprotocol", MIN_PROTO_VERSION);

    int64_t nStart = 0;

// ********************************************************* Step 5: Backup wallet and verify wallet database integrity

#ifdef ENABLE_WALLET
    if (!fDisableWallet) {

        LogPrintf("Using wallet %s\n", strWalletFile);
        uiInterface.InitMessage(_("Verifying wallet..."));

        if (!bitdb.Open(GetDataDir())) {
            // try moving the database env out of the way
            boost::filesystem::path pathDatabase = GetDataDir() / "database";
            boost::filesystem::path pathDatabaseBak = GetDataDir() / strprintf("database.%d.bak", GetTime());
            try {
                boost::filesystem::rename(pathDatabase, pathDatabaseBak);
                LogPrintf("Moved old %s to %s. Retrying.\n", pathDatabase.string(), pathDatabaseBak.string());
            } catch (boost::filesystem::filesystem_error& error) {
                // failure is ok (well, not really, but it's not worse than what we started with)
            }

            int max_tries = 10;
            bool isSuccess = false;
            // try again
            while (max_tries > 0)
            {
                if (bitdb.Open(GetDataDir())) {
                    isSuccess = true;
                    break;
                }

                max_tries--;
                MilliSleep(300);
            }

            if (!isSuccess)
            {
                // if it still fails, it probably means we can't even create the database env
                string msg = strprintf(_("Error initializing wallet database environment %s!"), strDataDir);
                return InitError(msg);
            }
        }

        if (GetBoolArg("-salvagewallet", false)) {
            // Recover readable keypairs:
            if (!CWalletDB::Recover(bitdb, strWalletFile, true))
                return false;
        }

        if (filesystem::exists(GetDataDir() / strWalletFile)) {
            CDBEnv::VerifyResult r = bitdb.Verify(strWalletFile, CWalletDB::Recover);
            if (r == CDBEnv::RECOVER_OK) {
                string msg = strprintf(_("Warning: wallet.dat corrupt, data salvaged!"
                                         " Original wallet.dat saved as wallet.{timestamp}.bak in %s; if"
                                         " your balance or transactions are incorrect you should"
                                         " restore from a backup."),
                    strDataDir);
                InitWarning(msg);
            }
            if (r == CDBEnv::RECOVER_FAIL)
                return InitError(_("wallet.dat corrupt, salvage failed"));
        }

        filesystem::path backupDir = GetDataDir() / "backups";
        if (!filesystem::exists(backupDir)) {
            // Always create backup folder to not confuse the operating system's file browser
            filesystem::create_directories(backupDir);
        }

        nWalletBackups = GetArg("-createwalletbackups", 10);
        nWalletBackups = std::max(0, std::min(10, nWalletBackups));

        if (nWalletBackups > 0 && filesystem::exists(backupDir)) {
            // Keep only the last 10 backups, including the new one of course
            typedef std::multimap<std::time_t, boost::filesystem::path> folder_set_t;
            folder_set_t folder_set;
            boost::filesystem::directory_iterator end_iter;
            boost::filesystem::path backupFolder = backupDir.string();
            backupFolder.make_preferred();
            // Build map of backup files for current(!) wallet sorted by last write time
            boost::filesystem::path currentFile;
            for (boost::filesystem::directory_iterator dir_iter(backupFolder); dir_iter != end_iter; ++dir_iter) {
                // Only check regular files
                if (boost::filesystem::is_regular_file(dir_iter->status())) {
                    currentFile = dir_iter->path().filename();
                    // Only add the backups for the current wallet, e.g. wallet.dat.*
                    if (dir_iter->path().stem().string() == strWalletFile) {
                        folder_set.insert(folder_set_t::value_type(boost::filesystem::last_write_time(dir_iter->path()), *dir_iter));
                    }
                }
            }
            // Loop backward through backup files and keep the N newest ones (1 <= N <= 10)
            int counter = 0;
            for (PAIRTYPE(const std::time_t, boost::filesystem::path) file : reverse_iterate(folder_set)) {
                counter++;
                if (counter >= nWalletBackups) {
                    // More than nWalletBackups backups: delete oldest one(s)
                    try {
                        boost::filesystem::remove(file.second);
                        LogPrintf("Old backup deleted: %s\n", file.second);
                    } catch (boost::filesystem::filesystem_error& error) {
                        LogPrintf("Failed to delete backup %s\n", error.what());
                    }
                }
            }

            // Create backup of the wallet
            std::string dateTimeStr = DateTimeStrFormat(".%Y-%m-%d-%H-%M", GetTime());
            std::string backupPathStr = backupDir.string();
            backupPathStr += "/" + strWalletFile;
            std::string sourcePathStr = GetDataDir().string();
            sourcePathStr += "/" + strWalletFile;
            boost::filesystem::path sourceFile = sourcePathStr;
            boost::filesystem::path backupFile = backupPathStr + dateTimeStr;
            sourceFile.make_preferred();
            backupFile.make_preferred();
            if(boost::filesystem::space(backupDir.string()).available > 1000000) {
                if (boost::filesystem::exists(sourceFile)) {
#if BOOST_VERSION >= 158000
                    try {
                        boost::filesystem::copy_file(sourceFile, backupFile);
                        LogPrintf("Creating backup of %s -> %s\n", sourceFile, backupFile);
                    } catch (boost::filesystem::filesystem_error& error) {
                        LogPrintf("Failed to create backup %s\n", error.what());
                    }
#else
                    std::ifstream src(sourceFile.string(), std::ios::binary);
                    std::ofstream dst(backupFile.string(), std::ios::binary);
                    dst << src.rdbuf();
#endif
                }
            } else {
                cwarn << "Not enough available space found for backup wallet on hard drive.";
            }
        }
    }  // (!fDisableWallet)
#endif // ENABLE_WALLET
    // ********************************************************* Step 6: network initialization

    RegisterNodeSignals(GetNodeSignals());

    if (mapArgs.count("-onlynet")) {
        std::set<enum Network> nets;
        for (std::string snet : mapMultiArgs["-onlynet"]) {
            enum Network net = ParseNetwork(snet);
            if (net == NET_UNROUTABLE)
                return InitError(strprintf(_("Unknown network specified in -onlynet: '%s'"), snet));
            nets.insert(net);
        }
        for (int n = 0; n < NET_MAX; n++) {
            enum Network net = (enum Network)n;
            if (!nets.count(net))
                SetLimited(net);
        }
    }

    if (mapArgs.count("-whitelist")) {
        for (const std::string& net : mapMultiArgs["-whitelist"]) {
            CSubNet subnet(net);
            if (!subnet.IsValid())
                return InitError(strprintf(_("Invalid netmask specified in -whitelist: '%s'"), net));
            CNode::AddWhitelistedRange(subnet);
        }
    }

    CService addrProxy;
    bool fProxy = false;
    if (mapArgs.count("-proxy")) {
        addrProxy = CService(mapArgs["-proxy"], 9050);
        if (!addrProxy.IsValid())
            return InitError(strprintf(_("Invalid -proxy address: '%s'"), mapArgs["-proxy"]));

        SetProxy(NET_IPV4, addrProxy);
        SetProxy(NET_IPV6, addrProxy);
        SetNameProxy(addrProxy);
        fProxy = true;
    }

    // -onion can override normal proxy, -noonion disables tor entirely
    if (!(mapArgs.count("-onion") && mapArgs["-onion"] == "0") &&
        (fProxy || mapArgs.count("-onion"))) {
        CService addrOnion;
        if (!mapArgs.count("-onion"))
            addrOnion = addrProxy;
        else
            addrOnion = CService(mapArgs["-onion"], 9050);
        if (!addrOnion.IsValid())
            return InitError(strprintf(_("Invalid -onion address: '%s'"), mapArgs["-onion"]));
        SetProxy(NET_TOR, addrOnion);
        SetReachable(NET_TOR);
    }

    // see Step 2: parameter interactions for more information about these
    fListen = GetBoolArg("-listen", DEFAULT_LISTEN);
    fDiscover = GetBoolArg("-discover", true);
    fNameLookup = GetBoolArg("-dns", true);

    bool fBound = false;
    if (fListen) {
        if (mapArgs.count("-bind") || mapArgs.count("-whitebind")) {
            for (std::string strBind : mapMultiArgs["-bind"]) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, GetListenPort(), false))
                    return InitError(strprintf(_("Cannot resolve -bind address: '%s'"), strBind));
                fBound |= Bind(addrBind, (BF_EXPLICIT | BF_REPORT_ERROR));
            }
            for (std::string strBind : mapMultiArgs["-whitebind"]) {
                CService addrBind;
                if (!Lookup(strBind.c_str(), addrBind, 0, false))
                    return InitError(strprintf(_("Cannot resolve -whitebind address: '%s'"), strBind));
                if (addrBind.GetPort() == 0)
                    return InitError(strprintf(_("Need to specify a port with -whitebind: '%s'"), strBind));
                fBound |= Bind(addrBind, (BF_EXPLICIT | BF_REPORT_ERROR | BF_WHITELIST));
            }
        } else {
            struct in_addr inaddr_any;
            inaddr_any.s_addr = INADDR_ANY;
            fBound |= Bind(CService(in6addr_any, GetListenPort()), BF_NONE);
            fBound |= Bind(CService(inaddr_any, GetListenPort()), !fBound ? BF_REPORT_ERROR : BF_NONE);
        }
        if (!fBound)
            return InitError(_("Failed to listen on any port. Use -listen=0 if you want this."));
    }

    if (mapArgs.count("-externalip")) {
        for (string strAddr : mapMultiArgs["-externalip"]) {
            CService addrLocal(strAddr, GetListenPort(), fNameLookup);
            if (!addrLocal.IsValid())
                return InitError(strprintf(_("Cannot resolve -externalip address: '%s'"), strAddr));
            AddLocal(CService(strAddr, GetListenPort(), fNameLookup), LOCAL_MANUAL);
        }
    }

    for (string strDest : mapMultiArgs["-seednode"])
        AddOneShot(strDest);

#if ENABLE_ZMQ
    pzmqNotificationInterface = CZMQNotificationInterface::CreateWithArguments(mapArgs);

    if (pzmqNotificationInterface) {
        RegisterValidationInterface(pzmqNotificationInterface);
    }
#endif

    // ********************************************************* Step 7: load block chain

    fReindex = GetBoolArg("-reindex", false);
    bool fReindexChainState = GetBoolArg("-reindex-chainstate", false);

    // Upgrading to 0.8; hard-link the old blknnnn.dat files into /blocks/
    filesystem::path blocksDir = GetDataDir() / "blocks";
    if (!filesystem::exists(blocksDir)) {
        filesystem::create_directories(blocksDir);
        bool linked = false;
        for (unsigned int i = 1; i < 10000; i++) {
            filesystem::path source = GetDataDir() / strprintf("blk%04u.dat", i);
            if (!filesystem::exists(source)) break;
            filesystem::path dest = blocksDir / strprintf("blk%05u.dat", i - 1);
            try {
                filesystem::create_hard_link(source, dest);
                LogPrintf("Hardlinked %s -> %s\n", source.string(), dest.string());
                linked = true;
            } catch (filesystem::filesystem_error& e) {
                // Note: hardlink creation failing is not a disaster, it just means
                // blocks will get re-downloaded from peers.
                LogPrintf("Error hardlinking blk%04u.dat : %s\n", i, e.what());
                break;
            }
        }
        if (linked) {
            fReindex = true;
        }
    }

    // cache size calculations
    int64_t nTotalCache = (GetArg("-dbcache", nDefaultDbCache) << 20);
    nTotalCache = max(nTotalCache, nMinDbCache << 20); // total cache cannot be less than nMinDbCache
    nTotalCache = min(nTotalCache, nMaxDbCache << 20); // total cache cannot be greater than nMaxDbCache
    int64_t nBlockTreeDBCache = nTotalCache / 8;
    nBlockTreeDBCache = min(nBlockTreeDBCache, (GetBoolArg("-txindex", DEFAULT_TXINDEX) ? nMaxBlockDBAndTxIndexCache : nMaxBlockDBCache) << 20);
    nTotalCache -= nBlockTreeDBCache;
    int64_t nCoinDBCache = std::min(nTotalCache / 2, (nTotalCache / 4) + (1 << 23)); // use 50% the remaining cache for coindb cache
    nCoinDBCache = min(nCoinDBCache, nMaxCoinsDBCache << 20); // cap total coins db cache
    nTotalCache -= nCoinDBCache;
    nCoinCacheUsage = nTotalCache; // the rest goes to in-memory cache (300bytes)

    bool fLoaded = false;
    while (!fLoaded && !fRequestShutdown) {
        bool fReset = fReindex;
        std::string strLoadError;

        uiInterface.InitMessage(_("Loading block index..."));

        LOCK(cs_main);

        nStart = GetTimeMillis();
        do {
            try {
                UnloadBlockIndex();
                delete pcoinsTip;
                delete pcoinsdbview;
                delete pcoinscatcher;
                delete pblocktree;
                delete pstorageresult;
                globalState.reset();
                globalSealEngine.reset();

                pblocktree = new CBlockTreeDB(nBlockTreeDBCache, false, fReindex);
                pcoinsdbview = new CCoinsViewDB(nCoinDBCache, false, fReindex || fReindexChainState);
                pcoinscatcher = new CCoinsViewErrorCatcher(pcoinsdbview);
                pcoinsTip = new CCoinsViewCache(pcoinscatcher);

                if (fReset) {
                    pblocktree->WriteReindexing(true);
                    if (fPruneMode)
                        DeleteAllBlockFiles();
                }

                if (fRequestShutdown) {
                    LogPrintf("Shutdown requested. Exiting.\n");
                    return false;
                }

                dev::eth::Ethash::init();

                boost::filesystem::path luxStateDir = GetDataDir() / "stateLux";

                bool fStatus = boost::filesystem::exists(luxStateDir);
                const std::string dirLux(luxStateDir.string());
                const dev::h256 hashDB(dev::sha3(dev::rlp("")));
                dev::eth::BaseState existsLuxState = fStatus ? dev::eth::BaseState::PreExisting : dev::eth::BaseState::Empty;
                globalState = std::unique_ptr<LuxState>(new LuxState(dev::u256(0), LuxState::openDB(dirLux, hashDB, dev::WithExisting::Trust), dirLux, existsLuxState));
                dev::eth::ChainParams cp((dev::eth::genesisInfo(dev::eth::Network::luxMainNetwork)));
                globalSealEngine = std::unique_ptr<dev::eth::SealEngineFace>(cp.createSealEngine());

                pstorageresult = new StorageResults(luxStateDir.string());
                if (fReset) {
                    pstorageresult->wipeResults();
                }

                // LoadBlockIndex will load fTxIndex from the db, or set it if
                // we're reindexing. It will also load fHavePruned if we've
                // ever removed a block file from disk.
                // Note that it also sets fReindex based on the disk flag!
                // From here on out fReindex and fReset mean something different!
                if (!LoadBlockIndex()) {
                    if (fRequestShutdown)
                    {
                        LogPrintf("Shutdown requested. Exiting.\n");
                        return false;
                    }
                    strLoadError = _("Error loading block database");
                    break;
                }

                // If the loaded chain has a wrong genesis, bail out immediately
                // (we're likely using a testnet datadir, or the other way around).
                if (!mapBlockIndex.empty() && mapBlockIndex.count(chainparams.GetConsensus().hashGenesisBlock) == 0)
                    return InitError(_("Incorrect or no genesis block found. Wrong datadir for network?"));

                /////////////////////////////////////////////////////////// lux
                if((IsArgSet("-dgpstorage") && IsArgSet("-dgpevm")) || (!IsArgSet("-dgpstorage") && IsArgSet("-dgpevm")) ||
                   (!IsArgSet("-dgpstorage") && !IsArgSet("-dgpevm"))) {
                    fGettingValuesDGP = true;
                } else {
                    fGettingValuesDGP = false;
                }

                if(chainActive.Tip() != nullptr && chainActive.Tip()->nHeight > Params().FirstSCBlock()){
                    globalState->setRoot(uintToh256(chainActive.Tip()->hashStateRoot));
                    globalState->setRootUTXO(uintToh256(chainActive.Tip()->hashUTXORoot));
                } else {
                    globalState->setRoot(dev::sha3(dev::rlp("")));
                    globalState->setRootUTXO(uintToh256(chainparams.GenesisBlock().hashUTXORoot));
                    globalState->populateFrom(cp.genesisState);
                }
                globalState->db().commit();
                globalState->dbUtxo().commit();


                fRecordLogOpcodes = GetBoolArg("-record-log-opcodes", true);
                fIsVMlogFile = boost::filesystem::exists(GetDataDir() / "vmExecLogs.json");
                ///////////////////////////////////////////////////////////

                // Initialize the block index (no-op if non-empty database was already loaded)
                if (!InitBlockIndex(chainparams)) {
                    strLoadError = _("Error initializing block database");
                    break;
                }

                // Check for changed -txindex state
                if (fTxIndex != GetBoolArg("-txindex", DEFAULT_TXINDEX)) {
                    strLoadError = _("You need to rebuild the database using -reindex to change -txindex");
                    break;
                }

                // Check for changed -logevents state
                if (fLogEvents != GetBoolArg("-logevents", DEFAULT_LOGEVENTS) && !fLogEvents) {
                    strLoadError = _("You need to rebuild the database using -reindex-chainstate to enable -logevents");
                    break;
                }

                if (!GetBoolArg("-logevents", DEFAULT_LOGEVENTS))
                {
                    pstorageresult->wipeResults();
                    pblocktree->WipeHeightIndex();
                    fLogEvents = false;
                    pblocktree->WriteFlag("logevents", fLogEvents);
                }

                nLogFile = GetArg("-nlogfile", 1);

                // Check for changed -prune state.  What we are concerned about is a user who has pruned blocks
                // in the past, but is now trying to run unpruned.
                if (fHavePruned && !fPruneMode) {
                    strLoadError = _("You need to rebuild the database using -reindex to go back to unpruned mode.  This will redownload the entire blockchain");
                    break;
                }

                uiInterface.InitMessage(_("Verifying blocks..."));

                if (fHavePruned && GetArg("-checkblocks", 288) > MIN_BLOCKS_TO_KEEP) {
                    LogPrintf("Prune: pruned datadir may not have more than %d blocks; -checkblocks=%d may fail\n",
                              MIN_BLOCKS_TO_KEEP, GetArg("-checkblocks", 288));
                }

                {
                    //LOCK(cs_main);
                    CBlockIndex* tip = chainActive.Tip();
                    RPCNotifyBlockChange(true, tip);
                    if (tip && tip->nTime > GetAdjustedTime() + 2 * 60 * 60) {
                        strLoadError = _("The block database contains a block which appears to be from the future. "
                                         "This may be due to your computer's date and time being set incorrectly. "
                                         "Only rebuild the block database if you are sure that your computer's date and time are correct");
                        break;
                    }
                }

                if (!CVerifyDB().VerifyDB(chainparams, pcoinsdbview, GetArg("-checklevel", 3),
                        GetArg("-checkblocks", 500))) {
                    strLoadError = _("Corrupted block database detected");
                    break;
                }
            } catch (std::exception& e) {
                LogPrintf("%s\n", e.what());
                strLoadError = _("Error opening block database");
                break;
            }

            fLoaded = true;
        } while (false);

        if (!fLoaded && !fRequestShutdown) {
            // first suggest a reindex
            if (!fReset) {
                bool fRet = uiInterface.ThreadSafeQuestion(
                    strLoadError + ".\n\n" + _("Do you want to rebuild the block database now?"),
                    strLoadError + ".\nPlease restart with -reindex or -reindex-chainstate to recover.",
                    "", CClientUIInterface::MSG_ERROR | CClientUIInterface::BTN_ABORT);
                if (fRet) {
                    fReindex = true;
                    fRequestShutdown = false;
                } else {
                    LogPrintf("Aborted block database rebuild. Exiting.\n");
                    return false;
                }
            } else {
                return InitError(strLoadError);
            }
        }
    }

    // As LoadBlockIndex can take several minutes, it's possible the user
    // requested to kill the GUI during the last operation. If so, exit.
    // As the program has not fully started yet, Shutdown() is possibly overkill.
    if (fRequestShutdown) {
        LogPrintf("Shutdown requested. Exiting.\n");
        return false;
    }

    boost::filesystem::path est_path = GetDataDir() / FEE_ESTIMATES_FILENAME;
    CAutoFile est_filein(fopen(est_path.string().c_str(), "rb"), SER_DISK, CLIENT_VERSION);
    // Allowed to fail as this file IS missing on first startup.
    if (!est_filein.IsNull())
        mempool.ReadFeeEstimates(est_filein);
    fFeeEstimatesInitialized = true;

// ********************************************************* Step 8: load wallet
#ifdef ENABLE_WALLET
    if (fDisableWallet) {
        pwalletMain = NULL;
        LogPrintf("Wallet disabled!\n");
    } else {
        // needed to restore wallet transaction meta data after -zapwallettxes
        std::vector<CWalletTx> vWtx;

        if (GetBoolArg("-zapwallettxes", false)) {
            uiInterface.InitMessage(_("Zapping all transactions from wallet..."));

            pwalletMain = new CWallet(strWalletFile);
            DBErrors nZapWalletRet = pwalletMain->ZapWalletTx(vWtx);
            if (nZapWalletRet != DB_LOAD_OK) {
                uiInterface.InitMessage(_("Error loading wallet.dat: Wallet corrupted"));
                return false;
            }

            delete pwalletMain;
            pwalletMain = NULL;
        }

        uiInterface.InitMessage(_("Loading wallet..."));

        nStart = GetTimeMillis();
        bool fFirstRun = true;
        pwalletMain = new CWallet(strWalletFile);
        DBErrors nLoadWalletRet = pwalletMain->LoadWallet(fFirstRun);
        if (nLoadWalletRet != DB_LOAD_OK) {
            if (nLoadWalletRet == DB_CORRUPT)
                strErrors << _("Error loading wallet.dat: Wallet corrupted") << "\n";
            else if (nLoadWalletRet == DB_NONCRITICAL_ERROR) {
                string msg(_("Warning: error reading wallet.dat! All keys read correctly, but transaction data"
                             " or address book entries might be missing or incorrect."));
                InitWarning(msg);
            } else if (nLoadWalletRet == DB_TOO_NEW)
                strErrors << _("Error loading wallet.dat: Wallet requires newer version of Luxcore") << "\n";
            else if (nLoadWalletRet == DB_NEED_REWRITE) {
                strErrors << _("Wallet needed to be rewritten: restart Luxcore to complete") << "\n";
                LogPrintf("%s", strErrors.str());
                return InitError(strErrors.str());
            } else
                strErrors << _("Error loading wallet.dat") << "\n";
        }

        if (GetBoolArg("-upgradewallet", fFirstRun)) {
            int nMaxVersion = GetArg("-upgradewallet", 0);
            if (nMaxVersion == 0) // the -upgradewallet without argument case
            {
                LogPrintf("Performing wallet upgrade to %i\n", FEATURE_LATEST);
                nMaxVersion = CLIENT_VERSION;
                pwalletMain->SetMinVersion(FEATURE_LATEST); // permanently upgrade the wallet immediately
            } else
                LogPrintf("Allowing wallet upgrade up to %i\n", nMaxVersion);
            if (nMaxVersion < pwalletMain->GetVersion())
                strErrors << _("Cannot downgrade wallet") << "\n";
            pwalletMain->SetMaxVersion(nMaxVersion);
        }

        // Upgrade to HD if explicit upgrade
        if (GetBoolArg("-upgradewallet", false)) {
            LOCK(pwalletMain->cs_wallet);
            bool hd_upgrade = false;
            bool useHD = GetBoolArg("-usehd", DEFAULT_USE_HD_WALLET);
            if (pwalletMain->CanSupportFeature(FEATURE_HD)) {
                if (useHD && !pwalletMain->IsHDEnabled()) {
                    LogPrintf("Upgrading wallet to HD\n");
                    pwalletMain->SetMinVersion(FEATURE_HD);
                    // generate a new master key
                    pwalletMain->GenerateNewHDChain();
                    hd_upgrade = true;
                }
            }

            if (!useHD && pwalletMain->IsHDEnabled()) {
                hd_upgrade = true; // NewKeyPool
            }

            // Regenerate the keypool if needed
            if (hd_upgrade) {
                if (!pwalletMain->NewKeyPool()) {
                    return InitError(_("Unable to generate keys") += "\n");
                }
            }
        }

        if (fFirstRun) {
            // Create new keyUser and set as default key

            if (GetBoolArg("-usehd", DEFAULT_USE_HD_WALLET) && !pwalletMain->IsHDEnabled()) {
                if (GetArg("-mnemonicpassphrase", "").size() > 256)
                    return InitError(_("Mnemonic passphrase is too long, must be at most 256 characters"));
                // generate a new master key
                pwalletMain->GenerateNewHDChain();
            }

            CPubKey newDefaultKey;
            if (pwalletMain->GetKeyFromPool(newDefaultKey, false)) {
                pwalletMain->SetDefaultKey(newDefaultKey);
                if (!pwalletMain->SetAddressBook(pwalletMain->vchDefaultKey.GetID(), "", "receive"))
                return InitError(_("Cannot write default address") += "\n");
            }

            pwalletMain->SetBestChain(chainActive.GetLocator());
        }

        else if (mapArgs.count("-usehd")) {
            bool useHD = GetBoolArg("-usehd", DEFAULT_USE_HD_WALLET);
            bool reset = GetBoolArg("-salvagewallet", false) || GetBoolArg("-upgradewallet", false);
            if (pwalletMain->IsHDEnabled() && (!useHD && !reset)) {
                return InitError(strprintf(_("Error loading %s: You can't disable HD on a already existing HD wallet"), strWalletFile));
            }
            if (!pwalletMain->IsHDEnabled() && useHD) {
                return InitError(strprintf(_("Error loading %s: You need to upgrade the wallet to enable HD on a non-HD wallet"), strWalletFile));
            }
        }
#if 0
        // Warn user every time he starts non-encrypted HD wallet
        if (GetBoolArg("-usehd", DEFAULT_USE_HD_WALLET) && !pwalletMain->IsLocked()) {
            InitWarning(_("Make sure to encrypt your wallet and delete all non-encrypted backups after you verified that wallet works!"));
        }
#endif
        LogPrintf("%s", strErrors.str());
        LogPrintf(" wallet      %15dms\n", GetTimeMillis() - nStart);

        RegisterValidationInterface(pwalletMain);
        pwalletMain->TopUpKeyPool();
        CBlockIndex* pindexRescan = chainActive.Tip();
        if (GetBoolArg("-rescan", false))
            pindexRescan = chainActive.Genesis();
        else {
            CWalletDB walletdb(strWalletFile);
            CBlockLocator locator;
            if (walletdb.ReadBestBlock(locator))
                pindexRescan = FindForkInGlobalIndex(chainActive, locator);
            else
                pindexRescan = chainActive.Genesis();
        }
        if (chainActive.Tip() && chainActive.Tip() != pindexRescan) {

            if (fPruneMode) {
                CBlockIndex *block = chainActive.Tip();
                while (block && block->pprev && (block->pprev->nStatus & BLOCK_HAVE_DATA) && block->pprev->nTx > 0 && pindexRescan != block)
                    block = block->pprev;
                if (pindexRescan != block) {
                    return InitError(_("Prune: last wallet synchronisation goes beyond pruned data. You need to -reindex (download the whole blockchain again in case of pruned node)") += "\n");
                }
            }

            uiInterface.InitMessage(_("Rescanning..."));
            LogPrintf("Rescanning last %i blocks (from block %i)...\n", chainActive.Height() - pindexRescan->nHeight, pindexRescan->nHeight);
            nStart = GetTimeMillis();
            pwalletMain->ScanForWalletTransactions(pindexRescan, true);
            LogPrintf(" rescan      %15dms\n", GetTimeMillis() - nStart);
            pwalletMain->SetBestChain(chainActive.GetLocator());
            CWalletDB::IncrementUpdateCounter();

            // Restore wallet transaction metadata after -zapwallettxes=1
            if (GetBoolArg("-zapwallettxes", false) && GetArg("-zapwallettxes", "1") != "2") {
                CWalletDB walletdb(pwalletMain->strWalletFile);
                for (const CWalletTx& wtxOld : vWtx) {
                    uint256 hash = wtxOld.GetHash();
                    std::map<uint256, CWalletTx>::iterator mi = pwalletMain->mapWallet.find(hash);
                    if (mi != pwalletMain->mapWallet.end()) {
                        const CWalletTx* copyFrom = &wtxOld;
                        CWalletTx* copyTo = &mi->second;
                        copyTo->mapValue = copyFrom->mapValue;
                        copyTo->vOrderForm = copyFrom->vOrderForm;
                        copyTo->nTimeReceived = copyFrom->nTimeReceived;
                        copyTo->nTimeSmart = copyFrom->nTimeSmart;
                        copyTo->fFromMe = copyFrom->fFromMe;
                        copyTo->strFromAccount = copyFrom->strFromAccount;
                        copyTo->nOrderPos = copyFrom->nOrderPos;
                        walletdb.WriteTx(*copyTo);
                    }
                }
            }
        }
        pwalletMain->SetBroadcastTransactions(GetBoolArg("-walletbroadcast", true));
    }  // (!fDisableWallet)
#else  // ENABLE_WALLET
    LogPrintf("No wallet compiled in!\n");
#endif // !ENABLE_WALLET
    // ********************************************************* Step 9: import blocks

    // if prune mode, unset NODE_NETWORK and prune block files
    if (fPruneMode) {
        LogPrintf("Unsetting NODE_NETWORK on prune mode\n");
        nLocalServices = ServiceFlags(nLocalServices | NODE_NETWORK);
        if (!fReindex) {
            uiInterface.InitMessage(_("Pruning blockstore..."));
            PruneAndFlush();
        }
    }

    if (!CheckDiskSpace())
        return false;

    if (chainparams.GetConsensus().vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout != 0) {
        // Only advertize witness capabilities if they have a reasonable start time.
        // This allows us to have the code merged without a defined softfork, by setting its
        // end time to 0.
        // Note that setting NODE_WITNESS is never required: the only downside from not
        // doing so is that after activation, no upgraded nodes will fetch from you.
        nLocalServices = ServiceFlags(nLocalServices | NODE_WITNESS);
        // Only care about others providing witness capabilities if there is a softfork
        // defined.
        nRelevantServices = ServiceFlags(nRelevantServices | NODE_WITNESS);
    }

    if (mapArgs.count("-blocknotify"))
        uiInterface.NotifyBlockTip.connect(BlockNotifyCallback);

    // scan for better chains in the block chain database, that are not yet connected in the active best chain
    CValidationState state;
    if (!ActivateBestChain(state, chainparams))
        strErrors << "Failed to connect best block";

    std::vector<boost::filesystem::path> vImportFiles;
    if (mapArgs.count("-loadblock")) {
        for (string strFile : mapMultiArgs["-loadblock"])
            vImportFiles.push_back(strFile);
    }
    threadGroup.create_thread(boost::bind(&ThreadImport, vImportFiles));
    if (chainActive.Tip() == NULL) {
        LogPrintf("Waiting for genesis block to be imported...\n");
        while (!fRequestShutdown && chainActive.Tip() == NULL)
            MilliSleep(10);
    }

    // Add wallet transactions that aren't already in a block to mempool
    // Do this here as mempool requires genesis block to be loaded
#ifdef ENABLE_WALLET
    if (pwalletMain)
        pwalletMain->ReacceptWalletTransactions();
#endif

    // ********************************************************* Step 10: setup Darksend

    if (!strErrors.str().empty())
        return InitError(strErrors.str());

    fMasterNode = GetBoolArg("-masternode", false);
    if (fMasterNode) {
        LogPrintf("IS DARKSEND MASTER NODE\n");

        strMasterNodeAddr = GetArg("-masternodeaddr", "");
        if (!strMasterNodeAddr.empty()) {
            CService addrTest = CService(strMasterNodeAddr);
            if (!addrTest.IsValid()) {
                return InitError("Invalid -masternodeaddr address: " + strMasterNodeAddr);
            }
        }

        strMasterNodePrivKey = GetArg("-masternodeprivkey", "");
        if (!strMasterNodePrivKey.empty()) {
            std::string errorMessage;

            CKey key;
            CPubKey pubkey;

            if (!darkSendSigner.SetKey(strMasterNodePrivKey, errorMessage, key, pubkey)) {
                return InitError(_("Invalid masternodeprivkey. Please see documenation."));
            }

            activeMasternode.pubKeyMasternode = pubkey;
        } else {
            return InitError(_("You must specify a masternodeprivkey in the configuration. Please see documentation for help."));
        }
    }

    fEnableDarksend = GetBoolArg("-enabledarksend", false);

    nDarksendRounds = GetArg("-darksendrounds", 2);
    if (nDarksendRounds > 16) nDarksendRounds = 16;
    if (nDarksendRounds < 1) nDarksendRounds = 1;

    nLiquidityProvider = GetArg("-liquidityprovider", 0); //0-100
    if (nLiquidityProvider != 0) {
        darkSendPool.SetMinBlockSpacing(std::min(nLiquidityProvider,100)*15);
        fEnableDarksend = true;
        nDarksendRounds = 99999;
    }

    nAnonymizeLuxAmount = GetArg("-anonymizeLuxamount", 0);
    if (nAnonymizeLuxAmount > 999999) nAnonymizeLuxAmount = 999999;
    if (nAnonymizeLuxAmount < 2) nAnonymizeLuxAmount = 2;

    LogPrintf("LUX darksend rounds %d\n", nDarksendRounds);
    LogPrintf("Anonymize Lux Amount %d\n", nAnonymizeLuxAmount);

    /* Denominations
       A note about convertability. Within Darksend pools, each denomination
       is convertable to another.
       For example:
       1Lux+1000 == (.1Lux+100)*10
       10Lux+10000 == (1Lux+1000)*10
    */
    darkSendDenominations.push_back( (100000      * COIN)+100000000 );
    darkSendDenominations.push_back( (10000       * COIN)+10000000 );
    darkSendDenominations.push_back( (1000        * COIN)+1000000 );
    darkSendDenominations.push_back( (100         * COIN)+100000 );
    darkSendDenominations.push_back( (10          * COIN)+10000 );
    darkSendDenominations.push_back( (1           * COIN)+1000 );
    darkSendDenominations.push_back( (.1          * COIN)+100 );
    /* Disabled till we need them
    darkSendDenominations.push_back( (.01      * COIN)+10 );
    darkSendDenominations.push_back( (.001     * COIN)+1 );
    */

    darkSendPool.InitCollateralAddress();

    threadGroup.create_thread(boost::bind(&ThreadCheckDarkSendPool));

    // ********************************************************* Step 11: start node


    StartNode(threadGroup, scheduler);

    //// debug print
    LogPrintf("mapBlockIndex.size() = %u\n", mapBlockIndex.size());
    LogPrintf("chainActive.Height() = %d\n", chainActive.Height());
#ifdef ENABLE_WALLET
    if (pwalletMain) {
        LOCK(pwalletMain->cs_wallet);
        LogPrintf("setExternalKeyPool.size() = %u\n",   pwalletMain->KeypoolCountExternalKeys());
        LogPrintf("setInternalKeyPool.size() = %u\n",   pwalletMain->KeypoolCountInternalKeys());
        LogPrintf("mapWallet.size() = %u\n",            pwalletMain->mapWallet.size());
        LogPrintf("mapAddressBook.size() = %u\n",       pwalletMain->mapAddressBook.size());
    } else {
        LogPrintf("wallet is NULL\n");
    }

#   ifdef ENABLE_CPUMINER
    // Generate coins in the background
    if (GetBoolArg("-gen", false) && pwalletMain) {
        GenerateBitcoins(pwalletMain, GetArg("-genproclimit", 1));
    }
#   endif
#endif

    // ********************************************************* Step 12: finished

    SetRPCWarmupFinished();
    uiInterface.InitMessage(_("Done loading"));

#ifdef ENABLE_WALLET
    if (pwalletMain) {
        // Add wallet transactions that aren't already in a block to mapTransactions
        pwalletMain->ReacceptWalletTransactions();
        std::atomic<bool> fFlushScheduled(false);
        // Run a thread to flush wallet periodically
        if (!fFlushScheduled.exchange(true)) {
        scheduler.scheduleEvery(MaybeFlushWalletDB, 500);
    }
    }
#endif

    threadGroup.create_thread(boost::bind(&ThreadSendAlert));

    return !fRequestShutdown;
}

void UnlockDataDirectory()
{
    // Try lock and unlock
    LockDataDirectory(true);
}
