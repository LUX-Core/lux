#include <uint256.h>
#include <primitives/transaction.h>
#include <libethereum/State.h>
#include <libethereum/Transaction.h>
#include "util.h"

using logEntriesSerializ = std::vector<std::pair<dev::Address, std::pair<dev::h256s, dev::bytes>>>;

struct TransactionReceiptInfo{
    uint256 blockHash;
    uint32_t blockNumber;
    uint256 transactionHash;
    uint32_t transactionIndex;
    dev::Address from;
    dev::Address to;
    uint64_t cumulativeGasUsed;
    uint64_t gasUsed;
    dev::Address contractAddress;
    dev::eth::LogEntries logs;
    dev::eth::TransactionException excepted;
};

struct TransactionReceiptInfoSerialized{
    std::vector<dev::h256> blockHashes;
    std::vector<uint32_t> blockNumbers;
    std::vector<dev::h256> transactionHashes;
    std::vector<uint32_t> transactionIndexes;
    std::vector<dev::h160> senders;
    std::vector<dev::h160> receivers;
    std::vector<dev::u256> cumulativeGasUsed;
    std::vector<dev::u256> gasUsed;
    std::vector<dev::h160> contractAddresses;
    std::vector<logEntriesSerializ> logs;
    std::vector<uint32_t> excepted;
};

class StorageResults{

public:

	StorageResults(std::string const& _path);
    ~StorageResults();

	void addResult(dev::h256 hashTx, std::vector<TransactionReceiptInfo>& result);

    void deleteResults(std::vector<CTransaction> const& txs);

    std::vector<TransactionReceiptInfo> getResult(dev::h256 const& hashTx);

	void commitResults();

    void clearCacheResult();

    void wipeResults();

private:

	bool readResult(dev::h256 const& _key, std::vector<TransactionReceiptInfo>& _result);

	logEntriesSerializ logEntriesSerialization(dev::eth::LogEntries const& _logs);

	dev::eth::LogEntries logEntriesDeserialize(logEntriesSerializ const& _logs);

	std::string path;

    leveldb::DB* db;

    leveldb::Options options;

	std::unordered_map<dev::h256, std::vector<TransactionReceiptInfo>> m_cache_result;
};
