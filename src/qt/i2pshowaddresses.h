// Copyright (c) 2013-2017 The Anoncoin Core developers
// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef SHOWI2PADDRESSES_H
#define SHOWI2PADDRESSES_H

#include <QDialog>

namespace Ui {
class ShowI2PAddresses;
}

class ShowI2PAddresses : public QDialog
{
    Q_OBJECT

public:
    explicit ShowI2PAddresses(QWidget *parent = 0);
    void UpdateParameters( void );
    ~ShowI2PAddresses();

private:
    Ui::ShowI2PAddresses *ui;

private Q_SLOTS:
    void setEnabled( void );
    void setStatic( void );
};

#endif // SHOWI2PADDRESSES_H
