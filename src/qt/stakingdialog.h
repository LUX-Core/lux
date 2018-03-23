#ifndef STAKINGDIALOG_H
#define STAKINGDIALOG_H

#include <QWidget>
#include <QComboBox>
#include "stakingfilterproxy.h"

#define COLOR_MINT_YOUNG QColor(0, 67, 99)
#define COLOR_MINT_MATURE QColor(29, 99, 0)
#define COLOR_MINT_OLD QColor(99, 0, 23)

class WalletModel;


QT_BEGIN_NAMESPACE
class QTableView;
class QMenu;
QT_END_NAMESPACE

class StakingDialog : public QWidget
{
    Q_OBJECT
public:
    explicit StakingDialog(QWidget *parent = 0);
    void setModel(WalletModel *model);

    enum StakingEnum
    {
        Staking10min,
        Staking1day,
        Staking7days,
        Staking30days,
        Staking60days,
        Staking90days
    };

private:
    WalletModel *model;
    QTableView *stakingDialog;
    QComboBox *stakingCombo;
    QMenu *contextMenu;

signals:

public slots:
    void exportClicked();
    void chooseStakingInterval(int idx);
    void copyTxID();
    void copyAddress();
    void showHideAddress();
    void showHideTxID();
    void contextualMenu(const QPoint &point);
};

#endif // STAKINGDIALOG_H
