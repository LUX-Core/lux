#ifndef ADDEDITLUXNODE_H
#define ADDEDITLUXNODE_H

#include <QDialog>

namespace Ui {
class AddEditLuxNode;
}


class AddEditLuxNode : public QDialog
{
    Q_OBJECT

public:
    explicit AddEditLuxNode(QWidget *parent = 0);
    ~AddEditLuxNode();

protected:

private slots:
    void on_okButton_clicked();
    void on_cancelButton_clicked();

signals:

private:
    Ui::AddEditLuxNode *ui;
};

#endif // ADDEDITLUXNODE_H
