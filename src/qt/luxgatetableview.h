#ifndef LUXGATETABLEVIEW_H
#define LUXGATETABLEVIEW_H

#include <QTableView>
#include <QMap>

class QMenu;

class LuxgateTableView : public QTableView
{
    Q_OBJECT
public:
    LuxgateTableView(QWidget *parent = nullptr);
    enum possibleColumns { RowsAction, SeparatorAction,
                           PriceColumn,  BaseAmountColumn,
                           QuoteTotalColumn, DateOpenOrderColumn,
                           DateCloseOrderColumn, IdColumn, nPossibleColumns };
    void setCopyColumns(QMap<int, int> copyColumns); //QMap<possibleColumn, realModelColumn>
private:
    QMap<int, int> copyColumns; //QMap<possibleColumn, realModelColumn>
    QMenu *copyTableValuesMenu;
    void copy(bool bRow, int iCol = 0);
private slots:
    void tableCopyContextMenuRequested(QPoint);
    void slotCopyActionTriggered(int iCol);
    void slotCopyRows();
signals:
};

#endif // LUXGATETABLEVIEW_H
