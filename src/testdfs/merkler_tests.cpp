#include "../lux/merkler.h"
#include "../hash.h"
#include "../util.h"

#include <boost/test/unit_test.hpp>
#include <boost/filesystem.hpp>
#include <fstream>


BOOST_AUTO_TEST_SUITE(merkler_tests)

BOOST_AUTO_TEST_CASE(calcdepth)
{
    int testsNumber = 2;
    std::vector<size_t> filesSizes       = {0, 10};
    std::vector<size_t> expectedDepthes  = {0, 4}; // layers length: 10, 5, 3, 2, 1

    for (int i = 0; i < testsNumber; ++i) {
        auto depth = Merkler::CalcDepth(filesSizes[i]);
        BOOST_CHECK_EQUAL(depth, expectedDepthes[i]);
    }
}

BOOST_AUTO_TEST_CASE(calcmerklesize)
{
    int testsNumber = 2;
    std::vector<size_t> blocksNumbers       = {0, 10};
    std::vector<size_t> expectedMerkleSize  = {0, 21}; // merkle size = 10 + 5 + 3 + 2 + 1

    for (int i = 0; i < testsNumber; ++i) {
        auto merkleSize = Merkler::CalcMerkleSize(blocksNumbers[i]);
        BOOST_CHECK_EQUAL(merkleSize, expectedMerkleSize[i]);
    }
}

BOOST_AUTO_TEST_CASE(calcfirstlayersize)
{
    int testsNumber = 5;
    std::vector<size_t> filesSizes          = {0, 128, 256, 384, 512};
    std::vector<size_t> expectedLayerSize   = {0, 4,   8,   12,  16};

    for (int i = 0; i < testsNumber; ++i) {
        auto layerSize = Merkler::CalcFirstLayerSize(filesSizes[i]);
        BOOST_CHECK_EQUAL(layerSize, expectedLayerSize[i]);
    }
}

BOOST_AUTO_TEST_CASE(calclayersize)
{
    int testsNumber = 8;
    std::vector<size_t> blocksNumbers       = {0, 0, 10, 10, 10, 10, 10, 10};
    std::vector<size_t> depths              = {0, 1, 0,  1,  2,  3,  4,  5};
    std::vector<size_t> expectedLayerSize   = {0, 0, 1,  2,  3,  5,  10, 0};

    for (int i = 0; i < testsNumber; ++i) {
        auto layerSize = Merkler::CalcLayerSize(blocksNumbers[i], depths[i]);
        BOOST_CHECK_EQUAL(layerSize, expectedLayerSize[i]);
    }
}

BOOST_AUTO_TEST_CASE(constructmerkletreefirstlayer)
{
    namespace fs = boost::filesystem;
    std::vector<std::string> expectedHexData{"55ad8512add4a0a8bc720b6b42545ca980543196ee637acb50ecd35e96916467",
                                             "6ebc21df543e9e0468960a6802d94aa82ecf9dca5e22208829ef3e613ce1e0c5"};
    fs::path dataFile = boost::filesystem::current_path() / "test.test";
    fs::path merkle_layerFile = boost::filesystem::current_path() / "merkle.layer";
    // TODO: if exists then remove files (SS)
    {
        std::ofstream out(dataFile.string());
        out << "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678"; // 128 bytes = size 1 RSA encrypt data
    }
    {
        std::fstream in;
        std::fstream merkleLayerFile;
        in.open(dataFile.string(), std::ios::binary|std::ios::in);
        merkleLayerFile.open(merkle_layerFile.string(), std::ios::binary|std::ios::out);
        const size_t size = fs::file_size(dataFile);

        Merkler::ConstructMerkleTreeFirstLayer(in, size, merkleLayerFile);
    }
    {
        std::ifstream result;
        result.open(merkle_layerFile.string(), std::ios::binary);
        const size_t numHashes = fs::file_size(merkle_layerFile) / SHA256_DIGEST_LENGTH;
        std::vector<unsigned char> pHash(SHA256_DIGEST_LENGTH);
        for (size_t i = 0; i < numHashes; ++i) {
            result.read((char *)pHash.data(), SHA256_DIGEST_LENGTH);
            uint256 hash{pHash};
            BOOST_CHECK_EQUAL(hash.ToString(), expectedHexData[i]);
        }
    }
    fs::remove(dataFile);
    fs::remove(merkle_layerFile);
}

BOOST_AUTO_TEST_CASE(constructmerkletreelayer)
{
    namespace fs = boost::filesystem;
    std::vector<std::string> data{"55ad8512add4a0a8bc720b6b42545ca980543196ee637acb50ecd35e96916467",
                                  "6ebc21df543e9e0468960a6802d94aa82ecf9dca5e22208829ef3e613ce1e0c5"};
    std::vector<std::string> expectedHexData{"78a0a1ccf07fb8159d391b256b057d9b0e119c3803e5c7d3d5af92922cdb73ee"};
    fs::path dataFile = boost::filesystem::current_path() / "merkle1.layer";
    fs::path merkle_layerFile = boost::filesystem::current_path() / "merkle2.layer";
    // TODO: if exists then remove files (SS)
    {
        std::ofstream out(dataFile.string());
        for (auto hexHash: data) {
            uint256 hash;
            hash.SetHex(hexHash);
            hash.Serialize(out, 0, PROTOCOL_VERSION);
        }
    }
    {
        std::fstream in;
        std::fstream merkleLayerFile;
        in.open(dataFile.string(), std::ios::binary|std::ios::in);
        merkleLayerFile.open(merkle_layerFile.string(), std::ios::binary|std::ios::out);
        const size_t numHashes = fs::file_size(dataFile) / SHA256_DIGEST_LENGTH;

        Merkler::ConstructMerkleTreeLayer(in, numHashes, merkleLayerFile);
    }
    {
        std::ifstream result;
        result.open(merkle_layerFile.string(), std::ios::binary);
        const size_t numHashes = fs::file_size(merkle_layerFile) / SHA256_DIGEST_LENGTH;
        std::vector<unsigned char> pHash(SHA256_DIGEST_LENGTH);
        for (size_t i = 0; i < numHashes; ++i) {
            result.read((char *)pHash.data(), SHA256_DIGEST_LENGTH);
            uint256 hash{pHash};
            BOOST_CHECK_EQUAL(hash.ToString(), expectedHexData[i]);
        }
    }
    fs::remove(dataFile);
    fs::remove(merkle_layerFile);
}

BOOST_AUTO_TEST_CASE(constructmerkletree)
{
    namespace fs = boost::filesystem;
    std::string expectedHash{"78a0a1ccf07fb8159d391b256b057d9b0e119c3803e5c7d3d5af92922cdb73ee"};
    fs::path dataFile = boost::filesystem::current_path() / "test.test";
    fs::path merkle_treeFile = boost::filesystem::current_path() / "merkle.tree";
    // TODO: if exists then remove files (SS)
    {
        std::ofstream out(dataFile.string());
        out << "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678"; // 128 bytes = size 1 RSA encrypt data
    }

    uint256 merkleRoot = Merkler::ConstructMerkleTree(dataFile, merkle_treeFile);

    BOOST_CHECK_EQUAL(merkleRoot.ToString(), expectedHash);
    fs::remove(dataFile);
    fs::remove(merkle_treeFile);
}

BOOST_AUTO_TEST_CASE(constructmerklepath)
{
    namespace fs = boost::filesystem;
    std::string expectedHash{"78a0a1ccf07fb8159d391b256b057d9b0e119c3803e5c7d3d5af92922cdb73ee"};
    fs::path dataFile = boost::filesystem::current_path() / "test.test";
    fs::path merkle_treeFile = boost::filesystem::current_path() / "merkleForPath.tree";
    std::vector<std::list<uint256>> expectedPaths = {
        {
            uint256{"3231303938373635343332313039383736353433323130393837363534333231"},
            uint256{"3433323130393837363534333231303938373635343332313039383736353433"},
            uint256{"6ebc21df543e9e0468960a6802d94aa82ecf9dca5e22208829ef3e613ce1e0c5"}
        },
        {
            uint256{"3231303938373635343332313039383736353433323130393837363534333231"},
            uint256{"3433323130393837363534333231303938373635343332313039383736353433"},
            uint256{"6ebc21df543e9e0468960a6802d94aa82ecf9dca5e22208829ef3e613ce1e0c5"}
        },
        {
            uint256{"55ad8512add4a0a8bc720b6b42545ca980543196ee637acb50ecd35e96916467"},
            uint256{"3635343332313039383736353433323130393837363534333231303938373635"},
            uint256{"3837363534333231303938373635343332313039383736353433323130393837"}
        },
        {
            uint256{"55ad8512add4a0a8bc720b6b42545ca980543196ee637acb50ecd35e96916467"},
            uint256{"3635343332313039383736353433323130393837363534333231303938373635"},
            uint256{"3837363534333231303938373635343332313039383736353433323130393837"}
        }
    };

    // TODO: if exists then remove files (SS)
    {
        std::ofstream out(dataFile.string());
        out << "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678"; // 128 bytes = size 1 RSA encrypt data
    }
    uint256 merkleRoot = Merkler::ConstructMerkleTree(dataFile, merkle_treeFile);

    for (size_t i = 0; i < 4; ++i) {
        std::list<uint256> path = Merkler::ConstructMerklePath(dataFile, merkle_treeFile, i);

        auto expectedPath_it = expectedPaths[i].begin();
        for (hash: path) {
            BOOST_CHECK_EQUAL(hash.ToString(), expectedPath_it->ToString());
            ++expectedPath_it;
        }
    }
    fs::remove(dataFile);
    fs::remove(merkle_treeFile);
}

BOOST_AUTO_TEST_SUITE_END()
