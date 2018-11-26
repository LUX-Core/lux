#ifndef LUX_LIB_CRYPTO_MERKLEBLOCK_H
#define LUX_LIB_CRYPTO_MERKLEBLOCK_H

#include <fstream>
#include <vector>

template <unsigned int BITS>
class base_uint {
protected:
    enum {
        WIDTH = BITS / 32
    };
    uint32_t pn[WIDTH];
};
class uint256 : public base_uint<256>{};

void ConstructMerkleTreeLayer(std::ifstream &prevLayer, size_t size, std::ofstream &outputLayer);
void ConstructMerkleTree(std::ifstream &firstLayer, size_t size, std::ofstream &outputStream);
uint256 ConstructMerklePath(std::vector<uint256> path, size_t blocksSize, size_t branchSize, size_t pos,
                            bool readOnly, size_t &hashIdx, int depth);

#endif //LUX_LIB_CRYPTO_MERKLEBLOCK_H
