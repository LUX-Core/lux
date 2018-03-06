#include "luxnodeconfigdialog.h"
#include "ui_luxnodeconfigdialog.h"

#include <QModelIndex>

LuxNodeConfigDialog::LuxNodeConfigDialog(QWidget *parent, QString nodeAddress, QString privkey) :
    QDialog(parent),
    ui(new Ui::LuxNodeConfigDialog)
{
    ui->setupUi(this);
    QString desc = "rpcallowip=127.0.0.1<br>rpcuser=REPLACEME<br>rpcpassword=REPLACEME<br>staking=0<br>server=1<br>listen=1<br>port=REPLACEMEWITHYOURPORT<br>masternode=1<br>masternodeaddr=" + nodeAddress + "<br>masternodeprivkey=" + privkey + "<br>";
    ui->detailText->setHtml(desc);
}

LuxNodeConfigDialog::~LuxNodeConfigDialog()
{
    delete ui;
}
