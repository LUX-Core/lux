#ifndef LUXGATECREATEORDERPANEL_H
#define LUXGATECREATEORDERPANEL_H

#include <QFrame>
#include "luxgategui_global.h"
#include "utilstrencodings.h"
#include "univalue/univalue.h"
#include "utilmoneystr.h"

#include "luxgate/luxgate.h"

namespace Ui {
class LuxgateCreateOrderPanel;
}

class WalletModel;

extern UniValue createorder(const UniValue& params, bool fHelp);
extern CLuxGate luxgate;
enum AmountParsingResult { OK, INVALID, OUT_OF_RANGE };
AmountParsingResult AmountFromQString(const QString& value, CAmount& amount);

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
    void updateWidgetToolTip(const AmountParsingResult result, QWidget* widget);

    CAmount currentPrice;
    CAmount currentQuantity;

private slots:
    void slotUpdateAverageHighLowBidAsk(float average, float highLow);
    void slotValueSliderPriceChanged(int value);
    void slotPriceEditChanged(const QString & text);
    void slotCalcNewTotal();
    void slotBuySellClicked();

    void slotQuantityChanged(const QString& text);
    void slotPriceChanged(const QString& text);
};

#endif // LUXGATECREATEORDERPANEL_H
