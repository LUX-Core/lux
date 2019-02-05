#ifndef LUXGATEHISTORYPANEL_H
#define LUXGATEHISTORYPANEL_H

#include <QFrame>
#include "luxgatehistorymodel.h"

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
public slots:
    void slotUpdateData(const LuxgateHistoryData & data);
private:
    WalletModel * wal_model;
    Ui::LuxgateHistoryPanel *ui;
};

#endif // LUXGATEHISTORYPANEL_H
