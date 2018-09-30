// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/lux-config.h"
#endif

#include "i2poptions.h"
#include "ui_i2poptions.h"

#include "optionsmodel.h"
#include "monitoreddatamapper.h"
#include "i2pshowaddresses.h"
#include "util.h"
#include "clientmodel.h"


I2POptions::I2POptions(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::I2POptions),
    clientModel(0)
{
    ui->setupUi(this);

    QObject::connect(ui->pushButtonCurrentI2PAddress,  SIGNAL(clicked()), this, SLOT(ShowCurrentI2PAddress()));
    QObject::connect(ui->pushButtonGenerateI2PAddress, SIGNAL(clicked()), this, SLOT(GenerateNewI2PAddress()));

    QObject::connect(ui->checkBoxAllowZeroHop         , SIGNAL(stateChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->checkBoxInboundAllowZeroHop  , SIGNAL(stateChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->checkBoxUseI2POnly           , SIGNAL(stateChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->lineEditSAMHost              , SIGNAL(textChanged(QString)), this, SIGNAL(settingsChanged()));
    QObject::connect(ui->lineEditTunnelName           , SIGNAL(textChanged(QString)), this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxInboundBackupQuality  , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxInboundIPRestriction  , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxInboundLength         , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxInboundLengthVariance , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxInboundQuantity       , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundBackupQuantity, SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundIPRestriction , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundLength        , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundLengthVariance, SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundPriority      , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxOutboundQuantity      , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
    QObject::connect(ui->spinBoxSAMPort               , SIGNAL(valueChanged(int))   , this, SIGNAL(settingsChanged()));
}

I2POptions::~I2POptions()
{
    delete ui;
}

void I2POptions::setMapper(MonitoredDataMapper& mapper)
{
    mapper.addMapping(ui->checkBoxUseI2POnly           , OptionsModel::eI2PUseI2POnly);
    mapper.addMapping(ui->lineEditSAMHost              , OptionsModel::eI2PSAMHost);
    mapper.addMapping(ui->spinBoxSAMPort               , OptionsModel::eI2PSAMPort);
    mapper.addMapping(ui->lineEditTunnelName           , OptionsModel::eI2PSessionName);
    mapper.addMapping(ui->spinBoxInboundQuantity       , OptionsModel::I2PInboundQuantity);
    mapper.addMapping(ui->spinBoxInboundLength         , OptionsModel::I2PInboundLength);
    mapper.addMapping(ui->spinBoxInboundLengthVariance , OptionsModel::I2PInboundLengthVariance);
    mapper.addMapping(ui->spinBoxInboundBackupQuality  , OptionsModel::I2PInboundBackupQuantity);
    mapper.addMapping(ui->checkBoxInboundAllowZeroHop  , OptionsModel::I2PInboundAllowZeroHop);
    mapper.addMapping(ui->spinBoxInboundIPRestriction  , OptionsModel::I2PInboundIPRestriction);
    mapper.addMapping(ui->spinBoxOutboundQuantity      , OptionsModel::I2POutboundQuantity);
    mapper.addMapping(ui->spinBoxOutboundLength        , OptionsModel::I2POutboundLength);
    mapper.addMapping(ui->spinBoxOutboundLengthVariance, OptionsModel::I2POutboundLengthVariance);
    mapper.addMapping(ui->spinBoxOutboundBackupQuantity, OptionsModel::I2POutboundBackupQuantity);
    mapper.addMapping(ui->checkBoxAllowZeroHop         , OptionsModel::I2POutboundAllowZeroHop);
    mapper.addMapping(ui->spinBoxOutboundIPRestriction , OptionsModel::I2POutboundIPRestriction);
    mapper.addMapping(ui->spinBoxOutboundPriority      , OptionsModel::I2POutboundPriority);
}

void I2POptions::setModel(ClientModel* model)
{
    clientModel = model;
}

void I2POptions::ShowCurrentI2PAddress()
{
    if (clientModel)
    {
        const QString pub = clientModel->getPublicI2PKey();
        const QString priv = clientModel->getPrivateI2PKey();
        const QString b32 = clientModel->getB32Address(pub);
        const QString configFile = QString::fromStdString(GetConfigFile().string());
#if 0
        ShowI2PAddresses i2pCurrDialog("Your current I2P-address", pub, priv, b32, configFile, this);
        i2pCurrDialog.exec();
#endif
    }
}

void I2POptions::GenerateNewI2PAddress()
{
    if (clientModel)
    {
        QString pub, priv;
        clientModel->generateI2PDestination(pub, priv);
        const QString b32 = clientModel->getB32Address(pub);
        const QString configFile = QString::fromStdString(GetConfigFile().string());
#if 0
        ShowI2PAddresses i2pCurrDialog("Generated I2P address", pub, priv, b32, configFile, this);
        i2pCurrDialog.exec();
#endif
    }
}


