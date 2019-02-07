#ifndef LUXGATEHISTORYPANEL_H
#define LUXGATEHISTORYPANEL_H

#include <QFrame>

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
private:
    WalletModel * wal_model;
    Ui::LuxgateHistoryPanel *ui;
private slots:
    void slotRowsDisplayChanged (const QString &text);
};

#endif // LUXGATEHISTORYPANEL_H
