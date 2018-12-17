#include "proposalsagent.h"

void ProposalsAgent::ListenProposal(const uint256 &orderHash)
{
    mapListenProposals[orderHash] = std::make_pair(true, std::time(nullptr));
}

void ProposalsAgent::StopListenProposal(const uint256 &orderHash)
{
    auto it = mapListenProposals.find(orderHash);
    if (it != mapListenProposals.end()) {
        mapListenProposals.erase(it);
        mapListenProposals[orderHash] = std::make_pair(false, std::time(nullptr));
    }
}

void ProposalsAgent::AddProposal(const StorageProposal &proposal)
{
    uint256 orderHash = proposal.orderHash;
    auto itListenProposals = mapListenProposals.find(orderHash);
    if (itListenProposals == mapListenProposals.end() || std::get<0>(*itListenProposals) == false) {
        return;
    }
    // remove previous proposal
    std::vector<StorageProposal> &vOrderProposal = mapProposals[orderHash];
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
    if (mapProposals.find(orderHash) == mapProposals.end()) {
        return {};
    }
    std::vector<StorageProposal> &vOrderProposal = mapProposals[orderHash];
    for (auto it = vOrderProposal.begin(); it != vOrderProposal.end(); ++it) {
        if (it->orderHash == orderHash) {
            vProposalsForOrder.push_back(*it);
        }
    }
    return vProposalsForOrder;
}


std::vector<uint256> ProposalsAgent::GetListenProposals()
{
    std::vector<uint256> vListenProposals;
    for (auto it = mapListenProposals.begin(); it != mapListenProposals.end(); ++it) {
        if (it->second.first == true) {
            vListenProposals.push_back(it->first);
        }
    }

    return vListenProposals;
}
