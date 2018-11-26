#include <openssl/sha.h>
#include <cmath>

#include "MerkleBlock.h"

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
    for (uint64_t i = 1; i < size; i+=2) {
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


uint256 ConstructMerklePath(std::vector<uint256> path, size_t blocksSize, size_t branchSize, size_t pos,
                            bool readOnly, size_t &hashIdx, int depth)
{
    if (hashIdx >= path.size()) {
        return {};
    }
    size_t maxDepth = blocksSize / 2 + blocksSize % 2;
    if (readOnly || depth == maxDepth) {
        return path[hashIdx++]; // ?
    }
    uint256 md;
//    uint256 hashes[2];
//    const size_t leftBranchSize = maxDepth - depth - 1;
//    const size_t rightBranchSize = branchSize - leftBranchSize;
//    const bool leftReadOnly = pos >= leftBranchSize;
//    const bool rightReadOnly = !leftBranchSize;
//    hashes[0] = ConstructMerklePath(path, blocksSize, leftBranchSize, pos, leftReadOnly, hashIdx, depth + 1);
//    hashes[1] = ConstructMerklePath(path, blocksSize, rightBranchSize, pos, rightReadOnly, hashIdx, depth + 1);
//    if (hashes[0] != uint256{} && hashes[0] != hashes[1]) {
//        if (hashes[1] == uint256{}) {
//            hashes[1] = hashes[0];
//        }
//        Hash(hashes, sizeof(hashes), md);
//    } else {
//        hashIdx = SIZE_MAX;
//    }
    return md;
}
