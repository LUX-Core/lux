#include "sctypewidget.h"
#include "ui_sctypewidget.h"
#include "sctypesmodel.h"

#include <QGraphicsDropShadowEffect>

ScTypeWidget::ScTypeWidget(const ScTypesItem & item, QWidget *parent) :
    QFrame(parent),
    ui(new Ui::csTypeWidget)
{
    ui->setupUi(this);
    if(!item.strIcon.isEmpty())
        ui->labelIcon->setPixmap(QPixmap(item.strIcon));
    ui->labelDescription->setText(item.strDescription);
    ui->labelType->setText(item.strType);
}

ScTypeWidget::~ScTypeWidget()
{
    delete ui;
}
