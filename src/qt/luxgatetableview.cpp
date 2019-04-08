#include "luxgatetableview.h"
#include "luxgategui_global.h"

#include <QMenu>
#include <QApplication>
#include <QClipboard>
#include <QSignalMapper>
#include <QAction>

#include <algorithm>

LuxgateTableView::LuxgateTableView(QWidget *parent):
    QTableView(parent),
    copyTableValuesMenu(new QMenu(this))
{
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setContextMenuPolicy(Qt::CustomContextMenu);

    QSignalMapper * sigMapper = new QSignalMapper(this);

    auto addAction = [this, sigMapper](int iCol, QString name)
            {
                auto action = copyTableValuesMenu->addAction(name);
                this->connect(action, SIGNAL(triggered()),
                        sigMapper,SLOT(map()));
                sigMapper->setMapping(action, iCol);
            };

    for (int i = 0; i < nPossibleColumns; i++){
        switch (i) {
        case RowsAction:
            copyTableValuesMenu->addAction(tr("Copy selected Rows"), this, SLOT(slotCopyRows()));
            break;
        case SeparatorAction:
            copyTableValuesMenu->addSeparator();
            break;
        case PriceColumn:
            addAction(PriceColumn, tr("Copy Price"));
            break;
        case BaseAmountColumn:
            addAction(BaseAmountColumn, tr("Copy Amount"));
            break;
        case QuoteTotalColumn:
            addAction(QuoteTotalColumn, tr("Copy Total"));
            break;
        case DateOpenOrderColumn:
            addAction(DateOpenOrderColumn, tr("Copy Open Date"));
            break;
        case DateCloseOrderColumn:
            addAction(DateCloseOrderColumn, tr("Copy Close Date"));
            break;
        case IdColumn:
            addAction(IdColumn, tr("Copy Order ID"));
            break;
        }
    }

    //signals-slots connects
    connect(sigMapper, SIGNAL(mapped(int)),
            this, SLOT(slotCopyActionTriggered(int)));
    connect(this, &LuxgateTableView::customContextMenuRequested,
            this, &LuxgateTableView::tableCopyContextMenuRequested);
}

void LuxgateTableView::copy(bool bRow, int iCol)
{
    QModelIndexList selectedRows = selectionModel()->selectedRows(copyColumns[iCol]);
    std::sort(selectedRows.begin(), selectedRows.end(),
              [](const QModelIndex & a, const QModelIndex & b)->bool
              {
                  return a.row() < b.row();
              });
    if (selectedRows.count() == 0)
        return;

    QStringList copyData;

    int role = Qt::DisplayRole;
    if (bRow)
        role = Luxgate::CopyRowRole;
    for (int i = 0; i < selectedRows.count(); i++)
        copyData << selectedRows[i].data(role).toString();

    QApplication::clipboard()->setText(copyData.join("\n"));
}

void LuxgateTableView::slotCopyActionTriggered(int iCol)
{
    copy(false, iCol);
}


void LuxgateTableView::slotCopyRows()
{
    copy(true);
}

void LuxgateTableView::tableCopyContextMenuRequested(QPoint point)
{
    int selectedCount = selectionModel()->selectedRows().count();

    if (selectedCount == 0)
        return;

    copyTableValuesMenu->exec(viewport()->mapToGlobal(point));
}

void LuxgateTableView::setCopyColumns(QMap<int, int> copyColumns_)
{
    copyColumns = copyColumns_;
    foreach(auto action, copyTableValuesMenu->actions())
        action->setVisible(false);
    copyTableValuesMenu->actions().at(RowsAction)->setVisible(true);
    copyTableValuesMenu->actions().at(SeparatorAction)->setVisible(true);
    foreach(int column, copyColumns.keys())
        copyTableValuesMenu->actions().at(column)->setVisible(true);
}


