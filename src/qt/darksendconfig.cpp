#include "darksendconfig.h"
#include "ui_darksendconfig.h"

#include "bitcoinunits.h"
#include "guiconstants.h"
#include "optionsmodel.h"
#include "walletmodel.h"
#include "init.h"

#include <QMessageBox>
#include <QPushButton>
#include <QKeyEvent>
#include <QSettings>

LuxsendConfig::LuxsendConfig(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LuxsendConfig),
    model(0)
{
    ui->setupUi(this);

    connect(ui->buttonBasic, SIGNAL(clicked()), this, SLOT(clickBasic()));
    connect(ui->buttonHigh, SIGNAL(clicked()), this, SLOT(clickHigh()));
    connect(ui->buttonMax, SIGNAL(clicked()), this, SLOT(clickMax()));
}

LuxsendConfig::~LuxsendConfig()
{
    delete ui;
}

void LuxsendConfig::setModel(WalletModel *model)
{
    this->model = model;
}

void LuxsendConfig::clickBasic()
{
    configure(true, 1000, 2);

    QString strAmount(BitcoinUnits::formatWithUnit(
        model->getOptionsModel()->getDisplayUnit(), 1000 * COIN));
    QMessageBox::information(this, tr("Luxsend Configuration"),
        tr(
            "Luxsend was successfully set to basic (%1 and 2 rounds). You can change this at any time by opening Lux's configuration screen."
        ).arg(strAmount)
    );

    close();
}

void LuxsendConfig::clickHigh()
{
    configure(true, 1000, 8);

    QString strAmount(BitcoinUnits::formatWithUnit(
        model->getOptionsModel()->getDisplayUnit(), 1000 * COIN));
    QMessageBox::information(this, tr("Luxsend Configuration"),
        tr(
            "Luxsend was successfully set to high (%1 and 8 rounds). You can change this at any time by opening Lux's configuration screen."
        ).arg(strAmount)
    );

    close();
}

void LuxsendConfig::clickMax()
{
    configure(true, 1000, 16);

    QString strAmount(BitcoinUnits::formatWithUnit(
        model->getOptionsModel()->getDisplayUnit(), 1000 * COIN));
    QMessageBox::information(this, tr("Luxsend Configuration"),
        tr(
            "Luxsend was successfully set to maximum (%1 and 16 rounds). You can change this at any time by opening Bitcoin's configuration screen."
        ).arg(strAmount)
    );

    close();
}

void LuxsendConfig::configure(bool enabled, int coins, int rounds) {

    QSettings settings;

    settings.setValue("nLuxsendRounds", rounds);
    settings.setValue("nAnonymizeLuxAmount", coins);

    nLuxsendRounds = rounds;
    nAnonymizeLuxAmount = coins;
}

