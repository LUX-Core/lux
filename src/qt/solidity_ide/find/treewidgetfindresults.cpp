#include "treewidgetfindresults.h"

TreeWidgetFindResults::TreeWidgetFindResults(QWidget *parent):
    QTreeWidget(parent)
{

}

QModelIndex TreeWidgetFindResults::indexFromItemPublic(const QTreeWidgetItem *item, int column) const
{
    return indexFromItem(item, column);
}
