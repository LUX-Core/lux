#include "tokentransactionview.h"

#include "walletmodel.h"
//#include "platformstyle.h"
#include "tokentransactiontablemodel.h"
#include "tokentransactionrecord.h"
#include "tokenfilterproxy.h"
#include "guiutil.h"
#include "optionsmodel.h"
#include "tokenitemmodel.h"
#include "tokendescdialog.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPoint>
#include <QScrollBar>
#include <QTableView>
#include <QVBoxLayout>
#include <QRegularExpressionValidator>
#include <QSettings>

#define paternTokenAmount "^[0-9]{1,59}\\.{1,1}[0-9]{0,18}"

TokenTransactionView::TokenTransactionView(QWidget *parent) :
    QWidget(parent),
    model(0),
    tokenProxyModel(0),
    tokenView(0),
    columnResizingFixer(0)
{
    // Build filter row
    setContentsMargins(0,0,0,0);

    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0,0,0,0);
    hlayout->setSpacing(0);
    hlayout->addSpacing(STATUS_COLUMN_WIDTH);

    dateWidget = new QComboBox(this);

    dateWidget->setFixedWidth(DATE_COLUMN_WIDTH);

    dateWidget->addItem(tr("All"), All);
    dateWidget->addItem(tr("Today"), Today);
    dateWidget->addItem(tr("This week"), ThisWeek);
    dateWidget->addItem(tr("This month"), ThisMonth);
    dateWidget->addItem(tr("Last month"), LastMonth);
    dateWidget->addItem(tr("This year"), ThisYear);
    dateWidget->addItem(tr("Range..."), Range);
    hlayout->addWidget(dateWidget);

    typeWidget = new QComboBox(this);
    typeWidget->setFixedWidth(TYPE_COLUMN_WIDTH);


    typeWidget->addItem(tr("All"), TokenFilterProxy::ALL_TYPES);
    typeWidget->addItem(tr("Received"), TokenFilterProxy::TYPE(TokenTransactionRecord::RecvWithAddress));
    typeWidget->addItem(tr("Sent"), TokenFilterProxy::TYPE(TokenTransactionRecord::SendToAddress));
    typeWidget->addItem(tr("To yourself"), TokenFilterProxy::TYPE(TokenTransactionRecord::SendToSelf));
    hlayout->addWidget(typeWidget);

    addressWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    addressWidget->setPlaceholderText(tr("Enter address to search"));
#endif
    hlayout->addWidget(addressWidget);

    nameWidget = new QComboBox(this);

    nameWidget->setFixedWidth(NAME_COLUMN_WIDTH);

    nameWidget->addItem(tr("All"), "");

    hlayout->addWidget(nameWidget);

    amountWidget = new QLineEdit(this);
#if QT_VERSION >= 0x040700
    amountWidget->setPlaceholderText(tr("Min amount"));
#endif

    amountWidget->setFixedWidth(AMOUNT_MINIMUM_COLUMN_WIDTH);
    QRegularExpression regEx;
    regEx.setPattern(paternTokenAmount);
    QRegularExpressionValidator *validator = new QRegularExpressionValidator(amountWidget);
    validator->setRegularExpression(regEx);
    amountWidget->setValidator(validator);
    hlayout->addWidget(amountWidget);

    QVBoxLayout *vlayout = new QVBoxLayout(this);
    vlayout->setContentsMargins(0,0,0,0);
    vlayout->setSpacing(0);

    QTableView *view = new QTableView(this);
    vlayout->addLayout(hlayout);
    vlayout->addWidget(createDateRangeWidget());
    vlayout->addWidget(view);
    vlayout->setSpacing(0);
    int width = view->verticalScrollBar()->sizeHint().width();
    // Cover scroll bar width with spacing
    hlayout->addSpacing(width);
    // Always show scroll bar
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    view->setTabKeyNavigation(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);

	QSettings settings;

	if (settings.value("theme").toString() == "dark grey") {
		QString styleSheet = ".QHeaderView { background-color: #262626; outline: 10; "
								"border-top: 1px solid #fff5f5; border-left: 1px solid #fff5f5; }"
								".QHeaderView::section { qproperty-alignment: center; background-color: #262626; "
								"color: #fff; min-height: 25px; min-width: 20px; font-weight: bold; font-size: 12px; "
								"border: 0; border-right: 1px solid #fff5f5; border-bottom: 1px solid #fff5f5; "
								"padding-left: 5px; padding-right: 5px; padding-top: 2px; padding-bottom: 2px; }"
								".QHeaderView::section:last { border-right: 1px solid #fff5f5; }"
								".QTableCornerButton::section { background-color:#262626; border: 0px solid #fff5f5 !important;}"
								".QTableView { background-color: #262626; alternate-background-color:#424242; "
								"gridline-color: #fff5f5; border: 0px solid #fff5f5; border-left: 1px solid #fff5f5 !important; "
								"border-bottom: 1px solid #fff5f5 !important; color: #fff; min-height:2em; margin-top: 5px  !important; }";					
		view->setStyleSheet(styleSheet);
	} else if (settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QHeaderView { background-color: #031d54; outline: 10; "
								"border-top: 1px solid #fff5f5; border-left: 1px solid #fff5f5; }"
								".QHeaderView::section { qproperty-alignment: center; background-color: #031d54; "
								"color: #fff; min-height: 25px; min-width: 20px; font-weight: bold; font-size: 12px; "
								"border: 0; border-right: 1px solid #fff5f5; border-bottom: 1px solid #fff5f5; "
								"padding-left: 5px; padding-right: 5px; padding-top: 2px; padding-bottom: 2px; }"
								".QHeaderView::section:last { border-right: 1px solid #fff5f5; }"
								".QTableCornerButton::section { background-color:#031d54; border: 0px solid #fff5f5 !important;}"
								".QTableView { background-color: #031d54; alternate-background-color:#0D2A64; "
								"gridline-color: #fff5f5; border: 0px solid #fff5f5; border-left: 1px solid #fff5f5 !important; "
								"border-bottom: 1px solid #fff5f5 !important; color: #fff; min-height:2em; margin-top: 5px  !important; }";	
		view->setStyleSheet(styleSheet);
	} else { 
		//code here
	}
    view->installEventFilter(this);
    tokenView = view;

    // Actions
    QAction *copyAddressAction = new QAction(tr("Copy address"), this);
    QAction *copyAmountAction = new QAction(tr("Copy amount"), this);
    QAction *copyTxIDAction = new QAction(tr("Copy transaction ID"), this);
    QAction *copyTxPlainText = new QAction(tr("Copy full transaction details"), this);
    QAction *showDetailsAction = new QAction(tr("Show transaction details"), this);

    contextMenu = new QMenu(tokenView);
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyAmountAction);
    contextMenu->addAction(copyTxIDAction);
    contextMenu->addAction(copyTxPlainText);
    contextMenu->addAction(showDetailsAction);

    connect(dateWidget, SIGNAL(activated(int)), this, SLOT(chooseDate(int)));
    connect(typeWidget, SIGNAL(activated(int)), this, SLOT(chooseType(int)));
    connect(addressWidget, SIGNAL(textChanged(QString)), this, SLOT(changedPrefix(QString)));
    connect(amountWidget, SIGNAL(textChanged(QString)), this, SLOT(changedAmount(QString)));
    connect(nameWidget, SIGNAL(activated(int)), this, SLOT(chooseName(int)));

    connect(view, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(copyAddress()));
    connect(copyAmountAction, SIGNAL(triggered()), this, SLOT(copyAmount()));
    connect(copyTxIDAction, SIGNAL(triggered()), this, SLOT(copyTxID()));
    connect(copyTxPlainText, SIGNAL(triggered()), this, SLOT(copyTxPlainText()));
    connect(showDetailsAction, SIGNAL(triggered()), this, SLOT(showDetails()));
    connect(tokenView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(showDetails()));
}

void TokenTransactionView::setModel(WalletModel *_model)
{
    this->model = _model;
    if(_model)
    {
        refreshNameWidget();
        connect(model->getTokenItemModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),this, SLOT(addToNameWidget(QModelIndex,int,int)));
        connect(model->getTokenItemModel(), SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),this, SLOT(removeFromNameWidget(QModelIndex,int,int)));

        tokenProxyModel = new TokenFilterProxy(this);
        tokenProxyModel->setSourceModel(_model->getTokenTransactionTableModel());
        tokenProxyModel->setDynamicSortFilter(true);
        tokenProxyModel->setSortCaseSensitivity(Qt::CaseInsensitive);
        tokenProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

        tokenProxyModel->setSortRole(Qt::EditRole);

        tokenView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        tokenView->setModel(tokenProxyModel);
        tokenView->setAlternatingRowColors(true);
        tokenView->setSelectionBehavior(QAbstractItemView::SelectRows);
        tokenView->setSelectionMode(QAbstractItemView::ExtendedSelection);
        tokenView->setSortingEnabled(true);
        tokenView->sortByColumn(TokenTransactionTableModel::Date, Qt::DescendingOrder);
        tokenView->verticalHeader()->hide();

        tokenView->setColumnWidth(TokenTransactionTableModel::Status, STATUS_COLUMN_WIDTH);
        tokenView->setColumnWidth(TokenTransactionTableModel::Date, DATE_COLUMN_WIDTH);
        tokenView->setColumnWidth(TokenTransactionTableModel::Type, TYPE_COLUMN_WIDTH);
        tokenView->setColumnWidth(TokenTransactionTableModel::Name, NAME_COLUMN_WIDTH);
        tokenView->setColumnWidth(TokenTransactionTableModel::Amount, AMOUNT_MINIMUM_COLUMN_WIDTH);

        columnResizingFixer = new GUIUtil::TableViewLastColumnResizingFixer(tokenView, AMOUNT_MINIMUM_COLUMN_WIDTH, MINIMUM_COLUMN_WIDTH, this, 2);
    }
}

QWidget *TokenTransactionView::createDateRangeWidget()
{
    dateRangeWidget = new QFrame();
    dateRangeWidget->setFrameStyle(QFrame::Panel | QFrame::Raised);
    dateRangeWidget->setContentsMargins(1,1,1,1);
    QHBoxLayout *layout = new QHBoxLayout(dateRangeWidget);
    layout->setContentsMargins(0,0,0,0);
    layout->addSpacing(23);
    layout->addWidget(new QLabel(tr("Range:")));

    dateFrom = new QDateTimeEdit(this);
    dateFrom->setDisplayFormat("dd/MM/yy");
    dateFrom->setCalendarPopup(true);
    dateFrom->setMinimumWidth(100);
    dateFrom->setDate(QDate::currentDate().addDays(-7));
    layout->addWidget(dateFrom);
    layout->addWidget(new QLabel(tr("to")));

    dateTo = new QDateTimeEdit(this);
    dateTo->setDisplayFormat("dd/MM/yy");
    dateTo->setCalendarPopup(true);
    dateTo->setMinimumWidth(100);
    dateTo->setDate(QDate::currentDate());
    layout->addWidget(dateTo);
    layout->addStretch();

    // Hide by default
    dateRangeWidget->setVisible(false);

    // Notify on change
    connect(dateFrom, SIGNAL(dateChanged(QDate)), this, SLOT(dateRangeChanged()));
    connect(dateTo, SIGNAL(dateChanged(QDate)), this, SLOT(dateRangeChanged()));

    return dateRangeWidget;
}

void TokenTransactionView::refreshNameWidget()
{
    if(model)
    {
        TokenItemModel *tim = model->getTokenItemModel();
        for(int i = 0; i < tim->rowCount(); i++)
        {
            QString name = tim->data(tim->index(i, 0), TokenItemModel::SymbolRole).toString();
            if(nameWidget->findText(name) == -1)
                nameWidget->addItem(name, name);
        }
    }
}

void TokenTransactionView::addToNameWidget(const QModelIndex &parent, int start, int /*end*/)
{
    if(model)
    {
        TokenItemModel *tim = model->getTokenItemModel();
        QString name = tim->index(start, TokenItemModel::Symbol, parent).data().toString();
        if(nameWidget->findText(name) == -1)
            nameWidget->addItem(name, name);
    }
}

void TokenTransactionView::removeFromNameWidget(const QModelIndex &parent, int start, int /*end*/)
{
    if(model)
    {
        TokenItemModel *tim = model->getTokenItemModel();
        QString name = tim->index(start, TokenItemModel::Symbol, parent).data().toString();
        int nameCount = 0;

        for(int i = 0; i < tim->rowCount(); i++)
        {
            QString checkName = tim->index(i, TokenItemModel::Symbol, parent).data().toString();
            if(name == checkName)
            {
                nameCount++;
            }
        }

        int nameIndex = nameWidget->findText(name);
        if(nameCount == 1 && nameIndex != -1)
            nameWidget->removeItem(nameIndex);
    }
}

void TokenTransactionView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    columnResizingFixer->stretchColumnWidth(TokenTransactionTableModel::ToAddress);
}

bool TokenTransactionView::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        if (ke->key() == Qt::Key_C && ke->modifiers().testFlag(Qt::ControlModifier))
        {
            GUIUtil::copyEntryData(tokenView, 0, TokenTransactionTableModel::TxPlainTextRole);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void TokenTransactionView::contextualMenu(const QPoint &point)
{
    QModelIndex index = tokenView->indexAt(point);
    QModelIndexList selection = tokenView->selectionModel()->selectedRows(0);
    if (selection.empty())
        return;

    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void TokenTransactionView::dateRangeChanged()
{
    if(!tokenProxyModel)
        return;
    tokenProxyModel->setDateRange(
                QDateTime(dateFrom->date()),
                QDateTime(dateTo->date()).addDays(1));
}

void TokenTransactionView::showDetails()
{
    if(!tokenView->selectionModel())
        return;
    QModelIndexList selection = tokenView->selectionModel()->selectedRows();
    if(!selection.isEmpty())
    {
        TokenDescDialog *dlg = new TokenDescDialog(selection.at(0));
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    }
}

void TokenTransactionView::copyAddress()
{
    GUIUtil::copyEntryData(tokenView, 0, TokenTransactionTableModel::AddressRole);
}

void TokenTransactionView::copyAmount()
{
    GUIUtil::copyEntryData(tokenView, 0, TokenTransactionTableModel::AmountRole);
}

void TokenTransactionView::copyTxID()
{
    GUIUtil::copyEntryData(tokenView, 0, TokenTransactionTableModel::TxHashRole);
}

void TokenTransactionView::copyTxPlainText()
{
    GUIUtil::copyEntryData(tokenView, 0, TokenTransactionTableModel::TxPlainTextRole);
}

void TokenTransactionView::chooseDate(int idx)
{
    if(!tokenProxyModel)
        return;
    QDate current = QDate::currentDate();
    dateRangeWidget->setVisible(false);
    switch(dateWidget->itemData(idx).toInt())
    {
    case All:
        tokenProxyModel->setDateRange(
                    TokenFilterProxy::MIN_DATE,
                    TokenFilterProxy::MAX_DATE);
        break;
    case Today:
        tokenProxyModel->setDateRange(
                    QDateTime(current),
                    TokenFilterProxy::MAX_DATE);
        break;
    case ThisWeek: {
        // Find last Monday
        QDate startOfWeek = current.addDays(-(current.dayOfWeek()-1));
        tokenProxyModel->setDateRange(
                    QDateTime(startOfWeek),
                    TokenFilterProxy::MAX_DATE);

    } break;
    case ThisMonth:
        tokenProxyModel->setDateRange(
                    QDateTime(QDate(current.year(), current.month(), 1)),
                    TokenFilterProxy::MAX_DATE);
        break;
    case LastMonth:
        tokenProxyModel->setDateRange(
                    QDateTime(QDate(current.year(), current.month(), 1).addMonths(-1)),
                    QDateTime(QDate(current.year(), current.month(), 1)));
        break;
    case ThisYear:
        tokenProxyModel->setDateRange(
                    QDateTime(QDate(current.year(), 1, 1)),
                    TokenFilterProxy::MAX_DATE);
        break;
    case Range:
        dateRangeWidget->setVisible(true);
        dateRangeChanged();
        break;
    }
}

void TokenTransactionView::chooseType(int idx)
{
    if(!tokenProxyModel)
        return;
    tokenProxyModel->setTypeFilter(
                typeWidget->itemData(idx).toInt());
}

void TokenTransactionView::chooseName(int idx)
{
    if(!tokenProxyModel)
        return;
    tokenProxyModel->setName(
                nameWidget->itemData(idx).toString());
}

void TokenTransactionView::changedPrefix(const QString &prefix)
{
    if(!tokenProxyModel)
        return;
    tokenProxyModel->setAddressPrefix(prefix);
}

void TokenTransactionView::changedAmount(const QString &amount)
{
    if(!tokenProxyModel)
        return;
    int256_t amount_parsed = 0;
    if(BitcoinUnits::parseToken(18, amount, &amount_parsed))
    {
        tokenProxyModel->setMinAmount(amount_parsed);
    }
    else
    {
        tokenProxyModel->setMinAmount(0);
    }
}
