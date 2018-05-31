// Copyright (c) 2015-2018 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUX_QT_EULA_H
#define LUX_QT_EULA_H

#include <QDialog>

namespace Ui {
class Eula;
}

class Eula : public QDialog
{
    Q_OBJECT

public:
    explicit Eula(QWidget *parent = 0);
    ~Eula();
    
    static void showDialog();
    
private slots:
    void on_cancel_clicked();
    void on_next_clicked();

private:
    Ui::Eula *ui;
    void closeEvent(QCloseEvent *event);
};

#endif // LUX_QT_EULA_H
