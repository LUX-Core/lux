#include "hexaddressconverter.h"
#include "forms/ui_hexaddressconverter.h"
#include "base58.h"
#include "uint256.h"
#include "utilstrencodings.h"

#include <iostream>
#include <QClipboard>

#define paternAddress "^[a-fA-F0-9]{40,40}$"

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
    bool isAddressValid = true;

    if(!address.isEmpty()) {
        bool hasHexAddress = false;
        bool hasLuxAddress = false;
        QRegularExpression regEx(paternAddress);
        hasHexAddress = regEx.match(address).hasMatch();
        if (hasHexAddress) {
            ui->label->setText("Address (hex)");
            ui->label_2->setText("Result (Lux)");
            uint160 key(ParseHex(address.toStdString()));
            CKeyID keyid(key);
            if(IsValidDestination(CTxDestination(keyid))) {
                ui->resultLabel->setText(QString::fromStdString(EncodeDestination(CTxDestination(keyid))));
            }
        }

        CTxDestination addr = DecodeDestination(address.toStdString());
        hasLuxAddress = IsValidDestination(addr);
        if (hasLuxAddress) {
            ui->label->setText("Address (Lux)");
            ui->label_2->setText("Result (hex)");
            CKeyID *keyid = boost::get<CKeyID>(&addr);

            if (keyid) {
                ui->resultLabel->setText(QString::fromStdString(HexStr(valtype(keyid->begin(),keyid->end()))));
            }

        }

        isAddressValid = hasHexAddress || hasLuxAddress;
    }

    if(!isAddressValid) ui->resultLabel->setText("");

    ui->addressEdit->setValid(isAddressValid);
}

void HexAddressConverter::copyButtonClicked() {
    if (QClipboard * clipboard = QApplication::clipboard()) {
        clipboard->setText(ui->resultLabel->text());
    }
}


