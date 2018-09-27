// Copyright (c) 2011-2013 The Bitcoin developers               -*- c++ -*-
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_COINCONTROL_H
#define BITCOIN_COINCONTROL_H

#include "primitives/transaction.h"
#include "script/standard.h"
#include "wallet.h"

/** Coin Control Features. */
class CCoinControl
{
public:
    CTxDestination destChange;
    bool useDarksend;
    bool useInstanTX;
    bool fSplitBlock;
    int nSplitBlock;
    //! If false, allows unselected inputs, but requires all selected inputs be used
    bool fAllowOtherInputs;
    //! Includes watch only addresses which match the ISMINE_WATCH_SOLVABLE criteria
    bool fAllowWatchOnly;
    //! Minimum absolute fee (not per kilobyte)
    CAmount nMinimumTotalFee;
    //! Override the default confirmation target, 0 = use default
    int nConfirmTarget;
    //! Signal BIP-125 replace by fee.
    bool signalRbf;

    CCoinControl()
    {
        SetNull();
    }

    void SetNull()
    {
        destChange = CNoDestination();
        setSelected.clear();
        useInstanTX = false;
        useDarksend = true;
        fAllowOtherInputs = false;
        fAllowWatchOnly = false;
        nMinimumTotalFee = 0;
        fSplitBlock = false;
        nSplitBlock = 1;
        nConfirmTarget = 0;
        signalRbf = fWalletRbf;
    }

    bool HasSelected() const
    {
        return (setSelected.size() > 0);
    }

    bool IsSelected(const uint256& hash, unsigned int n) const
    {
        COutPoint outpt(hash, n);
        return (setSelected.count(outpt) > 0);
    }

    void Select(const COutPoint& output)
    {
        setSelected.insert(output);
    }

    void UnSelect(const COutPoint& output)
    {
        setSelected.erase(output);
    }

    void UnSelectAll()
    {
        setSelected.clear();
    }

    void ListSelected(std::vector<COutPoint>& vOutpoints) const
    {
        vOutpoints.assign(setSelected.begin(), setSelected.end());
    }

private:
    std::set<COutPoint> setSelected;
};

#endif // BITCOIN_COINCONTROL_H
