#include "hexaddressconverter.h"
#include "forms/ui_hexaddressconverter.h"
#include "converter.h"

#include <iostream>
#include <QClipboard>

HexAddressConverter::HexAddressConverter(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::HexAddressConverter) {
    ui->setupUi(this);

    ui->copyButton->setIcon(QIcon(":/icons/editcopy"));

    connect(ui->addressEdit, SIGNAL(textChanged(const QString&)), this, SLOT(addressChanged(const QString&)));
    connect(ui->copyButton, SIGNAL(clicked()), this, SLOT(copyButtonClicked()));
}

HexAddressConverter::~HexAddressConverter() {
    delete ui;
}

void HexAddressConverter::addressChanged(const QString& address) {

    converter* addToHex = new converter();
    bool isAddressValid, checkedValidityHex, checkedValidityAddr = false;
    QString convertedAddr = "";
    addToHex->twoWayConverter(address, &convertedAddr, &checkedValidityHex, &checkedValidityAddr);

    if(!address.isEmpty()) {

        if (checkedValidityHex) {
            ui->label->setText("Address (hex)");
            ui->label_2->setText("Result (Lux)");
            ui->resultLabel->setText(convertedAddr);
        }

        if (checkedValidityAddr) {
            ui->label->setText("Address (Lux)");
            ui->label_2->setText("Result (hex)");
            ui->resultLabel->setText(convertedAddr);
        }
        isAddressValid = checkedValidityHex || checkedValidityAddr;
    }

    if(!isAddressValid) ui->resultLabel->setText("");

    ui->addressEdit->setValid(isAddressValid);
}

void HexAddressConverter::copyButtonClicked() {
    if (QClipboard * clipboard = QApplication::clipboard()) {
        clipboard->setText(ui->resultLabel->text());
    }
}


