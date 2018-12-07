#include "proposalsagent.h"

void ProposalsAgent::ListenProposal(const uint256 &orderHash)
{
    vListenProposals[orderHash] = std::make_pair(true, std::time(nullptr));
}

void ProposalsAgent::StopListenProposal(const uint256 &orderHash)
{
    auto it = vListenProposals.find(orderHash);
    if (it != vListenProposals.end()) {
        vListenProposals.erase(it);
        vListenProposals[orderHash] = std::make_pair(false, std::time(nullptr));
    }
}

void ProposalsAgent::AddProposal(const StorageProposal &proposal)
{
    uint256 orderHash = proposal.orderHash;
    auto itListenProposals = vListenProposals.find(orderHash);
    if (itListenProposals == vListenProposals.end() || std::get<0>(*itListenProposals) == false) {
        return;
    }
    // remove previous proposal
    std::vector<StorageProposal> &vOrderProposal = vProposals[orderHash];
    for (auto it = vOrderProposal.begin(); it != vOrderProposal.end(); ++it) {
        if (it->orderHash == proposal.orderHash &&
            it->address == proposal.address &&
            it->time > proposal.time) {
            vOrderProposal.erase(it);
            break;
        }
    }
    vOrderProposal.push_back(proposal);
}

std::vector<StorageProposal> ProposalsAgent::GetProposals(const uint256 &orderHash)
{
    std::vector<StorageProposal> vProposalsForOrder;
    if (vProposals.find(orderHash) == vProposals.end()) {
        return {};
    }
    std::vector<StorageProposal> &vOrderProposal = vProposals[orderHash];
    for (auto it = vOrderProposal.begin(); it != vOrderProposal.end(); ++it) {
        if (it->orderHash == orderHash) {
            vProposalsForOrder.push_back(*it);
        }
    }
    return vProposalsForOrder;
}
