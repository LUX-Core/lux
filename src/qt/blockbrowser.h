#ifndef BLOCKBROWSER_H
#define BLOCKBROWSER_H

#include "clientmodel.h"
#include "walletmodel.h"
#include "main.h"
#include "wallet.h"
#include "base58.h"
#include <QWidget>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTime>
#include <QTimer>
#include <QStringList>
#include <QMap>
#include <QSettings>
#include <QSlider>

double getBlockHardness(int);
double getTxTotalValue(std::string);
double convertCoins(int64_t);
double getTxFees(std::string);
int getBlockTime(int);
int getBlocknBits(int);
int getBlockNonce(int);
int blocksInPastHours(int);
int getBlockHashrate(int);
std::string getInputs(std::string);
std::string getOutputs(std::string);
std::string getBlockHash(int);
std::string getBlockMerkle(int);
bool addnode(std::string);
const CBlockIndex* getBlockIndex(int);
int64_t getInputValue(CTransaction, CScript);


namespace Ui {
class BlockBrowser;
}
class ClientModel;
class WalletModel;

QT_BEGIN_NAMESPACE
class QModelIndex;
QT_END_NAMESPACE

class BlockBrowser : public QWidget
{
    Q_OBJECT

public:
    explicit BlockBrowser(QWidget *parent = 0);
    ~BlockBrowser();
    
    void setModel(WalletModel *model);
    
    //Statistics
    int heightPrevious;
    int connectionPrevious;
    int volumePrevious;
    int stakeminPrevious;
    int stakemaxPrevious;
    QString stakecPrevious;
    double rewardPrevious;
    double netPawratePrevious;
    QString pawratePrevious;
    double hardnessPrevious;
    double hardnessPrevious2;


public slots:
    
    void blockClicked();
    void txClicked();
    void updateExplorer(bool);
    void updateStatistics();
    void updatePrevious(int, int, int, QString, double, double, double, double, QString, int, int);

signals:

private slots:

private:
    Ui::BlockBrowser *ui;
    WalletModel *model;
    ClientModel *clientModel;
};

#endif // BLOCKBROWSER_H
