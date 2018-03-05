#ifndef CREATECONTRACT_H
#define CREATECONTRACT_H

#include <QWidget>

namespace Ui {
class CreateContract;
}

class CreateContract : public QWidget
{
    Q_OBJECT

public:
    explicit CreateContract(QWidget *parent = 0);
    ~CreateContract();

private slots:
    void on_pushButtonCreateContract_clicked();

private:
    Ui::CreateContract *ui;
};

#endif // CREATECONTRACT_H
