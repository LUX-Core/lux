// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <luxgate.h>

#include <hash.h>
#include <net.h>
#include <utilstrencodings.h>
#include <util.h>


std::map<OrderId, std::shared_ptr<COrder>> orderbook;
std::map<OrderId, std::shared_ptr<COrder>> activeOrders;


bool IsLuxGateServiceSupported(const CNode* pfrom) {
    return pfrom->nServices & NODE_LUXGATE;
}

void ActivateOrder(OrderEntry order) {
    orderbook.erase(order.first);
    activeOrders.insert(order);
}


void ProcessMessageLuxgate(CNode *pfrom, const std::string &strCommand, CDataStream &vRecv, bool &isLuxgate)
{

    if (strCommand == "createorder") {
        auto order = std::make_shared<COrder>();
        vRecv >> *order;

        // if (order->Sender())
        //     order->SetSender(pfrom->addr);

        //orderbook.insert(std::make_pair(order->ComputeId(), order));

        bool relay = true;

        OrderEntry matchingEntry;
        for (auto const &e : orderbook) {
            auto localOrder = e.second;
            if (order->Matches(*localOrder)) {
                CNode* sender = ConnectNode(order->Sender());
                sender->PushMessage("ordermatch", *localOrder);
                matchingEntry = e;
                relay = false;
                break;
            }
        }


        if (relay) {
            //LOCK(cs_vNodes);
            for (CNode *pnode : vNodes) {
                if (pnode && pnode->addr != pfrom->addr) {
                    pnode->PushMessage("createorder", *order);
                }
            }
        } else {
            ActivateOrder(matchingEntry);
        }
    }

    else if (strCommand == "ordermatch") {
        COrder order;
        vRecv >> order;
        OrderEntry activatingOrder;
        bool isMatching = false;
        for (auto const &e : orderbook) {
            auto localOrder = e.second;
            if (localOrder->Matches(order)) {
                activatingOrder = e;
                isMatching = true;
                break;
            }
        }

        if (isMatching) {
            ActivateOrder(activatingOrder);
        }
    }

    else if (strCommand == "requestswap") {
    }
}

OrderId COrder::ComputeId() const
{
    return Hash(BEGIN(base), END(base),
                BEGIN(rel), END(rel),
                BEGIN(baseAmount), END(baseAmount),
                BEGIN(relAmount), END(relAmount));
}

bool COrder::Matches(COrder& order) const 
{
    return rel == order.base && base == order.rel
            && relAmount == order.baseAmount && baseAmount == order.relAmount;
}