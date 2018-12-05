#include "proposalsagent.h"

void ProposalsAgent::AddOrder(StorageOrder order, const std::string &path)
{
    uint256 hash = order.GetHash();
    mapAnnouncements[hash] = order;
    mapLocalFiles[hash] = path;
}

void ProposalsAgent::ListenProposal(const uint256 &orderHash)
{
    for (auto it = mapAnnouncements.begin(); it != mapAnnouncements.end(); ++it) {
        if (it->first == orderHash) {
            it->second.listen = true;
            return;
        }
    }
}

void ProposalsAgent::StopListenProposal(const uint256 &orderHash)
{
    for (auto it = mapAnnouncements.begin(); it != mapAnnouncements.end(); ++it) {
        if (it->first == orderHash) {
            it->second.listen = false;
            return;
        }
    }
}

void ProposalsAgent::ClearOldAnnouncments(size_t timestamp)
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

void ProposalsAgent::AddProposal(StorageProposal proposal)
{
    for (auto it = mapAnnouncements.begin(); it != mapAnnouncements.end(); ++it) {
        if (it->first != proposal.orderHash) continue;
        if (it->second.listen) {
            for (auto itProposal = vProposals.begin(); itProposal != vProposals.end(); ++itProposal) {
                if (itProposal->orderHash == proposal.orderHash &&
                    itProposal->address == proposal.address &&
                    itProposal->GetHash() != proposal.GetHash() &&
                    itProposal->time > proposal.time) {
                    vProposals.erase(itProposal);
                    break;
                }
            }
            vProposals.push_back(proposal);
        }
    }
}

std::vector<StorageProposal> ProposalsAgent::GetProposals(const uint256 &orderHash) const
{
    std::vector<StorageProposal> vProposalsForOrder;
    for (auto it = vProposals.begin(); it != vProposals.end(); ++it) {
        if (it->orderHash == orderHash) {
            vProposalsForOrder.push_back(*it);
        }
    }
    return vProposalsForOrder;
}
