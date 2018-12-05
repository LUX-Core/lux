#include "storagecontroller.h"
#include "protocol.h"
#include "net.h"
#include "main.h"

StorageController storageController;

void StorageController::AnnounceOrder(StorageOrder order, const std::string &path)
{
    uint256 hash = order.GetHash();
    proposalsAgent.AddOrder(order, path);

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
            if (pnode->pfilter == nullptr) {
                pnode->PushMessage("inv", vInv);
            }
        }
    }
}
