#ifndef LUX_STORAGECONTROLLER_H
#define LUX_STORAGECONTROLLER_H

#include "uint256.h"
#include "net.h"
#include "protocol.h"
#include "storageheap.h"
#include "storageorder.h"
#include "proposalsagent.h"

class StorageController;

extern StorageController storageController;

static const size_t STORAGE_MIN_RATE = 1e-8;

class StorageController
{
private:
    StorageHeap storageHeap;
    StorageHeap tempStorageHeap;
    ProposalsAgent proposalsAgent;
public:
    size_t rate;
    CService address;
    std::map<uint256, std::string> mapLocalFiles;
    std::map<uint256, StorageOrder> mapAnnouncements;

    StorageController() : rate(STORAGE_MIN_RATE), address(CService("127.0.0.1")) {} // TODO: Change to global-wide address (SS)

    void AnnounceOrder(const StorageOrder &order);
    void AnnounceOrder(const StorageOrder &order, const std::string &path);
    std::vector<StorageProposal> GetBestProposals(const StorageOrder &order, const int maxProposal);
    void ClearOldAnnouncments(std::time_t timestamp);
    void ProcessStorageMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isStorageCommand);
    // Proposals Agent
    void ListenProposal(const uint256 &orderHash);
    void StopListenProposal(const uint256 &orderHash);
    bool isListen(const uint256 &proposalHash);
    std::vector<StorageProposal> GetProposals(const uint256 &orderHash);
};

#endif //LUX_STORAGECONTROLLER_H
