#ifndef CSTYPEWIDGET_H
#define CSTYPEWIDGET_H

#include <QFrame>
#include "sctypesmodel.h"

namespace Ui {
class csTypeWidget;
}

class ScTypeWidget : public QFrame
{
    Q_OBJECT

public:
    explicit ScTypeWidget(const ScTypesItem & item, QWidget *parent = 0);
    ~ScTypeWidget();
private:
    Ui::csTypeWidget *ui;
};

#endif // CSTYPEWIDGET_H
