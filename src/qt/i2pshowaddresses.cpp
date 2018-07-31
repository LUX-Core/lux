// Copyright (c) 2013-2017 The Anoncoin Core developers
// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "i2pshowaddresses.h"
#include "ui_i2pshowaddresses.h"

#include "guiutil.h"
#include "util.h"

ShowI2PAddresses::ShowI2PAddresses(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ShowI2PAddresses)
{
    ui->setupUi(this);
    GUIUtil::restoreWindowGeometry("nI2pDestinationDetails", this->size(), this);

    // this->setWindowTitle( tr("Private I2P Destination Details") );  // now set correctly in i2pshowaddresses.ui

    QObject::connect(ui->privButton, SIGNAL(clicked()), ui->privText, SLOT(selectAll()));
    QObject::connect(ui->privButton, SIGNAL(clicked()), ui->privText, SLOT(copy()));

    QObject::connect(ui->pubButton, SIGNAL(clicked()), ui->pubText, SLOT(selectAll()));
    QObject::connect(ui->pubButton, SIGNAL(clicked()), ui->pubText, SLOT(copy()));

    QObject::connect(ui->b32Button, SIGNAL(clicked()), ui->b32Line, SLOT(selectAll()));
    QObject::connect(ui->b32Button, SIGNAL(clicked()), ui->b32Line, SLOT(copy()));

    QObject::connect(ui->okButton,  SIGNAL(clicked()), this, SLOT(hide()));

    // Stop the user from being able to change the checkboxes...
    QObject::connect(ui->enableI2pBox, SIGNAL(clicked()), this, SLOT(setEnabled()));
    QObject::connect(ui->staticI2pBox, SIGNAL(clicked()), this, SLOT(setStatic()));
}


ShowI2PAddresses::~ShowI2PAddresses()
{
    GUIUtil::saveWindowGeometry("nI2pDestinationDetails", this);
    delete ui;
}

void ShowI2PAddresses::UpdateParameters( void )
{
    ui->configText->setText( ui->configText->text() + QString::fromStdString( "<b>" + GetConfigFile().string() + "</b>" ) );

    setEnabled();
    setStatic();

    QString priv = QString::fromStdString( GetArg("-i2p.mydestination.privatekey", "???") );
    ui->privText->setText("<b>privatekey=</b>" + priv);

    QString pub = QString::fromStdString( GetArg("-i2p.mydestination.publickey", "???AAAA") );
    ui->pubText->setPlainText(pub);

    QString b32 = QString::fromStdString( GetArg("-i2p.mydestination.base32key", "???.b32.i2p") );
    ui->b32Line->setText(b32);
}

void ShowI2PAddresses::setEnabled( void )
{
    ui->enableI2pBox->setChecked( GetBoolArg("-i2p.options.enabled", false) );
}

void ShowI2PAddresses::setStatic( void )
{
    ui->staticI2pBox->setChecked( GetBoolArg("-i2p.mydestination.static", false) );
}
