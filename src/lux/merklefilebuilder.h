#ifndef LUX_LIB_CRYPTO_MERKLEBLOCK_H
#define LUX_LIB_CRYPTO_MERKLEBLOCK_H

#include <fstream>
#include <list>
#include "uint256.h"

struct MPBContext { // MerklePathBuilderContext
    size_t layerSize;
    size_t offsetLayer;
    size_t position;
};

void ConstructMerkleTreeLayer(std::ifstream &prevLayer, size_t size, std::ofstream &outputLayer);
void ConstructMerkleTree(std::ifstream &firstLayer, size_t size, std::ofstream &outputStream);
size_t GetMerkleSize(size_t blocksSize);
size_t GetLayerSize(size_t blocksSize, size_t depth);
void ConstructMerklePath(std::ifstream &merkleTree, size_t height,
                         std::list<uint256> &path, MPBContext &context);

#endif //LUX_LIB_CRYPTO_MERKLEBLOCK_H
