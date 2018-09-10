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

bool RemoveOrder(OrderMap &orders, COrder &match)
{
    for (auto it = orders.begin(); it != orders.end();) {
        if (match == *it->second)
            it = orders.erase(it);
        else
            ++it;
    }
}

bool FindClient(const Ticker ticker, ClientPtr &foundClient) 
{
    auto foundEntry = blockchainClientPool.find(ticker);
    if (foundEntry != blockchainClientPool.end()) {
        foundClient = foundEntry->second;
        return true;
    }

    return false;
}

bool EnsureClient(const Ticker ticker, ClientPtr &foundClient) {
    if (FindClient(ticker, foundClient)) {
        return true;
    } else {
        LogPrintf("Cant find blockchain client for %s\n", ticker);
        return false;
    }
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
            ClientPtr revClient;
            if (EnsureClient(activatingOrder.second->Rel(), revClient)) {
                auto addressString = revClient->GetNewAddress();
                pfrom->PushMessage("reqswap", *activatingOrder.second, addressString);
            }
        }
    }

    else if (strCommand == "reqswap") {

        COrder order;
        std::string baseAddr;
        vRecv >> order >> baseAddr;
        
        COrder localOrder;
        if (FindMatching(activeOrders, order, localOrder)) {
            LogPrintf("Received %s address: %s\n", order.Rel(), baseAddr);
            ClientPtr baseClient;
            if (EnsureClient(order.Rel(), baseClient)) {
                if (baseClient->IsValidAddress(baseAddr)) {
                    LogPrintf("%s address is valid: %s\n", order.Rel(), baseAddr);
                    ClientPtr relClient;
                    if (EnsureClient(localOrder.Rel(), relClient)) {
                        auto addressString = relClient->GetNewAddress();
                        LogPrintf("Created address for %s: %s\n", localOrder.Rel(), addressString);
                        pfrom->PushMessage("reqswapack", localOrder, addressString);
                    }
                } else {
                    LogPrintf("reqswap: %s address is invalid: %s\n", order.Rel(), baseAddr);
                }
            }
        } else {
            LogPrintf("reqswap: Cannot find matching order %s\n", order.ToString());
        }
    }

    else if (strCommand == "reqswapack") {
        COrder order;
        std::string baseAddr;
        vRecv >> order >> baseAddr;
        
        COrder localOrder;
        if (FindMatching(activeOrders, order, localOrder)) {
            LogPrintf("reqswapack: verifying %s address %s\n", order.Rel(), baseAddr);
            ClientPtr relClient;
            if (EnsureClient(order.Rel(), relClient)) {
                if (relClient->IsValidAddress(baseAddr)) {
                    LogPrintf("reqswapack: %s address is valid: %s\n", order.Rel(), baseAddr);
                    // TODO stub for create contract tx
                    std:string txid = "6763b1c1b3df2e4978669299b609c5dae53b6b0e8443651c3a03ae0be511f87f";
                    pfrom->PushMessage("swccreated", localOrder, txid);

                    // TODO start async timer for redeeming

                } else {
                    LogPrintf("reqswapack: %s address is invalid: %s\n", order.Rel(), baseAddr);
                }
            }
        } else {
            LogPrintf("reqswapack: Cannot find matching order %s\n", order.ToString());
        }
    }

    // Swap contract created
    else if (strCommand == "swccreated") {
        COrder order;
        std::string txid;
        vRecv >> order >> txid;

        COrder localOrder;
        if (FindMatching(activeOrders, order, localOrder)) {
            // TODO verify tx
            bool isValidTx = true;
            if (isValidTx) {
                // TODO create contract tx using extracted hash
                std::vector<unsigned char> secretHash = { 
                    0x00, 0x01, 0x02, 0x03,
                    0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0x0A, 0x0B,
                    0x0C, 0x0D, 0x0E, 0x0F
                };
                // TODO stub for create contract tx
                std::string rtxid = "f7901138cbe4eb42eca7b05ccf8de1d82a35e4b53f2a4f44905edb77362b1459";
                pfrom->PushMessage("swcack", localOrder, rtxid);
                LogPrintf("swccreated: Spending UTXO's: %s\n", txid);
                // TODO Poll async for inputs with secret, spend UTXO's and remove from activeOrders
                // Detach from current thread
                // Also start async timer for redeeming

                RemoveOrder(activeOrders, localOrder);

                // TODO stom redeeming timer

            } else {
                LogPrintf("swccreated: Bad tx %s\n", txid);
            }
        }
    }

    // Swap contract acknowledgement
    else if (strCommand == "swcack") {
        COrder order;
        std::string txid;
        vRecv >> order >> txid;

        COrder localOrder;
        if (FindMatching(activeOrders, order, localOrder)) {
            // TODO Verify tx
            bool isValidTx = true;
            if (isValidTx) {
                // TODO spend UTXO and remove from activeOrders
                LogPrintf("swcack: Spending UTXO: %s\n", txid);

                RemoveOrder(activeOrders, localOrder);

                // TODO stop redeeming timer
            } else {
                LogPrintf("swcack: Bad tx %s\n", txid);
            }
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