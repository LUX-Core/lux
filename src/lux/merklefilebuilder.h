#ifndef LUX_LIB_CRYPTO_MERKLEBLOCK_H
#define LUX_LIB_CRYPTO_MERKLEBLOCK_H

#include <fstream>
#include <vector>
#include "../uint256.h"

void ConstructMerkleTreeLayer(std::ifstream &prevLayer, size_t size, std::ofstream &outputLayer);
void ConstructMerkleTree(std::ifstream &firstLayer, size_t size, std::ofstream &outputStream);
size_t GetMerkleSize(size_t blocksSize);
size_t GetLayerSize(size_t blocksSize, size_t depth);
void ConstructMerklePath(std::ifstream &merkleTree, size_t blocksSize, size_t pos, std::vector<uint256> path);

#endif //LUX_LIB_CRYPTO_MERKLEBLOCK_H
