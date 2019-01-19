#include "luxgatebidpanel.h"
#include "ui_luxgatebidpanel.h"

LuxgateBidPanel::LuxgateBidPanel(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::LuxgateBidPanel)
{
    ui->setupUi(this);
}

LuxgateBidPanel::~LuxgateBidPanel()
{
    delete ui;
}
