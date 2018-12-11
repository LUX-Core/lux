#include "storagecontroller.h"
#include "main.h"
#include "streams.h"

StorageController storageController;

void StorageController::ProcessStorageMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isStorageCommand)
{
    if (strCommand == "dfsannounce") {
        isStorageCommand = true;
        StorageOrder order;
        vRecv >> order;

        uint256 hash = order.GetHash();
        if (storageController.mapAnnouncements.find(hash) == storageController.mapAnnouncements.end()) {
            storageController.AnnounceOrder(order); // TODO: Is need remove "pfrom" node from announcement? (SS)
            if (storageHeap.MaxAllocateSize() > order.fileSize) {
                StorageProposal proposal;
                proposal.time = std::time(0);
                proposal.orderHash = hash;
                proposal.rate = storageController.rate;
                proposal.address = storageController.address;
                storageController.proposalsAgent.AddProposal(proposal);
            }
        }
    }
}

void StorageController::AnnounceOrder(const StorageOrder &order)
{
    uint256 hash = order.GetHash();
    mapAnnouncements[hash] = order;

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

void StorageController::AnnounceOrder(const StorageOrder &order, const std::string &path)
{
    AnnounceOrder(order);
    mapLocalFiles[order.GetHash()] = path;
}

void StorageController::ClearOldAnnouncments(std::time_t timestamp)
{
    for (auto it = mapAnnouncements.begin(); it != mapAnnouncements.end(); ) {
        if (it->second.time < timestamp) {
            mapLocalFiles.erase(it->first);
            mapAnnouncements.erase(it++);
        } else {
            ++it;
        }
    }
}

void StorageController::ListenProposal(const uint256 &orderHash)
{
    proposalsAgent.ListenProposal(orderHash);
}

void StorageController::StopListenProposal(const uint256 &orderHash)
{
    proposalsAgent.StopListenProposal(orderHash);
}

std::vector<StorageProposal> StorageController::GetProposals(const uint256 &orderHash)
{
    return proposalsAgent.GetProposals(orderHash);
}
