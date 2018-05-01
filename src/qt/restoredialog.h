#ifndef RESTOREDIALOG_H
#define RESTOREDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui {
    class RestoreDialog;
}

class RestoreDialog : public QDialog
{
    Q_OBJECT

public:

    explicit RestoreDialog(QWidget *parent = 0);

    ~RestoreDialog();

    QString getParam();

    QString getFileName();

    void setModel(WalletModel *model);

private Q_SLOTS:
            void on_btnReset_clicked();

    void on_btnBoxRestore_accepted();

    void on_btnBoxRestore_rejected();

    void on_toolWalletPath_clicked();

private:
    Ui::RestoreDialog *ui;
    WalletModel *model;

};

#endif // RESTOREDIALOG_H
