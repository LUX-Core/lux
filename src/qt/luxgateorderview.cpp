// Copyright (c) 2019 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "luxgateorderview.h"

#include "guiutil.h"
#include "luxgategui_global.h"

OrderView::OrderView(const std::shared_ptr<const COrder> order)
{
    id = order->ComputeId();
    sId = QString::fromStdString(OrderIdToString(id));
    sState = stateToString(order->GetState());
    creationTime = order->OrderCreationTime();
    sCreationTime = GUIUtil::dateTimeStr(creationTime);
    completionTime = order->OrderCompletionTime();
    sCompletionTime = GUIUtil::dateTimeStr(completionTime);
    sType = order->IsSell() ? QString("Sell") : QString("Buy");
    isBuy = order->IsBuy();
    sPrice = QString(Luxgate::QStrFromAmount(order->Price()));
    sAmount = QString(Luxgate::QStrFromAmount(order->Amount()));
    sTotal = QString(Luxgate::QStrFromAmount(order->Total()));
}

OrderView::OrderView(const std::shared_ptr<COrder> order)
{
    id = order->ComputeId();
    sId = QString::fromStdString(OrderIdToString(id));
    sState = stateToString(order->GetState());
    creationTime = order->OrderCreationTime();
    sCreationTime = GUIUtil::dateTimeStr(creationTime);
    completionTime = order->OrderCompletionTime();
    sCompletionTime = GUIUtil::dateTimeStr(completionTime);
    sType = order->IsSell() ? QString("Sell") : QString("Buy");
    isBuy = order->IsBuy();
    sPrice = QString(Luxgate::QStrFromAmount(order->Price()));
    sAmount = QString(Luxgate::QStrFromAmount(order->Amount()));
    sTotal = QString(Luxgate::QStrFromAmount(order->Total()));
}

QString stateToString(const COrder::State state)
{
    switch (state) {
        case COrder::State::INVALID: return QString("Invalid");
        case COrder::State::NEW: return QString("New");
        case COrder::State::MATCH_FOUND: return QString("Match found");
        case COrder::State::SWAP_REQUESTED: return QString("Swap requested");
        case COrder::State::SWAP_ACK: return QString("Swap acknowledgement sent");
        case COrder::State::CONTRACT_CREATED: return QString("Contract created");
        case COrder::State::CONTRACT_ACK: return QString("Contract aknowledgement sent");
        case COrder::State::REFUNDING: return QString("Refunding");
        case COrder::State::CONTRACT_VERIFYING: return QString("Verifying contract");
        case COrder::State::CONTRACT_ACK_VERIFYING: return QString("Verifying contract acknowledgement");
        case COrder::State::COMPLETE: return QString("Completed");
        case COrder::State::REFUNDED: return QString("Refunded");
        case COrder::State::CANCELLED: return QString("Cancelled");
        default: return QString("Unknown");
    };
}

