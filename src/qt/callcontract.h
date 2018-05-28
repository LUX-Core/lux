#ifndef CALLCONTRACT_H
#define CALLCONTRACT_H

#include <QWidget>

class WalletModel;
class ClientModel;
class ContractTableModel;
class ExecRPCCommand;
class ABIFunctionField;
class ContractABI;
class TabBarInfo;

namespace Ui {
class CallContractPage;
}

class CallContractPage : public QWidget
{
    Q_OBJECT

public:
    explicit CallContractPage(QWidget *parent = 0);
    ~CallContractPage();

    void setClientModel(ClientModel *clientModel);
    void setModel(WalletModel *model);
    bool isValidContractAddress();
    bool isValidInterfaceABI();
    bool isDataValid();
    void setContractAddress(const QString &address);

Q_SIGNALS:

public Q_SLOTS:
    void on_clearAllClicked();
    void on_callContractClicked();
    void on_numBlocksChanged();
    void on_updateCallContractButton();
    void on_newContractABI();
    void on_saveInfoClicked();
    void on_loadInfoClicked();
    void on_pasteAddressClicked();
    void on_contractAddressChanged();

private:
    QString toDataHex(int func, QString& errorMessage);

private:
    Ui::CallContractPage *ui;
    WalletModel* m_model;
    ClientModel* m_clientModel;
    ContractTableModel* m_contractModel;
    ExecRPCCommand* m_execRPCCommand;
    ABIFunctionField* m_ABIFunctionField;
    ContractABI* m_contractABI;
    TabBarInfo* m_tabInfo;
    int m_results;
};

#endif // CALLCONTRACT_H
