#ifndef TOKENVIEW_H
#define TOKENVIEW_H

#include "guiutil.h"

#include <QWidget>
#include <QKeyEvent>

//class PlatformStyle;
class TokenFilterProxy;
class WalletModel;

QT_BEGIN_NAMESPACE
class QComboBox;
class QDateTimeEdit;
class QLineEdit;
class QMenu;
class QTableView;
QT_END_NAMESPACE

class TokenTransactionView : public QWidget
{
    Q_OBJECT
public:
    explicit TokenTransactionView(QWidget *parent = 0);

    void setModel(WalletModel *model);

    // Date ranges for filter
    enum DateEnum
    {
        All,
        Today,
        ThisWeek,
        ThisMonth,
        LastMonth,
        ThisYear,
        Range
    };

    enum ColumnWidths {
        STATUS_COLUMN_WIDTH = 30,
        DATE_COLUMN_WIDTH = 120,
        TYPE_COLUMN_WIDTH = 113,
        NAME_COLUMN_WIDTH = 90,
        AMOUNT_MINIMUM_COLUMN_WIDTH = 230,
        MINIMUM_COLUMN_WIDTH = 23
    };

private:
    WalletModel *model;
    TokenFilterProxy *tokenProxyModel;
    QTableView *tokenView;

    QComboBox *dateWidget;
    QComboBox *typeWidget;
    QComboBox *nameWidget;
    QLineEdit *addressWidget;
    QLineEdit *amountWidget;

    QMenu *contextMenu;

    QFrame *dateRangeWidget;
    QDateTimeEdit *dateFrom;
    QDateTimeEdit *dateTo;

    QWidget *createDateRangeWidget();

    GUIUtil::TableViewLastColumnResizingFixer *columnResizingFixer;

    virtual void resizeEvent(QResizeEvent* event);
    bool eventFilter(QObject *obj, QEvent *event);

private Q_SLOTS:
    void contextualMenu(const QPoint &);
    void dateRangeChanged();
    void showDetails();
    void copyAddress();
    void copyAmount();
    void copyTxID();
    void copyTxPlainText();

Q_SIGNALS:

public Q_SLOTS:
    void chooseDate(int idx);
    void chooseType(int idx);
    void chooseName(int idx);
    void changedPrefix(const QString &prefix);
    void changedAmount(const QString &amount);
    void addToNameWidget(const QModelIndex& parent, int start, int /*end*/);
    void removeFromNameWidget(const QModelIndex& parent, int start, int);
    void refreshNameWidget();
};

#endif // TOKENVIEW_H
