#include "merkler.h"

#include <openssl/sha.h>
#include <cmath>

#include "util.h"
#include "hash.h"

size_t Merkler::CalcDepth(size_t firstLayerBlocksNum)
{
    return (firstLayerBlocksNum != 0) ? ceil(log2(firstLayerBlocksNum)) : 0;
}

size_t Merkler::CalcMerkleSize(size_t firstLayerBlocksNum)
{
    size_t size = 0;
    size_t layerSize = firstLayerBlocksNum;
    for (; layerSize > 1; layerSize = layerSize / 2 + (layerSize % 2)) {
        size += layerSize;
    }
    return size + layerSize;
}


size_t Merkler::CalcFirstLayerSize(size_t fileSize)
{
    return fileSize / SHA256_DIGEST_LENGTH + ((fileSize % SHA256_DIGEST_LENGTH) != 0);
}

size_t Merkler::CalcLayerSize(size_t firstLayerBlocksNum, size_t depth)
{
    size_t layerSize = firstLayerBlocksNum;
    const size_t maxDepth = ceil(log2(firstLayerBlocksNum));
    if (maxDepth < depth) {
        return 0;
    }
    const size_t height = maxDepth - depth;
    for (size_t i = 0; layerSize > 1 && i < height; ++i) {
        layerSize = layerSize / 2 + (layerSize % 2);
    }
    return layerSize;
}

bool Merkler::ConstructMerkleTreeFirstLayer(std::istream &source, size_t size, std::ostream &dest)
{
    return ConstructMerkleTreeLayer(source, Merkler::CalcFirstLayerSize(size), dest);
}

bool Merkler::ConstructMerkleTreeLayer(std::istream &prevLayer, size_t blocksNum, std::ostream &outputLayer)
{
    using namespace std;

    if (!prevLayer.good() || !outputLayer.good()){
        return false;
    }
    unsigned char data[2 * SHA256_DIGEST_LENGTH + 1];
    unsigned char hashCh[SHA256_DIGEST_LENGTH + 1];
    for (uint64_t i = 1; i < blocksNum; i += 2) {
        prevLayer.read((char *)data, 2 * SHA256_DIGEST_LENGTH);
        Hash(data, 2 * SHA256_DIGEST_LENGTH, hashCh);
        outputLayer.write((char *)hashCh, SHA256_DIGEST_LENGTH);
    }

    if (blocksNum % 2) {
        prevLayer.read((char *)data, SHA256_DIGEST_LENGTH);
        Hash(data, SHA256_DIGEST_LENGTH, hashCh);
        outputLayer.write((char *)hashCh, SHA256_DIGEST_LENGTH);
    }
    return true;
}

uint256 Merkler::ConstructMerkleTree(const boost::filesystem::path &source, const boost::filesystem::path &dest)
{
    std::ifstream sourceStream(source.string(), std::ios::binary);
    if (!sourceStream.is_open()) {
        LogPrint("dfs", "%s file %s cannot be opened", __func__, source.string());
        return {};
    }
    std::ofstream outputStream(dest.string(), std::ios::binary);
    if (!outputStream.is_open()) {
        LogPrint("dfs", "%s file %s cannot be opened", __func__, dest.string());
        return {};
    }

    auto size = boost::filesystem::file_size(source);
    if (!ConstructMerkleTreeFirstLayer(sourceStream, size, outputStream)) {
        return {};
    }
    outputStream.flush();

    std::ifstream prevLayer(dest.string(), std::ios::binary);
    if (!prevLayer.is_open()) {
        LogPrint("dfs", "%s file %s cannot be opened", __func__, dest.string());
        return {};
    }

    auto firstLayerSize = Merkler::CalcFirstLayerSize(size);
    for (uint64_t currentSize = firstLayerSize / 2 + firstLayerSize % 2; currentSize > 1; currentSize = currentSize / 2 + currentSize % 2) {
        if (!ConstructMerkleTreeLayer(prevLayer, currentSize, outputStream)) {
            sourceStream.close();
            outputStream.close();
            prevLayer.close();
            return {};
        }
        outputStream.flush();
    }
    std::vector<unsigned char> pHash(SHA256_DIGEST_LENGTH);
    prevLayer.read((char *)pHash.data(), SHA256_DIGEST_LENGTH);
    uint256 hash{pHash};
    sourceStream.close();
    outputStream.close();
    prevLayer.close();
    return hash;
}

// Local function
bool ConstructMerklePath(std::istream &merkleTree, size_t height, std::list<uint256> &path, Merkler::MPBContext &context)
{
    if (height > 1) {
        if (!ConstructMerklePath(merkleTree, height - 1, path, context)) {
            return false;
        }
    }
    size_t currentPos = (context.position % 2) ? (context.position - 1) : (context.position + 1);
    if (currentPos < context.layerSize) {
        std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);
        merkleTree.seekg(context.offsetLayer + currentPos * SHA256_DIGEST_LENGTH, merkleTree.beg);
        merkleTree.read((char *) hash.data(), SHA256_DIGEST_LENGTH);
        if (context.position % 2) {
            path.push_front(uint256(hash));
        } else {
            path.push_back(uint256(hash));
        }
    } else {
        return false;
    }
    context.offsetLayer += context.layerSize * SHA256_DIGEST_LENGTH;
    context.position /= 2;
    context.layerSize = context.layerSize / 2 + (context.layerSize % 2);
    return true;
}


std::list<uint256> Merkler::ConstructMerklePath(const boost::filesystem::path &source, const boost::filesystem::path &merkleTree, size_t position)
{
    namespace fs = boost::filesystem;

    std::list<uint256> path = {};

    // get data from source file
    std::ifstream sourceStream(source.string(), std::ios::binary);

    {
        std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);
        sourceStream.seekg(position * SHA256_DIGEST_LENGTH, sourceStream.beg);
        sourceStream.read((char *) hash.data(), SHA256_DIGEST_LENGTH);
        path.push_back(uint256(hash));
    }
    size_t complenentPosition = (position % 2) ? (position - 1) : (position + 1);
    {
        std::vector<unsigned char> hash(SHA256_DIGEST_LENGTH);
        sourceStream.seekg(complenentPosition * SHA256_DIGEST_LENGTH, sourceStream.beg);
        sourceStream.read((char *) hash.data(), SHA256_DIGEST_LENGTH);
        if (position % 2) {
            path.push_front(uint256(hash));
        } else {
            path.push_back(uint256(hash));
        }
    }
    sourceStream.close();

    // get data from merkle tree file
    std::ifstream merkleTreeStream(merkleTree.string(), std::ios::binary);
    size_t sourceSize = fs::file_size(source);
    size_t firstLayerSize = Merkler::CalcFirstLayerSize(sourceSize);
    size_t restDepth = Merkler::CalcDepth(firstLayerSize) - 1;
    Merkler::MPBContext context{
            Merkler::CalcLayerSize(firstLayerSize, restDepth),
            0,
            position / 2
    };
    if (!ConstructMerklePath(merkleTreeStream, restDepth, path, context)) {
        return {};
    }
    merkleTreeStream.close();
    return path;
}
