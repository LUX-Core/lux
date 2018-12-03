#ifndef PROJECT_PROPOSALSAGENT_H
#define PROJECT_PROPOSALSAGENT_H

#include "storageorder.h"

class ProposalsAgent
{
private:
    std::map<uint256, StorageOrder> mapAnnouncements;
    std::map<uint256, std::string> mapLocalFiles;

public:
    void ListenProposal(const StorageOrder &order);
    void StopListenProposal(const uint256 &orderHash);
    void ClearOldAnnouncments();
    std::vector<StorageOrder> GetProposals() const;
};

#endif //PROJECT_PROPOSALSAGENT_H
