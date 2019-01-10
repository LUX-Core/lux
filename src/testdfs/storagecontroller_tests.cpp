#include "../lux/storagecontroller.h"
#include "../hash.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(storage_controller_tests)

BOOST_AUTO_TEST_CASE(createreplica)
{
    boost::filesystem::path fullFilename = "/home/user/test.temp";
    uint256 fileUri = SerializeHash(fullFilename.string(), SER_GETHASH, 0);
    std::string fileUriSring = "48941b144b0a86685faea4781614f9e9d6ba5587a9631e7b41515be72efff732";
    StorageController controller;
    StorageOrder order = {
        std::time(0),
        std::time(0) + 86400,
        100l * 1024 * 1024 * 1024, // 100 Gb
        fileUri,
        2,
        100,
        CService("127.0.0.1")
    };
    StorageProposal proposal = {
        std::time(0) + 10,
        order.GetHash(),
        1,
        CService("127.0.0.1")
    };

    controller.CreateReplica(fullFilename, order, proposal);
//
//    BOOST_CHECK_EQUAL(,);
}

BOOST_AUTO_TEST_SUITE_END()
