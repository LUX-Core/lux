#ifndef BITCOIN_QT_TOKENTRANSACTIONDESC_H
#define BITCOIN_QT_TOKENTRANSACTIONDESC_H

#include <QObject>
#include <QString>

class TokenTransactionRecord;

class CWallet;
class CWalletTx;

/** Provide a human-readable extended HTML description of a token transaction.
 */
class TokenTransactionDesc: public QObject
{
    Q_OBJECT

public:
    static QString toHTML(CWallet *wallet, CWalletTx &wtx, TokenTransactionRecord *rec, int unit);

private:
    TokenTransactionDesc() {}

    static QString FormatTxStatus(const CWalletTx& wtx);
};

#endif // BITCOIN_QT_TOKENTRANSACTIONDESC_H
