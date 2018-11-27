#include <openssl/sha.h>
#include <cmath>

#include "merklefilebuilder.h"

/** Compute the 256-bit hash of a void pointer */
inline void Hash(void* in, unsigned int len, unsigned char* out)
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, in, len);
    SHA256_Final(out, &sha256);
}

void ConstructMerkleTreeLayer(std::ifstream &prevLayer, size_t size, std::ofstream &outputLayer)
{
    using namespace std;

    if (!prevLayer.good() || !outputLayer.good()){
        // throw exception
        return;
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
}

void ConstructMerkleTree(std::ifstream &firstLayer, size_t size, std::ofstream &outputStream)
{
    for (uint64_t currentSize = size; currentSize > 1; currentSize = currentSize / 2 + currentSize % 2) {
        ConstructMerkleTreeLayer(firstLayer, currentSize, outputStream);
    }
}

size_t GetMerkleSize(size_t blocksSize)
{
    size_t size = 0;
    size_t layerSize = blocksSize;
    for (; layerSize > 1; layerSize = (layerSize / 2) + (layerSize % 2)) {
        size += layerSize;
    }
    return size + layerSize;
}

size_t GetLayerSize(size_t blocksSize, size_t depth)
{
    size_t layerSize = blocksSize;
    const size_t maxDepth = ceil(log2(blocksSize)) + 1;
    const size_t height = maxDepth - depth;
    for (size_t i = 0; layerSize > 1 && i <= height; ++i) {
        layerSize = (layerSize / 2) + (layerSize % 2);
    }
    return layerSize;
}

uint256 GetHashFromMerkleLayer(std::ifstream &merkleTree, uint64_t offsetToLayer, size_t layerSize, size_t pos)
{
    size_t startPos = (pos >> 1) << 1;
    if (startPos == (layerSize - 1)) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        merkleTree.seekg(offsetToLayer + startPos * SHA256_DIGEST_LENGTH, merkleTree.beg);
        merkleTree.read((char *)hash, SHA256_DIGEST_LENGTH);
        std::vector<unsigned char> vch(hash, hash + sizeof(hash)/sizeof(*hash));
        return uint256(vch);
    }

    unsigned char data[2 * SHA256_DIGEST_LENGTH];
    unsigned char hash[SHA256_DIGEST_LENGTH];
    merkleTree.seekg(offsetToLayer + startPos * SHA256_DIGEST_LENGTH, merkleTree.beg);
    merkleTree.read((char *)data, 2 * SHA256_DIGEST_LENGTH);
    Hash(data, 2 * SHA256_DIGEST_LENGTH, hash);
    std::vector<unsigned char> vch(hash, hash + sizeof(hash)/sizeof(*hash));
    return uint256(vch);
}

void ConstructMerklePath(std::ifstream &merkleTree, size_t blocksSize, size_t pos, std::vector<uint256> path)
{
    size_t offset = 0;
    size_t currentPos = pos;
    size_t layerSize = blocksSize;
    while (layerSize > 1) {
        uint256 md = GetHashFromMerkleLayer(merkleTree, offset, layerSize, currentPos);
        offset += layerSize;
        currentPos = (currentPos / 2) + (currentPos % 2);
        layerSize = (layerSize / 2) + (layerSize % 2);
    }
}
