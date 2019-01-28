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

class LuxgateOpenOrdersPanel;;
class LuxgateConfigPanel;
class LuxgateOrderBookPanel;
class TradingPlot;
class LuxgateHistoryPanel;
class LuxgateCreateOrderPanel;


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

    LuxgateConfigPanel * confPanel;
    QDockWidget* dockConfigPanel;

    LuxgateOpenOrdersPanel * openOrders;
    QDockWidget* dockOpenOrdersPanel;

    LuxgateOrderBookPanel * orderBookPanel;
    QDockWidget* dockOrderBookPanel;

    LuxgateHistoryPanel * historyPanel;
    QDockWidget* dockHistoryPanel;

    LuxgateCreateOrderPanel * createBuyOrderPanel;
    QDockWidget* dockBuyOrderPanel;

    LuxgateCreateOrderPanel * createSellOrderPanel;
    QDockWidget* dockSellOrderPanel;

    TradingPlot * chartsPanel;
    QDockWidget* dockChart;

    void createWidgetsAndDocks();
    QDockWidget* createDock(QWidget* widget, const QString& title);
};

#endif //__LUXGATE_DIALOG_H__
