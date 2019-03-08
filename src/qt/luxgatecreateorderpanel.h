#ifndef LUXGATECREATEORDERPANEL_H
#define LUXGATECREATEORDERPANEL_H

#include <QFrame>
#include "luxgategui_global.h"

namespace Ui {
class LuxgateCreateOrderPanel;
}

class WalletModel;

class LuxgateCreateOrderPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateCreateOrderPanel(QWidget *parent = nullptr);
    ~LuxgateCreateOrderPanel();
    void setBuySell(bool bBuy);
    void setModel(WalletModel *model);
private:
    Ui::LuxgateCreateOrderPanel *ui;
    bool bBuy;
    float average {0};
    float highLow {0};
    Luxgate::Decimals decimals;
    float minSliderPriceReal();
    float maxSliderPriceReal();
    void priceChangeUpdateGUI();
private slots:
    void slotUpdateAverageHighLowBidAsk(float average, float highLow);
    void slotValueSliderPriceChanged(int value);
    void slotPriceEditChanged(const QString & text);
    void slotCalcNewTotal();
};

#endif // LUXGATECREATEORDERPANEL_H
