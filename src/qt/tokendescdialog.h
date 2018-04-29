#ifndef TOKENDESCDIALOG_H
#define TOKENDESCDIALOG_H

#include <QDialog>

namespace Ui {
class TokenDescDialog;
}

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

/** Dialog showing token details. */
class TokenDescDialog : public QDialog
{
    Q_OBJECT

public:
    explicit TokenDescDialog(const QModelIndex &idx, QWidget *parent = 0);
    ~TokenDescDialog();

private:
    Ui::TokenDescDialog *ui;
};

#endif // TOKENDESCDIALOG_H
