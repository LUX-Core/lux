#ifndef LUX_STORAGECONTROLLER_H
#define LUX_STORAGECONTROLLER_H

#include "uint256.h"
#include "storageheap.h"
#include "storageorder.h"
#include "proposalsagent.h"

class StorageController;

extern StorageController storageController;

class StorageController
{
private:
    StorageHeap storageHeap;
    StorageHeap tempStorageHeap;
    ProposalsAgent proposalsAgent;
public:
    std::map<uint256, std::string> mapLocalFiles;
    std::map<uint256, StorageOrder> mapAnnouncements;
    void AnnounceOrder(const StorageOrder &order, const std::string &path);
    void ClearOldAnnouncments(std::time_t timestamp);
    std::vector<StorageProposal> GetProposals(const uint256 &orderHash);

};

#endif //LUX_STORAGECONTROLLER_H
