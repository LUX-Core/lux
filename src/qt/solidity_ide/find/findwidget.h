#ifndef FINDWIDGET_H
#define FINDWIDGET_H

#include <QWidget>
#include "find/globalsearch.h"

class QTreeWidgetItem;
namespace Ui {
class FindWidget;
}

class FindWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FindWidget(QWidget *parent = 0);
    ~FindWidget();

private:
    Ui::FindWidget *ui;

public slots:
    void slotReadyFindResults(findResults);
private slots:
    void slotCurrentFindResultChanged(QTreeWidgetItem *current);

signals:
    void sigNeedHiding();
    void sigFindResultChoosed(QString fileName,
                              int blockNumber,
                              int positionResult);
};

#endif // FINDWIDGET_H
