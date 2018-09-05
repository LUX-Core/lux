// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <luxgate.h>

#include <hash.h>
#include <net.h>
#include <utilstrencodings.h>
#include <util.h>

std::map<OrderId, std::shared_ptr<COrder>> orderbook;

void ProcessMessageLuxgate(CNode *pfrom, const std::string &strCommand, CDataStream &vRecv, bool &isLuxgate)
{

    if (strCommand == "createorder") {
        auto order = std::make_shared<COrder>();
        vRecv >> *order;
        orderbook.insert(std::make_pair(order->ComputeId(), order));

        // relay

        LOCK(cs_vNodes);
        for (CNode *pnode : vNodes) {
            if (pnode && pnode->addr != pfrom->addr) {
                pnode->PushMessage("createorder", *order);
            }
        }
    }

    else if (strCommand == "orderfound") {
    }

    else if (strCommand == "requestswap") {
    }
}

bool IsLuxGateServiceSupported(const CNode* pfrom) {
    return pfrom->nServices & NODE_LUXGATE;
}

OrderId COrder::ComputeId() const
{
    return Hash(BEGIN(base), END(base),
                BEGIN(rev), END(rev),
                BEGIN(baseAmount), END(baseAmount),
                BEGIN(revAmount), END(revAmount),
                BEGIN(sender), END(sender));
}