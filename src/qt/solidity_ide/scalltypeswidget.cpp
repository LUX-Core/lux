#include "scalltypeswidget.h"
#include "ui_scalltypeswidget.h"

#include "sctypesdelegate.h"
#include "sctypesmodel.h"

ScAllTypesWidget::ScAllTypesWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::CsAllTypesWidget)
{
    ui->setupUi(this);
    //fill in cs types list
    {
        ScTypesModel * model = new ScTypesModel(this);
        model->insertRows(0,_numTypes);
        model->setData(_CrowdsaleType, "", Qt::DecorationRole);
        model->setData(_CrowdsaleType, "Crowdsale contract", ScTypesModel::TypeRole);
        model->setData(_CrowdsaleType, "Start your ICO/Token sale", ScTypesModel::DescriptionRole);
        model->setData(_TokenType, "://imgs/cryptotoken.png", Qt::DecorationRole);
        model->setData(_TokenType, "Token contract", ScTypesModel::TypeRole);
        model->setData(_TokenType, "Create a Token", ScTypesModel::DescriptionRole);
        ui->listViewSC_Types->setModel(model);
        ui->listViewSC_Types->setItemDelegate(new ScTypesDelegate(this));
        ui->listViewSC_Types->setMouseTracking(true);
    }

    ui->widgetTytle->hide();

    connect(ui->listViewSC_Types, &QListView::clicked,
            this, &ScAllTypesWidget::slotItemClicked);
}

void ScAllTypesWidget::slotItemClicked(const QModelIndex &index)
{
    if(_TokenType == index.row())
        emit goToToken();
    if(_CrowdsaleType == index.row())
        emit goToCrowdSale();
}

ScAllTypesWidget::~ScAllTypesWidget()
{
    delete ui;
}
