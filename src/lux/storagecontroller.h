#ifndef LUX_STORAGECONTROLLER_H
#define LUX_STORAGECONTROLLER_H

#include "uint256.h"
#include "net.h"
#include "protocol.h"
#include "storageheap.h"
#include "storageorder.h"
#include "proposalsagent.h"

#include <boost/thread.hpp>

class StorageController;

extern std::unique_ptr<StorageController> storageController;

static const size_t STORAGE_MIN_RATE = 1;
static const size_t DEFAULT_STORAGE_MAX_BLOCK_GAP = 100;

static const uint64_t DEFAULT_STORAGE_SIZE = 100ull * 1024 * 1024 * 1024; // 100 Gb
static const unsigned short DEFAULT_DFS_PORT = 1507;

class StorageController
{
protected:
    boost::mutex mutex;
    boost::thread background;

    void StartHandshake(const StorageProposal &proposal);
    std::shared_ptr<AllocatedFile> CreateReplica(const boost::filesystem::path &filename, const StorageOrder &order);
    bool SendReplica(const StorageOrder &order, std::shared_ptr<AllocatedFile> pAllocatedFile, CNode* pNode);
    void BackgroundJob();

public:
    CAmount rate;
    size_t maxblocksgap;
    CAddress address;
    std::map<uint256, boost::filesystem::path> mapLocalFiles;
    std::map<uint256, StorageOrder> mapAnnouncements;
    std::map<uint256, StorageHandshake> mapReceivedHandshakes;
    StorageHeap storageHeap;
    StorageHeap tempStorageHeap;
    ProposalsAgent proposalsAgent;

    StorageController();

    void AnnounceOrder(const StorageOrder &order);
    void AnnounceOrder(const StorageOrder &order, const boost::filesystem::path &path);
    bool CancelOrder(const uint256 &orderHash);
    std::vector<StorageProposal> SortProposals(const StorageOrder &order);
    bool AcceptProposal(const StorageProposal &proposal);
    void ClearOldAnnouncments(std::time_t timestamp);
    void ProcessStorageMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isStorageCommand);
    // Proposals Agent
    //void ListenProposal(const uint256 &orderHash);
    //void StopListenProposal(const uint256 &orderHash);
    //bool isListen(const uint256 &proposalHash);
    //std::vector<StorageProposal> GetProposals(const uint256 &orderHash);
    // Temp
    bool FindReplicaKeepers(const StorageOrder &order, const int countReplica);
};

#endif //LUX_STORAGECONTROLLER_H
