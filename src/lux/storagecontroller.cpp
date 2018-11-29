#include "storagecontroller.h"

StorageController storageController;

void AnnounceOrder(const StorageOrder &order, const std::string &path)
{
    uint256 hash = order.GetHash();
    mapAnnouncements[hash] = order;
    mapLocalFiles[hash] = path;


}
