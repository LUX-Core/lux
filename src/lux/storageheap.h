#ifndef LUX_LIB_CRYPTO_STORAGE_HEAP_H
#define LUX_LIB_CRYPTO_STORAGE_HEAP_H

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>
#include "decryptionkeys.h"

class uint256;

struct AllocatedFile
{
    std::mutex cs;
    boost::filesystem::path fullpath;
    uint64_t size;
    uint256 uri;
    uint256 merkleRootHash;
    DecryptionKeys keys;
};


struct StorageChunk
{
    boost::filesystem::path path;
    std::vector<std::shared_ptr<AllocatedFile>> files;
    uint64_t totalSpace;
    uint64_t freeSpace;
};


class StorageHeap
{
protected:
    mutable std::mutex cs_dfs;
    std::vector<std::shared_ptr<StorageChunk>> chunks;
    std::map<uint256, std::shared_ptr<AllocatedFile>> files;

public:
    virtual ~StorageHeap() {}
    void AddChunk(const boost::filesystem::path& path, uint64_t size);
    void MoveChunk(size_t chunkIndex, const boost::filesystem::path &newpath);
    std::vector<std::shared_ptr<StorageChunk>> GetChunks() const;
    void FreeChunk(const boost::filesystem::path& path);
    uint64_t MaxAllocateSize() const;
    std::shared_ptr<AllocatedFile> AllocateFile(const uint256& uri, uint64_t size);
    void FreeFile(const uint256& uri);
    std::shared_ptr<AllocatedFile> GetFile(const uint256& uri) const;
    void SetDecryptionKeys(const uint256& uri, const RSAKey& rsaKey, const AESKey& aesKey);
    void SetMerkleRootHash(const uint256& uri, const uint256& merkleRootHash); // TODO: add tests (SS)
};

#endif //LUX_LIB_CRYPTO_STORAGE_HEAP_H
