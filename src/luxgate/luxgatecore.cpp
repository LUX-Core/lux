//
// Created by k155 on 17/08/18.
//

#include "luxgatecore.h"
#include "luxgatedex.h"
#include "rpcserver.h"
#include "net.h"
#include "util.h"
#include "ui_interface.h"

#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/chrono/chrono.hpp>
#include <boost/thread/thread.hpp>
#include <assert.h>
#include <openssl/rand.h>
#include <openssl/md5.h>

#define fromAddress 0
#define toAddress 1
#define package 2
#define resendFlag 3

LGate lGate;

boost::mutex                                  LuxgateCore::m_txLocker;
std::map<uint256, LuxgateTxDestination_Ptr> LuxgateCore::m_pendingTransactions;
std::map<uint256, LuxgateTxDestination_Ptr> LuxgateCore::m_transactions;
std::map<uint256, LuxgateTxDestination_Ptr> LuxgateCore::m_historicTransactions;
boost::mutex                                  LuxgateCore::m_UnconfirmedTxLocker;
std::map<uint256, LuxgateTxDestination_Ptr> LuxgateCore::m_unconfirmed;
boost::mutex                                  LuxgateCore::m_ppLocker;
std::map<uint256, std::pair<std::string, LuxgatePackage_Ptr> >  LuxgateCore::m_pendingpackages;

void l_gate() { int * a = 0; *a = 0; }

LuxgateCore::LuxgateCore() {}

LuxgateCore::~LuxgateCore() { 
    
    stop();

#ifdef WIN32
    WSACleanup();
#endif
}

LuxgateCore & LuxgateCore::instance() { static LuxgateCore app; return app; }

std::string LuxgateCore::version() {
    std::ostringstream o;
    o << LUXGATE_MAJOR_VERSION
      << "." << LUXGATE_MINER_VERSION
      << "." << LUXGATE_REVISION_VERSION
      << " [" << LUXGATE_VERSION << "]";
    return o.str();
}

bool LuxgateCore::isEnabled() {
    // enabled by default
    return true;
}

bool LuxgateCore::start() {
    m_Token.reset(new LuxgateTokens);

    // start luxgate
    m_Luxgate = Luxgate_Ptr(new Luxgate());

    return true;
}

const unsigned char hash[20] = {
    0x69, 0x96, 0x21, 0x15, 0x16, 0x65, 0x75, 0xee, 0xa6, 0x03,
    0x11, 0xa1, 0xf4, 0xc9, 0x9a, 0xa6, 0x28, 0x8c, 0x78, 0x69
};

bool LuxgateCore::init(int argc, char *argv[]) {
    // init luxgate settings
    Settings & s = settings();
    {
        std::string path(GetDataDir(false).string());
        path += "/luxgate.conf";
        s.read(path.c_str());
        s.parseCmdLine(argc, argv);
        LOG() << "Finished loading Luxgate config" << path;
    }
    
    luxgate::ECC_Start();

    // init Luxgate Dex
    LuxgateDex & e = LuxgateDex::instance();
    e.init();

    return true;
}

bool LuxgateCore::stop() {
    LOG() << "stopping threads...";

    m_Luxgate->stop();

    m_threads.join_all();
    
    luxgate::ECC_Stop();

    return true;
}

void LuxgateCore::onSend(const LuxgatePackage_Ptr & package){
    static UcharVector addr(20, 0);
    UcharVector v(package->header(), package->header()+package->allSize());
    onSend(addr, v);
}

void LuxgateCore::onSend(const UcharVector & id, const UcharVector & message) {
    UcharVector msg(id);
    if (msg.size() != 20) {
        ERR() << "bad send address " << __FUNCTION__;
        return;
    }
    
    uint64_t timestamp = std::time(0);
    unsigned char * dest_Ptr = reinterpret_cast<unsigned char *>(&timestamp);
    msg.insert(msg.end(), dest_Ptr, dest_Ptr + sizeof(uint64_t));
    
    msg.insert(msg.end(), message.begin(), message.end());

    uint256 hash = Hash(msg.begin(), msg.end());

    LOCK(cs_vNodes);
    for  (CNode * pnode : vNodes) {
        if (pnode->setKnown.insert(hash).second) {
            pnode->PushMessage("luxgate", msg);
        }
    }
}

void LuxgateCore::onSend(const UcharVector & id, const LuxgatePackage_Ptr & package){
    UcharVector v;
    std::copy(package->header(), package->header()+package->allSize(), std::back_inserter(v));
    onSend(id, v);
}

void LuxgateCore::onReceivedMessage(const UcharVector & id, const UcharVector & message) {
    if (isKnownMessage(message)) {
        return;
    }

    addToMessage(message);

    LuxgatePackage_Ptr package(new LuxgatePackages);
    if (!package->copyFrom(message)) {
        LOG() << "incorrect package received";
        return;
    }

    LOG() << "received message to " << util::base64_encode(std::string((char *)&id[0], 20)).c_str()
             << " command " << package->command();

    if (!LuxgateTokens::checkPackageVersion(package)) {
        ERR() << "incorrect protocol version <" << package->version() << "> " << __FUNCTION__;
        return;
    }

    LuxgateDest_Ptr dest_Ptr;

    {
        boost::mutex::scoped_lock l(m_sessionsLock);
        if (m_sessionAddrs.count(id)) {
            dest_Ptr = m_sessionAddrs[id];
        }
        
        else if (m_Token->sessionAddr() == id) {
            dest_Ptr = serviceSession();
        }

        else {
          //TODO:  LOG !!!
        }
    }

    if (dest_Ptr) {
        dest_Ptr->processpackage(package);
    }
}

LuxgateDest_Ptr LuxgateCore::serviceSession()
{
    return m_Token;
}


