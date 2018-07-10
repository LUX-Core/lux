// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Lux Test Suite

#include "main.h"
#include "random.h"
#include "txdb.h"
#include "ui_interface.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "db.h"
#include "wallet.h"
#endif

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

CClientUIInterface uiInterface;
CWallet* pwalletMain;

extern bool fPrintToConsole;
extern void noui_connect();

struct TestingSetup {
    CCoinsViewDB *pcoinsdbview;
    boost::filesystem::path pathTemp;
    boost::thread_group threadGroup;

    TestingSetup() {
        SetupEnvironment();
        fPrintToDebugLog = false; // don't want to write to debug.log file
        fCheckBlockIndex = true;
        SelectParams(CBaseChainParams::UNITTEST);
        noui_connect();
#ifdef ENABLE_WALLET
        bitdb.MakeMock();
#endif
        pathTemp = GetTempPath() / strprintf("test_lux_%lu_%i", (unsigned long)GetTime(), (int)(GetRand(100000)));
        boost::filesystem::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
        pblocktree = new CBlockTreeDB(1 << 20, true);
        pcoinsdbview = new CCoinsViewDB(1 << 23, true);
        pcoinsTip = new CCoinsViewCache(pcoinsdbview);
        InitBlockIndex(Params());
#ifdef ENABLE_WALLET
        bool fFirstRun;
        pwalletMain = new CWallet("wallet.dat");
        pwalletMain->LoadWallet(fFirstRun);
        RegisterValidationInterface(pwalletMain);
#endif
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        RegisterNodeSignals(GetNodeSignals());
    }
    ~TestingSetup()
    {
        threadGroup.interrupt_all();
        threadGroup.join_all();
        UnregisterNodeSignals(GetNodeSignals());
#ifdef ENABLE_WALLET
        delete pwalletMain;
        pwalletMain = NULL;
#endif
        delete pcoinsTip;
        delete pcoinsdbview;
        delete pblocktree;
#ifdef ENABLE_WALLET
        bitdb.Flush(true);
#endif
        boost::filesystem::remove_all(pathTemp);
    }
};

class CTxMemPoolEntry;

struct TestMemPoolEntryHelper
{
    // Default values
    CAmount nFee;
    int64_t nTime;
    unsigned int nHeight;
    double priority;
    CAmount inChainInputValue;
    bool spendsCoinbase;
    int64_t sigOpCost;
    LockPoints lp;
    bool poolHasNoInputsOf;
    CAmount minGasPrice;

    TestMemPoolEntryHelper() :
            nFee(0), nTime(0), nHeight(1), priority(0.0), inChainInputValue(0),
            spendsCoinbase(false), sigOpCost(4), poolHasNoInputsOf(false), minGasPrice(0) { }

    CTxMemPoolEntry FromTx(const CMutableTransaction& tx) {
        return FromTx(MakeTransactionRef(tx));
    }
    CTxMemPoolEntry FromTx(const CTransactionRef& tx) {
        return CTxMemPoolEntry(tx, nFee, nTime, priority, nHeight, inChainInputValue,
                               spendsCoinbase, sigOpCost, lp, poolHasNoInputsOf, minGasPrice);
    }

    // Change the default value
    TestMemPoolEntryHelper &Fee(CAmount _fee) { nFee = _fee; return *this; }
    TestMemPoolEntryHelper &Time(int64_t _time) { nTime = _time; return *this; }
    TestMemPoolEntryHelper &Height(unsigned int _height) { nHeight = _height; return *this; }
    TestMemPoolEntryHelper &Priority(double _priority) { priority = _priority; return *this; }
    TestMemPoolEntryHelper &InChainInputValue(CAmount _inputValue) { inChainInputValue = _inputValue; return *this; }
    TestMemPoolEntryHelper &SpendsCoinbase(bool _flag) { spendsCoinbase = _flag; return *this; }
    TestMemPoolEntryHelper &SigOpsCost(int64_t _sigopsCost) { sigOpCost = _sigopsCost; return *this; }
    TestMemPoolEntryHelper &PoolHasNoInputs(bool _poolHasNoInputsOf) { poolHasNoInputsOf = _poolHasNoInputsOf; return *this; }
    TestMemPoolEntryHelper &MinGasPrice(CAmount _minGasPrice) { minGasPrice = _minGasPrice; return *this; }
};

BOOST_GLOBAL_FIXTURE(TestingSetup);

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

bool ShutdownRequested()
{
  return false;
}
