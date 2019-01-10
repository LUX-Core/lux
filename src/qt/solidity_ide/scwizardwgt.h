#ifndef SCWIZARDWGT_H
#define SCWIZARDWGT_H

#include <QWidget>

namespace Ui {
class scWizardWgt;
}

class ScWizardWgt : public QWidget
{
    Q_OBJECT

public:
    explicit ScWizardWgt(QWidget *parent = 0);
    ~ScWizardWgt();

private:
    Ui::scWizardWgt *ui;

};

#endif // SCWIZARDWGT_H
