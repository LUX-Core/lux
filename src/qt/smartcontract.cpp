#include "smartcontract.h"

#include "callcontract.h"
#include "clientmodel.h"
#include "createcontract.h"
#include "sendtocontract.h"
#include "ui_smartcontract.h"
#include "walletmodel.h"

SmartContract::SmartContract(QWidget *parent) : QWidget(parent), ui(new Ui::SmartContract), m_model(0) ,m_clientModel(0) {
    ui->setupUi(this);

    createContractPage = new CreateContract();
    ui->SmartContractTabWidget->addTab(createContractPage, tr("Create Smart Contract"));

    callContractPage = new CallContractPage();
    ui->SmartContractTabWidget->addTab(callContractPage, tr("Read Smart Contract"));

    sendToContractPage = new SendToContract();
    ui->SmartContractTabWidget->addTab(sendToContractPage, tr("Write to Smart Contract"));
}

SmartContract::~SmartContract() {
    if (callContractPage) { delete callContractPage; callContractPage = nullptr; }
    if (createContractPage) { delete createContractPage; createContractPage = nullptr; }
    if (sendToContractPage) { delete sendToContractPage; sendToContractPage = nullptr; }
    delete ui;
}

void SmartContract::setClientModel(ClientModel *clientModel) {
    m_clientModel = clientModel;
    callContractPage->setClientModel(clientModel);
    createContractPage->setClientModel(clientModel);
    sendToContractPage->setClientModel(clientModel);
}

void SmartContract::setModel(WalletModel *model) {
    m_model = model;
    callContractPage->setModel(model);
    createContractPage->setModel(model);
    sendToContractPage->setModel(model);
}
