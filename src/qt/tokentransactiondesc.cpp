#include "tokentransactiondesc.h"
#include "tokentransactionrecord.h"
#include "main.h"
#include "wallet.h"
#include "timedata.h"
#include "bitcoinunits.h"
#include "guiutil.h"
#include "uint256.h"

QString TokenTransactionDesc::FormatTxStatus(CWallet *wallet, const CTokenTx& wtx)
{
    AssertLockHeld(cs_main);

    // Determine transaction status
    int nDepth= 0;
    if(wtx.blockNumber == -1)
    {
        nDepth = 0;
    }
    else
    {
        nDepth = chainActive.Height() - wtx.blockNumber + 1;
    }

    auto mi = wallet->mapWallet.find(wtx.transactionHash);
    if (nDepth < 0)
        return tr("conflicted with a transaction with %1 confirmations").arg(-nDepth);
    else if (mi != wallet->mapWallet.end() && (GetAdjustedTime() - mi->second.nTimeReceived > 2 * 60) && mi->second.GetRequestCount() == 0)
        return tr("%1/offline").arg(nDepth);
    else if (nDepth == 0)
    {
        if(mi != wallet->mapWallet.end() && mi->second.InMempool())
            return tr("0/unconfirmed, in memory pool");
        else
            return tr("0/unconfirmed, not in memory pool");
    }
    else if (nDepth < TokenTransactionRecord::RecommendedNumConfirmations)
        return tr("%1/unconfirmed").arg(nDepth);
    else
        return tr("%1 confirmations").arg(nDepth);
}

QString TokenTransactionDesc::toHTML(CWallet *wallet, CTokenTx &wtx, TokenTransactionRecord *rec)
{
    QString strHTML;

    LOCK2(cs_main, wallet->cs_wallet);
    strHTML.reserve(4000);
    strHTML += "<html><font face='verdana, arial, helvetica, sans-serif'>";

    int64_t nTime = rec->time;
    dev::s256 nDebit = rec->debit;
    dev::s256 nCredit = rec->credit;
    dev::s256 nNet = nCredit + nDebit;
    QString unit = QString::fromStdString(rec->tokenSymbol);

    strHTML += "<b>" + tr("Status") + ":</b> " + FormatTxStatus(wallet, wtx);

    strHTML += "<br>";

    strHTML += "<b>" + tr("Date") + ":</b> " + (nTime ? GUIUtil::dateTimeStr(nTime) : "") + "<br>";

    strHTML += "<b>" + tr("Transaction ID") + ":</b> " + rec->getTxID() + "<br>";

    strHTML += "<b>" + tr("Token Address") + ":</b> " + QString::fromStdString(wtx.strContractAddress) + "<br>";

    strHTML += "<b>" + tr("From") + ":</b> " + QString::fromStdString(wtx.strSenderAddress) + "<br>";

    strHTML += "<b>" + tr("To") + ":</b> " + QString::fromStdString(wtx.strReceiverAddress) + "<br>";

    if(nCredit > 0)
    {
         strHTML += "<b>" + tr("Credit") + ":</b> " + BitcoinUnits::formatTokenWithUnit(unit, rec->decimals, nCredit, true) + "<br>";
    }
    if(nDebit < 0)
    {
         strHTML += "<b>" + tr("Debit") + ":</b> " + BitcoinUnits::formatTokenWithUnit(unit, rec->decimals, nDebit, true) + "<br>";
    }
    strHTML += "<b>" + tr("Net Amount") + ":</b> " + BitcoinUnits::formatTokenWithUnit(unit, rec->decimals, nNet, true) + "<br>";


    strHTML += "</font></html>";
    return strHTML;
}
