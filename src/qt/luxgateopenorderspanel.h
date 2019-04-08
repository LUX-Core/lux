#ifndef LUXGATEOPENORDERSPANEL_H
#define LUXGATEOPENORDERSPANEL_H

#include <QFrame>

namespace Ui {
class LuxgateOpenOrdersPanel;
}

class WalletModel;

class LuxgateOpenOrdersPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateOpenOrdersPanel(QWidget *parent = nullptr);
    ~LuxgateOpenOrdersPanel();
    void setModel(WalletModel *model);
private:
    Ui::LuxgateOpenOrdersPanel *ui;
    WalletModel * wal_model;
private slots:
    void slotCancelClicked();
    void updateButtonsCancel();
};

#endif // LUXGATEOPENORDERSPANEL_H
