#ifndef SCTOKENWGT_H
#define SCTOKENWGT_H

#include <QWidget>

namespace Ui {
class ScTokenWgt;
}

class ScTokenWgt : public QWidget
{
    Q_OBJECT

public:
    explicit ScTokenWgt(QWidget *parent = 0);
    ~ScTokenWgt();

private:
    Ui::ScTokenWgt *ui;
};

#endif // SCTOKENWGT_H
