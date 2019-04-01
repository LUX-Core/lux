#ifndef LUX_STORAGECONTROLLER_H
#define LUX_STORAGECONTROLLER_H

#include "amount.h"
#include "concurrent_queue.h"
#include "decryptionkeys.h"
#include "handshakeagent.h"
#include "net.h"
#include "proposalsagent.h"
#include "protocol.h"
#include "script/standard.h"
#include "storageheap.h"
#include "storageorder.h"
#include "uint256.h"

#include <boost/thread.hpp>
#include <openssl/rsa.h>

class StorageController;
class CFileStorageDB;

extern std::unique_ptr<StorageController> storageController;

static const size_t STORAGE_MIN_RATE = 1;
static const int DEFAULT_STORAGE_MAX_BLOCK_GAP = 100;

static const uint64_t DEFAULT_STORAGE_SIZE = 100ull * 1024 * 1024 * 1024; // 100 Gb
static const unsigned short DEFAULT_DFS_PORT = 1507;

static const size_t MAX_ANNOUNCEMETS_SIZE = 100;

class StorageController
{
    enum BackgroundJobs {
        WAKEUP              = 1 << 0,
        CHECK_PROPOSALS     = 1 << 1,
        ACCEPT_PROPOSAL     = 1 << 2,
        FAIL_HANDSHAKE      = 1 << 3
    };

    using OrderHash = uint256;
    using ProposalHash = uint256;
    using OrderHashDB = uint256;
    using MerkleRootHash = uint256;

protected:
    boost::mutex mutex;

    boost::mutex jobsMutex;
    boost::condition_variable jobsHandler;
    concurrent_queue<BackgroundJobs> qJobs;

    boost::mutex handshakesMutex;
    boost::condition_variable handshakesHandler;
    concurrent_queue<std::pair<bool, StorageHandshake>> qHandshakes;

    concurrent_queue<StorageProposal> qProposals;

    bool shutdownThreads;
    time_t lastCheckIp;

    boost::thread pingThread;
    boost::thread proposalsManagerThread;
    boost::thread handshakesManagerThread;

    ProposalsAgent proposalsAgent;
    HandshakeAgent handshakeAgent;

    StorageHeap storageHeap;
    StorageHeap tempStorageHeap;

    std::map<OrderHash, boost::filesystem::path> mapLocalFiles;
    std::map<OrderHash, StorageOrder> mapAnnouncements;
    std::map<OrderHash, std::shared_ptr<CancelingSetTimeout>> mapTimers;
    std::map<OrderHash, std::map<ProposalHash, std::pair<uint256, DecryptionKeys>>> mapMetadata;
    std::map<OrderHashDB, StorageOrderDB> mapOrders;
    std::map<MerkleRootHash, std::vector<StorageProofDB>> mapProofs;

    CFileStorageDB *db;
public:
    CAmount rate;
    int maxblocksgap;
    CAddress address;

    StorageController() : pingThread(boost::bind(&StorageController::FoundMyIP, this)),
                          proposalsManagerThread(boost::bind(&StorageController::ProcessProposalsMessages, this)),
                          handshakesManagerThread(boost::bind(&StorageController::ProcessHandshakesMessages, this)),
                          shutdownThreads(false),
                          rate(STORAGE_MIN_RATE),
                          maxblocksgap(DEFAULT_STORAGE_MAX_BLOCK_GAP),
                          db(nullptr)
    {
    }
    // Init functions
    void InitStorages(const boost::filesystem::path &dataDir, const boost::filesystem::path &tempDataDir);
    void InitDB(size_t nCacheSize, bool fMemory, bool fWipe);
    // ProcessMessage function
    void ProcessStorageMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isStorageCommand);
    // MultiThread signals
    void PushHandshake(const StorageHandshake &handshake, const bool status = true);
    // Orders functions
    void AnnounceOrder(const StorageOrder &order);
    void AnnounceNewOrder(const StorageOrder &order, const boost::filesystem::path &path);
    bool CancelOrder(const uint256 &orderHash);
    void ClearOldAnnouncments(std::time_t timestamp);
    // Proposals function
    bool AcceptProposal(const StorageProposal &proposal);
    // Replica function
    void DecryptReplica(const uint256 &orderHash, const boost::filesystem::path &decryptedFile);
    // DB functions
    void LoadOrders();
    void AddOrder(const StorageOrderDB &orderDB);
    void SaveOrder(const StorageOrderDB &orderDB);
    void LoadProofs();
    void AddProof(const StorageProofDB &proof);
    void SaveProof(const StorageProofDB &proof);
    // Get functions
    std::map<uint256, StorageOrder> GetAnnouncements();
    const StorageOrder *GetOrder(const uint256 &hash);
    std::vector<std::shared_ptr<StorageChunk>> GetChunks(const bool tempChunk = false);
    void MoveChunk(size_t chunkIndex, const boost::filesystem::path &newpath,const bool tempChunk = false);
    std::vector<StorageProposal> GetProposals(const uint256 &orderHash);
    StorageProposal GetProposal(const uint256 &orderHash, const uint256 &proposalHash);
    std::pair<uint256, DecryptionKeys> GetMetadata(const uint256 &orderHash, const uint256 &proposalHash);
    const StorageOrderDB *GetOrderDB(const uint256 &merkleRootHash);
    const std::vector<StorageProofDB> GetProofs(const uint256 &merkleRootHash);
    const std::list<uint256> ConstructMerklePath(const uint256 &fileURI, const size_t position);
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
    CNode *TryConnectToNode(const CService &address, int maxAttempt = 20);
    void Notify(const StorageController::BackgroundJobs job);
    void FoundMyIP();                   // pingThread
    void ProcessProposalsMessages();    // proposalsManagerThread
    void ProcessHandshakesMessages();   // handshakesManagerThread
};

#endif //LUX_STORAGECONTROLLER_H
