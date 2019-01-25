#ifndef BLOCKEXPLORER_H
#define BLOCKEXPLORER_H

#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>

#include "bitcoinunits.h"
#include "clientmodel.h"
#include "base58.h"
#include "guiconstants.h"
#include "uint256.h"
#include "optionsmodel.h"
#include "platformstyle.h"

#undef loop

namespace Ui
{
class BlockExplorer;
}


class CBlockIndex;
class CTransaction;
class CBlockTreeDB;
class NetworkDisplayStatusBarControl;

std::string getexplorerBlockHash(int64_t);
const CBlockIndex* getexplorerBlockIndex(int64_t);
CTxOut getPrevOut(const COutPoint& out);
void getNextIn(const COutPoint* Out, uint256* Hash, unsigned int n);

class BlockExplorer : public QMainWindow
{
    Q_OBJECT

public:
    explicit BlockExplorer(const PlatformStyle* platformStyle, QWidget* parent = 0);
    ~BlockExplorer();

    void setClientModel(ClientModel *clientModel);

protected:
    void keyPressEvent(QKeyEvent* event);
    void showEvent(QShowEvent*);

private Q_SLOTS:
    void onSearch();
    void goTo(const QString& query);
    void back();
    void forward();
    void home();
private:
    Ui::BlockExplorer* ui;
    bool m_NeverShown;
    int m_HistoryIndex;
    QStringList m_History;

    ClientModel *model;
    NetworkDisplayStatusBarControl* unitDisplayControl;

    void setBlock(CBlockIndex* pBlock);
    bool switchTo(const QString& query);
    void setContent(const std::string& content);
    void updateNavButtons();

    void Translations();
};

class NetworkDisplayStatusBarControl : public QLabel
{
    Q_OBJECT

public:
    explicit NetworkDisplayStatusBarControl(const PlatformStyle* platformStyle);
    /** Lets the control know about the Options Model (and its signals) */
    void setOptionsModel(OptionsModel* optionsModel);

private:
    OptionsModel* optionsModel;

private Q_SLOTS:
    /** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
    void updateDisplayUnit(int newUnits);
};

#endif // BLOCKEXPLORER_H
