#include "handshakeagent.h"
#include "storagecontroller.h"

#include <boost/date_time/posix_time/posix_time.hpp>

void HandshakeAgent::StartHandshake(const StorageProposal &proposal, CNode* pNode)
{
    if (!pNode) {
        return ;
    }
    StorageHandshake handshake { std::time(nullptr), proposal.orderHash, proposal.GetHash(), DEFAULT_DFS_PORT };

    pNode->PushMessage("dfshandshake", handshake);

    mapTimers[proposal.orderHash] =  std::make_shared<CancelingSetTimeout>(std::chrono::milliseconds(30000),
                                                                           nullptr,
                                                                           [handshake](){ storageController->PushHandshake(handshake, false); });
    return ;
}

void HandshakeAgent::Add(StorageHandshake handshake)
{
    mapReceivedHandshakes[handshake.orderHash] = handshake;
}

const StorageHandshake *HandshakeAgent::Find(uint256 orderHash)
{
    auto itHandshake = mapReceivedHandshakes.find(orderHash);
    if (itHandshake == mapReceivedHandshakes.end()) {
        return nullptr;
    }
    StorageHandshake handshake = itHandshake->second;
    StorageHandshake *ret = &handshake;
    return ret;
}

void HandshakeAgent::CancelWait(const uint256 &orderHash)
{
    auto it = mapTimers.find(orderHash);
    if (it == mapTimers.end()) {
        return ;
    }
    auto timer = it->second;
    timer->Cancel();
    mapTimers.erase(it);
}
