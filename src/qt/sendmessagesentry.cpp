#include "sendmessagesentry.h"
#include "ui_sendmessagesentry.h"
#include "guiutil.h"
#include "addressbookpage.h"
#include "walletmodel.h"
#include "messagemodel.h"
#include "optionsmodel.h"
#include "addresstablemodel.h"

#include "smessage.h"

#include <QApplication>
#include <QClipboard>

SendMessagesEntry::SendMessagesEntry(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::SendMessagesEntry),
    model(0)
{
    ui->setupUi(this);

#ifdef Q_OS_MAC
    ui->sendToLayout->setSpacing(4);
#endif
#if QT_VERSION >= 0x040700
    /* Do not move this to the XML file, Qt before 4.7 will choke on it */
    ui->addAsLabel->setPlaceholderText(tr("Enter a label for this address to add it to your address book"));
    ui->sendTo->setPlaceholderText(tr("Enter a Lux address (e.g. CKPnZKDzaDXqEgKJ4GdUg58gXxgzA7mkny)"));
    ui->publicKey->setPlaceholderText(tr("Enter the public key for the address above, it is not in the blockchain"));
    ui->messageText->setErrorText(tr("You cannot send a blank message!"));
#endif
    setFocusPolicy(Qt::TabFocus);
    setFocusProxy(ui->sendTo);

    GUIUtil::setupAddressWidget(ui->sendTo, this);
}

SendMessagesEntry::~SendMessagesEntry()
{
    delete ui;
}

void SendMessagesEntry::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->sendTo->setText(QApplication::clipboard()->text());
}

void SendMessagesEntry::on_addressBookButton_clicked()
{
    if(!model)
        return;

    AddressBookPage dlg(AddressBookPage::ForSending, AddressBookPage::SendingTab, this);

    dlg.setModel(model->getWalletModel()->getAddressTableModel());

    if(dlg.exec())
    {

        ui->sendTo->setText(dlg.getReturnValue());

        if(ui->publicKey->text() == "")
            ui->publicKey->setFocus();
        else
            ui->messageText->setFocus();
    }
}

void SendMessagesEntry::on_sendTo_textChanged(const QString &address)
{
    if(!model)
        return;

    QString pubkey;
    QString sendTo = address;

    if(model->getAddressOrPubkey(sendTo, pubkey))
    {

        ui->publicKey->setText(pubkey);
    }
    else
    {
        ui->publicKey->show();
        ui->publicKeyLabel->show();
    }

    // Fill in label from address book, if address has an associated label
    QString associatedLabel = model->getWalletModel()->getAddressTableModel()->labelForAddress(address);

    if(!associatedLabel.isEmpty())
        ui->addAsLabel->setText(associatedLabel);
}

void SendMessagesEntry::setModel(MessageModel *model)
{

    this->model = model;

    //clear();
}

void SendMessagesEntry::loadRow(int row)
{
    if(model->data(model->index(row, model->Type, QModelIndex()), Qt::DisplayRole).toString() == MessageModel::Received)
        ui->sendTo->setText(model->data(model->index(row, model->FromAddress, QModelIndex()), Qt::DisplayRole).toString());
    else
        ui->sendTo->setText(model->data(model->index(row, model->ToAddress, QModelIndex()), Qt::DisplayRole).toString());
}

void SendMessagesEntry::setRemoveEnabled(bool enabled)
{
    ui->deleteButton->setEnabled(enabled);
}

void SendMessagesEntry::clear()
{
    ui->sendTo->clear();
    ui->addAsLabel->clear();
    ui->messageText->clear();
    ui->sendTo->setFocus();
}

void SendMessagesEntry::on_deleteButton_clicked()
{
    emit removeEntry(this);
}


bool SendMessagesEntry::validate()
{
    // Check input validity
    bool retval = true;

    if(ui->messageText->toPlainText() == "")
    {
        ui->messageText->setValid(false);

        retval = false;
    }

    if(!ui->sendTo->hasAcceptableInput() || (!model->getWalletModel()->validateAddress(ui->sendTo->text())))
    {
        ui->sendTo->setValid(false);

        retval = false;
    }

    if(ui->publicKey->text() == "")
    {
        ui->publicKey->setValid(false);
        ui->publicKey->show();

        retval = false;
    }

    return retval;
}

SendMessagesRecipient SendMessagesEntry::getValue()
{
    SendMessagesRecipient rv;

    rv.address = ui->sendTo->text();
    rv.label = ui->addAsLabel->text();
    rv.pubkey = ui->publicKey->text();
    rv.message = ui->messageText->toPlainText();

    return rv;
}


QWidget *SendMessagesEntry::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, ui->sendTo);
    QWidget::setTabOrder(ui->sendTo, ui->addressBookButton);
    QWidget::setTabOrder(ui->addressBookButton, ui->pasteButton);
    QWidget::setTabOrder(ui->pasteButton, ui->deleteButton);
    QWidget::setTabOrder(ui->deleteButton, ui->addAsLabel);
    QWidget::setTabOrder(ui->addAsLabel, ui->publicKey);
    QWidget::setTabOrder(ui->publicKey, ui->messageText);

    return ui->messageText;

}

void SendMessagesEntry::setValue(const SendMessagesRecipient &value)
{
    ui->sendTo->setText(value.address);
    ui->addAsLabel->setText(value.label);
    ui->publicKey->setText(value.pubkey);
    ui->messageText->setPlainText(value.message);
}

bool SendMessagesEntry::isClear()
{
    return ui->sendTo->text().isEmpty();
}

void SendMessagesEntry::setFocus()
{
    ui->sendTo->setFocus();
}
