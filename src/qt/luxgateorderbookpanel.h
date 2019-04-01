#ifndef LUXGATEORDERBOOKPANEL_H
#define LUXGATEORDERBOOKPANEL_H

#include <QFrame>

namespace Ui {
class LuxgateOrderBookPanel;
}

class WalletModel;

class LuxgateOrderBookPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateOrderBookPanel(QWidget *parent = nullptr);
    ~LuxgateOrderBookPanel();
    void setModel(WalletModel *model);
private:
    Ui::LuxgateOrderBookPanel *ui;
    WalletModel * wal_model;
private slots:
    void slotReplaceBidsAsks();
};

#endif // LUXGATEORDERBOOKPANEL_H
