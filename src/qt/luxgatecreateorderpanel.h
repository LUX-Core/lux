#ifndef LUXGATECREATEORDERPANEL_H
#define LUXGATECREATEORDERPANEL_H

#include <QFrame>

namespace Ui {
class LuxgateCreateOrderPanel;
}

class LuxgateCreateOrderPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateCreateOrderPanel(QWidget *parent = nullptr);
    ~LuxgateCreateOrderPanel();
    void setBidAsk(bool bBid);
private:
    Ui::LuxgateCreateOrderPanel *ui;
    bool bBid;
};

#endif // LUXGATECREATEORDERPANEL_H
