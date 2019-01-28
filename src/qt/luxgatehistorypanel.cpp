#include "luxgatehistorypanel.h"
#include "ui_luxgatehistorypanel.h"

LuxgateHistoryPanel::LuxgateHistoryPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateHistoryPanel)
{
    ui->setupUi(this);
}

LuxgateHistoryPanel::~LuxgateHistoryPanel()
{
    delete ui;
}
