#ifndef LUXGATEORDERBOOKPANEL_H
#define LUXGATEORDERBOOKPANEL_H

#include <QFrame>
#include "luxgate_options.h"

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
    Luxgate::Decimals decimals;
private slots:
    void slotReplaceBidsAsks();
    void slotBXBT_Index(double dbIndex);
    void slotRowsDisplayChanged (const QString &text);
    void slotGroupsChanged (const QString &text);
    void slotDecimalsChanged(const Luxgate::Decimals & decimals_);
};

#endif // LUXGATEORDERBOOKPANEL_H
