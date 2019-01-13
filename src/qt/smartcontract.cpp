#include "smartcontract.h"

#include "callcontract.h"
#include "clientmodel.h"
#include "createcontract.h"
#include "sendtocontract.h"
#include "solidity_ide/editcontract.h"
#include "ui_smartcontract.h"
#include "walletmodel.h"

SmartContract::SmartContract(QWidget *parent) : QWidget(parent), ui(new Ui::SmartContract), m_model(0) ,m_clientModel(0) {
    ui->setupUi(this);

    createContractPage = new EditContract(this);
    ui->SmartContractTabWidget->addTab(createContractPage, tr("Create Smart Contract"));

    callContractPage = new CallContractPage(this);
    ui->SmartContractTabWidget->addTab(callContractPage, tr("Read Smart Contract"));

    sendToContractPage = new SendToContract(this);
    ui->SmartContractTabWidget->addTab(sendToContractPage, tr("Write to Smart Contract"));
}

SmartContract::~SmartContract() {
    delete ui;
}

void SmartContract::setClientModel(ClientModel *clientModel) {
    m_clientModel = clientModel;
    callContractPage->setClientModel(clientModel);
    //createContractPage->setClientModel(clientModel);
    sendToContractPage->setClientModel(clientModel);
}

void SmartContract::setModel(WalletModel *model) {
    m_model = model;
    callContractPage->setModel(model);
    //createContractPage->setModel(model);
    sendToContractPage->setModel(model);
}
