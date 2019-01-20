#ifndef LUXGATEBIDPANEL_H
#define LUXGATEBIDPANEL_H

#include <QFrame>
#include "ui_luxgatebidpanel.h"

namespace Ui {
class LuxgateBidPanel;
}

class WalletModel;

class LuxgateBidPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateBidPanel(QWidget *parent = nullptr);
    ~LuxgateBidPanel();
    void setModel(WalletModel *model);
private:
    Ui::LuxgateBidPanel *ui;
    WalletModel * wal_model;
};

#endif // LUXGATEBIDPANEL_H
