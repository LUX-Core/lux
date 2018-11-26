#include "StorageHeap.h"

void StorageHeap::AddChunk(const std::string& path, uint64_t size)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    StorageChunk chunk;
    chunk.path = path;
    chunk.totalSpace = size;
    chunk.freeSpace = size;
    chunks.push_back(chunk);
}

void StorageHeap::FreeChunk(const std::string& path)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    for (auto it = chunks.begin(); it != chunks.end(); ++it) {
        auto files_cs = it->files;
        for (auto&& file_ptr : files_cs) {
            file_ptr->cs.lock();
        }

        if (it->path == path) {
            chunks.erase(it);
            break;
        }

        for (auto&& file_ptr : files_cs) {
            file_ptr->cs.unlock();
        }
    }
}

std::vector<StorageChunk> StorageHeap::GetChunks() const
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    return chunks;
};

uint64_t StorageHeap::MaxAllocateSize() const
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    uint64_t nMaxAllocateSize = 0;
    for (auto&& chunk : chunks) {
        if (chunk.freeSpace > nMaxAllocateSize) {
            nMaxAllocateSize = chunk.freeSpace;
        }
    }
    return nMaxAllocateSize;
}

std::shared_ptr<AllocatedFile> StorageHeap::AllocateFile(const std::string& uri, uint64_t size)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    unsigned long nBestChunkIndex = chunks.size();
    uint64_t nBestChunkFreeSize = INT64_MAX;
    for (unsigned int i = 0; i < chunks.size(); ++i) {
        if (chunks[i].freeSpace >= size && chunks[i].freeSpace < nBestChunkFreeSize) {
            nBestChunkIndex = i;
            nBestChunkFreeSize = chunks[i].freeSpace;
        }
    }
    if (nBestChunkIndex < chunks.size()) {
        std::shared_ptr<AllocatedFile> file_ptr = std::make_shared<AllocatedFile>();
        // file->filename = ; // TODO: generate it
        file_ptr->size = size;
        file_ptr->uri = uri;
        // add file to heap
        files[uri] = file_ptr;
        chunks[nBestChunkIndex].files.push_back(file_ptr);
        chunks[nBestChunkIndex].freeSpace -= size;
        return file_ptr;
    }
    return {};
}

void StorageHeap::FreeFile(const std::string& uri)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    for (auto&& chunk : chunks) {
        auto&& pFiles = chunk.files;
        for (auto it = pFiles.begin(); it != pFiles.end(); ++it) {
            auto file_ptr = *it;
            std::lock_guard<std::mutex> file_scoped_lock(file_ptr->cs);
            if (file_ptr->uri == uri) {
                chunk.freeSpace += file_ptr->size;
                pFiles.erase(it);
                files.erase(files.find(uri));
                return;
            }
        }
    }
}

std::shared_ptr<AllocatedFile> StorageHeap::GetFile(const std::string& uri) const
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    auto file_it = files.find(uri);
    if (file_it != files.end())
        return {file_it->second};
    return {};
}

void StorageHeap::SetPubKey(const std::string& uri, const std::string& pubkey)
{
    std::lock_guard<std::mutex> scoped_lock(cs_dfs);
    for (auto&& chunk : chunks) {
        for (auto&& file : chunk.files) {
            if (file->uri == uri) {
                file->pubkey = pubkey;
                files[uri]->pubkey = pubkey;
                return;
            }
        }
    }
}

