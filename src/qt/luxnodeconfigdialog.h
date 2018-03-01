#ifndef LUXNODECONFIGDIALOG_H
#define LUXNODECONFIGDIALOG_H

#include <QDialog>

namespace Ui {
    class LuxNodeConfigDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog showing transaction details. */
class LuxNodeConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LuxNodeConfigDialog(QWidget *parent = 0, QString nodeAddress = "123.456.789.123:28666", QString privkey="MASTERNODEPRIVKEY");
    ~LuxNodeConfigDialog();

private:
    Ui::LuxNodeConfigDialog *ui;
};

#endif // LUXNODECONFIGDIALOG_H
