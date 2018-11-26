#ifndef LUX_LIB_CRYPTO_STORAGE_HEAP_H
#define LUX_LIB_CRYPTO_STORAGE_HEAP_H

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <boost/optional.hpp>

struct AllocatedFile
{
    std::mutex cs;
    std::string filename;
    uint64_t size;
    std::string uri;
    std::string pubkey;
};


struct StorageChunk
{
    std::string path;
    std::vector<std::shared_ptr<AllocatedFile>> files;
    uint64_t totalSpace;
    uint64_t freeSpace;
};


class StorageHeap
{
protected:
    mutable std::mutex cs_dfs;
    std::vector<StorageChunk> chunks;
    std::map<std::string, std::shared_ptr<AllocatedFile>> files;

public:
    virtual ~StorageHeap() {}
    void AddChunk(const std::string& path, uint64_t size);
    void FreeChunk(const std::string& path);
    std::vector<StorageChunk> GetChunks() const;
    uint64_t MaxAllocateSize() const;
    std::shared_ptr<AllocatedFile> AllocateFile(const std::string& uri, uint64_t size);
    void FreeFile(const std::string& uri);
    std::shared_ptr<AllocatedFile> GetFile(const std::string& uri) const;
    void SetPubKey(const std::string& uri, const std::string& pubkey);
};

#endif //LUX_LIB_CRYPTO_STORAGE_HEAP_H
