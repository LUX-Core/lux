#include "storagecontroller.h"
#include "protocol.h"
#include "net.h"

StorageController storageController;

void AnnounceOrder(const StorageOrder &order, const std::string &path)
{
    uint256 hash = order.GetHash();
    mapAnnouncements[hash] = order;
    mapLocalFiles[hash] = path;

    CInv inv(MSG_STORAGE_ORDER_ANNOUNCE, hash);
    vector <CInv> vInv;
    vInv.push_back(inv);

    std::vector<CNode*> vNodesCopy;
    {
        LOCK(cs_vNodes);
        vNodesCopy = vNodes;
    }
    for (CNode* pnode : vNodesCopy) {
        if (!pnode) {
            continue;
        }
        if (pnode->nVersion >= ActiveProtocol()) {
            LOCK(pnode->cs_filter);
            if (pnode->pfilter==nullptr) {
                pnode->PushMessage("inv", vInv);
            }
        }
    }
}
