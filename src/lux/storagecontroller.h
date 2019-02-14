#ifndef LUX_STORAGECONTROLLER_H
#define LUX_STORAGECONTROLLER_H

#include "uint256.h"
#include "net.h"
#include "protocol.h"
#include "storageheap.h"
#include "storageorder.h"
#include "proposalsagent.h"
#include "handshakeagent.h"
#include "decryptionkeys.h"
#include "concurrent_queue.h"

#include <boost/thread.hpp>
#include <openssl/rsa.h>

class StorageController;

extern std::unique_ptr<StorageController> storageController;

static const size_t STORAGE_MIN_RATE = 1;
static const int DEFAULT_STORAGE_MAX_BLOCK_GAP = 100;

static const uint64_t DEFAULT_STORAGE_SIZE = 100ull * 1024 * 1024 * 1024; // 100 Gb
static const unsigned short DEFAULT_DFS_PORT = 1507;

class StorageController
{
    enum BackgroundJobs {
        WAKEUP              = 1 << 0,
        CHECK_PROPOSALS     = 1 << 1,
        ACCEPT_PROPOSAL     = 1 << 2,
        FAIL_HANDSHAKE      = 1 << 3
    };
protected:
    boost::mutex mutex;

    boost::mutex jobsMutex;
    boost::condition_variable jobsHandler;
    concurrent_queue<BackgroundJobs> qJobs;

    boost::mutex handshakesMutex;
    boost::condition_variable handshakesHandler;
    concurrent_queue<std::pair<bool, StorageHandshake>> qHandshakes;

    concurrent_queue<StorageProposal> qProposals;

    boost::thread pingThread;
    boost::thread proposalsManagerThread;
    boost::thread handshakesManagerThread;

    ProposalsAgent proposalsAgent;
    HandshakeAgent handshakeAgent;

    StorageHeap storageHeap;
    StorageHeap tempStorageHeap;

    std::map<uint256, boost::filesystem::path> mapLocalFiles;
    std::map<uint256, StorageOrder> mapAnnouncements;

    bool shutdownThreads;

public:
    CAmount rate;
    int maxblocksgap;
    CAddress address;

    StorageController() : pingThread(boost::bind(&StorageController::FoundMyIP, this)),
                          proposalsManagerThread(boost::bind(&StorageController::ProcessProposalsMessages, this)),
                          handshakesManagerThread(boost::bind(&StorageController::ProcessHandshakesMessages, this)),
                          shutdownThreads(false),
                          rate(STORAGE_MIN_RATE),
                          maxblocksgap(DEFAULT_STORAGE_MAX_BLOCK_GAP) {}
    // Init function
    void InitStorages(const boost::filesystem::path &dataDir, const boost::filesystem::path &tempDataDir);
    // ProcessMessage function
    void ProcessStorageMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isStorageCommand);
    // MultiThread signals
    void PushHandshake(const StorageHandshake &handshake, const bool status = true);
    // Orders functions
    void AnnounceOrder(const StorageOrder &order);
    void AnnounceOrder(const StorageOrder &order, const boost::filesystem::path &path);
    bool CancelOrder(const uint256 &orderHash);
    void ClearOldAnnouncments(std::time_t timestamp);
    // Proposals functions
    bool AcceptProposal(const StorageProposal &proposal);
    // Replica functions
    void DecryptReplica(const uint256 &orderHash, const boost::filesystem::path &decryptedFile);
    // Transactions
    bool CreateOrderTransaction();
    bool CreateProofTransaction();
    // Get functions
    std::map<uint256, StorageOrder> GetAnnouncements();
    const StorageOrder *GetAnnounce(const uint256 &hash);
    std::vector<std::shared_ptr<StorageChunk>> GetChunks(const bool tempChunk = false);
    void MoveChunk(size_t chunkIndex, const boost::filesystem::path &newpath,const bool tempChunk = false);
    std::vector<StorageProposal> GetProposals(const uint256 &orderHash);
    StorageProposal GetProposal(const uint256 &orderHash, const uint256 &proposalHash);
    // Shutdown function
    void StopThreads();

protected:
    std::shared_ptr<AllocatedFile> CreateReplica(const boost::filesystem::path &filename,
                                                 const StorageOrder &order,
                                                 const DecryptionKeys &keys,
                                                 RSA *rsa);
    bool SendReplica(const StorageOrder &order,
                     const uint256 merkleRootHash,
                     std::shared_ptr<AllocatedFile> pAllocatedFile,
                     const DecryptionKeys &keys,
                     CNode* pNode);
    bool CheckReceivedReplica(const uint256 &orderHash, const uint256 &receivedMerkleRootHash, const boost::filesystem::path &replica);
    CNode *TryConnectToNode(const CService &address);
    void Notify(const StorageController::BackgroundJobs job);
    void FoundMyIP();                   // pingThread
    void ProcessProposalsMessages();    // proposalsManagerThread
    void ProcessHandshakesMessages();   // handshakesManagerThread
};

#endif //LUX_STORAGECONTROLLER_H
