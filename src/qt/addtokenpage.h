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

Q_SIGNALS:
    void on_addNewToken(QString _address, QString _name, QString _symbol, int _decimals, double _balance);

private:
    Ui::AddTokenPage *ui;
    Token *m_tokenABI;
    WalletModel* m_model;
    ClientModel* m_clientModel;
};

#endif // ADDTOKENPAGE_H
