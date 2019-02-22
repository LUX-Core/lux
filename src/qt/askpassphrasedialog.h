// Copyright (c) 2011-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ASKPASSPHRASEDIALOG_H
#define BITCOIN_QT_ASKPASSPHRASEDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui
{
class AskPassphraseDialog;
}

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class AskPassphraseDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        Encrypt,         /**< Ask passphrase twice and encrypt */
        UnlockAnonymize, /**< Ask passphrase and unlock only for anonymization */
        Unlock,          /**< Ask passphrase and unlock */
        ChangePass,      /**< Ask old passphrase + new passphrase twice */
        Decrypt          /**< Ask passphrase and decrypt wallet */
    };

    explicit AskPassphraseDialog(Mode mode, QWidget* parent, WalletModel* model);
    ~AskPassphraseDialog();

    void accept();

    void setModel(WalletModel* model);

private:
    Ui::AskPassphraseDialog* ui;
    Mode mode;
    WalletModel* model;
    bool fCapsLock;

private Q_SLOTS:
    void textChanged();
    void secureClearPassFields();

protected:
    bool event(QEvent* event);
    bool eventFilter(QObject* object, QEvent* event);
};

#endif // BITCOIN_QT_ASKPASSPHRASEDIALOG_H
