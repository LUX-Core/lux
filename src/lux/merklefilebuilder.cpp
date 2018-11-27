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

void ConstructMerklePath(std::ifstream *replica, std::ifstream *merkleTree, size_t height,
                         std::list<uint256> &path, MPBContext &context)
{
    size_t currentPos;
    size_t currentOffset;
    std::ifstream *stream;
    if (height > 1) {
        ConstructMerklePath(replica, merkleTree, height - 1, path, context);
        currentPos = (context.position % 2) ? (context.position - 1) : (context.position + 1);
        currentOffset = context.offsetLayer + currentPos * SHA256_DIGEST_LENGTH;
        stream = merkleTree;
    } else {
        currentPos = context.position;
        currentOffset = currentPos * context.fileBlockSize;
        stream = replica;
    }
    if (currentPos < context.layerSize) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        stream->seekg(currentOffset, stream->beg);
        stream->read((char *) hash, SHA256_DIGEST_LENGTH);
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
}
