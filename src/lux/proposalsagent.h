#ifndef LUX_PROPOSALSAGENT_H
#define LUX_PROPOSALSAGENT_H

#include "storageorder.h"

class ProposalsAgent
{
private:
    std::map<uint256, std::vector<StorageProposal>> mapProposals;
    std::map<uint256, std::pair<bool, std::time_t>> mapListenProposals;
public:
    void ListenProposal(const uint256 &orderHash);
    void StopListenProposal(const uint256 &orderHash);
    void AddProposal(const StorageProposal &proposal);
    std::vector<StorageProposal> GetProposals(const uint256 &orderHash);
    StorageProposal GetProposal(const uint256 &orderHash, const uint256 &proposalHash);
    void EraseOrdersProposals(const uint256 &orderHash);
    std::vector<uint256> GetListenProposals();
};

#endif //LUX_PROPOSALSAGENT_H
