#ifndef SENDTOKENPAGE_H
#define SENDTOKENPAGE_H

#include <QWidget>

class WalletModel;
class ClientModel;
class Token;
struct SelectedToken;

namespace Ui {
class SendTokenPage;
}

class SendTokenPage : public QWidget
{
    Q_OBJECT

public:
    explicit SendTokenPage(QWidget *parent = 0);
    ~SendTokenPage();

    void setModel(WalletModel *_model);
    void setClientModel(ClientModel *clientModel);
    void clearAll();
    bool isValidAddress();
    bool isDataValid();

    void setTokenData(std::string address, std::string sender, std::string symbol, int8_t decimals, std::string balance);

private Q_SLOTS:
    void on_clearButton_clicked();
    void on_numBlocksChanged();
    void on_updateConfirmButton();
    void on_confirmClicked();

private:
    Ui::SendTokenPage *ui;
    WalletModel* m_model;
    ClientModel* m_clientModel;
    Token *m_tokenABI;
    SelectedToken *m_selectedToken;
};

#endif // SENDTOKENPAGE_H
