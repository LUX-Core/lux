#ifndef LUX_LIB_CRYPTO_MERKLER_H
#define LUX_LIB_CRYPTO_MERKLER_H

#include <fstream>
#include <list>
#include <boost/filesystem.hpp>

#include "uint256.h"

namespace Merkler {
    struct MPBContext { // MerklePathBuilderContext
        size_t layerSize;
        size_t offsetLayer;
        size_t position;
    };

    size_t CalcDepth(size_t firstLayerBlocksNum);
    size_t CalcMerkleSize(size_t firstLayerBlocksNum);
    size_t CalcFirstLayerSize(size_t fileSize);
    size_t CalcLayerSize(size_t firstLayerBlocksNum, size_t depth);
    bool ConstructMerkleTreeFirstLayer(std::istream &source, size_t size, std::ostream &dest);
    bool ConstructMerkleTreeLayer(std::istream &prevLayer, size_t blocksNum, std::ostream &outputLayer);
    uint256 ConstructMerkleTree(const boost::filesystem::path &source, const boost::filesystem::path &dest);
    std::list<uint256> ConstructMerklePath(const boost::filesystem::path &source, const boost::filesystem::path &merkleTree, size_t position);
} // namespace Merkler

#endif //LUX_LIB_CRYPTO_MERKLER_H
