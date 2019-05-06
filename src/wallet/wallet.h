// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2017 The LUX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLET_H
#define BITCOIN_WALLET_WALLET_H

#include <amount.h>
#include <policy/feerate.h>
#include <streams.h>
#include <tinyformat.h>
#include <ui_interface.h>
#include <utilstrencodings.h>
#include <validationinterface.h>
#include <script/ismine.h>
#include <script/sign.h>
#include <util.h>
#include <wallet/crypter.h>
#include <wallet/walletdb.h>
#include <wallet/rpcwallet.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

typedef CWallet* CWalletRef;
extern std::vector<CWalletRef> vpwallets;

/**
 * Settings
 */
extern CFeeRate payTxFee;
extern CAmount maxTxFee;
extern unsigned int nTxConfirmTarget;
extern bool bSpendZeroConfChange;
extern bool fWalletRbf;
extern bool bZeroBalanceAddressToken;
extern bool fSendFreeTransactions;
extern bool fPayAtLeastCustomFee;
extern bool fNotUseChangeAddress;
extern bool fWalletProcessingMode;
extern bool fCheckUpdates;
static const bool CHECKUPDATES = true;
extern bool fWalletProcessingMode;
extern bool fWalletUnlockStakingOnly;

static const unsigned int DEFAULT_KEYPOOL_SIZE = 1000;
//! -paytxfee default
static const CAmount DEFAULT_TRANSACTION_FEE = 1000;
//! -fallbackfee default
static const CAmount DEFAULT_FALLBACK_FEE = 20000;
//! -m_discard_rate default
static const CAmount DEFAULT_DISCARD_FEE = 10000;
//! -maxtxfee default
static const CAmount DEFAULT_TRANSACTION_MAXFEE = 1 * COIN;
//! minimum recommended increment for BIP 125 replacement txs
static const CAmount WALLET_INCREMENTAL_RELAY_FEE = 5000;
//! target minimum change amount
static const CAmount MIN_CHANGE = CENT;
//! final minimum change amount after paying for fees
static const CAmount MIN_FINAL_CHANGE = MIN_CHANGE/2;
//! Default for -spendzeroconfchange
static const bool DEFAULT_SPEND_ZEROCONF_CHANGE = true;
//! Default for -walletrejectlongchains
static const bool DEFAULT_WALLET_REJECT_LONG_CHAINS = false;
//! -txconfirmtarget default
static const unsigned int DEFAULT_TX_CONFIRM_TARGET = 6;
//! Default for -zerobalanceaddresstoken
static const bool DEFAULT_ZERO_BALANCE_ADDRESS_TOKEN = true;
static const bool DEFAULT_NOT_USE_CHANGE_ADDRESS = true;

//! -walletrbf default
static const bool DEFAULT_WALLET_RBF = false;
static const bool DEFAULT_WALLETBROADCAST = true;
static const bool DEFAULT_DISABLE_WALLET = false;

extern const char * DEFAULT_WALLET_DAT;

static const int64_t TIMESTAMP_MIN = 0;

class CBlockIndex;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CScheduler;
class CTxMemPool;
class CBlockPolicyEstimator;
class CWalletTx;
struct FeeCalculation;
enum class FeeEstimateMode;
class CContractBookData;
class CTokenTx;

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getwalletinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_HD = 120200, // Hierarchical key derivation after BIP32 (HD Wallet)

    FEATURE_HD_SPLIT = 139900, // Wallet with HD chain split (change outputs will use m/0'/1'/k)

    FEATURE_NO_DEFAULT_KEY = 159900, // Wallet without a default key written

    FEATURE_LATEST = 61000 // HD is optional, use FEATURE_COMPRPUBKEY as latest version
};

enum OutputType : int
{
    OUTPUT_TYPE_NONE,
    OUTPUT_TYPE_LEGACY,
    OUTPUT_TYPE_P2SH_SEGWIT,
    OUTPUT_TYPE_BECH32,

    OUTPUT_TYPE_DEFAULT = OUTPUT_TYPE_LEGACY
};

extern OutputType g_address_type;
extern OutputType g_change_type;

enum AvailableCoinsType {
    ALL_COINS = 1,
    ONLY_DENOMINATED = 2,
    ONLY_NONDENOMINATED = 3, // ONLY_NOT10000IFMN = 3,
    ONLY_NONDENOMINATED_NOTMN = 4, // ONLY_NONDENOMINATED_NOT10000IFMN = 4, ONLY_NONDENOMINATED and not 10000 LUX at the same time
};

struct CompactTallyItem {
    CTxDestination address;
    CAmount nAmount;
    std::vector<CTxIn> vecTxIn;
    CompactTallyItem()
    {
        nAmount = 0;
    }
};

/** A key pool entry */
class CKeyPool
{
public:
    int64_t nTime;
    CPubKey vchPubKey;
    bool fInternal; // for change outputs

    CKeyPool();
    CKeyPool(const CPubKey& vchPubKeyIn, bool internalIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        if (ser_action.ForRead()) {
            try {
                READWRITE(fInternal);
            }
            catch (std::ios_base::failure&) {
                /* flag as external address if we can't read the internal boolean
                   (this will be the case for any wallet before the HD chain split version) */
                fInternal = false;
            }
        }
        else {
            READWRITE(fInternal);
        }
    }
};

/** Address book data */
class CAddressBookData
{
public:
    std::string name;
    std::string purpose;

    CAddressBookData() : purpose("unknown") {}

    typedef std::map<std::string, std::string> StringMap;
    StringMap destdata;
};

struct CRecipient
{
    CScript scriptPubKey;
    CAmount nAmount;
    bool fSubtractFeeFromAmount;
};

typedef std::map<std::string, std::string> mapValue_t;


static inline void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n"))
    {
        nOrderPos = -1; // TODO: calculate elsewhere
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static inline void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry
{
    CTxDestination destination;
    CAmount amount;
    int vout;
};

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx
{
private:
  /** Constant used in hashBlock to indicate tx has been abandoned */
    static const uint256 ABANDON_HASH;

public:
    CTransactionRef tx;
    uint256 hashBlock;

    /* An nIndex == -1 means that hashBlock (in nonzero) refers to the earliest
     * block in the chain we know this or any in-wallet dependency conflicts
     * with. Older clients interpret nIndex == -1 as unconfirmed for backward
     * compatibility.
     */
    int nIndex;

    CMerkleTx()
    {
        SetTx(MakeTransactionRef());
        Init();
    }

    explicit CMerkleTx(CTransactionRef arg)
    {
        SetTx(std::move(arg));
        Init();
    }

    void Init()
    {
        hashBlock = uint256();
        nIndex = -1;
    }

    void SetTx(CTransactionRef arg)
    {
        tx = std::move(arg);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        std::vector<uint256> vMerkleBranch; // For compatibility with older versions.
        READWRITE(tx);
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    void SetMerkleBranch(const CBlockIndex* pIndex, int posInBlock);

    /**
     * Return depth of transaction in blockchain:
     * <0  : not in the blockchain, and not in memory pool (conflicted transaction)
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */
    int GetDepthInMainChain(const CBlockIndex* &pindexRet, bool enableIX = true) const;
    int GetDepthInMainChain(bool enableIX = true) const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet, enableIX); }
    bool IsInMainChain() const { const CBlockIndex *pindexRet; return GetDepthInMainChain(pindexRet) > 0; }
    int GetBlocksToMaturity() const;
    bool hashUnset() const { return (hashBlock.IsNull() || hashBlock == ABANDON_HASH); }
    bool isAbandoned() const { return (hashBlock == ABANDON_HASH); }
    void setAbandoned() { hashBlock = ABANDON_HASH; }

    const uint256& GetHash() const { return tx->GetHash(); }
    bool IsCoinBase() const { return tx->IsCoinBase(); }
    bool IsCoinStake() const { return tx->IsCoinStake(); }
};

/**
* A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
* and provides the ability to create new transactions.
*/
class CWallet final : public CCryptoKeyStore, public CValidationInterface
{
    std::atomic<bool> fAbortRescan{false};

    static std::atomic<bool> fFlushScheduled;
    CWalletDB* pwalletdbEncryption;

    //! the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion;

    //! the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nWalletMaxVersion;

    int64_t nNextResend;
    int64_t nLastResend;

    /**
    * Select a set of coins such that nValueRet >= nTargetValue and at least
    * all coins from coinControl are selected; Never select unconfirmed coins
    * if they are not ours
    */
    bool SelectCoins(const std::string &account, const CAmount& nTargetValue, std::set<std::pair<const CWalletTx*, unsigned int> >& setCoinsRet, CAmount& nValueRet, const CCoinControl* coinControl = NULL, AvailableCoinsType coin_type = ALL_COINS, bool useIX = true) const;

    /**
     * Used to keep track of spent outpoints, and
     * detect and report conflicts (double-spends or
     * mutated transactions where the mutant gets mined).
     */
    typedef std::multimap<COutPoint, uint256> TxSpends;
    TxSpends mapTxSpends;
    void AddToSpends(const COutPoint& outpoint, const uint256& wtxid);
    void AddToSpends(const uint256& wtxid);

    void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>);

public:
    std::atomic<bool> fScanningWallet{false}; // controlled by WalletRescanReserver
    std::mutex mutexScanning;

    bool MintableCoins();
    bool SelectCoinsDark(int64_t nValueMin, int64_t nValueMax, std::vector<CTxIn>& setCoinsRet, int64_t& nValueRet, int nDarksendRoundsMin, int nDarksendRoundsMax) const;
    bool SelectCoinsByDenominations(int nDenom, int64_t nValueMin, int64_t nValueMax, std::vector<CTxIn>& vCoinsRet, std::vector<COutput>& vCoinsRet2, int64_t& nValueRet, int nDarksendRoundsMin, int nDarksendRoundsMax);
    bool SelectCoinsDarkDenominated(int64_t nTargetValue, std::vector<CTxIn>& setCoinsRet, int64_t& nValueRet) const;
    std::vector<uint256> vStakingWalletUpdated;
    bool HasCollateralInputs(bool fOnlyConfirmed = true) const;
    bool IsCollateralAmount(int64_t nInputAmount) const;
    int CountInputsWithAmount(int64_t nInputAmount);

    bool SelectCoinsCollateral(std::vector<CTxIn>& setCoinsRet, int64_t& nValueRet) const;

    void LoadKeyPool(int64_t nIndex, const CKeyPool &keypool);

    /*
     * Main wallet lock.
     * This lock protects all the fields added by CWallet
     *   except for:
     *      fFileBacked (immutable after instantiation)
     *      strWalletFile (immutable after instantiation)
     */
    mutable CCriticalSection cs_wallet;

    bool fFileBacked;
    bool fWalletUnlockAnonymizeOnly;
    std::string strWalletFile;

    //std::set<int64_t> setKeyPool;
    std::set<int64_t> setInternalKeyPool;
    std::set<int64_t> setExternalKeyPool;
    int64_t m_max_keypool_index = 0;
    std::map<CKeyID, int64_t> m_pool_key_to_index;
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID;

    //MultiSend
    std::vector<std::pair<std::string, int> > vMultiSend;
    bool fMultiSendStake;
    bool fMultiSendMasternodeReward;
    bool fMultiSendNotify;
    std::string strMultiSendChangeAddress;
    int nLastMultiSendHeight;
    std::vector<std::string> vDisabledAddresses;

    //Auto Combine Inputs
    bool fCombineDust;
    CAmount nAutoCombineThreshold;

    std::map<std::string, CLuxNodeConfig> mapMyLuxNodes;
    bool AddLuxNodeConfig(CLuxNodeConfig nodeConfig);

    CWallet()
    {
        SetNull();
    }

    CWallet(std::string strWalletFileIn)
    {
        SetNull();

        strWalletFile = strWalletFileIn;
        fFileBacked = true;
    }

    ~CWallet()
    {
        delete pwalletdbEncryption;
    }

    void SetNull()
    {
        nWalletVersion = FEATURE_BASE;
        nWalletMaxVersion = FEATURE_BASE;
        fFileBacked = false;
        nMasterKeyMaxID = 0;
        pwalletdbEncryption = NULL;
        nOrderPosNext = 0;
        nNextResend = 0;
        nLastResend = 0;
        nTimeFirstKey = 0;
        fWalletUnlockAnonymizeOnly = false;

        //MultiSend
        vMultiSend.clear();
        fMultiSendStake = false;
        fMultiSendMasternodeReward = false;
        fMultiSendNotify = false;
        strMultiSendChangeAddress = "";
        nLastMultiSendHeight = 0;
        vDisabledAddresses.clear();

        //Auto Combine Dust
        fCombineDust = false;
        nAutoCombineThreshold = 0;
    }

    bool isMultiSendEnabled()
    {
        return fMultiSendMasternodeReward || fMultiSendStake;
    }

    void setMultiSendDisabled()
    {
        fMultiSendMasternodeReward = false;
        fMultiSendStake = false;
    }

    std::map<uint256, CWalletTx> mapWallet;

    int64_t nOrderPosNext;
    std::map<uint256, int> mapRequestCount;

    std::map<CTxDestination, CAddressBookData> mapAddressBook;

    std::map<std::string, CContractBookData> mapContractBook;

    CPubKey vchDefaultKey;

    std::set<COutPoint> setLockedCoins;

    int64_t nTimeFirstKey;

    std::map<uint256, CTokenInfo> mapToken;

    std::map<uint256, CTokenTx> mapTokenTx;

    const CWalletTx* GetWalletTx(const uint256& hash) const;

    //! check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf)
    {
        AssertLockHeld(cs_wallet);
        return nWalletMaxVersion >= wf;
    }
    
    void AvailableCoinsMN(std::vector<COutput>& vCoins, bool fOnlyConfirmed=true, const CCoinControl *coinControl = NULL, AvailableCoinsType coin_type=ALL_COINS, bool useIX = false) const;
    std::map<CTxDestination, std::vector<COutput> > AvailableCoinsByAddress(bool fConfirmed = true, CAmount maxCoinValue = 0);
    bool SelectCoinsMinConf(const std::string &account, const CAmount& nTargetValue, int nConfMine, int nConfTheirs, std::vector<COutput> vCoins, std::set<std::pair<const CWalletTx*, unsigned int> >& setCoinsRet, CAmount& nValueRet) const;

    /// Get 1000DASH output and keys which can be used for the Masternode
    bool GetMasternodeVinAndKeys(CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet, std::string strTxHash = "", std::string strOutputIndex = "");
    /// Extract txin information and keys from output
    bool GetVinAndKeysFromOutput(COutput out, CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet);

    bool IsSpent(const uint256& hash, unsigned int n) const;

    bool IsLockedCoin(uint256 hash, unsigned int n) const;
    void LockCoin(COutPoint& output);
    void UnlockCoin(COutPoint& output);
    void UnlockAllCoins();
    void ListLockedCoins(std::vector<COutPoint>& vOutpts);
    int64_t GetTotalValue(std::vector<CTxIn> vCoins);

     /*
     * Rescan abort properties
     */
    void AbortRescan() { fAbortRescan = true; }
    bool IsAbortingRescan() { return fAbortRescan; }

    /**
     * keystore implementation
     * Generate a new key
     */
    CPubKey GenerateNewKey(bool internal = false);
    //! Adds a key to the store, and saves it to disk.
    bool AddKeyPubKey(const CKey& key, const CPubKey& pubkey);
    //! Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key, const CPubKey& pubkey) { return CCryptoKeyStore::AddKeyPubKey(key, pubkey); }
    //! Load metadata (used by LoadWallet)
    bool LoadKeyMetadata(const CPubKey& pubkey, const CKeyMetadata& metadata);

    bool LoadMinVersion(int nVersion)
    {
        AssertLockHeld(cs_wallet);
        nWalletVersion = nVersion;
        nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion);
        return true;
    }

    //! Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret) override;
    //! Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey& vchPubKey, const std::vector<unsigned char>& vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript) override;
    bool LoadCScript(const CScript& redeemScript);

    //! Adds a destination data tuple to the store, and saves it to disk
    bool AddDestData(const CTxDestination& dest, const std::string& key, const std::string& value);
    //! Erases a destination data tuple in the store and on disk
    bool EraseDestData(const CTxDestination& dest, const std::string& key);
    //! Adds a destination data tuple to the store, without saving it to disk
    bool LoadDestData(const CTxDestination& dest, const std::string& key, const std::string& value);
    //! Look up a destination data tuple in the store, return true if found false otherwise
    bool GetDestData(const CTxDestination& dest, const std::string& key, std::string* value) const;

    //! Adds a watch-only address to the store, and saves it to disk.
    bool AddWatchOnly(const CScript& dest);
    bool RemoveWatchOnly(const CScript& dest);
    //! Adds a watch-only address to the store, without saving it to disk (used by LoadWallet)
    bool LoadWatchOnly(const CScript& dest);

    bool Unlock(const SecureString& strWalletPassphrase, bool anonimizeOnly = false);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    void GetKeyBirthTimes(std::map<CKeyID, int64_t>& mapKeyBirth) const;

    bool LoadToken(const CTokenInfo &token);

    bool LoadTokenTx(const CTokenTx &tokenTx);

    //! Adds a contract data tuple to the store, without saving it to disk
    bool LoadContractData(const std::string &address, const std::string &key, const std::string &value);

    /**
     * Increment the next transaction order id
     * @return next transaction order id
     */
    int64_t IncOrderPosNext(CWalletDB* pwalletdb = NULL);
    bool GetAccountDestination(CTxDestination &dest, std::string strAccount, bool bForceNew = false);

    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair> TxItems;

    /**
     * Get the wallet's activity log
     * @return multimap of ordered transactions and accounting entries
     * @warning Returned pointers are *only* valid within the scope of passed acentries
     */
    TxItems OrderedTxItems(std::list<CAccountingEntry>& acentries, std::string strAccount = "");

    void MarkDirty();
    bool AddToWallet(const CWalletTx& wtxIn, bool fFromLoadWallet = false, bool fFlushOnClose=true);
    void SyncTransaction(const CTransaction& tx, const CBlock* pblock);
    bool AddToWalletIfInvolvingMe(const CTransaction& tx, const CBlock* pblock, bool fUpdate);
    void EraseFromWallet(const uint256& hash);
    int ScanForWalletTransactions(CBlockIndex* pindexStart, bool fUpdate = false);
    void ReacceptWalletTransactions();
    void ResendWalletTransactions();
    CAmount GetBalance() const;
    CAmount GetUnconfirmedBalance() const;
    CAmount GetImmatureBalance() const;
    CAmount GetAnonymizableBalance() const;
    CAmount GetAnonymizedBalance() const;
    double GetAverageAnonymizedRounds() const;
    CAmount GetNormalizedAnonymizedBalance() const;
    CAmount GetDenominatedBalance(bool unconfirmed/*=false*/) const;
    CAmount GetDenominatedBalance(bool onlyDenom/*=true*/, bool onlyUnconfirmed/*=false*/) const;
    CAmount GetWatchOnlyBalance() const;
    CAmount GetUnconfirmedWatchOnlyBalance() const;
    CAmount GetImmatureWatchOnlyBalance() const;
    OutputType TransactionChangeType(const std::vector<std::pair<CScript, CAmount> >& vecSend);
    bool CreateTransaction(const std::vector<std::pair<CScript, CAmount> >& vecSend,
                           CWalletTx& wtxNew,
                           CReserveKey& reservekey,
                           CAmount& nFeeRet,
                           std::string& strFailReason,
                           const CCoinControl* coinControl = NULL,
                           AvailableCoinsType coin_type = ALL_COINS,
                           bool useIX = false,
                           CAmount nFeePay = 0, CAmount nGasFee=0);
    bool CreateTransaction(CScript scriptPubKey, const CAmount& nValue, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, std::string& strFailReason, const CCoinControl* coinControl = NULL, AvailableCoinsType coin_type = ALL_COINS, bool useIX = false, CAmount nFeePay = 0);
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey, std::string strCommand = "tx");
    std::string PrepareDarksendDenominate(int minRounds, int maxRounds);
    bool CreateCollateralTransaction(CMutableTransaction& txCollateral, std::string& strReason);
    bool ConvertList(std::vector<CTxIn> vCoins, std::vector<int64_t>& vecAmounts);
    bool MultiSend();
    void AutoCombineDust();

    bool AddTokenEntry(const CTokenInfo &token, bool fFlushOnClose=true);

    bool AddTokenTxEntry(const CTokenTx& tokenTx, bool fFlushOnClose=true);

    bool GetTokenTxDetails(const CTokenTx &wtx, uint256& credit, uint256& debit, std::string& tokenSymbol, uint8_t& decimals) const;

    bool IsTokenTxMine(const CTokenTx &wtx) const;

    bool RemoveTokenEntry(const uint256& tokenHash, bool fFlushOnClose=true);

    static CFeeRate minTxFee;
    static CAmount GetMinimumFee(unsigned int nTxBytes, unsigned int nConfirmTarget, const CTxMemPool& pool);

    bool NewKeyPool();
    size_t KeypoolCountExternalKeys();
    bool TopUpKeyPool(unsigned int kpSize = 0);
    bool ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, bool internal);
    void KeepKey(int64_t nIndex);
    void ReturnKey(int64_t nIndex, bool fInternal, const CPubKey& pubkey);
    bool GetKeyFromPool(CPubKey &key, bool internal = false);
    int64_t GetOldestKeyPoolTime();
    void GetAllReserveKeys(std::set<CKeyID>& setAddress) const;

    std::set<std::set<CTxDestination> > GetAddressGroupings();
    std::map<CTxDestination, CAmount> GetAddressBalances();

    std::set<CTxDestination> GetAccountAddresses(const std::string &strAccount) const;

    bool GetBudgetSystemCollateralTX(CTransaction& tx, uint256 hash, bool useIX);
    bool GetBudgetSystemCollateralTX(CWalletTx& tx, uint256 hash, bool useIX);

    // get the DarkSend chain depth for a given input
    int GetRealInputDarkSendRounds(CTxIn in, int rounds) const;
    // respect current settings
    int GetInputDarkSendRounds(CTxIn in) const;

    bool IsDenominated(const CTxIn& txin) const;
    bool IsDenominated(const CTransaction& tx) const;
    bool IsDenominatedAmount(int64_t nInputAmount) const;

    isminetype IsMine(const CTxIn& txin) const;
    isminetype IsMine(const CTxOut& txout) const { return ::IsMine(*this, txout.scriptPubKey); }
    bool IsMine(const CTransaction& tx) const
    {
        for (const CTxOut& txout : tx.vout)
        if (IsMine(txout) != ISMINE_NO) return true;
        return false;
    }

    /** should probably be renamed to IsRelevantToMe */
    bool IsFromMe(const CTransaction& tx) const
    {
        return (GetDebit(tx, ISMINE_ALL) > 0);
    }

    CAmount GetDebit(const CTxIn& txin, const isminefilter& filter) const;
    CAmount GetDebit(const CTransaction& tx, const isminefilter& filter) const
    {
        CAmount nDebit = 0;
        for (const CTxIn& txin : tx.vin) {
            nDebit += GetDebit(txin, filter);
            if (!MoneyRange(nDebit))
                throw std::runtime_error("CWallet::GetDebit() : value out of range");
        }
        return nDebit;
    }

    CAmount GetCredit(const CTxOut& txout, const isminefilter& filter) const
    {
        if (!MoneyRange(txout.nValue))
            throw std::runtime_error("CWallet::GetCredit() : value out of range");
        return ((IsMine(txout) & filter) ? txout.nValue : 0);
    }
    CAmount GetCredit(const CTransaction& tx, const isminefilter& filter) const
    {
        CAmount nCredit = 0;
        for (const CTxOut& txout : tx.vout) {
            nCredit += GetCredit(txout, filter);
            if (!MoneyRange(nCredit))
                throw std::runtime_error("CWallet::GetCredit() : value out of range");
        }
        return nCredit;
    }

    bool IsChange(const CTxOut& txout) const;

    CAmount GetChange(const CTxOut& txout) const
    {
        if (!MoneyRange(txout.nValue))
            throw std::runtime_error("CWallet::GetChange() : value out of range");
        return (IsChange(txout) ? txout.nValue : 0);
    }
    CAmount GetChange(const CTransaction& tx) const
    {
        CAmount nChange = 0;
        for (const CTxOut& txout : tx.vout) {
            nChange += GetChange(txout);
            if (!MoneyRange(nChange))
                throw std::runtime_error("CWallet::GetChange() : value out of range");
        }
        return nChange;
    }

    void SetBestChain(const CBlockLocator& loc);

    DBErrors LoadWallet(bool& fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);

    bool SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& purpose);

    bool DelAddressBook(const CTxDestination& address);

    bool SetContractBook(const std::string& strAddress, const std::string& strName, const std::string& strAbi);

    bool DelContractBook(const std::string& strAddress);

    bool UpdatedTransaction(const uint256& hashTx);

    void Inventory(const uint256& hash)
    {
        LOCK(cs_wallet);
        std::map<uint256, int>::iterator mi = mapRequestCount.find(hash);
        if (mi != mapRequestCount.end()) (*mi).second++;
    }

    unsigned int GetKeyPoolSize()
    {
        AssertLockHeld(cs_wallet); // setKeyPool
        return setInternalKeyPool.size() + setExternalKeyPool.size();
    }

    bool SetDefaultKey(const CPubKey& vchPubKey);

    //! signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    bool SetMinVersion(enum WalletFeature, CWalletDB* pwalletdbIn = NULL, bool fExplicit = false);

    //! change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

    //! get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion()
    {
        LOCK(cs_wallet);
        return nWalletVersion;
    }

    //! Get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> GetConflicts(const uint256& txid) const;

    /**
     * Address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void(CWallet* wallet, const CTxDestination& address, const std::string& label, bool isMine, const std::string& purpose, ChangeType status)> NotifyAddressBookChanged;

    /**
     * Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */

    /** Wallet transaction added, removed or updated. */
    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashToken,
                                  ChangeType status)> NotifyTokenChanged;

    boost::signals2::signal<void(CWallet* wallet, const uint256& hashTx, ChangeType status)> NotifyTransactionChanged;

    /** Contract book entry changed. */
    boost::signals2::signal<void (CWallet *wallet, const std::string &address,
            const std::string &label, const std::string &abi,
            ChangeType status)> NotifyContractBookChanged;

    /**
     * Wallet token transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashTx,
                                  ChangeType status)> NotifyTokenTransactionChanged;


    /** Show progress e.g. for rescan */
    boost::signals2::signal<void(const std::string& title, int nProgress)> ShowProgress;

    /** Watch-only address added */
    boost::signals2::signal<void(bool fHaveWatchOnly)> NotifyWatchonlyChanged;

    /**
     * Explicitly make the wallet learn the related scripts for outputs to the
     * given key. This is purely to make the wallet file compatible with older
     * software, as CBasicKeyStore automatically does this implicitly for all
     * keys now.
     */
    void LearnRelatedScripts(const CPubKey& key, OutputType);

    /**
     * Same as LearnRelatedScripts, but when the OutputType is not known (and could
     * be anything).
     */
    void LearnAllRelatedScripts(const CPubKey& key);

    bool GetStakeWeight(uint64_t& nWeight);

    /**
     * Get a destination of the requested type (if possible) to the specified script.
     * This function will automatically add the necessary scripts to the wallet.
     */
    CTxDestination AddAndGetDestinationForScript(const CScript& script, OutputType);

    void GetScriptForMining(std::shared_ptr<CReserveScript> &script);
};

/**
 * A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    /**
     * Key/value map with information about the transaction.
     *
     * The following keys can be read and written through the map and are
     * serialized in the wallet database:
     *
     *     "comment", "to"   - comment strings provided to sendtoaddress,
     *                         sendfrom, sendmany wallet RPCs
     *     "replaces_txid"   - txid (as HexStr) of transaction replaced by
     *                         bumpfee on transaction created by bumpfee
     *     "replaced_by_txid" - txid (as HexStr) of transaction created by
     *                         bumpfee on transaction replaced by bumpfee
     *     "from", "message" - obsolete fields that could be set in UI prior to
     *                         2011 (removed in commit 4d9b223)
     *
     * The following keys are serialized in the wallet database, but shouldn't
     * be read or written through the map (they will be temporarily added and
     * removed from the map during serialization):
     *
     *     "fromaccount"     - serialized strFromAccount value
     *     "n"               - serialized nOrderPos value
     *     "timesmart"       - serialized nTimeSmart value
     *     "spent"           - serialized vfSpent value that existed prior to
     *                         2014 (removed in commit 93a18a3)
     */
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived; //!< time received by this node
    /**
     * Stable timestamp that never changes, and reflects the order a transaction
     * was added to the wallet. Timestamp is based on the block time for a
     * transaction added as part of a block, or else the time when the
     * transaction was received if it wasn't part of a block, with the timestamp
     * adjusted in both cases so timestamp order matches the order transactions
     * were added to the wallet. More details can be found in
     * CWallet::ComputeTimeSmart().
     */
    unsigned int nTimeSmart;
    /**
     * From me flag is set to 1 for transactions that were created by the wallet
     * on this bitcoin node, and set to 0 for transactions that were created
     * externally and came in through the network or sendrawtransaction RPC.
     */
    char fFromMe;
    std::string strFromAccount;
    int64_t nOrderPos; //!< position in ordered transaction list
    std::multimap<int64_t, std::pair<CWalletTx*, CAccountingEntry*>>::const_iterator m_it_wtxOrdered;

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fImmatureCreditCached;
    mutable bool fAvailableCreditCached;
    mutable bool fAnonymizableCreditCached;
    mutable bool fAnonymizedCreditCached;
    mutable bool fDenomUnconfCreditCached;
    mutable bool fDenomConfCreditCached;
    mutable bool fWatchDebitCached;
    mutable bool fWatchCreditCached;
    mutable bool fImmatureWatchCreditCached;
    mutable bool fAvailableWatchCreditCached;
    mutable bool fChangeCached;
    mutable bool fInMempool;
    mutable CAmount nDebitCached;
    mutable CAmount nCreditCached;
    mutable CAmount nImmatureCreditCached;
    mutable CAmount nAvailableCreditCached;
    mutable CAmount nAnonymizableCreditCached;
    mutable CAmount nAnonymizedCreditCached;
    mutable CAmount nDenomUnconfCreditCached;
    mutable CAmount nDenomConfCreditCached;
    mutable CAmount nWatchDebitCached;
    mutable CAmount nWatchCreditCached;
    mutable CAmount nImmatureWatchCreditCached;
    mutable CAmount nAvailableWatchCreditCached;
    mutable CAmount nChangeCached;

    CWalletTx()
    {
        Init(nullptr);
    }

    CWalletTx(const CWallet* pwalletIn, CTransactionRef arg) : CMerkleTx(std::move(arg))
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        strFromAccount.clear();
        fDebitCached = false;
        fCreditCached = false;
        fImmatureCreditCached = false;
        fAvailableCreditCached = false;
        fAnonymizableCreditCached = false;
        fAnonymizedCreditCached = false;
        fDenomUnconfCreditCached = false;
        fDenomConfCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fChangeCached = false;
        fInMempool = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nImmatureCreditCached = 0;
        nAvailableCreditCached = 0;
        nAnonymizableCreditCached = 0;
        nAnonymizedCreditCached = 0;
        nDenomUnconfCreditCached = 0;
        nDenomConfCreditCached = 0;
        nWatchDebitCached = 0;
        nWatchCreditCached = 0;
        nAvailableWatchCreditCached = 0;
        nImmatureWatchCreditCached = 0;
        nChangeCached = 0;
        nOrderPos = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (ser_action.ForRead())
            Init(nullptr);
        char fSpent = false;

        if (!ser_action.ForRead()) {
            mapValue["fromaccount"] = strFromAccount;

            WriteOrderPos(nOrderPos, mapValue);

            if (nTimeSmart)
                mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        READWRITE(*(CMerkleTx*)this);
        std::vector<CMerkleTx> vUnused; //!< Used to be vtxPrev
        READWRITE(vUnused);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (ser_action.ForRead()) {
            strFromAccount = mapValue["fromaccount"];

            ReadOrderPos(nOrderPos, mapValue);

            nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(mapValue["timesmart"]) : 0;
        }

        mapValue.erase("fromaccount");
        mapValue.erase("version");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
    }

    //! make sure balances are recalculated
    void MarkDirty()
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fAnonymizableCreditCached = false;
        fAnonymizedCreditCached = false;
        fDenomUnconfCreditCached = false;
        fDenomConfCreditCached = false;
        fImmatureCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void BindWallet(CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

    //! filter decides which addresses will count towards the debit
    CAmount GetDebit(const isminefilter& filter) const;
    CAmount GetCredit(const isminefilter& filter) const;
    CAmount GetImmatureCredit(bool fUseCache=true) const;
    CAmount GetAvailableCredit(bool fUseCache=true) const;
    CAmount GetImmatureWatchOnlyCredit(const bool fUseCache=true) const;
    CAmount GetAvailableWatchOnlyCredit(const bool fUseCache=true) const;
    CAmount GetChange() const;

//    // Get the marginal bytes if spending the specified output from this transaction
//    int GetSpendSize(unsigned int out) const
//    {
//        return CalculateMaximumSignedInputSize(tx->vout[out], pwallet);
//    }

    void GetAmounts(std::list<COutputEntry>& listReceived,
                    std::list<COutputEntry>& listSent, CAmount& nFee, std::string& strSentAccount, const isminefilter& filter) const;

    void GetAccountAmounts(const std::string& strAccount, CAmount& nReceived, CAmount& nSent, CAmount& nFee, const isminefilter& filter) const;

    bool IsFromMe(const isminefilter& filter) const
    {
        return (GetDebit(filter) > 0);
    }

    // True if only scriptSigs are different
    bool IsEquivalentTo(const CWalletTx& tx) const;

    bool InMempool() const;
    bool IsTrusted() const;

    int64_t GetTxTime() const;

    // RelayWalletTransaction may only be called if fBroadcastTransactions!
    bool RelayWalletTransaction(CConnman* connman);

    /** Pass this transaction to the mempool. Fails if absolute fee exceeds absurd fee. */
    bool AcceptToMemoryPool(const CAmount& nAbsurdFee, CValidationState& state);

    std::set<uint256> GetConflicts() const;
};

class COutput
{
public:
    const CWalletTx *tx;
    int i;
    int nDepth;

    /** Whether we have the private keys to spend this output */
    bool fSpendable;

    /** Whether we know how to spend this output, ignoring the lack of keys */
    bool fSolvable;

    /**
     * Whether this output is considered safe to spend. Unconfirmed transactions
     * from outside keys and unconfirmed replacement transactions are considered
     * unsafe and will not be used to fund new spending transactions.
     */
    bool fSafe;

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn, bool fSpendableIn, bool fSolvableIn, bool fSafeIn)
    {
        tx = txIn; i = iIn; nDepth = nDepthIn; fSpendable = fSpendableIn; fSolvable = fSolvableIn; fSafe = fSafeIn;
    }

    //Used with DarkSend. Will return largest nondenom, then denominations, then very small inputs
    int Priority() const;

    std::string ToString() const;
};




/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
    //! todo: add something to note what created it (user, getnewaddress, change)
    //!   maybe should have a map<string, string> property map

    explicit CWalletKey(int64_t nExpires=0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(LIMITED_STRING(strComment, 65536));
    }
};

/**
 * Internal transfers.
 * Database key is acentry<account><counter>.
 */
class CAccountingEntry
{
public:
    std::string strAccount;
    CAmount nCreditDebit;
    int64_t nTime;
    std::string strOtherAccount;
    std::string strComment;
    mapValue_t mapValue;
    int64_t nOrderPos; //!< position in ordered transaction list
    uint64_t nEntryNo;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
        nEntryNo = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        //! Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(LIMITED_STRING(strOtherAccount, 65536));

        if (!ser_action.ForRead())
        {
            WriteOrderPos(nOrderPos, mapValue);

            if (!(mapValue.empty() && _ssExtra.empty()))
            {
                CDataStream ss(s.GetType(), s.GetVersion());
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                strComment.append(ss.str());
            }
        }

        READWRITE(LIMITED_STRING(strComment, 65536));

        size_t nSepPos = strComment.find("\0", 0, 1);
        if (ser_action.ForRead())
        {
            mapValue.clear();
            if (std::string::npos != nSepPos)
            {
                CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()), s.GetType(), s.GetVersion());
                ss >> mapValue;
                _ssExtra = std::vector<char>(ss.begin(), ss.end());
            }
            ReadOrderPos(nOrderPos, mapValue);
        }
        if (std::string::npos != nSepPos)
            strComment.erase(nSepPos);

        mapValue.erase("n");
    }

private:
    std::vector<char> _ssExtra;
};

/** A key allocated from the key pool. */
class CReserveKey final : public CReserveScript
{
protected:
    CWallet* pwallet;
    int64_t nIndex;
    CPubKey vchPubKey;
    bool fInternal;
public:
    explicit CReserveKey(CWallet* pwalletIn)
    {
        nIndex = -1;
        pwallet = pwalletIn;
        fInternal = false;
    }

    CReserveKey() = default;
    CReserveKey(const CReserveKey&) = delete;
    CReserveKey& operator=(const CReserveKey&) = delete;

    ~CReserveKey()
    {
        ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey, bool internal = false);
    void KeepKey();
    void KeepScript() override { KeepKey(); }
};


/** 
 * Account information.
 * Stored in wallet with key "acc"+string account name.
 */
class CAccount
{
public:
    CPubKey vchPubKey;

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey = CPubKey();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
    }
};

/** RAII object to check and reserve a wallet rescan */
class WalletRescanReserver
{
private:
    CWalletRef m_wallet;
    bool m_could_reserve;
public:
    explicit WalletRescanReserver(CWalletRef w) : m_wallet(w), m_could_reserve(false) {}

    bool reserve()
    {
        assert(!m_could_reserve);
        std::lock_guard<std::mutex> lock(m_wallet->mutexScanning);
        if (m_wallet->fScanningWallet) {
            return false;
        }
        m_wallet->fScanningWallet = true;
        m_could_reserve = true;
        return true;
    }

    bool isReserved() const
    {
        return (m_could_reserve && m_wallet->fScanningWallet);
    }

    ~WalletRescanReserver()
    {
        std::lock_guard<std::mutex> lock(m_wallet->mutexScanning);
        if (m_could_reserve) {
            m_wallet->fScanningWallet = false;
        }
    }
};

/** Contract book data */
class CContractBookData
{
public:
    std::string name;
    std::string abi;

    CContractBookData()
    {}
};

class CTokenInfo
{
public:
    static const int CURRENT_VERSION=1;
    int nVersion;
    std::string strContractAddress;
    std::string strTokenName;
    std::string strTokenSymbol;
    uint8_t nDecimals;
    std::string strSenderAddress;
    int64_t nCreateTime;
    uint256 blockHash;
    int64_t blockNumber;

    CTokenInfo()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
        {
            READWRITE(nVersion);
            READWRITE(nCreateTime);
            READWRITE(strTokenName);
            READWRITE(strTokenSymbol);
            READWRITE(blockHash);
            READWRITE(blockNumber);
        }
        READWRITE(nDecimals);
        READWRITE(strContractAddress);
        READWRITE(strSenderAddress);
    }

    void SetNull()
    {
        nVersion = CTokenInfo::CURRENT_VERSION;
        nCreateTime = 0;
        strContractAddress = "";
        strTokenName = "";
        strTokenSymbol = "";
        nDecimals = 0;
        strSenderAddress = "";
        blockHash.SetNull();
        blockNumber = -1;
    }

    uint256 GetHash() const;
};

class CTokenTx
{
public:
    static const int CURRENT_VERSION=1;
    int nVersion;
    std::string strContractAddress;
    std::string strSenderAddress;
    std::string strReceiverAddress;
    uint256 nValue;
    uint256 transactionHash;
    int64_t nCreateTime;
    uint256 blockHash;
    int64_t blockNumber;
    std::string strLabel;

    CTokenTx()
    {
        SetNull();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        if (!(nType & SER_GETHASH))
        {
            READWRITE(nVersion);
            READWRITE(nCreateTime);
            READWRITE(blockHash);
            READWRITE(blockNumber);
            READWRITE(LIMITED_STRING(strLabel, 65536));
        }
        READWRITE(strContractAddress);
        READWRITE(strSenderAddress);
        READWRITE(strReceiverAddress);
        READWRITE(nValue);
        READWRITE(transactionHash);
    }

    void SetNull()
    {
        nVersion = CTokenTx::CURRENT_VERSION;
        nCreateTime = 0;
        strContractAddress = "";
        strSenderAddress = "";
        strReceiverAddress = "";
        nValue.SetNull();
        transactionHash.SetNull();
        blockHash.SetNull();
        blockNumber = -1;
        strLabel = "";
    }

    uint256 GetHash() const;
};

#endif // BITCOIN_WALLET_WALLET_H
