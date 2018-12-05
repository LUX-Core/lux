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
    void AnnounceOrder(StorageOrder order, const std::string &path);
    //std::vector<StorageOrder> GetAnnounces();

};

#endif //LUX_STORAGECONTROLLER_H
