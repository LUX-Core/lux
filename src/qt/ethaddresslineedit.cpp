#include <QAction>
#include <QToolTip>
#include "ethaddresslineedit.h"


EthAddressLineEdit::EthAddressLineEdit(QWidget* parent) : QValidatedLineEdit(parent)
{
    init();
}

void EthAddressLineEdit::init()
{
    hexConvertAction = addAction(QIcon(":/icons/eth"), QLineEdit::TrailingPosition);
    hexConvertAction->setVisible(false);
    hexConvertAction->setToolTip("Address maybe not in LUX format\nPress to convert to HEX");

    connect(this, &QValidatedLineEdit::validationDidChange, 
        [&](QValidatedLineEdit *validatedLineEdit){
            bool checkedValidityHex, checkedValidityAddr = false;
            QString convertedAddr = "";
            addToHex.twoWayConverter(text(), &convertedAddr, &checkedValidityHex, &checkedValidityAddr);
            hexConvertAction->setVisible(checkedValidityAddr);
            if(checkedValidityAddr)
                 QToolTip::showText(this->mapToGlobal(geometry().topRight()), "Address maybe not in LUX format\nPress to convert to HEX", this);
        }
    );

    connect(hexConvertAction, &QAction::triggered, 
        [&](){
            bool checkedValidityHex, checkedValidityAddr = false;
            QString convertedAddr = "";
            addToHex.twoWayConverter(text(), &convertedAddr, &checkedValidityHex, &checkedValidityAddr);
            if(checkedValidityHex || checkedValidityAddr)
                setText(convertedAddr);
        }
    );
}