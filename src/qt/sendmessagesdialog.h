#ifndef SENDMESSAGESDIALOG_H
#define SENDMESSAGESDIALOG_H

#include <QDialog>
#include <QString>

namespace Ui {
    class SendMessagesDialog;
}

class MessageModel;
class SendMessagesEntry;
class SendMessagesRecipient;

//QT_BEGIN_NAMESPACE
//class QUrl;
//QT_END_NAMESPACE

/** Dialog for sending messages */
class SendMessagesDialog : public QDialog
{
    Q_OBJECT

public:

    enum Mode {
        Encrypted,
        Anonymous,
    };

    enum Type {
        Page,
        Dialog,
    };

    explicit SendMessagesDialog(Mode mode, Type type, QWidget *parent = 0);
    ~SendMessagesDialog();

    void setModel (MessageModel *model);
    void loadRow(int row);
    bool checkMode(Mode mode);
    bool validate ();

    /** Set up the tab chain manually, as Qt messes up the tab chain by default in some cases (issue https://bugreports.qt-project.org/browse/QTBUG-10907).
     */
    QWidget *setupTabChain(QWidget *prev);

    void pasteEntry(const SendMessagesRecipient &rv);

public slots:
    void done(int retval);
    void clear();
    void reject();
    void accept();
    SendMessagesEntry *addEntry();
    void updateRemoveEnabled();

private:
    Ui::SendMessagesDialog *ui;
    MessageModel *model;
    bool fNewRecipientAllowed;
    Mode mode;
    Type type;

private slots:
    void on_sendButton_clicked();
    void removeEntry(SendMessagesEntry* entry);
    void on_addressBookButton_clicked();
    void on_pasteButton_clicked();
};

#endif // SENDMESSAGESDIALOG_H
