// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "init.h"
#include "darksend.h"
#include "darksendconfig.h"
#include "optionsmodel.h"
#include "transactionfilterproxy.h"
#include "transactiontablemodel.h"
#include "utilitydialog.h"
#include "walletmodel.h"
#include "tokenitemmodel.h"
#include "platformstyle.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QSettings>
#include <QTimer>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

#include <QMessageBox>
#include <QPixmap>

#define DECORATION_SIZE 38
#define ICON_OFFSET 16
#define NUM_ITEMS 5
#define TOKEN_NUM_ITEMS 3
#define TOKEN_SIZE 35
#define MARGIN 4
#define NAME_WIDTH 290 /* to align with txs history amounts */

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(const PlatformStyle *platformStyle):
            QAbstractItemDelegate(), unit(BitcoinUnits::LUX),
            platformStyle(platformStyle)    {
    }

    inline void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        mainRect.moveLeft(ICON_OFFSET);
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2 * ypad) / 2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top() + ypad, mainRect.width() - xspace - ICON_OFFSET, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top() + ypad + halfheight, mainRect.width() - xspace, halfheight);
        icon = platformStyle->SingleColorIcon(icon);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
		QSettings settings;
		
		QColor foreground;
		
		if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
		   	foreground = SECOND_COLOR_WHITE;	
	   	} else { 
	   		foreground = COLOR_BLACK;
	   	}
        
        if (value.canConvert<QBrush>()) {
            QBrush brush = qvariant_cast<QBrush>(value);
            foreground = brush.color();
        }

        painter->setPen(foreground);
        QRect boundingRect;
        painter->drawText(addressRect, Qt::AlignLeft | Qt::AlignVCenter, address, &boundingRect);

        if (index.data(TransactionTableModel::WatchonlyRole).toBool()) {
            QIcon iconWatchonly = qvariant_cast<QIcon>(index.data(TransactionTableModel::WatchonlyDecorationRole));
            QRect watchonlyRect(boundingRect.right() + 5, mainRect.top() + ypad + halfheight, 16, halfheight);
            iconWatchonly.paint(painter, watchonlyRect);
        }

		if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
			if (amount < 0) {
				foreground = SECOND_COLOR_NEGATIVE;
			} else if (!confirmed) {
				foreground = SECOND_COLOR_UNCONFIRMED;
			} else {
				foreground = SECOND_COLOR_WHITE;
			}	
		} else { 
			if (amount < 0) {
				foreground = COLOR_NEGATIVE;
			} else if (!confirmed) {
				foreground = COLOR_UNCONFIRMED;
			} else {
				foreground = COLOR_BLACK;
			}
		}

        painter->setPen(foreground);
        QString amountText = BitcoinUnits::formatWithUnit(unit, amount, true, BitcoinUnits::separatorAlways);
        if (!confirmed) {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight | Qt::AlignVCenter, amountText);
		if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
			painter->setPen(SECOND_COLOR_WHITE);
		} else { 
			painter->setPen(COLOR_BLACK);
		}
        
        painter->drawText(amountRect, Qt::AlignLeft | Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;
    const PlatformStyle* platformStyle;
};

class TknViewDelegate : public QAbstractItemDelegate
{
public:
    TknViewDelegate(QObject *parent) :
            QAbstractItemDelegate(parent)
    {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const
    {
        if(index.row() >=TOKEN_NUM_ITEMS)
            return;
        painter->save();

        QIcon tokenIcon/*(":/icons/token")*/;
        QString tokenName = index.data(TokenItemModel::NameRole).toString() + ":";
        QString tokenBalance = index.data(TokenItemModel::BalanceRole).toString();
        QString tokenSymbol = index.data(TokenItemModel::SymbolRole).toString();
        tokenBalance.append(" " + tokenSymbol);
        QString receiveAddress = index.data(TokenItemModel::SenderRole).toString();

        QRect mainRect = option.rect;
        mainRect.setWidth(option.rect.width());

        QColor rowColor = Qt::transparent; // index.row() % 2 ? QColor("#ededed") : QColor("#e3e3e3");
        painter->fillRect(mainRect, rowColor);

        int decorationSize = 12; // font line height (2 lines)
        int leftTopMargin = 5;
        QRect decorationRect(mainRect.topLeft() + QPoint(leftTopMargin, leftTopMargin), QSize(decorationSize, decorationSize));
        tokenIcon.paint(painter, decorationRect);

        QFont font = option.font;

        QFontMetrics fmName(font);
        QString clippedName = fmName.elidedText(tokenName, Qt::ElideRight, NAME_WIDTH);

        QString balanceString = tokenBalance;
        balanceString.append(tokenSymbol);
        QFontMetrics fmBalance(font);
        int balanceWidth = fmBalance.width(balanceString);

        QRect nameRect(decorationRect.right() + MARGIN, decorationRect.top(), NAME_WIDTH, decorationSize);
        painter->drawText(nameRect, Qt::AlignLeft|Qt::AlignVCenter, clippedName);

        font.setBold(true);
        painter->setFont(font);
        QRect tokenBalanceRect(nameRect.right() + MARGIN, decorationRect.top(), balanceWidth, decorationSize);
        painter->drawText(tokenBalanceRect, Qt::AlignLeft|Qt::AlignVCenter, tokenBalance);

        QFont addressFont = option.font;
        //addressFont.setPixelSize(addressFont.pixelSize() * 0.9);
        painter->setFont(addressFont);
		QSettings settings;
		if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
			painter->setPen(SECOND_COLOR_UNCONFIRMED);
		} else { 
			painter->setPen(COLOR_UNCONFIRMED);
		}
        
        QRect receiveAddressRect(decorationRect.right() + MARGIN, decorationSize +mainRect.top()
                                 , mainRect.width() - decorationSize, decorationSize * 2);
        painter->drawText(receiveAddressRect, Qt::AlignLeft|Qt::AlignBottom, receiveAddress);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QFont font = option.font;
        font.setBold(true);

        QString balanceString = index.data(TokenItemModel::BalanceRole).toString();
        balanceString.append(" " + index.data(TokenItemModel::SymbolRole).toString());
        QFontMetrics fm(font);
        int balanceWidth = fm.width(balanceString);

        int width = TOKEN_SIZE - 10 + NAME_WIDTH + balanceWidth + 2*MARGIN;
        return QSize(width, TOKEN_SIZE);
    }
};

#include "overviewpage.moc"

OverviewPage::OverviewPage(const PlatformStyle *platformStyle, QWidget* parent) : QWidget(parent),
                                              ui(new Ui::OverviewPage),
                                              clientModel(0),
                                              walletModel(0),
                                              currentBalance(-1),
                                              currentUnconfirmedBalance(-1),
                                              currentImmatureBalance(-1),
                                              currentWatchOnlyBalance(-1),
                                              currentWatchUnconfBalance(-1),
                                              currentWatchImmatureBalance(-1),
                                              txdelegate(new TxViewDelegate(platformStyle)),
                                              tkndelegate(new TknViewDelegate(this)),
                                              filter(0)
{
    nDisplayUnit = 0; // just make sure it's not unitialized
    ui->setupUi(this);
	QSettings settings;
	
    //set Logo
	if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
		ui->labelLogo->setPixmap(QPixmap(":/images/lux_logo_horizontal-white")
								.scaled(410,80,Qt::KeepAspectRatio,
										Qt::SmoothTransformation));
	} else { 
		ui->labelLogo->setPixmap(QPixmap(":/images/lux_logo_horizontal")
								.scaled(410,80,Qt::KeepAspectRatio,
										Qt::SmoothTransformation));
	}


    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    ui->listTokens->setItemDelegate(tkndelegate);
    ui->listTokens->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTokens->setMinimumHeight(TOKEN_NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTokens->setAttribute(Qt::WA_MacShowFocusRect, false);
    connect(ui->listTokens, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTokenClicked(QModelIndex)));

    // init "out of sync" warning labels
	
	ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelDarksendSyncStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");
	if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue") {
		QString styleSheet = ".QLabel { color: #ff7d7d; }";
		ui->labelWalletStatus->setStyleSheet(styleSheet);
		ui->labelDarksendSyncStatus->setStyleSheet(styleSheet);
		ui->labelTransactionsStatus->setStyleSheet(styleSheet);
	} else { 

	}

#if 0
    if (!fLiteMode) {
        ui->frameDarksend->setVisible(false);
    }
#else
       {
        if (fMasterNode) {
            ui->toggleDarksend->setText("(" + tr("Disabled") + ")");
            ui->darksendAuto->setText("(" + tr("Disabled") + ")");
            ui->darksendReset->setText("(" + tr("Disabled") + ")");
            ui->frameDarksend->setEnabled(false);
        } else {
            if (!fEnableDarksend) {
                ui->darksendEnabled->setText(tr("Disabled"));
                ui->toggleDarksend->setText(tr("Start Luxsend"));
            } else {
                ui->darksendEnabled->setText(tr("Enabled"));
                ui->toggleDarksend->setText(tr("Stop Luxsend"));
            }
            timer = new QTimer(this);
            connect(timer, SIGNAL(timeout()), this, SLOT(darkSendStatus()));
            timer->start(2000);
        }
    }
#endif

    if (this->walletModel)
        updateAdvancedUI(this->walletModel->getOptionsModel()->getShowAdvancedUI());

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);
}

void OverviewPage::handleTransactionClicked(const QModelIndex& index)
{
    if (filter)
        Q_EMIT transactionClicked(filter->mapToSource(index));
}

void OverviewPage::handleTokenClicked(const QModelIndex& index)
{
    if (tokenProxyModel)
        emit tokenClicked(tokenProxyModel->mapToSource(index));
}

void OverviewPage::handleOutOfSyncWarningClicks()
{
    Q_EMIT outOfSyncWarningClicked();
}

OverviewPage::~OverviewPage()
{
    if (!fMasterNode)
        delete timer;
}

void OverviewPage::setBalance(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& anonymizedBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance)
{
    currentBalance = balance;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    currentAnonymizedBalance = anonymizedBalance;
    currentWatchOnlyBalance = watchOnlyBalance;
    currentWatchUnconfBalance = watchUnconfBalance;
    currentWatchImmatureBalance = watchImmatureBalance;


    ui->labelBalance->setText(BitcoinUnits::floorHtmlWithUnit(nDisplayUnit, balance, false, BitcoinUnits::separatorAlways));
    ui->labelUnconfirmed->setText(BitcoinUnits::floorHtmlWithUnit(nDisplayUnit, unconfirmedBalance, false, BitcoinUnits::separatorAlways));
    ui->labelImmature->setText(BitcoinUnits::floorHtmlWithUnit(nDisplayUnit, immatureBalance, false, BitcoinUnits::separatorAlways));
    ui->labelAnonymized->setText(BitcoinUnits::floorHtmlWithUnit(nDisplayUnit, anonymizedBalance, false, BitcoinUnits::separatorAlways));
    ui->labelTotal->setText(BitcoinUnits::floorHtmlWithUnit(nDisplayUnit, balance + unconfirmedBalance + immatureBalance, false, BitcoinUnits::separatorAlways));


    ui->labelWatchAvailable->setText(BitcoinUnits::floorHtmlWithUnit(nDisplayUnit, watchOnlyBalance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchPending->setText(BitcoinUnits::floorHtmlWithUnit(nDisplayUnit, watchUnconfBalance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchImmature->setText(BitcoinUnits::floorHtmlWithUnit(nDisplayUnit, watchImmatureBalance, false, BitcoinUnits::separatorAlways));
    ui->labelWatchTotal->setText(BitcoinUnits::floorHtmlWithUnit(nDisplayUnit, watchOnlyBalance + watchUnconfBalance + watchImmatureBalance, false, BitcoinUnits::separatorAlways));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    bool showWatchOnlyImmature = watchImmatureBalance != 0;

    // for symmetry reasons also show immature label when the watch-only one is shown
    ui->labelImmature->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelImmatureText->setVisible(showImmature || showWatchOnlyImmature);
    ui->labelWatchImmature->setVisible(showWatchOnlyImmature); // show watch-only immature balance

    updateDarkSendProgress();

    static int cachedTxLocks = 0;

    if (cachedTxLocks != nCompleteTXLocks) {
        cachedTxLocks = nCompleteTXLocks;
    }
    ui->listTransactions->update();
}

// show/hide watch-only labels
void OverviewPage::updateWatchOnlyLabels(bool showWatchOnly)
{
    ui->labelSpendable->setVisible(showWatchOnly);      // show spendable label (only when watch-only is active)
    ui->labelWatchonly->setVisible(showWatchOnly);      // show watch-only label
    ui->lineWatchBalance->setVisible(showWatchOnly);    // show watch-only balance separator line
    ui->labelWatchAvailable->setVisible(showWatchOnly); // show watch-only available balance
    ui->labelWatchPending->setVisible(showWatchOnly);   // show watch-only pending balance
    ui->labelWatchTotal->setVisible(showWatchOnly);     // show watch-only total balance

    if (!showWatchOnly) {
        ui->labelWatchImmature->hide();
    } else {
        ui->labelBalance->setIndent(20);
        ui->labelUnconfirmed->setIndent(20);
        ui->labelImmature->setIndent(20);
        ui->labelTotal->setIndent(20);
    }
}

void OverviewPage::setClientModel(ClientModel* model)
{
    this->clientModel = model;
    if (model) {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel* model)
{
    this->walletModel = model;
    if (model && model->getOptionsModel()) {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Date, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getUnconfirmedBalance(), model->getImmatureBalance(), model->getAnonymizedBalance(),
            model->getWatchBalance(), model->getWatchUnconfirmedBalance(), model->getWatchImmatureBalance());
        connect(model, SIGNAL(balanceChanged(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)), this, SLOT(setBalance(CAmount, CAmount, CAmount, CAmount, CAmount, CAmount, CAmount)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));
        connect(model->getOptionsModel(), SIGNAL(advancedUIChanged(bool)), this, SLOT(updateAdvancedUI(bool)));
        updateAdvancedUI(model->getOptionsModel()->getShowAdvancedUI());

        connect(ui->darksendAuto, SIGNAL(clicked()), this, SLOT(darksendAuto()));
        connect(ui->darksendReset, SIGNAL(clicked()), this, SLOT(darksendReset()));
        connect(ui->toggleDarksend, SIGNAL(clicked()), this, SLOT(toggleDarksend()));
        updateWatchOnlyLabels(model->haveWatchOnly());
        connect(model, SIGNAL(notifyWatchonlyChanged(bool)), this, SLOT(updateWatchOnlyLabels(bool)));
    }

    if(model && model->getTokenItemModel())
    {
        // Sort tokens by name
        tokenProxyModel = new QSortFilterProxyModel(this);
        TokenItemModel* tokenModel = model->getTokenItemModel();
        tokenProxyModel->setSourceModel(tokenModel);
        tokenProxyModel->sort(0, Qt::AscendingOrder);

        // Set tokens model
        ui->listTokens->setModel(tokenProxyModel);

        // Hide button if tokens are visible (same behavior)
        ui->buttonAddToken->setVisible(tokenProxyModel->rowCount() == 0);
    }

    // update the display unit, to not use the default ("LUX")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if (walletModel && walletModel->getOptionsModel()) {
        nDisplayUnit = walletModel->getOptionsModel()->getDisplayUnit();
        if (currentBalance != -1)
            setBalance(currentBalance, currentUnconfirmedBalance, currentImmatureBalance, currentAnonymizedBalance,
                currentWatchOnlyBalance, currentWatchUnconfBalance, currentWatchImmatureBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = nDisplayUnit;

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString& warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelDarksendSyncStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}

void OverviewPage::on_buttonAddToken_clicked()
{
    Q_EMIT addTokenClicked(true);
}

void OverviewPage::updateDarkSendProgress()
{
    if (!pwalletMain) return;
    if (IsInitialBlockDownload()) {
        float progress = 0.0;
        ui->darksendProgress->setValue(progress);
        QString strToolPip = ("<b>" + tr("Currently syncing") + "</b>");
        ui->darksendProgress->setToolTip(strToolPip);
        return; // don't update the darksend progress while syncing - there's no point!
    }

    QString strAmountAndRounds;
    QString strAnonymizeLuxAmount = BitcoinUnits::formatHtmlWithUnit(nDisplayUnit, nAnonymizeLuxAmount * COIN, false, BitcoinUnits::separatorAlways);

    if (!fEnableDarksend) {
        ui->darksendProgress->setValue(0);
        ui->darksendProgress->setToolTip(tr("Darksend disabled"));

        // when balance is zero just show info from settings
        strAnonymizeLuxAmount = strAnonymizeLuxAmount.remove(strAnonymizeLuxAmount.indexOf("."), BitcoinUnits::decimals(nDisplayUnit) + 1);
        strAmountAndRounds = strAnonymizeLuxAmount + " / " + tr("%n Rounds", "", nDarksendRounds);

        ui->labelAmountRounds->setToolTip(tr("Darksend disabled"));
        ui->labelAmountRounds->setText(tr("Disabled"));
        return;
    }

    if (currentBalance == 0) {
        ui->darksendProgress->setValue(0);
        ui->darksendProgress->setToolTip(tr("No inputs detected"));

        // when balance is zero just show info from settings
        strAnonymizeLuxAmount = strAnonymizeLuxAmount.remove(strAnonymizeLuxAmount.indexOf("."), BitcoinUnits::decimals(nDisplayUnit) + 1);
        strAmountAndRounds = strAnonymizeLuxAmount + " / " + tr("%n Rounds", "", nDarksendRounds);

        ui->labelAmountRounds->setToolTip(tr("No inputs detected"));
        ui->labelAmountRounds->setText(strAmountAndRounds);
        return;
    }

    CAmount nDenominatedConfirmedBalance;
    CAmount nDenominatedUnconfirmedBalance;
    CAmount nAnonymizableBalance;
    CAmount nNormalizedAnonymizedBalance;
    double nAverageAnonymizedRounds;

    {
        //TRY_LOCK(cs_main, lockMain);
        //if (!lockMain) return;

        nDenominatedConfirmedBalance = pwalletMain->GetDenominatedBalance(false);
        nDenominatedUnconfirmedBalance = pwalletMain->GetDenominatedBalance(true, false);
        nAnonymizableBalance = pwalletMain->GetAnonymizableBalance();
        nNormalizedAnonymizedBalance = pwalletMain->GetNormalizedAnonymizedBalance();
        nAverageAnonymizedRounds = pwalletMain->GetAverageAnonymizedRounds();
    }

    CAmount nMaxToAnonymize = nAnonymizableBalance + currentAnonymizedBalance + nDenominatedUnconfirmedBalance;

    // If it's more than the anon threshold, limit to that.
    if (nMaxToAnonymize > nAnonymizeLuxAmount * COIN) nMaxToAnonymize = nAnonymizeLuxAmount * COIN;

    if (nMaxToAnonymize == 0) return;

    if (nMaxToAnonymize >= nAnonymizeLuxAmount * COIN) {
        ui->labelAmountRounds->setToolTip(tr("Found enough compatible inputs to anonymize %1")
                                              .arg(strAnonymizeLuxAmount));
        strAnonymizeLuxAmount = strAnonymizeLuxAmount.remove(strAnonymizeLuxAmount.indexOf("."), BitcoinUnits::decimals(nDisplayUnit) + 1);
        strAmountAndRounds = strAnonymizeLuxAmount + " / " + tr("%n Rounds", "", nDarksendRounds);
    } else {
        QString strMaxToAnonymize = BitcoinUnits::formatHtmlWithUnit(nDisplayUnit, nMaxToAnonymize, false, BitcoinUnits::separatorAlways);
        ui->labelAmountRounds->setToolTip(tr("Not enough compatible inputs to anonymize <span style='color:red;'>%1</span>,<br>"
                                             "will anonymize <span style='color:red;'>%2</span> instead")
                                              .arg(strAnonymizeLuxAmount)
                                              .arg(strMaxToAnonymize));
        strMaxToAnonymize = strMaxToAnonymize.remove(strMaxToAnonymize.indexOf("."), BitcoinUnits::decimals(nDisplayUnit) + 1);
        strAmountAndRounds = "<span style='color:red;'>" +
                             QString(BitcoinUnits::factor(nDisplayUnit) == 1 ? "" : "~") + strMaxToAnonymize +
                             " / " + tr("%n Rounds", "", nDarksendRounds) + "</span>";
    }
    ui->labelAmountRounds->setText(strAmountAndRounds);

    // calculate parts of the progress, each of them shouldn't be higher than 1
    // progress of denominating
    float denomPart = 0;
    // mixing progress of denominated balance
    float anonNormPart = 0;
    // completeness of full amount anonimization
    float anonFullPart = 0;

    CAmount denominatedBalance = nDenominatedConfirmedBalance + nDenominatedUnconfirmedBalance;
    denomPart = (float)denominatedBalance / nMaxToAnonymize;
    denomPart = denomPart > 1 ? 1 : denomPart;
    denomPart *= 100;

    anonNormPart = (float)nNormalizedAnonymizedBalance / nMaxToAnonymize;
    anonNormPart = anonNormPart > 1 ? 1 : anonNormPart;
    anonNormPart *= 100;

    anonFullPart = (float)currentAnonymizedBalance / nMaxToAnonymize;
    anonFullPart = anonFullPart > 1 ? 1 : anonFullPart;
    anonFullPart *= 100;

    // apply some weights to them ...
    float denomWeight = 1;
    float anonNormWeight = nDarksendRounds;
    float anonFullWeight = 2;
    float fullWeight = denomWeight + anonNormWeight + anonFullWeight;
    // ... and calculate the whole progress
    float denomPartCalc = ceilf((denomPart * denomWeight / fullWeight) * 100) / 100;
    float anonNormPartCalc = ceilf((anonNormPart * anonNormWeight / fullWeight) * 100) / 100;
    float anonFullPartCalc = ceilf((anonFullPart * anonFullWeight / fullWeight) * 100) / 100;
    float progress = denomPartCalc + anonNormPartCalc + anonFullPartCalc;
    if (progress >= 100) progress = 100;

    ui->darksendProgress->setValue(progress);

    QString strToolPip = ("<b>" + tr("Overall progress") + ": %1%</b><br/>" +
                          tr("Denominated") + ": %2%<br/>" +
                          tr("Mixed") + ": %3%<br/>" +
                          tr("Anonymized") + ": %4%<br/>" +
                          tr("Denominated inputs have %5 of %n rounds on average", "", nDarksendRounds))
                             .arg(progress)
                             .arg(denomPart)
                             .arg(anonNormPart)
                             .arg(anonFullPart)
                             .arg(nAverageAnonymizedRounds);
    ui->darksendProgress->setToolTip(strToolPip);
}

void OverviewPage::updateAdvancedUI(bool fShowAdvancedPSUI) {
    ui->labelCompletitionText->setVisible(fShowAdvancedPSUI);
    ui->darksendProgress->setVisible(fShowAdvancedPSUI);
    ui->labelAnonymizedText->setVisible(fShowAdvancedPSUI);
    ui->labelAnonymized->setVisible(fShowAdvancedPSUI);
    ui->labelAmountRoundsText->setVisible(fShowAdvancedPSUI);
    ui->labelAmountRounds->setVisible(fShowAdvancedPSUI);
    ui->labelSubmittedDenomText->setVisible(fShowAdvancedPSUI);
    ui->labelSubmittedDenom->setVisible(fShowAdvancedPSUI);
    ui->darkSendStatus->setVisible(fShowAdvancedPSUI);
    ui->darksendAuto->setVisible(fShowAdvancedPSUI);
    ui->darksendReset->setVisible(fShowAdvancedPSUI);
}

void OverviewPage::darkSendStatus()
{
    //bool fIsInitialBlockDownload = IsInitialBlockDownload();
    static int64_t nLastDSProgressBlockTime = 0;

    int nBestHeight = chainActive.Height();

    // we we're processing more then 1 block per second, we'll just leave
    if (((nBestHeight - darkSendPool.cachedNumBlocks) / (GetTimeMillis() - nLastDSProgressBlockTime + 1) > 1)) return;
    nLastDSProgressBlockTime = GetTimeMillis();

    if (!fEnableDarksend) {
        if (nBestHeight != darkSendPool.cachedNumBlocks) {
            darkSendPool.cachedNumBlocks = nBestHeight;
            updateDarkSendProgress();

            ui->darksendEnabled->setText(tr("Disabled"));
            ui->darkSendStatus->setText("");
            ui->toggleDarksend->setText(tr("Start Luxsend"));
        }

        return;
    }

    // check darksend status and unlock if needed
    if (nBestHeight != darkSendPool.cachedNumBlocks) {
        // Balance and number of transactions might have changed
        darkSendPool.cachedNumBlocks = nBestHeight;
        updateDarkSendProgress();

        ui->darksendEnabled->setText(tr("Enabled"));
    }

    QString strStatus;
    int poolStatus = darkSendPool.GetState();
    switch (poolStatus) {
        case POOL_STATUS_UNKNOWN:
        case POOL_STATUS_IDLE:
            strStatus = tr("Waiting for update");
            break;
        case POOL_STATUS_QUEUE:
            strStatus = tr("Waiting in a queue");
            break;
        case POOL_STATUS_ACCEPTING_ENTRIES:
            strStatus = tr("Accepting entries");
            break;
        case POOL_STATUS_FINALIZE_TRANSACTION:
            strStatus = tr("Master node broadcast");
            break;
        case POOL_STATUS_SIGNING:
            strStatus = tr("Signing");
            break;
        case POOL_STATUS_TRANSMISSION:
            strStatus = tr("Transmitting");
            break;
        case POOL_STATUS_ERROR:
            strStatus = tr("Errored");
            break;
        case POOL_STATUS_SUCCESS:
            strStatus = tr("Success");
            break;
        default:
            strStatus = QString("");
    }

    if (darkSendPool.sessionDenom == 0) {
        ui->labelSubmittedDenom->setText(tr("N/A"));
    } else {
        std::string out;
        darkSendPool.GetDenominationsToString(darkSendPool.sessionDenom, out);
        QString s2(out.c_str());
        ui->labelSubmittedDenom->setText(s2);
    }

    if (strStatus != ui->darkSendStatus->text() && !strStatus.isEmpty()) {
        LogPrintf("Last Luxsend message: %s\n", strStatus.toStdString().c_str());
        ui->darkSendStatus->setText(strStatus);
    } else {
        ui->darkSendStatus->setText("");
    }
}

void OverviewPage::darksendAuto()
{

    darkSendPool.DoAutomaticDenominating();

}

void OverviewPage::darksendReset()
{

    darkSendPool.Reset();

    QMessageBox::warning(this, tr("Darksend"),
        tr("Darksend was successfully reset."),
        QMessageBox::Ok, QMessageBox::Ok);

}

void OverviewPage::toggleDarksend()
{

    QSettings settings;
    // Popup some information on first mixing
    QString hasMixed = settings.value("hasMixed").toString();
    if (hasMixed.isEmpty()) {
        QMessageBox::information(this, tr("Darksend"),
            tr("If you don't want to see internal Darksend fees/transactions select \"Most Common\" as Type on the \"Transactions\" tab."),
            QMessageBox::Ok, QMessageBox::Ok);
        settings.setValue("hasMixed", "hasMixed");
    }
    if (!fEnableDarksend) {
        int64_t balance = currentBalance;
        float minAmount = 14.90 * COIN;
        if (balance < minAmount) {
            QString strMinAmount(BitcoinUnits::formatWithUnit(nDisplayUnit, minAmount));
            QMessageBox::warning(this, tr("Darksend"),
                tr("Darksend requires at least %1 to use.").arg(strMinAmount),
                QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

        // if wallet is locked, ask for a passphrase
        if (walletModel->getEncryptionStatus() == WalletModel::Locked) {
            WalletModel::UnlockContext ctx(walletModel->requestUnlock(false));
            if (!ctx.isValid()) {
                //unlock was cancelled
                darkSendPool.cachedNumBlocks = std::numeric_limits<int>::max();
                QMessageBox::warning(this, tr("Darksend"),
                    tr("Wallet is locked and user declined to unlock. Disabling Darksend."),
                    QMessageBox::Ok, QMessageBox::Ok);
                if (fDebug) LogPrintf("Wallet is locked and user declined to unlock. Disabling Darksend.\n");
                return;
            }
        }
    }

    fEnableDarksend = !fEnableDarksend;
    darkSendPool.cachedNumBlocks = std::numeric_limits<int>::max();

    updateAdvancedUI(this->walletModel->getOptionsModel()->getShowAdvancedUI());

    if (!fEnableDarksend) {
        ui->toggleDarksend->setText(tr("Start Luxsend"));
        darkSendPool.UnlockCoins();
    } else {
        ui->toggleDarksend->setText(tr("Stop Luxsend"));

        /* show darksend configuration if client has defaults set */

        if (nAnonymizeLuxAmount == 0) {
            DarksendConfig dlg(this);
            dlg.setModel(walletModel);
            dlg.exec();
        }
    }

}
