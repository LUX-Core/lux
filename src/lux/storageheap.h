#ifndef LUX_LIB_CRYPTO_STORAGE_HEAP_H
#define LUX_LIB_CRYPTO_STORAGE_HEAP_H

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <boost/optional.hpp>
#include <boost/filesystem.hpp>

struct DecryptionKeys
{
    using bytes = std::vector<unsigned char>;
    bytes rsaKey;
    bytes aesKey;

    static std::string ToString(bytes data)
    {
        return std::string(data.begin(), data.end());
    }

    static bytes ToBytes(std::string data)
    {
        return bytes(data.begin(), data.end());
    }
};

struct AllocatedFile
{
    std::mutex cs;
    std::string filename; // TODO: change full path to short name, or change type to boost::filesystem::path
    uint64_t size;
    std::string uri; // TODO: change type to uint256
    DecryptionKeys keys;
};


struct StorageChunk
{
    std::string path; // TODO: change type to boost::filesystem::path
    std::vector<std::shared_ptr<AllocatedFile>> files;
    uint64_t totalSpace;
    uint64_t freeSpace;
};


class StorageHeap
{
protected:
    mutable std::mutex cs_dfs;
    std::vector<std::shared_ptr<StorageChunk>> chunks;
    std::map<std::string, std::shared_ptr<AllocatedFile>> files;

public:
    virtual ~StorageHeap() {}
    void AddChunk(const std::string& path, uint64_t size);
    void MoveChunk(size_t chunkIndex, const boost::filesystem::path &newpath);
    std::vector<std::shared_ptr<StorageChunk>> GetChunks() const;
    void FreeChunk(const std::string& path);
    uint64_t MaxAllocateSize() const;
    std::shared_ptr<AllocatedFile> AllocateFile(const std::string& uri, uint64_t size);
    void FreeFile(const std::string& uri);
    std::shared_ptr<AllocatedFile> GetFile(const std::string& uri) const;
    void SetDecryptionKeys(const std::string& uri, const std::vector<unsigned char>& rsaKey, const std::vector<unsigned char>& aesKey);
};

#endif //LUX_LIB_CRYPTO_STORAGE_HEAP_H
