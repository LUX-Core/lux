#include "findwidget.h"
#include "ui_findwidget.h"

#include "find/searchresulttreeitemdelegate.h"

FindWidget::FindWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FindWidget)
{
    ui->setupUi(this);

    auto resultDelegate = new SearchResultTreeItemDelegate(10, this);
    ui->treeWidgetFindResults->setItemDelegate(resultDelegate);
    ui->treeWidgetFindResults->setColumnCount(1);

    //signals-slots connects
    {
        //find results
        connect(ui->pushButtonCloseFindResults, &QPushButton::clicked,
                this, &FindWidget::sigNeedHiding);
        connect(ui->treeWidgetFindResults, &TreeWidgetFindResults::currentItemChanged,
                this, &FindWidget::slotCurrentFindResultChanged);
    }
}

void FindWidget::slotReadyFindResults(findResults newResults)
{
    ui->treeWidgetFindResults->clear();
    auto model = ui->treeWidgetFindResults->model();
    foreach(QString fileName, newResults.keys())
    {
        QTreeWidgetItem * topItem = new QTreeWidgetItem(ui->treeWidgetFindResults,
                                                        QStringList() << fileName);
        ui->treeWidgetFindResults->insertTopLevelItem(0,topItem);
        for(int i=0; i< newResults[fileName].size(); i++)
        {
            auto searchItem = newResults[fileName][i];
            auto item = new QTreeWidgetItem(topItem,
                                            QStringList() <<searchItem.strLine);
            topItem->addChild(item);
            auto index = ui->treeWidgetFindResults->indexFromItemPublic(item,0);
            model->setData(index,searchItem.length, defLengthFindRole);
            model->setData(index,searchItem.start, defStartFindRole);
            model->setData(index,searchItem.numLine, defNumberLineRole);
        }
    }
}

void FindWidget::slotCurrentFindResultChanged(QTreeWidgetItem *current)
{
    if(current != nullptr)
    {
        auto model = ui->treeWidgetFindResults->model();
        auto index = ui->treeWidgetFindResults->indexFromItemPublic(current,0);
        if(current->parent() != nullptr)
        {
            int blockNumber = model->data(index, defNumberLineRole).toInt() - 1;
            int posResult = model->data(index, defStartFindRole).toInt();
            auto parentItem = current->parent();
            auto parentIndex = ui->treeWidgetFindResults->indexFromItemPublic(parentItem,0);
            auto nameFile = model->data(parentIndex, Qt::DisplayRole).toString();
            emit sigFindResultChoosed(nameFile, blockNumber, posResult);
        }
    }
}

FindWidget::~FindWidget()
{
    delete ui;
}
