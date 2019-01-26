#ifndef __LUXGATE_DIALOG_H__
#define __LUXGATE_DIALOG_H__

#include <QWidget>
#include <QObject>
#include <QTableWidget>
#include <stdint.h>
#include "clientmodel.h"
#include "walletmodel.h"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "qcustomplot.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkReply>
#include <QMainWindow>


namespace Ui {
    class LuxgateDialog;
}


class WalletModel;
class QDockWidget;

class LuxgateOpenOrdersPanel;
class LuxgateBidPanel;
class LuxgateBidAskPanel;
class LuxgateConfigPanel;
class LuxgateOrderBookPanel;


class LuxgateDialog : public QMainWindow
{
    Q_OBJECT

public:
    explicit LuxgateDialog(QWidget *parent = 0);
    ~LuxgateDialog();

    void setModel(WalletModel *model);

private:
    Ui::LuxgateDialog *ui;
    WalletModel *model;

    LuxgateBidPanel * bidsPanel;
    QDockWidget* dockBidPanel;

    LuxgateBidAskPanel * asksPanel;
    QDockWidget* dockAskPanel;

    LuxgateConfigPanel * confPanel;
    QDockWidget* dockConfigPanel;

    LuxgateOpenOrdersPanel * openOrders;
    QDockWidget* dockOpenOrdersPanel;

    LuxgateOrderBookPanel * orderBookPanel;
    QDockWidget* dockOrderBookPanel;

    void createWidgetsAndDocks();
    QDockWidget* createDock(QWidget* widget, const QString& title);
};

#endif //__LUXGATE_DIALOG_H__
