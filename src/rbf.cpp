// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rbf.h"

bool SignalsOptInRBF(const CTransaction &tx)
{
    for (const CTxIn &txin : tx.vin) {
        if (txin.nSequence < std::numeric_limits<unsigned int>::max()-1) {
            return true;
        }
    }
    return false;
}

bool IsRBFOptIn(const CTxMemPoolEntry &entry, CTxMemPool &pool)
{
    AssertLockHeld(pool.cs);

    CTxMemPool::setEntries setAncestors;

    // First check the transaction itself.
    if (SignalsOptInRBF(entry.GetTx())) {
        return true;
    }

    // If this transaction is not in our mempool, then we can't be sure
    // we will know about all its inputs.
    if (!pool.exists(entry.GetTx().GetHash())) {
        throw std::runtime_error("Cannot determine RBF opt-in signal for non-mempool transaction\n");
    }

    // If all the inputs have nSequence >= maxint-1, it still might be
    // signaled for RBF if any unconfirmed parents have signaled.
    uint64_t noLimit = std::numeric_limits<uint64_t>::max();
    std::string dummy;
    pool.CalculateMemPoolAncestors(entry, setAncestors, noLimit, noLimit, noLimit, noLimit, dummy, false);

    for (CTxMemPool::txiter it : setAncestors) {
        if (SignalsOptInRBF(it->GetTx())) {
            return true;
        }
    }
    return false;
}

