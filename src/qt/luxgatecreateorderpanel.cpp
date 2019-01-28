#include "luxgatecreateorderpanel.h"
#include "ui_luxgatecreateorderpanel.h"

LuxgateCreateOrderPanel::LuxgateCreateOrderPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateCreateOrderPanel)
{
    ui->setupUi(this);
}

LuxgateCreateOrderPanel::~LuxgateCreateOrderPanel()
{
    delete ui;
}
