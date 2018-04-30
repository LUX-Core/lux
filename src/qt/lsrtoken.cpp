#include "lsrtoken.h"
#include "ui_lsrtoken.h"
#include "tokenitemmodel.h"
#include "walletmodel.h"
#include "tokentransactionview.h"
//#include "platformstyle.h"

#include <QPainter>
#include <QAbstractItemDelegate>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QActionGroup>
#include <iostream>
#include <QSortFilterProxyModel>
#include <QSizePolicy>
#include <QMenu>

#define TOKEN_SIZE 54
#define SYMBOL_WIDTH 100
#define MARGIN 5

class TokenViewDelegate : public QAbstractItemDelegate
{
public:
    /*
    enum DataRole{
        AddressRole = Qt::UserRole + 1,
        NameRole = Qt::UserRole + 2,
        SymbolRole = Qt::UserRole + 3,
        DecimalsRole = Qt::UserRole + 4,
        SenderRole = Qt::UserRole + 5,
        BalanceRole = Qt::UserRole + 6,
    };
    */
    TokenViewDelegate(QObject *parent) :
            QAbstractItemDelegate(parent)
    {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const
    {
        painter->save();

        QIcon tokenIcon/*(":/icons/token")*/;
        QString tokenSymbol = index.data(TokenItemModel::SymbolRole).toString();
        QString tokenBalance = index.data(TokenItemModel::BalanceRole).toString();
        QString receiveAddress = index.data(TokenItemModel::SenderRole).toString();

        QRect mainRect = option.rect;
        mainRect.setWidth(option.rect.width());
        QColor rowColor = index.row() % 2 ? QColor("#ededed") : QColor("#e3e3e3");
        painter->fillRect(mainRect, rowColor);

        bool selected = option.state & QStyle::State_Selected;
        if(selected)
        {
            painter->fillRect(mainRect,QColor("#cbcbcb"));
        }

        int decorationSize = TOKEN_SIZE - 20;
        int leftTopMargin = 10;
        QRect decorationRect(mainRect.topLeft() + QPoint(leftTopMargin, leftTopMargin), QSize(decorationSize, decorationSize));
        tokenIcon.paint(painter, decorationRect);

        QFontMetrics fmName(option.font);
        QString clippedSymbol = fmName.elidedText(tokenSymbol, Qt::ElideRight, SYMBOL_WIDTH);

        QColor foreground = option.palette.color(QPalette::Text);
        painter->setPen(foreground);
        QRect tokenSymbolRect(decorationRect.right() + MARGIN, decorationRect.top(), SYMBOL_WIDTH, decorationSize / 2);
        painter->drawText(tokenSymbolRect, Qt::AlignLeft|Qt::AlignTop, clippedSymbol);

        QFont font = option.font;
        font.setBold(true);
        painter->setFont(font);

        int amountWidth = (mainRect.width() - decorationRect.width() - 2 * MARGIN - tokenSymbolRect.width()- leftTopMargin);
        QRect tokenBalanceRect(tokenSymbolRect.right(), decorationRect.top(), amountWidth, decorationSize / 2);
        painter->drawText(tokenBalanceRect, Qt::AlignRight|Qt::AlignTop, tokenBalance);

        QFont addressFont = option.font;
        addressFont.setPixelSize(addressFont.pixelSize() * 0.9);
        painter->setFont(addressFont);
        QRect receiveAddressRect(decorationRect.right() + MARGIN, tokenSymbolRect.bottom(), mainRect.width() - decorationSize, decorationSize / 2);
        painter->drawText(receiveAddressRect, Qt::AlignLeft|Qt::AlignBottom, receiveAddress);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(TOKEN_SIZE, TOKEN_SIZE);
    }
};

LSRToken::LSRToken(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::LSRToken),
    m_model(0),
    m_clientModel(0),
    m_tokenModel(0),
    m_tokenDelegate(0),
    m_tokenTransactionView(0)
{
    ui->setupUi(this);

    m_sendTokenPage = new SendTokenPage(this);
    m_receiveTokenPage = new ReceiveTokenPage(this);
    m_addTokenPage = new AddTokenPage(this);
    m_tokenDelegate = new TokenViewDelegate(this);

    m_sendTokenPage->setEnabled(false);
    m_receiveTokenPage->setEnabled(false);

    ui->stackedWidget->addWidget(m_sendTokenPage);
    ui->stackedWidget->addWidget(m_receiveTokenPage);
    ui->stackedWidget->addWidget(m_addTokenPage);

    m_tokenTransactionView = new TokenTransactionView(this);
    m_tokenTransactionView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->tokenViewLayout->addWidget(m_tokenTransactionView);

    ui->tokensList->setItemDelegate(m_tokenDelegate);
    ui->tokensList->setContextMenuPolicy(Qt::CustomContextMenu);

    QActionGroup *actionGroup = new QActionGroup(this);
    m_sendAction = new QAction(tr("Send"), actionGroup);
    m_receiveAction = new QAction(tr("Receive"), actionGroup);
    m_addTokenAction = new QAction(tr("AddToken"), actionGroup);
    actionGroup->setExclusive(true);

    m_sendAction->setCheckable(true);
    m_receiveAction->setCheckable(true);
    m_addTokenAction->setCheckable(true);

    ui->sendButton->setDefaultAction(m_sendAction);
    ui->receiveButton->setDefaultAction(m_receiveAction);
    ui->addTokenButton->setDefaultAction(m_addTokenAction);

    QAction *copyTokenAddressAction = new QAction(tr("Copy token address"), this);
    QAction *copyTokenBalanceAction = new QAction(tr("Copy token balance"), this);
    QAction *copyTokenNameAction = new QAction(tr("Copy token name"), this);
    QAction *copySenderAction = new QAction(tr("Copy sender address"), this);
    QAction *removeTokenAction = new QAction(tr("Remove token"), this);

    contextMenu = new QMenu(ui->tokensList);
    contextMenu->addAction(copyTokenAddressAction);
    contextMenu->addAction(copyTokenBalanceAction);
    contextMenu->addAction(copyTokenNameAction);
    contextMenu->addAction(copySenderAction);
    contextMenu->addAction(removeTokenAction);

    connect(copyTokenAddressAction, SIGNAL(triggered(bool)), this, SLOT(copyTokenAddress()));
    connect(copyTokenBalanceAction, SIGNAL(triggered(bool)), this, SLOT(copyTokenBalance()));
    connect(copyTokenNameAction, SIGNAL(triggered(bool)), this, SLOT(copyTokenName()));
    connect(copySenderAction, SIGNAL(triggered(bool)), this, SLOT(copySenderAddress()));
    connect(removeTokenAction, SIGNAL(triggered(bool)), this, SLOT(removeToken()));

    connect(m_sendAction, SIGNAL(triggered()), this, SLOT(on_goToSendTokenPage()));
    connect(m_receiveAction, SIGNAL(triggered()), this, SLOT(on_goToReceiveTokenPage()));
    connect(m_addTokenAction, SIGNAL(triggered()), this, SLOT(on_goToAddTokenPage()));
    connect(ui->tokensList, SIGNAL(clicked(QModelIndex)), this, SLOT(on_currentTokenChanged(QModelIndex)));
    connect(ui->tokensList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(contextualMenu(QPoint)));

    on_goToSendTokenPage();
}

LSRToken::~LSRToken()
{
    delete ui;
}

void LSRToken::setModel(WalletModel *_model)
{
    m_model = _model;
    m_addTokenPage->setModel(m_model);
    m_sendTokenPage->setModel(m_model);
    m_tokenTransactionView->setModel(_model);
    if(m_model && m_model->getTokenItemModel())
    {
        // Sort tokens by symbol
        QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(this);
        TokenItemModel* tokenModel = m_model->getTokenItemModel();
        proxyModel->setSourceModel(tokenModel);
        proxyModel->sort(1, Qt::AscendingOrder);
        m_tokenModel = proxyModel;

        // Set tokens model
        ui->tokensList->setModel(m_tokenModel);
        connect(ui->tokensList->selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)), this, SLOT(on_currentChanged(QModelIndex,QModelIndex)));

        // Set current token
        connect(m_tokenModel, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), SLOT(on_dataChanged(QModelIndex,QModelIndex,QVector<int>)));
        connect(m_tokenModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(on_rowsInserted(QModelIndex,int,int)));
        if(m_tokenModel->rowCount() > 0)
        {
            QModelIndex currentToken(m_tokenModel->index(0, 0));
            ui->tokensList->setCurrentIndex(currentToken);
            on_currentTokenChanged(currentToken);
        }
    }
}

void LSRToken::setClientModel(ClientModel *_clientModel)
{
    m_clientModel = _clientModel;
    m_sendTokenPage->setClientModel(_clientModel);
    m_addTokenPage->setClientModel(_clientModel);
}

void LSRToken::on_goToSendTokenPage()
{
    m_sendAction->setChecked(true);
    ui->stackedWidget->setCurrentIndex(0);
}

void LSRToken::on_goToReceiveTokenPage()
{
    m_receiveAction->setChecked(true);
    ui->stackedWidget->setCurrentIndex(1);
}

void LSRToken::on_goToAddTokenPage()
{
    m_addTokenAction->setChecked(true);
    ui->stackedWidget->setCurrentIndex(2);
}

void LSRToken::on_currentTokenChanged(QModelIndex index)
{
    if(m_tokenModel)
    {
        if(index.isValid())
        {
            m_selectedTokenHash = m_tokenModel->data(index, TokenItemModel::HashRole).toString();
            std::string address = m_tokenModel->data(index, TokenItemModel::AddressRole).toString().toStdString();
            std::string symbol = m_tokenModel->data(index, TokenItemModel::SymbolRole).toString().toStdString();
            std::string sender = m_tokenModel->data(index, TokenItemModel::SenderRole).toString().toStdString();
            int8_t decimals = m_tokenModel->data(index, TokenItemModel::DecimalsRole).toInt();
            std::string balance = m_tokenModel->data(index, TokenItemModel::RawBalanceRole).toString().toStdString();
            m_sendTokenPage->setTokenData(address, sender, symbol, decimals, balance);
            m_receiveTokenPage->setAddress(QString::fromStdString(sender));

            if(!m_sendTokenPage->isEnabled())
                m_sendTokenPage->setEnabled(true);
            if(!m_receiveTokenPage->isEnabled())
                m_receiveTokenPage->setEnabled(true);
        }
        else
        {
            m_sendTokenPage->setEnabled(false);
            m_receiveTokenPage->setEnabled(false);
            m_receiveTokenPage->setAddress(QString::fromStdString(""));
        }
    }
}

void LSRToken::on_dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    Q_UNUSED(bottomRight);
    Q_UNUSED(roles);

    if(m_tokenModel)
    {
        QString tokenHash = m_tokenModel->data(topLeft, TokenItemModel::HashRole).toString();
        if(m_selectedTokenHash.isEmpty() ||
           tokenHash == m_selectedTokenHash)
        {
            on_currentTokenChanged(topLeft);
        }
    }
}

void LSRToken::on_currentChanged(QModelIndex current, QModelIndex previous)
{
    Q_UNUSED(previous);

    on_currentTokenChanged(current);
}

void LSRToken::on_rowsInserted(QModelIndex index, int first, int last)
{
    Q_UNUSED(index);
    Q_UNUSED(first);
    Q_UNUSED(last);

    if(m_tokenModel->rowCount() == 1)
    {
        QModelIndex currentToken(m_tokenModel->index(0, 0));
        ui->tokensList->setCurrentIndex(currentToken);
        on_currentTokenChanged(currentToken);
    }
}

void LSRToken::contextualMenu(const QPoint &point)
{
    QModelIndex index = ui->tokensList->indexAt(point);
    QModelIndexList selection = ui->tokensList->selectionModel()->selectedIndexes();
    if (selection.empty())
        return;

    if(index.isValid())
    {
        contextMenu->exec(QCursor::pos());
    }
}

void LSRToken::copyTokenAddress()
{
    GUIUtil::copyEntryDataFromList(ui->tokensList, TokenItemModel::AddressRole);
}

void LSRToken::copyTokenBalance()
{
    GUIUtil::copyEntryDataFromList(ui->tokensList, TokenItemModel::BalanceRole);
}

void LSRToken::copyTokenName()
{
    GUIUtil::copyEntryDataFromList(ui->tokensList, TokenItemModel::NameRole);
}

void LSRToken::copySenderAddress()
{
    GUIUtil::copyEntryDataFromList(ui->tokensList, TokenItemModel::SenderRole);
}

void LSRToken::removeToken()
{
    QMessageBox::StandardButton btnRetVal = QMessageBox::question(this, tr("Confirm token remove"), tr("The selected token will be removed from the list. Are you sure?"),
                                                                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

    if(btnRetVal == QMessageBox::Yes)
    {
        QModelIndexList selection = ui->tokensList->selectionModel()->selectedIndexes();
        if (selection.empty() && !m_model)
            return;

        QModelIndex index = selection[0];
        std::string sHash = index.data(TokenItemModel::HashRole).toString().toStdString();
        m_model->removeTokenEntry(sHash);
    }
}
