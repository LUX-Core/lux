#include "blockexplorer.h"

#include "chainparams.h"
#include "core_io.h"
#include "main.h"
#include "net.h"
#include "txdb.h"
#include "guiutil.h"
#include "ui_blockexplorer.h"
#include "ui_interface.h"
#include "utilmoneystr.h"
#include "utilstrencodings.h"
#include "script/standard.h"
#include "spentindex.h"
#include "pubkey.h"
#include "utilstrencodings.h"

#include <QDateTime>
#include <QKeyEvent>
#include <QMessageBox>
#include <set>
#include <QSettings>

// tr() requires to be in QObject or QWidget methods
// This function allow the lupdate parser to detect our std::string without QString requirement.
#define _(x) BlockExplorer::tr(x).toStdString()

void BlockExplorer::Translations()
{
    QString toTranslate[] = {
        tr("unknown"), tr("Size"), tr("Timestamp"), tr("Value In"),
        tr("Number of Transactions"), tr("Difficulty"), tr("Address"),
        tr("Value Out"), tr("Fees"), tr("Type"), tr("Generated"),
        tr("Bits"), tr("Nonce"), tr("Version"), tr("Hash"),
        tr("Merkle Root"), tr("State Root"), tr("UTXO Root"),
        tr("Block"), tr("Transaction"), tr("Call SC"), tr("Create SC"),
        tr("Smart Contract"), tr("Inputs"), tr("Outputs"),
        tr("Tx Hash"), tr("Amount"), tr("From"), tr("To"), tr("Redeemed in"),
        tr("Balance"), tr("Delta"), tr("In Block"), tr("Taken from"),
        tr("outputs (see tx)"), tr("unhandled or invalid address"),
        tr("movements displayed."), tr("Transactions to/from"),
        tr("-addressindex parameter is required to show address history")
    };
    Q_UNUSED(toTranslate)
}

extern double GetDifficulty(const CBlockIndex* blockindex = NULL);

inline std::string utostr(unsigned int n)
{
    return strprintf("%u", n);
}

static std::string makeHRef(const std::string& Str, const size_t maxChars = 0)
{
    std::string displayed = Str;
    if (maxChars && Str.size() > maxChars)
        displayed = Str.substr(0, maxChars) + "&hellip;";
    return "<a href=\"" + Str + "\">" + displayed + "</a>";
}

static CAmount getTxIn(const CTransaction& tx)
{
    if (tx.IsCoinBase())
        return 0;

    CAmount Sum = 0;
    for(size_t i = 0; i < tx.vin.size(); i++) {
        CTxOut PrevOut = getPrevOut(tx.vin[i].prevout);
        Sum += (PrevOut.nValue > 0 ? PrevOut.nValue : 0);
    }
    return Sum;
}

static std::string ValueToString(CAmount nValue, bool AllowNegative = false, bool units = true)
{
    if (nValue < 0 && !AllowNegative)
        return _("unknown");

    std::string Str = FormatMoney(nValue);
    if (units) Str += "&nbsp;" + CURRENCY_UNIT;
    if (AllowNegative && nValue > 0)
        Str = '+' + Str;
    return Str;
}

static std::string ValueToStringShort(CAmount nValue)
{
    return ValueToString(nValue, false, false);
}

static std::string ScriptToString(const CScript& Script, bool Long = false, bool Highlight = false, std::string hlClass = "hl")
{
    if (Script.empty())
        return "";

    CTxDestination Dest;
    if (ExtractDestination(Script, Dest) && !Script.HasOpCall()) {
        if (Highlight)
            return "<span class=\"" + hlClass + "\">" + EncodeDestination(Dest) + "</span>";
        else
            return makeHRef(EncodeDestination(Dest));
    } else if (Long) {
        std::string out = FormatScript(Script);
        // on SC create, can be very long, split data in multiple lines...
        if (out.length() > 120) for (size_t p=79; p < out.length(); p += 80) {
            out.insert(p, "\n");
        }
        if (Script.HasOpCall()) out.insert(0, _("Call SC") + "<br/>");
        if (Script.HasOpCreate()) out.insert(0, _("Smart Contract") + "<br/>");
        return "<pre class=\"sc\">" + out + "</pre>";
    } else {
       return _("unknown");
    }
}

static std::string TimeToString(uint64_t Time)
{
    QDateTime timestamp;
    timestamp.setTime_t(Time);
    return timestamp.toString("yyyy-MM-dd hh:mm:ss").toUtf8().data();
}

static std::string makeHTMLTableRow(const std::string* pCells, int n)
{
    std::string Result = "<tr>";
    for (int i = 0; i < n; i++) {
        Result += "<td class=\"d" + utostr(i) + "\">";
        Result += pCells[i];
        Result += "</td>";
    }
    Result += "</tr>";
    return Result;
}

static const std::string table = "<table>";

static std::string makeHTMLTable(const std::string* pCells, int nRows, int nColumns)
{
    std::string Table = table;
    for (int i = 0; i < nRows; i++)
        Table += makeHTMLTableRow(pCells + i * nColumns, nColumns);
    Table += "</table>";
    return Table;
}

static std::string TxToRow(const CTransaction& tx, CBlock * const ptrBlock= nullptr,
                           const CTxDestination& Highlight = CTxDestination(),
                           const std::string& Prepend = std::string(), CAmount* pSum = NULL)
{
    std::string InAmounts, InAddresses, OutAmounts, OutAddresses;
    CAmount Delta = 0;

    bool bHasOpCall = false;
    bool bHasOpCreate = false;
    //calc bHasOpCall and bHasOpCreate
    if(ptrBlock != nullptr)
    {
        for (unsigned int i = 0; i < ptrBlock->vtx.size(); i++)
        {
            const CTransaction& tx_ = ptrBlock->vtx[i];
            for (const auto& txout : tx_.vout)
            {
                if (txout.scriptPubKey.HasOpCall()) {
                    bHasOpCall = true;
                    break;
                }
                if (txout.scriptPubKey.HasOpCreate()) {
                    bHasOpCreate = true;
                    break;
                }
            }
            if(bHasOpCall || bHasOpCreate)
                break;
        }
    }

    for (unsigned int j = 0; j < tx.vin.size(); j++) {
        if (tx.IsCoinBase()) {
            InAmounts += ValueToStringShort(tx.GetValueOut());
            std::string strAddr = "coinbase";
            if(tx.HasWitness())
                strAddr = "coinbase(segwit)";
            else if (ptrBlock != nullptr)
            {
                if (ptrBlock->IsProofOfStake())
                    strAddr = "stake";
            }
            InAddresses += strAddr;
        } else {
            CTxOut PrevOut = getPrevOut(tx.vin[j].prevout);
            CTxDestination inAddr;
            ExtractDestination(PrevOut.scriptPubKey, inAddr);
            if (pSum && tx.vin.size() > 50 && inAddr != Highlight) continue;
            InAddresses += ScriptToString(PrevOut.scriptPubKey, false, inAddr == Highlight);
            if (inAddr == Highlight) {
                Delta -= PrevOut.nValue;
                InAmounts += "<span class=\"hli\">" + ValueToStringShort(PrevOut.nValue) + "</span>";
            } else {
                InAmounts += ValueToStringShort(PrevOut.nValue);
            }
        }
        if (j + 1 != tx.vin.size()) {
            InAmounts += "<br/>";
            InAddresses += "<br/>";
        }
    }

    bool bCalcOutAddr = false;
    for (unsigned int j = 0; j < tx.vout.size(); j++) {
        CTxOut Out = tx.vout[j];
        if (Out.nValue == 0 && tx.IsCoinStake()) continue;
        CTxDestination outAddr;
        ExtractDestination(Out.scriptPubKey, outAddr);
        if (pSum && tx.vout.size() > 50 && outAddr != Highlight) continue; // for pools payouts
        auto strAddress = ScriptToString(Out.scriptPubKey, false, outAddr == Highlight);
        bCalcOutAddr = true;
        bool bSC_Amount = false;
        if(bHasOpCall && Out.nValue == 0)
        {
            CKeyID *keyid = boost::get<CKeyID>(&outAddr);
            strAddress = _("Call SC");
            if (keyid) strAddress += " " + HexStr(valtype(keyid->begin(),keyid->end()));
            bSC_Amount = true;
        }
        if(bHasOpCreate && Out.nValue == 0)
        {
            uint160 contract = uint160(LuxState::createLuxAddress(uintToh256(tx.GetHash()), j).asBytes());
            strAddress = _("Create SC") + " " + contract.ToStringReverseEndian();
            bSC_Amount = true;
        }
        if(bSC_Amount && strAddress != "")
        {
            CAmount nAmount = getTxIn(tx) - tx.GetValueOut();
            if (outAddr == Highlight && Highlight != CTxDestination()) {
                Delta += nAmount;
            }
            OutAmounts += ValueToStringShort(nAmount);
        }
        else if(strAddress != "")
        {
            if (outAddr == Highlight) {
                Delta += Out.nValue;
                OutAmounts += "<span class=\"hlo\">" + ValueToStringShort(Out.nValue) + "</span>";
            } else {
                OutAmounts += ValueToStringShort(Out.nValue);
            }
        }
        OutAddresses += strAddress;
        if (j + 1 != tx.vout.size()) {
            OutAmounts += "<br/>";
            OutAddresses += "<br/>";
        }
    }
    if(!bCalcOutAddr)
    {
        if (OutAddresses == "") {
            OutAddresses = itostr(tx.vout.size()) + " " + _("outputs (see tx)");
        }
    }

    std::string List[8] = {
            Prepend,
            "<tt>" + makeHRef(tx.GetHash().GetHex(), pSum?24:0) + "</tt>",
            InAddresses,
            InAmounts,
            OutAddresses,
            OutAmounts,
            "",
            ""
    };

    int n = sizeof(List) / sizeof(std::string) - 2;

    if (pSum) {
        List[n++] = std::string("<font color=\"") + ((Delta > 0) ? "green" : "red") + "\">" + ValueToString(Delta, true) + "</font>";
        *pSum += Delta;
        List[n++] = ValueToString(*pSum);
        return makeHTMLTableRow(List, n);
    }
    return makeHTMLTableRow(List + 1, n - 1);
}

CTxOut getPrevOut(const COutPoint& out)
{
    CTransaction tx;
    uint256 hashBlock;
    if (GetTransaction(out.hash, tx, Params().GetConsensus(), hashBlock, true))
        return tx.vout[out.n];
    return CTxOut();
}

void getNextIn(const COutPoint& Out, uint256& Hash, unsigned int& n)
{
    // Hash = 0;
    // n = 0;
    // if (paddressmap)
    //    paddressmap->ReadNextIn(Out, Hash, n);
}

const CBlockIndex* getexplorerBlockIndex(int64_t height)
{
    std::string hex = getexplorerBlockHash(height);
    uint256 hash = uint256S(hex);
    return mapBlockIndex[hash];
}

std::string getexplorerBlockHash(int64_t Height)
{
    std::string genesisblockhash = "0000037a438b746437529ddecad8f992b9e5536368686f6de24cfac13ff8acd2";
    CBlockIndex* pindexBest = mapBlockIndex[chainActive.Tip()->GetBlockHash()];
    if ((Height < 0) || (Height > pindexBest->nHeight)) {
        return genesisblockhash;
    }

    CBlock block;
    CBlockIndex* pblockindex = mapBlockIndex[chainActive.Tip()->GetBlockHash()];
    while (pblockindex->nHeight > Height)
        pblockindex = pblockindex->pprev;
    return pblockindex->GetBlockHash().GetHex();
}

static std::string BlockToString(CBlockIndex* pBlock)
{
    if (!pBlock)
        return "";

    CBlock block;
    ReadBlockFromDisk(block, pBlock, Params().GetConsensus());

    std::string TxLabels[] = {_("Hash"), _("From"), _("Amount"), _("To"), _("Amount")};
    std::string TxContent = table + "<thead>" + makeHTMLTableRow(TxLabels, sizeof(TxLabels) / sizeof(std::string)) + "</thead>";
    TxContent += "<tbody class=\"b\">";

    CAmount InVolume = 0, OutVolume = 0;
    for (size_t i = 0; i < block.vtx.size(); i++) {
        const CTransaction& tx = block.vtx[i];
        if (!tx.IsCoinBase()) {
            InVolume += getTxIn(tx);
            OutVolume += tx.GetValueOut();
        }
        TxContent += TxToRow(tx, &block);
    }
    TxContent += "</tbody></table>";

    CAmount Fees = InVolume - OutVolume;
    CAmount Generated;
    if (pBlock->nHeight == 0)
        Generated = OutVolume;
    else if (pBlock->IsProofOfStake())
        Generated = GetProofOfStakeReward(0, pBlock->nHeight);
    else
        Generated = GetProofOfWorkReward(0, pBlock->nHeight);

    std::string BlockContentCells[] = {
        _("Type"), pBlock->IsProofOfStake() ? "PoS" : "PoW",
        _("Timestamp"), TimeToString(block.nTime),
        _("Generated"), ValueToString(Generated),
        //_("Value Out"), ValueToString(OutVolume),
        _("Value In"), ValueToString(InVolume),
        _("Fees"), Fees > 0 ? ValueToString(Fees) : "-",
        _("Size"), itostr(GetSerializeSize(block, SER_NETWORK, PROTOCOL_VERSION)),
        _("Difficulty"), strprintf("%.4f", GetDifficulty(pBlock)),
        _("Bits"), "<tt>" + strprintf("%08x", block.nBits) + "</tt>",
        _("Nonce"), "<tt>" + strprintf("%08x", block.nNonce) + "</tt>",
        _("Version"), "<tt>" + strprintf("%08x", block.nVersion) + "</tt>",
        _("Hash"), "<tt>" + block.GetHash(pBlock->nHeight).GetHex() + "</tt>",
        _("Merkle Root"), "<tt>" + block.hashMerkleRoot.GetHex() + "</tt>",
        _("State Root"), "<tt>" + block.hashStateRoot.GetHex() + "</tt>",
        _("UTXO Root"), "<tt>" + block.hashUTXORoot.GetHex() + "</tt>",
        _("Number of Transactions"), itostr(block.vtx.size())
        //_("Miner Signature"), "<tt>" + block.MinerSignature.ToString() + "</tt>"
    };

    std::string BlockContent = makeHTMLTable(BlockContentCells, sizeof(BlockContentCells) / (2 * sizeof(std::string)), 2);

    std::string Content;
    Content += "<h2><a class=\"nav\" href=";
    Content += itostr(pBlock->nHeight - 1);
    Content += ">◄&nbsp;</a>";
    Content += _("Block");
    Content += " ";
    Content += itostr(pBlock->nHeight);
    Content += "<a class=\"nav\" href=";
    Content += itostr(pBlock->nHeight + 1);
    Content += ">&nbsp;►</a></h2>";
    Content += BlockContent;
    Content += "</br>";
    /*
    if (block.nHeight > getThirdHardforkBlock())
    {
        std::vector<std::string> votes[2];
        for (int i = 0; i < 2; i++)
        {
            for (unsigned int j = 0; j < block.vvotes[i].size(); j++)
            {
                votes[i].push_back(block.vvotes[i][j].hash.ToString() + ':' + itostr(block.vvotes[i][j].n));
            }
        }
        Content += "<h3>" + _("Votes +") + "</h3>";
        Content += makeHTMLTable(&votes[1][0], votes[1].size(), 1);
        Content += "</br>";
        Content += "<h3>" + _("Votes -") + "</h3>";
        Content += makeHTMLTable(&votes[0][0], votes[0].size(), 1);
        Content += "</br>";
    }
    */
    Content += "<h2>" + _("Transactions") + "</h2>";
    Content += TxContent;

    return Content;
}

static std::string TxToString(uint256 BlockHash, const CTransaction& tx)
{
    CAmount Input = 0;
    CAmount Output = tx.GetValueOut();

    std::string InputsContentCells[] = {_("#"), _("Taken from"), _("Address"), _("Amount")};
    std::string InputsContent = "<thead>"+makeHTMLTableRow(InputsContentCells, sizeof(InputsContentCells) / sizeof(std::string))+"</thead>";
    std::string OutputsContentCells[] = {_("#"), _("Redeemed in"), _("Address"), _("Amount")};
    std::string OutputsContent = "<thead>"+makeHTMLTableRow(OutputsContentCells, sizeof(OutputsContentCells) / sizeof(std::string))+"</thead>";

    InputsContent += "<tbody class=\"tx\">";
    if (tx.IsCoinBase()) {
        std::string InputsContentCells[] = {
                "0",
                "coinbase",
                "-",
                ValueToString(Output)
        };
        InputsContent += makeHTMLTableRow(InputsContentCells, sizeof(InputsContentCells) / sizeof(std::string));
    } else
        for (unsigned int i = 0; i < tx.vin.size(); i++) {
            COutPoint Out = tx.vin[i].prevout;
            CTxOut PrevOut = getPrevOut(tx.vin[i].prevout);
            if (PrevOut.nValue < 0)
                Input = -MAX_MONEY;
            else
                Input += PrevOut.nValue;
            std::string InputsContentCells[] = {
                    itostr(i),
                    "<tt>" + makeHRef(Out.hash.GetHex()) + ":" + itostr(Out.n) + "</tt>",
                    ScriptToString(PrevOut.scriptPubKey, true),
                    ValueToString(PrevOut.nValue)
            };
            InputsContent += makeHTMLTableRow(InputsContentCells, sizeof(InputsContentCells) / sizeof(std::string));
        }
    InputsContent += "</tbody>";

    uint256 TxHash = tx.GetHash();
    OutputsContent += "<tbody class=\"tx\">";
    for (unsigned int i = 0; i < tx.vout.size(); i++) {
        const CTxOut& Out = tx.vout[i];
        if (Out.nValue == 0 && tx.IsCoinStake()) continue;
        uint256 HashNext = uint256();
        unsigned int nNext = 0;
        CSpentIndexKey key(TxHash, i);
        CSpentIndexValue value;
        bool isSpent = GetSpentIndex(key, value);
        if (isSpent) {
            HashNext = value.txhash;
            nNext = value.inputIndex;
        } else if (!fSpentIndex) {
            getNextIn(COutPoint(TxHash, i), HashNext, nNext);
        }
        CAmount amount = Out.nValue;
        if (amount == 0 && (Out.scriptPubKey.HasOpCreate() || Out.scriptPubKey.HasOpCall())) {
            amount = getTxIn(tx) - tx.GetValueOut();
        }
        std::string OutputsContentCells[] = {
                itostr(i),
                (HashNext == uint256()) ? (fSpentIndex ? _("no") : _("unknown")) : "<tt>" + makeHRef(HashNext.GetHex()) + ":" + itostr(nNext) + "</tt>",
                ScriptToString(Out.scriptPubKey, true),
                ValueToString(amount)
        };
        OutputsContent += makeHTMLTableRow(OutputsContentCells, sizeof(OutputsContentCells) / sizeof(std::string));
    }
    OutputsContent += "</tbody>";

    InputsContent = table + InputsContent + "</table>";
    OutputsContent = table + OutputsContent + "</table>";

    std::string Labels[] = {
            _("In Block"), "",
            _("Timestamp"), "",
            _("Size"), itostr(GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION)),
            _("Value In"), tx.IsCoinBase() ? "-" : ValueToString(Input),
            _("Value Out"), ValueToString(Output),
            _("Fees"), tx.IsCoinGenerated() ? "-" : ValueToString(Input - Output),
            _("Hash"), "<tt>" + tx.GetWitnessHash().GetHex() + "</tt>",
    };

    CBlockIndex* pindex = LookupBlockIndex(BlockHash);
    if (pindex) {
        Labels[0 * 2 + 1] = makeHRef(itostr(pindex->nHeight));
        Labels[1 * 2 + 1] = TimeToString(pindex->nTime);
    }

    std::string Content;
    Content += "<h2>" + _("Transaction") + "&nbsp;<span>" + TxHash.GetHex() + "</span></h2>";
    Content += makeHTMLTable(Labels, sizeof(Labels) / (2 * sizeof(std::string)), 2);
    Content += "</br>";
    Content += "<h3>" + _("Inputs") + "</h3>";
    Content += InputsContent;
    Content += "</br>";
    Content += "<h3>" + _("Outputs") + "</h3>";
    Content += OutputsContent;

    return Content;
}

static std::string AddressToString(const CTxDestination& Address)
{
    std::string TxLabels[] = {
            _("Date"),
            _("Tx Hash"),
            _("From"),
            _("Amount"),
            _("To"),
            _("Amount"),
            _("Delta"),
            _("Balance")
    };

    std::string TxContent = table + "<thead>";
    TxContent += makeHTMLTableRow(TxLabels, sizeof(TxLabels) / sizeof(std::string)) + "</thead>";
    TxContent += "<tbody class=\"h\">";

    CAmount Sum = 0;
    AddressIndexVector aIndex;
    uint160 hash160 = GetHashForDestination(Address);

    std::string TxRows;
    std::string TdFull = std::string("<tr><td colspan=\"8\">");
    std::string TdEnd = std::string("</td></tr>");

    const size_t nMaxEntries = 500;

    if (hash160 == uint160())
        TxContent += TdFull + _("unhandled or invalid address") + TdEnd;
    else if (GetAddressIndex(hash160, Address.which(), aIndex))
    {
        size_t nEntries = aIndex.size();
        size_t nEntry = 0;
        int nTxSummed = 0;
        uint256 lastTxHash = 0;
        for (AddressIndexVector::const_iterator it=aIndex.begin(); it!=aIndex.end(); it++, nEntry++)
        {
            CTransaction tx;
            uint256 hashBlock = 0;
            // we need to limit max displayed (last) rows due to html perf limits of Qt
            bool displayRow = (nEntries < nMaxEntries || nEntry > nEntries - nMaxEntries);

            if (!displayRow) {
                // old history is hidden
                Sum += it->second;
                continue;
            } else if (!nTxSummed && nEntry > 1) {
                // avoid partial deltas + TxToRow sum
                for (AddressIndexVector::const_iterator rit = (aIndex.begin() + (nEntry-1)); rit!=aIndex.begin(); rit--) {
                    if (rit->first.txhash != it->first.txhash) break;
                    Sum -= rit->second;
                }
            } else if (lastTxHash == it->first.txhash) {
                // addressindex can have multiple times the same tx (PoS/Split etc)
                // TxToRow() already sums the multiple vouts deltas
                continue;
            }

            if (!GetTransaction(it->first.txhash, tx, Params().GetConsensus(), hashBlock, true))
                continue;

            time_t txTime = tx.nTime;
            if (!txTime) {
                CBlockIndex* pindex = LookupBlockIndex(hashBlock);
                if (!pindex || !chainActive.Contains(pindex))
                    continue;
                txTime = pindex->nTime;
            }

            std::string Prepend = "<a href=\"" + itostr(it->first.blockHeight) + "\">" + TimeToString(txTime) + "</a>";
            std::string TxRow = TxToRow(tx, nullptr, Address, Prepend, &Sum);
            nTxSummed++;
            // show latest ones first to avoid scrolling requierement for the current balance
            TxRows = TxRow + TxRows;

            lastTxHash = it->first.txhash;
        }
        TxContent += TxRows;
        if (nEntries > nMaxEntries)
            TxContent += TdFull + strprintf("%zu/%zu ", nMaxEntries, nEntries) + _("movements displayed.") + TdEnd;
        TxRows.clear();
    } else {
        TxContent += TdFull + _("-addressindex parameter is required to show address history") + TdEnd;
    }

    TxContent += "</tbody></table>";

    std::string Content;
    Content += "<h2>" + _("Transactions to/from") + "&nbsp;<span>" + EncodeDestination(Address) + "</span></h2>";
    Content += TxContent;
    TxContent.clear();
    return Content;
}

BlockExplorer::BlockExplorer(const PlatformStyle* platformStyle, QWidget* parent) :
    QMainWindow(parent), ui(new Ui::BlockExplorer),
    m_NeverShown(true), m_HistoryIndex(0),
    model(0)
{
    ui->setupUi(this);

    connect(ui->pushSearch, SIGNAL(released()), this, SLOT(onSearch()));
    connect(ui->content, SIGNAL(linkActivated(const QString&)), this, SLOT(goTo(const QString&)));
    connect(ui->back, SIGNAL(released()), this, SLOT(back()));
    connect(ui->pushButtonHome, SIGNAL(released()), this, SLOT(home()));
    connect(ui->forward, SIGNAL(released()), this, SLOT(forward()));

}

BlockExplorer::~BlockExplorer()
{
    delete ui;
}

void BlockExplorer::setClientModel(ClientModel *clientModel)
{

}

void BlockExplorer::keyPressEvent(QKeyEvent* event)
{
    switch ((Qt::Key)event->key()) {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        onSearch();
        return;

    default:
        return QMainWindow::keyPressEvent(event);
    }
}

void BlockExplorer::showEvent(QShowEvent* ev)
{
    if (m_NeverShown) {
        m_NeverShown = false;
        home();
        if (!GetBoolArg("-txindex", DEFAULT_TXINDEX)) {
            QString Warning = tr("Not all transactions will be shown and block explorer is not available while running in pruned mode. To view all transactions you need to set txindex=1 in the configuration file (lux.conf).");
            QMessageBox::warning(this, tr("Luxcore Blockchain Explorer"), Warning, QMessageBox::Ok);
        }
    }
    QMainWindow::showEvent(ev);
}

void BlockExplorer::home()
{
    CBlockIndex* pindexBest = mapBlockIndex[chainActive.Tip()->GetBlockHash()];

    setBlock(pindexBest);
    QString text = QString("%1").arg(pindexBest->nHeight);
    ui->searchBox->setText(text);
    while (m_History.size() > m_HistoryIndex + 1)
        m_History.pop_back();
    m_History.push_back(text);
    m_HistoryIndex = m_History.size() - 1;
    updateNavButtons();
}

bool BlockExplorer::switchTo(const QString& query)
{
    bool IsOk;
    int64_t AsInt = query.toInt(&IsOk);
    // If query is integer, get hash from height
    if (IsOk && AsInt >= 0 && AsInt <= chainActive.Height()) {
        std::string hex = getexplorerBlockHash(AsInt);
        uint256 hash = uint256S(hex);
        CBlockIndex* pIndex = mapBlockIndex[hash];
        if (pIndex) {
            setBlock(pIndex);
            return true;
        }
    }

    // If the query is not an integer, assume it is a block hash
    uint256 hash = uint256S(query.toUtf8().constData());

    CBlockIndex* pindex = LookupBlockIndex(hash);
    if (pindex) {
        setBlock(pindex);
        return true;
    }

    // If the query is neither an integer nor a block hash, assume a transaction hash
    CTransaction tx;
    uint256 hashBlock = 0;
    if (GetTransaction(hash, tx, Params().GetConsensus(), hashBlock, true)) {
        setContent(TxToString(hashBlock, tx));
        return true;
    }

    // If the query is not an integer, nor a block hash, nor a transaction hash, assume an address
    CTxDestination Address = DecodeDestination(query.toUtf8().constData());
    if (IsValidDestination(Address)) {
        std::string Content = AddressToString(Address);
        if (Content.empty())
            return false;
        setContent(Content);
        return true;
    }

    return false;
}

void BlockExplorer::goTo(const QString& query)
{
    if (switchTo(query)) {
        ui->searchBox->setText(query);
        while (m_History.size() > m_HistoryIndex + 1)
            m_History.pop_back();
        m_History.push_back(query);
        m_HistoryIndex = m_History.size() - 1;
        updateNavButtons();
    }
}

void BlockExplorer::onSearch()
{
    goTo(ui->searchBox->text());
}

void BlockExplorer::setBlock(CBlockIndex* pBlock)
{
    setContent(BlockToString(pBlock));
}

void BlockExplorer::setContent(const std::string& Content)
{
	QSettings settings;
	QString CSS;
	
	if (settings.value("theme").toString() == "dark grey" || settings.value("theme").toString() == "dark blue" ) {
		CSS = "body { font-size:12px; background-color: #A6A6A6; color: #262626; margin: 0; }\n"
			"span.hl  { color: #fff5f5; }\n"
			"span.hli { color: red; }\n"
			"span.hlo { color: green; }\n"
			"table tr td { padding: 3px; border: none; background-color: #A6A6A6; color: #262626; }\n"
			//"tbody.tx a { font-family: monospace; white-space: pre; }\n"
			"tbody.tx tr td.d3 { text-align: right; }\n"
			"tbody.h td.d3, tbody.h td.d5, tbody.h td.d6, tbody.h td.d7 { text-align: right; }\n"
			"tbody.b td.d2, tbody.b td.d4 { text-align: right; }\n"
			"thead tr td { padding: 3px; border: none; background-color: #424242; color: #fff; font-weight: bold; text-align: center; }\n"
			"thead tr td.d0 { text-align: left; }\n"
			"h2, h3 { white-space: nowrap; color: #fff; }\n"
			"a { text-decoration: none; color: #031d54; }\n"
			"a.nav { color: #fff5f5; }\n";

	} else { 
		CSS = "body { font-size:12px; background-color: white; color: #444444; margin: 0; }\n"
			"span.hl  { color: #031d54; }\n"
			"span.hli { color: red; }\n"
			"span.hlo { color: green; }\n"
			"table tr td { padding: 3px; border: none; background-color: #ffffff; color: #031d54; }\n"
			//"tbody.tx a { font-family: monospace; white-space: pre; }\n"
			"tbody.tx tr td.d3 { text-align: right; }\n"
			"tbody.h td.d3, tbody.h td.d5, tbody.h td.d6, tbody.h td.d7 { text-align: right; }\n"
			"tbody.b td.d2, tbody.b td.d4 { text-align: right; }\n"
			"thead tr td { padding: 3px; border: none; background-color: #031d54; color: #e0e0e0; font-weight: bold; text-align: center; }\n"
			"thead tr td.d0 { text-align: left; }\n"
			"h2, h3 { white-space: nowrap; color: #031d54; }\n"
			"a { text-decoration: none; color: #34A9FF; }\n"
			"a.nav { color: #031d54; }\n";
	}
    QString FullContent = "<html><head><style type=\"text/css\">" + CSS + "</style></head>" + "<body>" + Content.c_str() + "</body></html>";
    // printf(FullContent.toUtf8());
    ui->content->setText(FullContent);
}

void BlockExplorer::back()
{
    int NewIndex = m_HistoryIndex - 1;
    if (0 <= NewIndex && NewIndex < m_History.size()) {
        m_HistoryIndex = NewIndex;
        ui->searchBox->setText(m_History[NewIndex]);
        switchTo(m_History[NewIndex]);
        updateNavButtons();
    }
}

void BlockExplorer::forward()
{
    int NewIndex = m_HistoryIndex + 1;
    if (0 <= NewIndex && NewIndex < m_History.size()) {
        m_HistoryIndex = NewIndex;
        ui->searchBox->setText(m_History[NewIndex]);
        switchTo(m_History[NewIndex]);
        updateNavButtons();
    }
}

void BlockExplorer::updateNavButtons()
{
    ui->back->setEnabled(m_HistoryIndex - 1 >= 0);
    ui->forward->setEnabled(m_HistoryIndex + 1 < m_History.size());
}

