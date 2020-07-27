#ifndef ETHADDRESSLINEEDIT_H
#define ETHADDRESSLINEEDIT_H

#include "qvalidatedlineedit.h"
#include "converter.h"

class QAction;

class EthAddressLineEdit : public QValidatedLineEdit
{
    public:
        EthAddressLineEdit(QWidget* parent);

    protected:
        void init();
        QAction *hexConvertAction;
        converter addToHex;
};

#endif
