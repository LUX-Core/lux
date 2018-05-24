#include "tokentransactionrecord.h"

#include "base58.h"
#include "primitives/block.h"
#include "main.h"
#include "timedata.h"
#include "wallet.h"

#include <stdint.h>

#include <boost/foreach.hpp>

/*
 * Decompose CWallet transaction to model transaction records.
 */
QList<TokenTransactionRecord> TokenTransactionRecord::decomposeTransaction(const CWallet *wallet, const CTokenTx &wtx)
{
    // Initialize variables
    QList<TokenTransactionRecord> parts;
    uint256 credit;
    uint256 debit;
    std::string tokenSymbol;
    uint8_t decimals = 18;
    if(wallet && !wtx.nValue.IsNull() && wallet->GetTokenTxDetails(wtx, credit, debit, tokenSymbol, decimals))
    {
        // Get token transaction data
        TokenTransactionRecord rec;
        rec.time = wtx.nCreateTime;
        rec.credit = dev::u2s(uintTou256(credit));
        rec.debit = -dev::u2s(uintTou256(debit));
        rec.hash = wtx.GetHash();
        rec.txid = wtx.transactionHash;
        rec.tokenSymbol = tokenSymbol;
        rec.decimals = decimals;
        rec.label = wtx.strLabel;
        dev::s256 net = rec.credit + rec.debit;

        // Determine type
        if(net == 0)
        {
            rec.type = SendToSelf;
        }
        else if(net > 0)
        {
           rec.type = RecvWithAddress;
        }
        else
        {
            rec.type = SendToAddress;
        }

        if(net)
        {
            rec.status.countsForBalance = true;
        }

        // Set address
        switch (rec.type) {
        case SendToSelf:
        case SendToAddress:
        case SendToOther:
        case RecvWithAddress:
        case RecvFromOther:
            rec.address = wtx.strReceiverAddress;
        default:
            break;
        }

        // Append record
        if(rec.type != Other)
            parts.append(rec);
    }

    return parts;
}

void TokenTransactionRecord::updateStatus(const CWallet *wallet, const CTokenTx &wtx)
{
    AssertLockHeld(cs_main);
    // Determine transaction status
    status.cur_num_blocks = chainActive.Height();
    if(wtx.blockNumber == -1)
    {
        status.depth = 0;
    }
    else
    {
        status.depth = status.cur_num_blocks - wtx.blockNumber + 1;
    }

    auto mi = wallet->mapWallet.find(wtx.transactionHash);
    if (mi != wallet->mapWallet.end() && (GetAdjustedTime() - mi->second.nTimeReceived > 2 * 60) && mi->second.GetRequestCount() == 0)
    {
        status.status = TokenTransactionStatus::Offline;
    }
    else if (status.depth == 0)
    {
        status.status = TokenTransactionStatus::Unconfirmed;
    }
    else if (status.depth < RecommendedNumConfirmations)
    {
        status.status = TokenTransactionStatus::Confirming;
    }
    else
    {
        status.status = TokenTransactionStatus::Confirmed;
    }
}

bool TokenTransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    return status.cur_num_blocks != chainActive.Height();
}

QString TokenTransactionRecord::getTxID() const
{
    return QString::fromStdString(txid.ToString());
}
