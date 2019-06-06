// Copyright (c) 2019 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUXGATEHISTORYPANEL_H
#define LUXGATEHISTORYPANEL_H

#include <QFrame>


namespace Ui {
class LuxgateHistoryPanel;
}

class WalletModel;

class LuxgateHistoryPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateHistoryPanel(QWidget *parent = nullptr);
    ~LuxgateHistoryPanel();
    void setModel(WalletModel *model);
private:
    WalletModel *wal_model;
    Ui::LuxgateHistoryPanel *ui;
};

#endif // LUXGATEHISTORYPANEL_H

