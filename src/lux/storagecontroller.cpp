#include "storagecontroller.h"
#include "protocol.h"
#include "net.h"
#include "main.h"
#include "streams.h"

StorageController storageController;

void ProcessStorageMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isStorageCommand)
{
    if (strCommand == "dfsannounce") {

    }
}

void StorageController::AnnounceOrder(const StorageOrder &order, const std::string &path)
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
            if (pnode->pfilter == nullptr) {
                pnode->PushMessage("inv", vInv);
            }
        }
    }
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

void StorageController::AddProposal(const StorageProposal &proposal)
{
    proposalsAgent.AddProposal(proposal);
}

std::vector<StorageProposal> StorageController::GetProposals(const uint256 &orderHash)
{
    return proposalsAgent.GetProposals(orderHash);
}
