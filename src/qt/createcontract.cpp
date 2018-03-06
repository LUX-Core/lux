#include "createcontract.h"
#include "ui_createcontract.h"

CreateContract::CreateContract(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CreateContract)
{
    ui->setupUi(this);
    ui->labelAmount->setText("Qt Label1");


    /*QFont font = label1->font();
    font.setPointSize(72);
    font.setBold(true);
    label1->setFont(font);*/
}

CreateContract::~CreateContract()
{
    delete ui;

}

void CreateContract::on_pushButtonCreateContract_clicked()
{
    ui->labelSolidity->setText("Qt Label1");
}
