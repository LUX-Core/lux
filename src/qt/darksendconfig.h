#ifndef DARKSENDCONFIG_H
#define DARKSENDCONFIG_H

#include <QDialog>

namespace Ui {
    class LuxsendConfig;
}
class WalletModel;

/** Multifunctional dialog to ask for passphrases. Used for encryption, unlocking, and changing the passphrase.
 */
class LuxsendConfig : public QDialog
{
    Q_OBJECT

public:

    LuxsendConfig(QWidget *parent = 0);
    ~LuxsendConfig();

    void setModel(WalletModel *model);


private:
    Ui::LuxsendConfig *ui;
    WalletModel *model;
    void configure(bool enabled, int coins, int rounds);

private slots:

    void clickBasic();
    void clickHigh();
    void clickMax();
};

#endif // DARKSENDCONFIG_H

