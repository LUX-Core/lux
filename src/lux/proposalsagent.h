#ifndef LUX_PROPOSALSAGENT_H
#define LUX_PROPOSALSAGENT_H

#include "storageorder.h"

class ProposalsAgent
{
private:
    std::vector<StorageProposal> vProposals;
    std::map<uint256, StorageOrder> mapAnnouncements;
    std::map<uint256, std::string> mapLocalFiles;

public:
    void AddOrder(StorageOrder order, const std::string &path);
    void ListenProposal(const uint256 &orderHash);
    void StopListenProposal(const uint256 &orderHash);
    void ClearOldAnnouncments(size_t timestamp);
    void AddProposal(StorageProposal proposal);
    std::vector<StorageProposal> GetProposals(const uint256 &orderHash) const;
};

#endif //LUX_PROPOSALSAGENT_H
