//
// Created by k155 on 17/08/18.
//

#ifndef LUXGATECORE_H
#define LUXGATECORE_H

#include <atomic>

#include "luxgate.h"
#include "uint256.h"

#include <thread>
#include <vector>
#include <map>
#include <tuple>
#include <set>
#include <queue>

#ifdef WIN32
// #include <Ws2tcpip.h>
#endif

namespace rpc
{
class AcceptedConnection;
}

class LuxgateCore
{
    typedef std::vector<unsigned char> UcharVector;

    friend void callback(void * closure, int event,
                         const unsigned char * info_hash,
                         const void * data, size_t data_len);

private:
    LuxgateCore();
    virtual ~LuxgateCore();

public:
    static LuxgateCore & instance();

    static std::string version();

    static bool isEnabled();

    bool init(int argc, char *argv[]);
    bool start();

    uint256 sendLuxgateTX(const std::string & from,
                                   const std::string & fromCurrency,
                                   const uint64_t & fromAmount,
                                   const std::string & to,
                                   const std::string & toCurrency,
                                   const uint64_t & toAmount);
    bool sendAwaitingTX(LuxgateTxDestination_Ptr & dest_Ptr);

    uint256 acceptLuxgateTX(const uint256 & id,
                                     const std::string & from,
                                     const std::string & to);
    bool sendAcceptingTransaction(LuxgateTxDestination_Ptr & dest_Ptr);

    bool CTxLuxgate(const uint256 & id, const CTxReason & reason);
    bool sendCTx(const uint256 & txid, const CTxReason & reason);

    bool RfLuxgateTx(const uint256 & id);
    bool sendRefundTx(const uint256 & txid);

public:
    bool stop();

    LuxgateDest_Ptr tokenTokens(const std::string & token) const;
    std::vector<std::string> tokensTokens() const;

    // store tokens
    void addToken(LuxgateDest_Ptr token);
    // store token addresses in local table
    void storageStore(LuxgateDest_Ptr token, const std::vector<unsigned char> & id);

    bool isLocalAddress(const std::vector<unsigned char> & id);
    bool isKnownMessage(const std::vector<unsigned char> & message);
    void addToMessage(const std::vector<unsigned char> & message);

    LuxgateDest_Ptr servicetoken();

    void tokenAddressBookEntry(const std::string & token,
                               const std::string & name,
                               const std::string & address);
    void getTokenAddressBook();

public:
    // send messave via luxgate
    void onSend(const LuxgatePackage_Ptr & package);
    void onSend(const UcharVector & id, const LuxgatePackage_Ptr & package);

    // call received message from luxgate network
    void onReceivedMessage(const std::vector<unsigned char> & id, const std::vector<unsigned char> & message);
    // broadcast message
    void onReceivedBroadcast(const std::vector<unsigned char> & message);

private:
    void onSend(const UcharVector & id, const UcharVector & message);

public:
    static void sleep(const unsigned int umilliseconds);

private:
    boost::thread_group m_threads;

    Luxgate_Ptr        m_Luxgate;

    mutable boost::mutex m_tokensLock;
    typedef std::map<std::vector<unsigned char>, LuxgateDest_Ptr> tokenAddrMap;
    tokenAddrMap m_tokenAddrs;
    typedef std::map<std::string, LuxgateDest_Ptr> tokenIdMap;
    tokenIdMap m_tokenIds;
    typedef std::queue<LuxgateDest_Ptr> tokenQueue;
    tokenQueue m_tokenQueue;

    // service token
    LuxgateDest_Ptr m_servicetoken;

    boost::mutex m_messagesLock;
    typedef std::set<uint256> ProcessedMessages;
    ProcessedMessages m_processedMessages;

    boost::mutex m_addressBookLock;
    typedef std::tuple<std::string, std::string, std::string> AddressBookEntry;
    typedef std::vector<AddressBookEntry> AddressBook;
    AddressBook m_addressBook;
    std::set<std::string> m_addresses;

public:
    static boost::mutex                                  m_txLocker;
    static std::map<uint256, LuxgateTxDestination_Ptr> m_pendingTransactions;
    static std::map<uint256, LuxgateTxDestination_Ptr> m_transactions;
    static std::map<uint256, LuxgateTxDestination_Ptr> m_historicTransactions;

    static boost::mutex                                  m_UnconfirmedTxLocker;
    static std::map<uint256, LuxgateTxDestination_Ptr> m_unconfirmed;

    static boost::mutex                                  m_ppLocker;
    static std::map<uint256, std::pair<std::string, LuxgatePackage_Ptr> > m_pendingpackages;
};

#endif //LUXGATECORE_H
