#ifndef TREEWIDGETFINDRESULTS_H
#define TREEWIDGETFINDRESULTS_H

#include <QTreeWidget>

class TreeWidgetFindResults : public QTreeWidget
{
public:
    TreeWidgetFindResults(QWidget *parent = Q_NULLPTR);
    QModelIndex indexFromItemPublic(const QTreeWidgetItem *item, int column = 0) const;
};

#endif // TREEWIDGETFINDRESULTS_H
