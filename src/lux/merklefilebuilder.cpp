#include "merklefilebuilder.h"

#include <openssl/sha.h>
#include <cmath>

#include "util.h"
#include "hash.h"

size_t CalcMerkleSize(size_t firstLayerBlocksNum)
{
    size_t size = 0;
    size_t layerSize = firstLayerBlocksNum;
    for (; layerSize > 1; layerSize = (layerSize / 2) + (layerSize % 2)) {
        size += layerSize;
    }
    return size + layerSize;
}

size_t CalcLayerSize(size_t firstLayerBlocksNum, size_t depth)
{
    size_t layerSize = firstLayerBlocksNum;
    const size_t maxDepth = ceil(log2(firstLayerBlocksNum)) + 1;
    const size_t height = maxDepth - depth;
    for (size_t i = 0; layerSize > 1 && i <= height; ++i) {
        layerSize = (layerSize / 2) + (layerSize % 2);
    }
    return layerSize;
}

bool ConstructMerkleTreeLayer(std::ifstream &prevLayer, size_t size, std::ofstream &outputLayer)
{
    using namespace std;

    if (!prevLayer.good() || !outputLayer.good()){
        return false;
    }
    unsigned char data[2 * SHA256_DIGEST_LENGTH];
    unsigned char hash[SHA256_DIGEST_LENGTH];
    for (uint64_t i = 1; i < size; i += 2) {
        prevLayer.read((char *)data, 2 * SHA256_DIGEST_LENGTH);
        Hash(data, 2 * SHA256_DIGEST_LENGTH, hash);
        outputLayer.write((char *)hash, SHA256_DIGEST_LENGTH);
    }

    if (size % 2) {
        prevLayer.read((char *)hash, SHA256_DIGEST_LENGTH);
        outputLayer.write((char *)hash, SHA256_DIGEST_LENGTH);
    }
    return true;
}

bool ConstructMerkleTreeFirstLayer(std::ifstream &source, size_t fileSize, std::ofstream &outputFirstLayer)
{
    return ConstructMerkleTreeLayer(source, fileSize, outputFirstLayer);
}

bool ConstructMerkleTree(const boost::filesystem::path &source, const boost::filesystem::path &dest)
{
    std::ifstream sourceStream;
    sourceStream.open(source.string(), std::ios::binary);
    if (!sourceStream.is_open()) {
        LogPrint("dfs", "%s file %s cannot be opened", __func__, source.string());
        return false;
    }
    std::ofstream outputStream;
    outputStream.open(dest.string(), std::ios::binary);
    if (!outputStream.is_open()) {
        LogPrint("dfs", "%s file %s cannot be opened", __func__, dest.string());
        return false;
    }

    auto size = boost::filesystem::file_size(source);
    if (!ConstructMerkleTreeFirstLayer(sourceStream, size, outputStream)) {
        return false;
    }

    std::ifstream prevLayer;
    prevLayer.open(dest.string(), std::ios::binary);
    if (!prevLayer.is_open()) {
        LogPrint("dfs", "%s file %s cannot be opened", __func__, dest.string());
        return false;
    }
    for (uint64_t currentSize = size; currentSize > 1; currentSize = currentSize / 2 + currentSize % 2) {
        if (!ConstructMerkleTreeLayer(prevLayer, currentSize, outputStream)) {
            sourceStream.close();
            outputStream.close();
            prevLayer.close();
            return false;
        }
    }
    sourceStream.close();
    outputStream.close();
    prevLayer.close();
    return true;
}

bool ConstructMerklePath(std::ifstream *merkleTree, size_t height,
                         std::list<uint256> &path, MerkleBuilder::MPBContext &context)
{
    size_t currentPos;
    if (height > 1) {
        if (!ConstructMerklePath(merkleTree, height - 1, path, context)) {
            return false; // never
        }
        currentPos = (context.position % 2) ? (context.position - 1) : (context.position + 1);
    } else {
        currentPos = context.position;
    }
    if (currentPos < context.layerSize) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        merkleTree->seekg(context.offsetLayer + currentPos * SHA256_DIGEST_LENGTH, merkleTree->beg);
        merkleTree->read((char *) hash, SHA256_DIGEST_LENGTH);
        std::vector<unsigned char> vch(hash, hash + sizeof(hash) / sizeof(*hash));
        if (context.position % 2) {
            path.push_front(uint256(vch));
        } else {
            path.push_back(uint256(vch));
        }
    }
    context.offsetLayer += context.layerSize * SHA256_DIGEST_LENGTH;
    context.position /= 2;
    context.layerSize = (context.layerSize / 2) + (context.layerSize % 2);
    return true;
}
