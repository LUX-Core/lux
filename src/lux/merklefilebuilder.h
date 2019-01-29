#ifndef LUX_LIB_CRYPTO_MERKLEFILEBUILDER_H
#define LUX_LIB_CRYPTO_MERKLEFILEBUILDER_H

#include <fstream>
#include <list>
#include <boost/filesystem.hpp>

#include "uint256.h"

namespace MerkleBuilder {
    struct MPBContext { // MerklePathBuilderContext
        size_t layerSize;
        size_t offsetLayer;
        size_t position;
    };

    size_t CalcMerkleSize(size_t firstLayerBlocksNum);
    size_t CalcLayerSize(size_t firstLayerBlocksNum, size_t depth);
    bool ConstructMerkleTreeFirstLayer(std::ifstream &source, size_t fileSize, std::ofstream &dest);
    bool ConstructMerkleTreeLayer(std::ifstream &prevLayer, size_t size, std::ofstream &outputLayer);
    bool ConstructMerkleTree(const boost::filesystem::path &source, const boost::filesystem::path &dest);
    bool ConstructMerklePath(std::ifstream &merkleTree, size_t height,
                             std::list<uint256> &path, MPBContext &context);
} // namespace MerkleBuilder

#endif //LUX_LIB_CRYPTO_MERKLEFILEBUILDER_H
