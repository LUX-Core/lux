#include "blockbrowser.h"
#include "ui_blockbrowser.h"

#include "main.h"
#include "wallet.h"
#include "base58.h"
#include "clientmodel.h"
#include "walletmodel.h"
#include "rpcconsole.h"
#include "transactionrecord.h"
#include "optionsmodel.h"
#include "guiutil.h"
#include "guiconstants.h"
#include "init.h"
#include "rpcserver.h"

using namespace json_spirit;

#include <sstream>
#include <string>

#include <QAbstractItemDelegate>
#include <QPainter>

#define DECORATION_SIZE 48
#define NUM_ITEMS 10

BlockBrowser::BlockBrowser(QWidget *parent) :
    ui(new Ui::BlockBrowser),
    model(0)
{
    ui->setupUi(this);

    connect(ui->blockButton, SIGNAL(pressed()), this, SLOT(blockClicked()));
    connect(ui->txButton, SIGNAL(pressed()), this, SLOT(txClicked()));

    // Statistics
    connect(ui->startButton, SIGNAL(pressed()), this, SLOT(updateStatistics()));
}
//Statistics
int heightPrevious = -1;
int connectionPrevious = -1;
int volumePrevious = -1;
double rewardPrevious = -1;
double netPawratePrevious = -1;
double pawratePrevious = -1;
double hardnessPrevious = -1;
double hardnessPrevious2 = -1;
int stakeminPrevious = -1;
int stakemaxPrevious = -1;
QString stakecPrevious = "";

BlockBrowser::~BlockBrowser()
{
    delete ui;
}

double getBlockHardness(int height)
{
    const CBlockIndex* blockindex = getBlockIndex(height);

    int nShift = (blockindex->nBits >> 24) & 0xff;

    double dDiff =
        (double)0x0000ffff / (double)(blockindex->nBits & 0x00ffffff);

    while (nShift < 29)
    {
        dDiff *= 256.0;
        nShift++;
    }
    while (nShift > 29)
    {
        dDiff /= 256.0;
        nShift--;
    }

    return dDiff;
}

int getBlockHashrate(int height)
{
    int lookup = height;

    double timeDiff = getBlockTime(height) - getBlockTime(1);
    double timePerBlock = timeDiff / lookup;

    return (boost::int64_t)(((double)getBlockHardness(height) * pow(2.0, 32)) / timePerBlock);
}

const CBlockIndex* getBlockIndex(int height)
{
    std::string hex = getBlockHash(height);
    uint256 hash(hex);
    return mapBlockIndex[hash];
}

std::string getBlockHash(int Height)
{
    if(Height > pindexBest->nHeight) { return "351c6703813172725c6d660aa539ee6a3d7a9fe784c87fae7f36582e3b797058"; }
    if(Height < 0) { return "351c6703813172725c6d660aa539ee6a3d7a9fe784c87fae7f36582e3b797058"; }
    int desiredheight;
    desiredheight = Height;
    if (desiredheight < 0 || desiredheight > nBestHeight)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hashBestChain];
    while (pblockindex->nHeight > desiredheight)
        pblockindex = pblockindex->pprev;
    return pblockindex->phashBlock->GetHex();
}

int getBlockTime(int Height)
{
    std::string strHash = getBlockHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->nTime;
}

std::string getBlockMerkle(int Height)
{
    std::string strHash = getBlockHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->hashMerkleRoot.ToString().substr(0,10).c_str();
}

int getBlocknBits(int Height)
{
    std::string strHash = getBlockHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->nBits;
}

int getBlockNonce(int Height)
{
    std::string strHash = getBlockHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->nNonce;
}

std::string getBlockDebug(int Height)
{
    std::string strHash = getBlockHash(Height);
    uint256 hash(strHash);

    if (mapBlockIndex.count(hash) == 0)
        return 0;

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[hash];
    return pblockindex->ToString();
}

int blocksInPastHours(int hours)
{
    int wayback = hours * 3600;
    bool check = true;
    int height = pindexBest->nHeight;
    int heightHour = pindexBest->nHeight;
    int utime = (int)time(NULL);
    int target = utime - wayback;

    while(check)
    {
        if(getBlockTime(heightHour) < target)
        {
            check = false;
            return height - heightHour;
        } else {
            heightHour = heightHour - 1;
        }
    }

    return 0;
}

double getTxTotalValue(std::string txid)
{
    uint256 hash;
    hash.SetHex(txid);

    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock))
        return 1000;

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;

    double value = 0;
    double buffer = 0;
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];

        buffer = value + convertCoins(txout.nValue);
        value = buffer;
    }

    return value;
}

double convertCoins(int64_t amount)
{
    return (double)amount / (double)COIN;
}

std::string getOutputs(std::string txid)
{
    uint256 hash;
    hash.SetHex(txid);

    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock))
        return "fail";

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;

    std::string str = "";
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    

    return str;
}

std::string getInputs(std::string txid)
{
    uint256 hash;
    hash.SetHex(txid);

    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock))
        return "fail";

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;

    std::string str = "";
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    
    return str;
}

int64_t getInputValue(CTransaction tx, CScript target)
{
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];
        if(txout.scriptPubKey == target)
        {
            return txout.nValue;
        }
    }
    return 0;
}

double getTxFees(std::string txid)
{
    uint256 hash;
    hash.SetHex(txid);


    CTransaction tx;
    uint256 hashBlock = 0;
    if (!GetTransaction(hash, tx, hashBlock))
        return 51;

    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << tx;

    double value = 0;
    double buffer = 0;
    for (unsigned int i = 0; i < tx.vout.size(); i++)
    {
        const CTxOut& txout = tx.vout[i];

        buffer = value + convertCoins(txout.nValue);
        value = buffer;
    }

    double value0 = 0;
    double buffer0 = 0;
    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        uint256 hash0;
        const CTxIn& vin = tx.vin[i];
        hash0.SetHex(vin.prevout.hash.ToString());
        CTransaction wtxPrev;
        uint256 hashBlock0 = 0;
        if (!GetTransaction(hash0, wtxPrev, hashBlock0))
             return 0;
        CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
        ssTx << wtxPrev;
        const CScript target = wtxPrev.vout[vin.prevout.n].scriptPubKey;
        buffer0 = value0 + convertCoins(getInputValue(wtxPrev, target));
        value0 = buffer0;
    }

    return value0 - value;
}

void BlockBrowser::updateExplorer(bool block)
{    
    if(block)
    {
        ui->heightLabel->show();
        ui->heightLabel_2->show();
        ui->hashLabel->show();
        ui->hashBox->show();
        ui->merkleLabel->show();
        ui->merkleBox->show();
        ui->nonceLabel->show();
        ui->nonceBox->show();
        ui->bitsLabel->show();
        ui->bitsBox->show();
        ui->timeLabel->show();
        ui->timeBox->show();
        ui->hardLabel->show();
        ui->hardBox->show();;
        ui->pawLabel->show();
        int height = ui->heightBox->value();
        if (height > pindexBest->nHeight)
        {
            ui->heightBox->setValue(pindexBest->nHeight);
            height = pindexBest->nHeight;
        }
        int Pawrate = getBlockHashrate(height);
        double Pawrate2 = 0.000;
        Pawrate2 = ((double)Pawrate / 1000);
        std::string hash = getBlockHash(height);
        std::string merkle = getBlockMerkle(height);ui->pawLabel->show();
        int nBits = getBlocknBits(height);
        int nNonce = getBlockNonce(height);
        int atime = getBlockTime(height);
        double hardness = getBlockHardness(height);
        QString QHeight = QString::number(height);
        QString QHash = QString::fromUtf8(hash.c_str());
        QString QMerkle = QString::fromUtf8(merkle.c_str());
        QString QBits = QString::number(nBits);
        QString QNonce = QString::number(nNonce);
        QString QTime = QString::number(atime);
        QString QHardness = QString::number(hardness, 'f', 6);
        ui->heightLabel->setText(QHeight);
        ui->hashBox->setText(QHash);
        ui->merkleBox->setText(QMerkle);
        ui->bitsBox->setText(QBits);
        ui->nonceBox->setText(QNonce);
        ui->timeBox->setText(QTime);     
        ui->hardBox->setText(QHardness);
    } 
    
    if(block == false) {
        ui->txID->show();
        ui->txLabel->show();
        ui->valueLabel->show();
        ui->valueBox->show();
        ui->inputLabel->show();
        ui->inputBox->show();
        ui->outputLabel->show();
        ui->outputBox->show();
        ui->feesLabel->show();
        ui->feesBox->show();
        std::string txid = ui->txBox->text().toUtf8().constData();
        double value = getTxTotalValue(txid);
        double fees = getTxFees(txid);
        std::string outputs = getOutputs(txid);
        std::string inputs = getInputs(txid);
        QString QValue = QString::number(value, 'f', 6);
        QString QID = QString::fromUtf8(txid.c_str());
        QString QOutputs = QString::fromUtf8(outputs.c_str());
        QString QInputs = QString::fromUtf8(inputs.c_str());
        QString QFees = QString::number(fees, 'f', 6);
        ui->valueBox->setText(QValue + " LUX");
        ui->txID->setText(QID);
        ui->outputBox->setText(QOutputs);
        ui->inputBox->setText(QInputs);
        ui->feesBox->setText(QFees + " LUX");
    }
}


void BlockBrowser::txClicked()
{
    updateExplorer(false);
}

void BlockBrowser::blockClicked()
{
    updateExplorer(true);
}


//Statistics Section
void BlockBrowser::updateStatistics()
{
    double pHardness = GetDifficulty();
    double pHardness2 = GetDifficulty(GetLastBlockIndex(pindexBest, true));
    int pPawrate = GetPoWMHashPS();
    double pPawrate2 = 0.000;
    int nHeight = pindexBest->nHeight;
    double nSubsidy = 10;
    if(pindexBest->nHeight < 500 && pindexBest->nHeight > 1)
    {
        nSubsidy = 1;
    }
    else if(pindexBest->nHeight < 6000000)
    {
        nSubsidy = 10;
    }
    uint64_t nMinWeight = 0; //, nMaxWeight = 0, nWeight = 0
    //pwalletMain->GetStakeWeight(*pwalletMain, nMinWeight, nMaxWeight, nWeight);
    pwalletMain->GetStakeWeight();
    uint64_t nNetworkWeight = GetPoSKernelPS();
    int64_t volume = ((pindexBest->nMoneySupply)/500000000);
    int peers = this->clientModel->getNumConnections();
    pPawrate2 = (double)pPawrate;
    QString height = QString::number(nHeight);
    QString stakemin = QString::number(nMinWeight);
    QString stakemax = QString::number(nNetworkWeight);
    QString phase = "";
    if (pindexBest->nHeight < 1000)
    {
        phase = "PoW";
    }
    else if (pindexBest->nHeight > 1000)
    {
        phase = "PoW+PoS";
    }

    QString subsidy = QString::number(nSubsidy, 'f', 6);
    QString hardness = QString::number(pHardness, 'f', 6);
    QString hardness2 = QString::number(pHardness2, 'f', 6);
    QString pawrate = QString::number(pPawrate2, 'f', 3);
    QString Qlpawrate = clientModel->getLastBlockDate().toString();

    QString QPeers = QString::number(peers);
    // QString qVolume = QLocale(QLocale::English).toString(volume);
    QString qVolume = QString::number(volume);

    if(nHeight > heightPrevious)
    {
        ui->heightBox_3->setText("<b><font color=\"green\">" + height + "</font></b>");
    } else {
    ui->heightBox_3->setText(height);
    }

    if(0 > stakeminPrevious)
    {
        ui->minBox_2->setText("<b><font color=\"green\">" + stakemin + "</font></b>");
    } else {
    ui->minBox_2->setText(stakemin);
    }
    if(0 > stakemaxPrevious)
    {
        ui->maxBox_2->setText("<b><font color=\"green\">" + stakemax + "</font></b>");
    } else {
    ui->maxBox_2->setText(stakemax);
    }

    if(phase != stakecPrevious)
    {
        ui->cBox_2->setText("<b><font color=\"green\">" + phase + "</font></b>");
    } else {
    ui->cBox_2->setText(phase);
    }


    if(nSubsidy < rewardPrevious)
    {
        ui->rewardBox_2->setText("<b><font color=\"red\">" + subsidy + "</font></b>");
    } else {
    ui->rewardBox_2->setText(subsidy);
    }

    if(pHardness > hardnessPrevious)
    {
        ui->diffBox_2->setText("<b><font color=\"green\">" + hardness + "</font></b>");
    } else if(pHardness < hardnessPrevious) {
        ui->diffBox_2->setText("<b><font color=\"red\">" + hardness + "</font></b>");
    } else {
        ui->diffBox_2->setText(hardness);
    }

    if(pHardness2 > hardnessPrevious2)
    {
        ui->diffBox2_2->setText("<b><font color=\"green\">" + hardness2 + "</font></b>");
    } else if(pHardness2 < hardnessPrevious2) {
        ui->diffBox2_2->setText("<b><font color=\"red\">" + hardness2 + "</font></b>");
    } else {
        ui->diffBox2_2->setText(hardness2);
    }

    if(pPawrate2 > netPawratePrevious)
    {
        ui->pawrateBox_2->setText("<b><font color=\"green\">" + pawrate + " MH/s</font></b>");
    } else if(pPawrate2 < netPawratePrevious) {
        ui->pawrateBox_2->setText("<b><font color=\"red\">" + pawrate + " MH/s</font></b>");
    } else {
        ui->pawrateBox_2->setText(pawrate + " GH/s");
    }

    if(Qlpawrate != pawratePrevious)
    {
        ui->localBox_2->setText("<b><font color=\"green\">" + Qlpawrate + "</font></b>");
    } else {
    ui->localBox_2->setText(Qlpawrate);
    }

    if(peers > connectionPrevious)
    {
        ui->connectionBox_2->setText("<b><font color=\"green\">" + QPeers + "</font></b>");
    } else if(peers < connectionPrevious) {
        ui->connectionBox_2->setText("<b><font color=\"red\">" + QPeers + "</font></b>");
    } else {
        ui->connectionBox_2->setText(QPeers);
    }

    if(volume > volumePrevious)
    {
        ui->volumeBox_2->setText("<b><font color=\"green\">" + qVolume + " LUX" + "</font></b>");
    } else if(volume < volumePrevious) {
        ui->volumeBox_2->setText("<b><font color=\"red\">" + qVolume + " LUX" + "</font></b>");
    } else {
        ui->volumeBox_2->setText(qVolume + " LUX");
    }
    updatePrevious(nHeight, nMinWeight, nNetworkWeight, phase, nSubsidy, pHardness, pHardness2, pPawrate2, Qlpawrate, peers, volume);
}

void BlockBrowser::updatePrevious(int nHeight, int nMinWeight, int nNetworkWeight, QString phase, double nSubsidy, double pHardness, double pHardness2, double pPawrate2, QString Qlpawrate, int peers, int volume)
{
    heightPrevious = nHeight;
    stakeminPrevious = nMinWeight;
    stakemaxPrevious = nNetworkWeight;
    stakecPrevious = phase;
    rewardPrevious = nSubsidy;
    hardnessPrevious = pHardness;
    hardnessPrevious2 = pHardness2;
    netPawratePrevious = pPawrate2;
    pawratePrevious = Qlpawrate;
    connectionPrevious = peers;
    volumePrevious = volume;
}

void BlockBrowser::setModel(WalletModel *model)
{
    this->model = model;
}
