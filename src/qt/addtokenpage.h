#ifndef ADDTOKENPAGE_H
#define ADDTOKENPAGE_H

#include <QWidget>
class Token;
class WalletModel;
class ClientModel;

namespace Ui {
    class AddTokenPage;
}

class AddTokenPage : public QWidget
{
    Q_OBJECT

public:
    explicit AddTokenPage(QWidget *parent = 0);
    ~AddTokenPage();
    void clearAll();
    void setModel(WalletModel *_model);
    void setClientModel(ClientModel *clientModel);

private Q_SLOTS:
            void on_clearButton_clicked();
    void on_confirmButton_clicked();
    void on_addressChanged();
    void on_numBlocksChanged();
    void on_updateConfirmButton();
    void on_zeroBalanceAddressToken(bool enable);

    Q_SIGNALS:

private:
    Ui::AddTokenPage *ui;
    Token *m_tokenABI;
    WalletModel* m_model;
    ClientModel* m_clientModel;
    bool m_validTokenAddress;
};

#endif // ADDTOKENPAGE_H
