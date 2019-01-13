#include "sctokenwgt.h"
#include "ui_sctokenwgt.h"

ScTokenWgt::ScTokenWgt(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ScTokenWgt)
{
    ui->setupUi(this);
}

ScTokenWgt::~ScTokenWgt()
{
    delete ui;
}
