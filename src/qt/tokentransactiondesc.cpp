#include "tokentransactiondesc.h"
#include "tokentransactionrecord.h"
#include "main.h"
#include "wallet.h"

QString TokenTransactionDesc::FormatTxStatus(const CWalletTx& wtx)
{
    AssertLockHeld(cs_main);
    return QString();
}

QString TokenTransactionDesc::toHTML(CWallet *wallet, CWalletTx &wtx, TokenTransactionRecord *rec, int unit)
{
    QString strHTML;

    LOCK2(cs_main, wallet->cs_wallet);
    strHTML.reserve(4000);
    strHTML += "<html><font face='verdana, arial, helvetica, sans-serif'>";

    strHTML += "</font></html>";
    return strHTML;
}
