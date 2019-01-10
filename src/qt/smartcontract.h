#ifndef SMARTCONTRACT_H
#define SMARTCONTRACT_H
#include "solidity_ide/editcontract.h"
#include <QWidget>

class CallContractPage;
class CreateContract;
class SendToContract;
class WalletModel;
class ClientModel;
//class EditContract;

namespace Ui {
class SmartContract;
}

class SmartContract : public QWidget {
    Q_OBJECT

public:
    explicit SmartContract(QWidget *parent = 0);
    ~SmartContract();

    void setClientModel(ClientModel *clientModel);
    void setModel(WalletModel *model);

private:

    Ui::SmartContract* ui;

    CallContractPage* callContractPage;
    EditContract* createContractPage;
    SendToContract* sendToContractPage;

    WalletModel* m_model;
    ClientModel* m_clientModel;
};


#endif //SMARTCONTRACT_H
