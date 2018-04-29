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
QList<TokenTransactionRecord> TokenTransactionRecord::decomposeTransaction(const CWallet *wallet, const CWalletTx &wtx)
{
    return QList<TokenTransactionRecord>();
}

void TokenTransactionRecord::updateStatus(const CWalletTx &wtx)
{
    AssertLockHeld(cs_main);
    // Determine transaction status
}

bool TokenTransactionRecord::statusUpdateNeeded()
{
    AssertLockHeld(cs_main);
    return status.cur_num_blocks != chainActive.Height();
}

