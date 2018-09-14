// Copyright (c) 2009-2010 Satoshi Nakamoto             -*- c++ -*-
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Server/client environment: argument handling, config file parsing,
 * logging, thread wrappers
 */
#ifndef BITCOIN_UTIL_H
#define BITCOIN_UTIL_H

#if defined(HAVE_CONFIG_H)
#include "config/lux-config.h"
#endif

#include "compat.h"
#include "tinyformat.h"
#include "utiltime.h"

#include <exception>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include <boost/filesystem/path.hpp>
#include <boost/thread/exceptions.hpp>

//LUX only features
extern std::atomic<bool> hideLogMessage;

extern int nLogFile;
extern bool fMasterNode;
extern bool fEnableInstanTX;
extern int nInstanTXDepth;
extern int nDarksendRounds;
extern int nWalletBackups;
extern int nAnonymizeLuxAmount;
extern int nLiquidityProvider;
extern bool fEnableDarksend;
extern int64_t enforceMasternodePaymentsTime;
extern std::string strMasterNodeAddr;
extern int keysLoaded;
extern bool fSucessfullyLoaded;
extern std::vector<int64_t> darkSendDenominations;
extern std::string strBudgetMode;

extern std::map<std::string, std::string> mapArgs;
extern std::map<std::string, std::vector<std::string> > mapMultiArgs;
extern bool fDebug;
extern bool fDebugMnSecurity;
extern bool fPrintToConsole;
extern bool fPrintToDebugLog;
extern bool fServer;
extern std::string strMiscWarning;
extern bool fLogTimestamps;
extern bool fLogIPs;
extern volatile bool fReopenDebugLog;

void SetupEnvironment();

/** Return true if log accepts specified category */
bool LogAcceptCategory(const char* category);
/** Send a string to the log output */
int LogPrintStr(const std::string& str, bool useVMLog = false);
/** Push debug files */
void pushDebugLog(std::string pathDebugStr, int debugNum);

#define LogPrintf(...) LogPrint(NULL, __VA_ARGS__)

/**
 * When we switch to C++11, this can be switched to variadic templates instead
 * of this macro-based construction (see tinyformat.h).
 */
#define MAKE_ERROR_AND_LOG_FUNC(n)                                                              \
    /**   Print to debug.log if -debug=category switch is given OR category is NULL. */         \
    template <TINYFORMAT_ARGTYPES(n)>                                                           \
    static inline int LogPrint(const char* category, const char* format, TINYFORMAT_VARARGS(n)) \
    {                                                                                           \
        try {                                                                                   \
            if (!LogAcceptCategory(category)) return 0;                                         \
            LogPrintStr(tfm::format(format, TINYFORMAT_PASSARGS(n)));                           \
        } catch (std::runtime_error &e) {                                                       \
            /* Original format string will have newline so don't add one here */                \
            LogPrintStr("ERROR \"" + std::string(e.what()) + "\" while formatting log message: "\
                                   + format);                                                   \
        }                                                                                       \
        return 0;                                                                               \
    }                                                                                           \
    /**   Log error and return false */                                                         \
    template <TINYFORMAT_ARGTYPES(n)>                                                           \
    static inline bool error(const char* format, TINYFORMAT_VARARGS(n))                         \
    {                                                                                           \
        try {                                                                                   \
            LogPrintStr("ERROR: " + tfm::format(format, TINYFORMAT_PASSARGS(n)) + "\n");        \
        } catch (std::runtime_error &e) {                                                       \
            LogPrintStr("ERROR \"" + std::string(e.what()) + "\" while formatting log message: "\
                                   + format + "\n");                                            \
        }                                                                                       \
        return false;                                                                           \
    }

TINYFORMAT_FOREACH_ARGNUM(MAKE_ERROR_AND_LOG_FUNC)

/**
 * Zero-arg versions of logging and error, these are not covered by
 * TINYFORMAT_FOREACH_ARGNUM
 */
static inline int LogPrint(const char* category, const char* format)
{
    if (!LogAcceptCategory(category)) return 0;
    return LogPrintStr(format);
}
static inline bool error(const char* format)
{
    LogPrintStr(std::string("ERROR: ") + format + "\n");
    return false;
}

void PrintExceptionContinue(std::exception* pex, const char* pszThread);
void ParseParameters(int argc, const char* const argv[]);
void FileCommit(FILE* fileout);
bool TruncateFile(FILE* file, unsigned int length);
int RaiseFileDescriptorLimit(int nMinFD);
void AllocateFileRange(FILE* file, unsigned int offset, unsigned int length);
bool RenameOver(boost::filesystem::path src, boost::filesystem::path dest);
bool TryCreateDirectory(const boost::filesystem::path& p);
boost::filesystem::path GetDefaultDataDir();
const boost::filesystem::path& GetDataDir(bool fNetSpecific = true);
boost::filesystem::path GetConfigFile();
boost::filesystem::path GetMasternodeConfigFile();
#ifndef WIN32
boost::filesystem::path GetPidFile();
void CreatePidFile(const boost::filesystem::path& path, pid_t pid);
#endif
void ReadConfigFile(std::map<std::string, std::string>& mapSettingsRet, std::map<std::string, std::vector<std::string> >& mapMultiSettingsRet);
void WriteConfigToFile(std::string strKey, std::string strValue);
#ifdef WIN32
boost::filesystem::path GetSpecialFolderPath(int nFolder, bool fCreate = true);
#endif
boost::filesystem::path GetTempPath();
void ShrinkDebugFile();
void runCommand(std::string strCommand);

inline bool IsSwitchChar(char c)
{
#ifdef WIN32
    return c == '-' || c == '/';
#else
    return c == '-';
#endif
}

/**
 * Return true if the given argument has been manually set
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @return true if the argument has been set
 */
bool IsArgSet(const std::string& strArg);

/**
 * Return string argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. "1")
 * @return command-line argument or default value
 */
std::string GetArg(const std::string& strArg, const std::string& strDefault);

/**
 * Return integer argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (e.g. 1)
 * @return command-line argument (0 if invalid number) or default value
 */
int64_t GetArg(const std::string& strArg, int64_t nDefault);

/**
 * Return boolean argument or default value
 *
 * @param strArg Argument to get (e.g. "-foo")
 * @param default (true or false)
 * @return command-line argument or default value
 */
bool GetBoolArg(const std::string& strArg, bool fDefault);

/**
 * Set an argument if it doesn't already have a value
 *
 CBlock block;
    CBlockIndex* pblockindex =chainActive[nHeight];
    std::string strHash =  pblockindex->GetBlockHash().GetHex();
    uint256 hash(strHash);
    CBlockIndex* pblockindex2 = mapBlockIndex[hash];
    //  a.push_back();
    return pblockindex2;
} to set (e.g. "-foo")
 * @param strValue Value (e.g. "1")
 * @return true if argument gets set, false if it already had a value
 */

bool SoftSetArg(const std::string& strArg, const std::string& strValue);

/**
 * Set a boolean argument if it doesn't already have a value
 *
 * @param strArg Argument to set (e.g. "-foo")
 * @param fValue Value (e.g. false)
 * @return true if argument gets set, false if it already had a value
 */
bool SoftSetBoolArg(const std::string& strArg, bool fValue);

// Forces a arg setting
void ForceSetArg(const std::string& strArg, const std::string& strValue);

void SetThreadPriority(int nPriority);
void RenameThread(const char* name);

inline uint32_t ByteReverse(uint32_t value)
{
    value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
    return (value<<16) | (value>>16);
}

/**
 * .. and a wrapper that just calls func once
 */
template <typename Callable>
void TraceThread(const char* name, Callable func)
{
    std::string s = strprintf("lux-%s", name);
    RenameThread(s.c_str());
    try {
        LogPrintf("%s thread start\n", name);
        func();
        LogPrintf("%s thread exit\n", name);
    } catch (boost::thread_interrupted) {
        LogPrintf("%s thread interrupt\n", name);
        // rethrow exception if current thread is not the "net" thread
        if (strcmp(name, "net")) throw;
    } catch (std::exception& e) {
        PrintExceptionContinue(&e, name);
        // rethrow exception if current thread is not the "net" thread
        if (strcmp(name, "net")) throw;
    } catch (...) {
        PrintExceptionContinue(NULL, name);
        // rethrow exception if current thread is not the "net" thread
        if (strcmp(name, "net")) throw;
    }
}

bool CheckHex(const std::string& str);

#endif // BITCOIN_UTIL_H
