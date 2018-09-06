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
/**
 * @param [out] result Found order
 * @return Any entry was found
 */
bool FindMatching(const OrderMap &orders, const COrder &match, COrder &result)
{
    for (auto const &e : orders) {
        auto localOrder = e.second;
        if (localOrder->Matches(match)) {
            result = *localOrder;
            return true;
        }
    }
    return false;
}


void ProcessMessageLuxgate(CNode *pfrom, const std::string &strCommand, CDataStream &vRecv, bool &isLuxgate)
{

    if (strCommand == "createorder") {
        auto order = std::make_shared<COrder>();
        vRecv >> *order;

        LogPrintf("createorder: %s\n", order->ToString());

         if (!order->SenderIsValid())
             order->SetSender(pfrom->addr);

        bool relay = true;

        OrderEntry matchingEntry;
        for (auto const &e : orderbook) {
            auto localOrder = e.second;
            if (order->Matches(*localOrder)) {
                LogPrintf("Received order %s matches to local order %s\n", order->ToString(), localOrder->ToString());
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
                    LogPrintf("Pushing createorder to %s\n", pnode->addr.ToStringIPPort());
                    pnode->PushMessage("createorder", *order);
                }
            }
        } else {
            ActivateOrder(matchingEntry);
        }
    }

    else if (strCommand == "ordermatch") {

        LogPrintf("Received ordermatch from %s\n", pfrom->addr.ToStringIPPort());

        COrder order;
        vRecv >> order;

        if (!order.SenderIsValid())
             order.SetSender(pfrom->addr);

        OrderEntry activatingOrder;
        bool isMatching = false;
        for (auto const &e : orderbook) {
            auto localOrder = e.second;
            if (localOrder->Matches(order)) {
                LogPrintf("Confirm matching order %s (local:%s)\n", order.ToString(), localOrder->ToString());
                activatingOrder = e;
                isMatching = true;
                break;
            }
        }

        if (isMatching) {
            ActivateOrder(activatingOrder);
            auto foundEntry = blockchainClientPool.find(activatingOrder.second->Rel());
            // Stub for
            // foundEntry->second->GetNewAddress();
            std::string addressString =  "2Msx2aNs6h5nEm6R4LZvaPE7NaWtkvsBUXK"; // BTC testnet addr
            pfrom->PushMessage("requestswap", *activatingOrder.second, addressString);
        }
    }

    else if (strCommand == "requestswap") {

        COrder order;
        std::string baseAddr;
        vRecv >> order >> baseAddr;
        
        COrder localOrder;
        if (FindMatching(activeOrders, order, localOrder)) {
            LogPrintf("Received %s address: %s\n", order.Rel(), baseAddr);
            // TODO Verify address
            LogPrintf("Creating address for %s\n", localOrder.Rel());
            auto foundEntry = blockchainClientPool.find(localOrder.Rel());
            // TODO stub for luxclient
            // foundEntry->second->GetNewAddress()
            std::string addressString =  "LUc1gizb2kijhs4GuwZteuKBVaMPTkRzfD"; // LUX testnet addr
            pfrom->PushMessage("requestswapack", localOrder, addressString);
        } else {
            LogPrintf("requestswap: Cannot find matching order %s\n", order.ToString());
        }
    }

    else if (strCommand == "requestswapack") {
        COrder order;
        std::string baseAddr;
        vRecv >> order >> baseAddr;
        
        COrder localOrder;
        if (FindMatching(activeOrders, order, localOrder)) {
            // TODO Verify address
            LogPrintf("requestswapack: verifying %s address %s\n", localOrder.Base(), baseAddr);
        } else {
            LogPrintf("requestswapack: Cannot find matching order %s\n", order.ToString());
        }
    }
}

OrderId COrder::ComputeId() const
{
    return Hash(BEGIN(base), END(base),
                BEGIN(rel), END(rel),
                BEGIN(baseAmount), END(baseAmount),
                BEGIN(relAmount), END(relAmount));
}

bool COrder::Matches(const COrder& order) const 
{
    return rel == order.base && base == order.rel
            && relAmount == order.baseAmount && baseAmount == order.relAmount;
}

std::string COrder::ToString() const 
{
    return strprintf("%u%s->%u%s(from %s)", base, baseAmount, rel, relAmount, sender.ToStringIPPort());
}

bool COrder::SenderIsValid() const 
{
    return sender.GetPort() != 0 && sender.IsValid();
}