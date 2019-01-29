#include "../lux/storagecontroller.h"
#include "../hash.h"
#include "../util.h"

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>

class TestStorageController : public StorageController
{
public:

    TestStorageController()
    {
        rate = STORAGE_MIN_RATE;
        maxblocksgap = DEFAULT_STORAGE_MAX_BLOCK_GAP;
    }

    bool StartHandshake(const StorageProposal &proposal, const DecryptionKeys &keys)
    {
        return StorageController::StartHandshake(proposal, keys);
    }

    DecryptionKeys GenerateKeys(RSA **rsa)
    {
        return StorageController::GenerateKeys(rsa);
    }

    RSA* CreatePublicRSA(const std::string &key)
    {
        return StorageController::CreatePublicRSA(key);
    }

    std::shared_ptr<AllocatedFile> CreateReplica(const boost::filesystem::path &filename,
                                                 const StorageOrder &order,
                                                 const DecryptionKeys &keys,
                                                 RSA *rsa)
    {
        return StorageController::CreateReplica(filename, order, keys, rsa);
    }
    // bool SendReplica(const StorageOrder &order, std::shared_ptr<AllocatedFile> pAllocatedFile, CNode* pNode);
    // bool DecryptReplica(std::shared_ptr<AllocatedFile> pAllocatedFile, const uint64_t fileSize, const boost::filesystem::path &decryptedFile);
};

BOOST_AUTO_TEST_SUITE(storage_controller_tests)

BOOST_AUTO_TEST_CASE(createreplica)
{
    namespace fs = boost::filesystem;
    TestStorageController controller{};
    fs::path fullFilename = boost::filesystem::current_path() / "test.test";
    std::ofstream out(fullFilename.string());
    out << "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678";
    out.close();
    fs::path defaultDfsPath = boost::filesystem::current_path() / "dfs_test";
    fs::path defaultDfsTempPath = boost::filesystem::current_path() / "dfstemp_test";
    controller.InitStorages(defaultDfsPath, defaultDfsTempPath);// TODO: need test for this (SS)
    uint256 fileUri = SerializeHash(fullFilename.filename().string(), SER_GETHASH, 0);
    std::string fileUriString = "48941b144b0a86685faea4781614f9e9d6ba5587a9631e7b41515be72efff732";
    StorageOrder order = {
        std::time(0),
        std::time(0) + 86400,
        fullFilename.filename().string(),
        fs::file_size(fullFilename),
        fileUri,
        2,
        100,
        CService("127.0.0.1")
    };
    RSA *rsa;
    DecryptionKeys keys = controller.GenerateKeys(&rsa);// TODO: need test for this (SS)

    auto tempFile = controller.CreateReplica(fullFilename, order, keys, rsa);

    auto length = fs::file_size(tempFile->fullpath);
    BOOST_CHECK_EQUAL(length, tempFile->size);
    fs::remove(fullFilename);
    fs::remove(tempFile->fullpath);
    fs::remove_all(defaultDfsPath);
    fs::remove_all(defaultDfsTempPath);
}

BOOST_AUTO_TEST_SUITE_END()
