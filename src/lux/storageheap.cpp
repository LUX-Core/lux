#include "storageheap.h"
#include "uint256.h"

#include <iostream>

void StorageHeap::AddChunk(const boost::filesystem::path& path, uint64_t size)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    std::shared_ptr<StorageChunk> chunk_ptr = std::make_shared<StorageChunk>();
    chunk_ptr->path = path;
    chunk_ptr->totalSpace = size;
    chunk_ptr->freeSpace = size;
    chunks.push_back(chunk_ptr);
}

void StorageHeap::MoveChunk(size_t chunkIndex, const boost::filesystem::path &newpath)
{
    if (chunkIndex >= chunks.size()) {
        return ;
    }

    auto chunk = chunks[chunkIndex];
    chunk->path = newpath;

    for(auto &&file : chunk->files) {
        auto shortname = file->fullpath.filename();
        boost::filesystem::rename(file->fullpath, newpath / shortname);
        boost::filesystem::remove(file->fullpath);
        file->fullpath = chunk->path / shortname;
    }

    return ;
}

std::vector<std::shared_ptr<StorageChunk>> StorageHeap::GetChunks() const
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    return chunks;
};


void StorageHeap::FreeChunk(const boost::filesystem::path& path)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    for (auto it = chunks.begin(); it != chunks.end(); ++it) {
        auto files_cs = (*it)->files;
        for (auto&& file_ptr : files_cs) {
            file_ptr->cs.lock();
        }

        if ((*it)->path == path) {
            chunks.erase(it);
            break;
        }

        for (auto&& file_ptr : files_cs) {
            file_ptr->cs.unlock();
        }
    }
}

uint64_t StorageHeap::MaxAllocateSize() const
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    uint64_t nMaxAllocateSize = 0;
    for (auto&& chunk : chunks) {
        if (chunk->freeSpace > nMaxAllocateSize) {
            nMaxAllocateSize = chunk->freeSpace;
        }
    }
    return nMaxAllocateSize;
}

std::shared_ptr<AllocatedFile> StorageHeap::AllocateFile(const uint256& uri, uint64_t size)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    if (uri.size() == 0) {
        return {};
    }
    unsigned long nBestChunkIndex = chunks.size();
    uint64_t nBestChunkFreeSize = INT64_MAX;
    for (unsigned int i = 0; i < chunks.size(); ++i) {
        if (chunks[i]->freeSpace >= size && chunks[i]->freeSpace < nBestChunkFreeSize) {
            nBestChunkIndex = i;
            nBestChunkFreeSize = chunks[i]->freeSpace;
        }
    }
    if (nBestChunkIndex < chunks.size()) {
        std::shared_ptr<AllocatedFile> file_ptr = std::make_shared<AllocatedFile>();
        file_ptr->fullpath = chunks[nBestChunkIndex]->path / (uri.ToString() + "_" + std::to_string(std::time(0)) + ".luxfs");
        file_ptr->size = size;
        file_ptr->uri = uri;
        // add file to heap
        files[uri] = file_ptr;
        chunks[nBestChunkIndex]->files.push_back(file_ptr);
        chunks[nBestChunkIndex]->freeSpace -= size;
        return file_ptr;
    }
    return {};
}

void StorageHeap::FreeFile(const uint256& uri)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    for (auto&& chunk : chunks) {
        auto&& pFiles = chunk->files;
        for (auto it = pFiles.begin(); it != pFiles.end(); ++it) {
            auto file_ptr = *it;
            std::lock_guard<std::mutex> file_scoped_lock(file_ptr->cs);
            if (file_ptr->uri == uri) {
                chunk->freeSpace += file_ptr->size;
                pFiles.erase(it);
                files.erase(files.find(uri));
                return;
            }
        }
    }
}

std::shared_ptr<AllocatedFile> StorageHeap::GetFile(const uint256& uri) const
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    auto file_it = files.find(uri);
    if (file_it != files.end())
        return {file_it->second};
    return {};
}

void StorageHeap::SetDecryptionKeys(const uint256& uri, const RSAKey& rsaKey, const AESKey& aesKey)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    for (auto&& chunk : chunks) {
        for (auto&& file : chunk->files) {
            if (file->uri == uri) {
                DecryptionKeys decryptionKeys = {rsaKey, aesKey};
                file->keys = decryptionKeys;
                files[uri]->keys = decryptionKeys;
                return;
            }
        }
    }
}

void StorageHeap::SetMerkleRootHash(const uint256& uri, const uint256& merkleRootHash)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    for (auto&& chunk : chunks) {
        for (auto&& file : chunk->files) {
            if (file->uri == uri) {
                file->merkleRootHash = merkleRootHash;
                files[uri]->merkleRootHash = merkleRootHash;
                return;
            }
        }
    }
}
