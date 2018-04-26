#include "stakingdialog.h"
#include "stakingfilterproxy.h"
#include "transactionrecord.h"
#include "stakingtablemodel.h"
#include "walletmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "csvmodelwriter.h"


#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QTableView>
#include <QScrollBar>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>
#include <QMenu>


StakingDialog::StakingDialog(QWidget *parent) :
    QWidget(parent), model(0), stakingDialog(0)
{
    QHBoxLayout *hlayout = new QHBoxLayout();
    hlayout->setContentsMargins(0,0,0,0);

    QString legendBoxStyle = "background-color: rgb(%1,%2,%3); border: 1px solid black;";

    QLabel *youngColor = new QLabel(" ");
    youngColor->setMaximumHeight(15);
    youngColor->setMaximumWidth(10);
    youngColor->setStyleSheet(legendBoxStyle.arg(COLOR_MINT_YOUNG.red()).arg(COLOR_MINT_YOUNG.green()).arg(COLOR_MINT_YOUNG.blue()));
    QLabel *youngLegend = new QLabel(tr("transaction is too young"));
    youngLegend->setContentsMargins(5,0,15,0);

    QLabel *matureColor = new QLabel(" ");
    matureColor->setMaximumHeight(15);
    matureColor->setMaximumWidth(10);
    matureColor->setStyleSheet(legendBoxStyle.arg(COLOR_MINT_MATURE.red()).arg(COLOR_MINT_MATURE.green()).arg(COLOR_MINT_MATURE.blue()));
    QLabel *matureLegend = new QLabel(tr("transaction is mature"));
    matureLegend->setContentsMargins(5,0,15,0);

    QLabel *oldColor = new QLabel(" ");
    oldColor->setMaximumHeight(15);
    oldColor->setMaximumWidth(10);
    oldColor->setStyleSheet(legendBoxStyle.arg(COLOR_MINT_OLD.red()).arg(COLOR_MINT_OLD.green()).arg(COLOR_MINT_OLD.blue()));
    QLabel *oldLegend = new QLabel(tr("transaction has reached maximum probability"));
    oldLegend->setContentsMargins(5,0,15,0);

    QHBoxLayout *legendLayout = new QHBoxLayout();
    legendLayout->setContentsMargins(10,10,0,0);
    legendLayout->addWidget(youngColor);
    legendLayout->addWidget(youngLegend);
    legendLayout->addWidget(matureColor);
    legendLayout->addWidget(matureLegend);
    legendLayout->addWidget(oldColor);
    legendLayout->addWidget(oldLegend);
    legendLayout->insertStretch(-1);
	
	QLabel *label1 = new QLabel(this);
	label1->setText("Staking Estimations");
	label1->setAlignment(Qt::AlignLeft);
	QFont font( "Arial", 16, QFont::Bold);
	label1->setFont(font);
	label1->setFixedWidth(250);
	
	QLabel *stakingLabel = new QLabel(tr("Check the arrow icon below for more staking information. *BETA*"));

    QLabel *stakingLabel2 = new QLabel(tr(" [Display staking probability within]: "));
    stakingCombo = new QComboBox();
    stakingCombo->addItem(tr("10 min"), Staking10min);
    stakingCombo->addItem(tr("24 hours"), Staking1day);
    stakingCombo->addItem(tr("7 days"), Staking7days);
    stakingCombo->addItem(tr("30 days"), Staking30days);
    stakingCombo->addItem(tr("60 days"), Staking60days);
    stakingCombo->addItem(tr("90 days"), Staking90days);
    stakingCombo->setFixedWidth(120);

    hlayout->insertStretch(0);
    hlayout->addWidget(stakingLabel);
	hlayout->addWidget(stakingLabel2);
    hlayout->addWidget(stakingCombo);

    QVBoxLayout *vlayout = new QVBoxLayout(this);
    vlayout->setContentsMargins(0,0,0,0);
    vlayout->setSpacing(0);

    QTableView *view = new QTableView(this);
    vlayout->addLayout(hlayout);
    vlayout->addWidget(view);
    vlayout->addLayout(legendLayout);

    vlayout->setSpacing(0);
    int width = view->verticalScrollBar()->sizeHint().width();
    // Cover scroll bar width with spacing
#ifdef Q_WS_MAC
    hlayout->addSpacing(width+2);
#else
    hlayout->addSpacing(width);
#endif
    // Always show scroll bar
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    view->setTabKeyNavigation(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);

    stakingDialog = view;

    connect(stakingCombo, SIGNAL(activated(int)), this, SLOT(chooseStakingInterval(int)));

    // Actions
    QAction *copyTxIDAction = new QAction(tr("Copy transaction ID of input"), this);
    QAction *copyAddressAction = new QAction(tr("Copy address of input"), this);
    QAction *showHideAddressAction = new QAction(tr("Show/hide 'Address' column"), this);
    QAction *showHideTxIDAction = new QAction(tr("Show/hide 'Transaction' column"), this);

    contextMenu = new QMenu();
    contextMenu->addAction(copyAddressAction);
    contextMenu->addAction(copyTxIDAction);
    contextMenu->addAction(showHideAddressAction);
    contextMenu->addAction(showHideTxIDAction);

    connect(copyAddressAction, SIGNAL(triggered()), this, SLOT(copyAddress()));
    connect(copyTxIDAction, SIGNAL(triggered()), this, SLOT(copyTxID()));
    connect(showHideAddressAction, SIGNAL(triggered()), this, SLOT(showHideAddress()));
    connect(showHideTxIDAction, SIGNAL(triggered()), this, SLOT(showHideTxID()));

    connect(view, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));
}


void StakingDialog::setModel(WalletModel *model)
{
    this->model = model;
    if(model)
    {
        stakingDialog->setAlternatingRowColors(true);
        stakingDialog->setSelectionBehavior(QAbstractItemView::SelectRows);
        stakingDialog->setSelectionMode(QAbstractItemView::ExtendedSelection);
        stakingDialog->setSortingEnabled(true);
        stakingDialog->sortByColumn(StakingTableModel::CoinDay, Qt::DescendingOrder);
        stakingDialog->verticalHeader()->hide();

        stakingDialog->horizontalHeader()->resizeSection(
                StakingTableModel::Age, 60);
        stakingDialog->horizontalHeader()->resizeSection(
                StakingTableModel::Balance, 160);
        stakingDialog->horizontalHeader()->resizeSection(
                StakingTableModel::CoinDay, 60);
        stakingDialog->horizontalHeader()->resizeSection(
                StakingTableModel::MintProbability, 105);
        stakingDialog->horizontalHeader()->resizeSection(
            StakingTableModel::Address, 335);
        stakingDialog->horizontalHeader()->resizeSection(
            StakingTableModel::TxHash, 545);
    }
}


void StakingDialog::chooseStakingInterval(int idx)
{
    /*
    int interval = 10;
    switch(stakingCombo->itemData(idx).toInt())
    {
        case Staking10min:
            interval = 10;
            break;
        case Staking1day:
            interval = 60*24;
            break;
        case Staking7days:
            interval = 60*24*7;
            break;
        case Staking30days:
            interval = 60*24*30;
            break;
        case Staking60days:
            interval = 60*24*60;
            break;
        case Staking90days:
            interval = 60*24*90;
            break;       
    }
    */
}

void StakingDialog::exportClicked()
{
	//T.B.D
}

void StakingDialog::copyTxID()
{
    GUIUtil::copyEntryData(stakingDialog, StakingTableModel::TxHash, 0);
}

void StakingDialog::copyAddress()
{
    GUIUtil::copyEntryData(stakingDialog, StakingTableModel::Address, 0 );
}

void StakingDialog::showHideAddress()
{
    stakingDialog->horizontalHeader()->setSectionHidden(StakingTableModel::Address, 
        !(stakingDialog->horizontalHeader()->isSectionHidden(StakingTableModel::Address)));
}

void StakingDialog::showHideTxID()
{
    stakingDialog->horizontalHeader()->setSectionHidden(StakingTableModel::TxHash, 
        !(stakingDialog->horizontalHeader()->isSectionHidden(StakingTableModel::TxHash)));
}

void StakingDialog::contextualMenu(const QPoint &point)
{
    QModelIndex index = stakingDialog->indexAt(point);
    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}
