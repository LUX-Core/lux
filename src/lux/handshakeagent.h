#ifndef LUX_HANDSHAKEAGENT_H
#define LUX_HANDSHAKEAGENT_H

#include "uint256.h"
#include "net.h"
#include "protocol.h"
#include "storageorder.h"
#include "decryptionkeys.h"
#include "cancelingsettimeout.h"

#include <boost/asio.hpp>

class HandshakeAgent
{
protected:
    std::map<uint256, StorageHandshake> mapReceivedHandshakes;
    std::map<uint256, std::shared_ptr<CancelingSetTimeout>> mapTimers;
public:
    void StartHandshake(const StorageProposal &proposal, CNode* pNode);
    void Add(StorageHandshake handshake);
    const StorageHandshake *Find(uint256 orderHash);
    void CancelWait(const uint256 &orderHash);
protected:
};

#endif //LUX_HANDSHAKEAGENT_H
