#ifndef LUXGATEASKPANEL_H
#define LUXGATEASKPANEL_H

#include <QFrame>

namespace Ui {
class LuxgateBidAskPanel;
}

class WalletModel;

class LuxgateBidAskPanel : public QFrame
{
    Q_OBJECT

public:
    explicit LuxgateBidAskPanel(QWidget *parent = nullptr);
    ~LuxgateBidAskPanel();
    void setData(bool bBids_, WalletModel *model);
    void setOrientation(bool bLeft);
    void setRowsDisplayed (int RowsDisplayed_);
public slots:
    void slotGroup(bool bGroup, double dbStep = 0.f);
private:
    Ui::LuxgateBidAskPanel *ui;
    WalletModel * wal_model;
    bool bBids;
};

#endif // LUXGATEASKPANEL_H
