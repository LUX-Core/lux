#include "proposalsagent.h"

#include <algorithm>

void ProposalsAgent::ListenProposals(const uint256 &orderHash)
{
    mapListenProposals[orderHash] = std::make_pair(true, std::time(nullptr));
}

void ProposalsAgent::StopListenProposals(const uint256 &orderHash)
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
        return ;
    }
    // remove previous proposal
    std::list<StorageProposal> &vOrderProposal = mapProposals[orderHash];
    vOrderProposal.remove_if([proposal](const StorageProposal &p){
        return p.orderHash == proposal.orderHash &&
               p.address == proposal.address &&
               p.time > proposal.time;
    });
    vOrderProposal.push_back(proposal);
}

void ProposalsAgent::EraseOrdersProposals(const uint256 &orderHash)
{
    if (mapProposals.find(orderHash) == mapProposals.end()) {
        return ;
    }
    mapProposals.erase(orderHash);
}

void ProposalsAgent::RemoveOrdersProposal(const uint256 &orderHash, const uint256 &proposalHash)
{
    if (mapProposals.find(orderHash) == mapProposals.end()) {
        return ;
    }
    auto proposals = mapProposals[orderHash];
    proposals.remove_if([proposalHash](const StorageProposal &p) {
        return p.GetHash() == proposalHash;
    });
}

std::vector<StorageProposal> ProposalsAgent::GetProposals(const uint256 &orderHash)
{
    if (mapProposals.find(orderHash) == mapProposals.end()) {
        return {};
    }
    return std::vector<StorageProposal>{ mapProposals[orderHash].begin(),
                                         mapProposals[orderHash].end() };
}

std::vector<StorageProposal> ProposalsAgent::GetSortedProposals(const uint256 &orderHash)
{
    if (mapProposals.find(orderHash) == mapProposals.end()) {
        return {};
    }
    auto proposals = mapProposals[orderHash];
    if (proposals.size()) {
        proposals.sort([](const StorageProposal &a, const StorageProposal &b) {
            return a.rate < b.rate;
        });
        return { proposals.begin(), proposals.end() };
    }
    return {};
}

StorageProposal ProposalsAgent::GetProposal(const uint256 &orderHash, const uint256 &proposalHash)
{
    if (mapProposals.find(orderHash) == mapProposals.end()) {
        return {};
    }
    std::list<StorageProposal> &vOrderProposal = mapProposals[orderHash];
    std::list<StorageProposal>::iterator it = std::find_if(vOrderProposal.begin(),
                                                            vOrderProposal.end(),
                                                            [proposalHash](const StorageProposal &p) {
        return p.GetHash() == proposalHash;
    });

    if (it != vOrderProposal.end()) {
        return *it;
    }

    return {};
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
