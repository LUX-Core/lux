#include "sidetoolbar.h"
#include "ui_sidetoolbar.h"

sidetoolbar::sidetoolbar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::sidetoolbar)
{
    ui->setupUi(this);
}

sidetoolbar::~sidetoolbar()
{
    delete ui;
}

void sidetoolbar::on_sidetoolbar_destroyed()
{

}
