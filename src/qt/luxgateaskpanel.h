#ifndef LUXGATEASKPANEL_H
#define LUXGATEASKPANEL_H

#include <QFrame>
#include "ui_luxgatebidpanel.h"

namespace Ui {
class LuxgateAskPanel;
}

class WalletModel;

class LuxgateAskPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateAskPanel(QWidget *parent = nullptr);
    ~LuxgateAskPanel();
    void setModel(WalletModel *model);
private:
    Ui::LuxgateAskPanel *ui;
    WalletModel * wal_model;
};

#endif // LUXGATEASKPANEL_H
