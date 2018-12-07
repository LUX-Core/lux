#ifndef LUX_PROPOSALSAGENT_H
#define LUX_PROPOSALSAGENT_H

#include "storageorder.h"

class ProposalsAgent
{
private:
    std::map<uint256, std::vector<StorageProposal>> vProposals;
    std::map<uint256, std::pair<bool, std::time_t>> vListenProposals;

public:
    void ListenProposal(const uint256 &orderHash);
    void StopListenProposal(const uint256 &orderHash);
    void AddProposal(const StorageProposal &proposal);
    std::vector<StorageProposal> GetProposals(const uint256 &orderHash);
};

#endif //LUX_PROPOSALSAGENT_H
