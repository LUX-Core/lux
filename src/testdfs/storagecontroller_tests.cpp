#include "../lux/storagecontroller.h"
#include "../hash.h"
#include "../util.h"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>

class TestStorageController : public StorageController
{
public:
    boost::mutex mutex;
    boost::thread background;

    void StartHandshake(const StorageProposal &proposal);
    std::shared_ptr<AllocatedFile> CreateReplica(const boost::filesystem::path &filename, const StorageOrder &order);
    void SendReplica(const StorageOrder &order, std::shared_ptr<AllocatedFile> pAllocatedFile, const CNode &pNode);
    void BackgroundJob();
};

BOOST_AUTO_TEST_SUITE(storage_controller_tests)

BOOST_AUTO_TEST_CASE(createreplica)
{
    namespace fs = boost::filesystem;
    TestStorageController controller{};
    fs::path fullFilename = GetDefaultDataDir() / "test.temp";
    fs::path defaultDfsPath = GetDefaultDataDir() / "dfs";
    if (!fs::exists(defaultDfsPath)) {
        fs::create_directories(defaultDfsPath);
    }
    controller.storageHeap.AddChunk(defaultDfsPath.string(), DEFAULT_STORAGE_SIZE);
    fs::path defaultDfsTempPath = GetDefaultDataDir() / "dfstemp";
    if (!fs::exists(defaultDfsTempPath)) {
        fs::create_directories(defaultDfsTempPath);
    }
    controller.tempStorageHeap.AddChunk(defaultDfsTempPath.string(), DEFAULT_STORAGE_SIZE);
    uint256 fileUri = SerializeHash(fullFilename.string(), SER_GETHASH, 0);
    std::string fileUriSring = "48941b144b0a86685faea4781614f9e9d6ba5587a9631e7b41515be72efff732";
    StorageOrder order = {
        std::time(0),
        std::time(0) + 86400,
        100ull * 1024 * 1024 * 1024, // 100 Gb
        fileUri,
        2,
        100,
        CService("127.0.0.1")
    };

    auto tempFile = controller.CreateReplica(fullFilename, order);

    auto length = fs::file_size(tempFile->filename);
    BOOST_CHECK_EQUAL(length, tempFile->size);
    fs::remove(tempFile->filename);
}

BOOST_AUTO_TEST_SUITE_END()
