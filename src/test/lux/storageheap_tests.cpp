#include "storageheap.h"

#define BOOST_TEST_MODULE Lux Lib Crypto Test

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(storage_heap_tests)

class TestStorageHeap : public StorageHeap
{
public:
    std::vector<StorageChunk>& _chunks()
    {
        return chunks;
    }

    std::map<std::string, std::shared_ptr<AllocatedFile>>& _files()
    {
        return files;
    }
};

BOOST_AUTO_TEST_CASE(add_chunk)
{
    StorageHeap heap;
    auto *pTestHeap = static_cast<TestStorageHeap*>(&heap);
    uint64_t size = 100ull * 1024 * 1024 * 1024;    // 100 Gb
    std::string path = "/path/to/some/directory";

    heap.AddChunk(path, size);

    BOOST_CHECK_EQUAL(pTestHeap->_chunks().size(), 1);
    StorageChunk& chunk = pTestHeap->_chunks()[0];
    BOOST_CHECK_EQUAL(chunk.path, path);
    BOOST_CHECK_EQUAL(chunk.totalSpace, size);
    BOOST_CHECK_EQUAL(chunk.freeSpace, size);
    BOOST_CHECK_EQUAL(chunk.files.size(), 0);
}

BOOST_AUTO_TEST_CASE(free_chunk)
{
    StorageHeap heap;
    auto *pTestHeap = static_cast<TestStorageHeap*>(&heap);
    uint64_t size = 100ull * 1024 * 1024 * 1024;    // 100 Gb
    std::string path = "/path/to/some/directory";
    heap.AddChunk(path, size);
    BOOST_CHECK_EQUAL(pTestHeap->_chunks().size(), 1);

    heap.FreeChunk(path);

    BOOST_CHECK_EQUAL(pTestHeap->_chunks().size(), 0);
}

BOOST_AUTO_TEST_CASE(get_chunks)
{
    StorageHeap heap;
    std::vector<uint64_t> sizes = {50ull * 1024 * 1024 * 1024,      // 50 Gb
                                   50ull * 1024 * 1024 * 1024};     // 50 Gb
    std::vector<std::string> paths = {"/path/to/some/directory/1",
                                      "/path/to/some/directory/2"};
    for(unsigned int i = 0; i < paths.size(); ++i) {
        heap.AddChunk(paths[i], sizes[i]);
    }

    std::vector<StorageChunk> chunks = heap.GetChunks();

    BOOST_CHECK_EQUAL(chunks.size(), paths.size());
    for(unsigned int i = 0; i < chunks.size(); ++i) {
        BOOST_CHECK_EQUAL(chunks[i].path, paths[i]);
        BOOST_CHECK_EQUAL(chunks[i].totalSpace, sizes[i]);
        BOOST_CHECK_EQUAL(chunks[i].freeSpace, sizes[i]);
        BOOST_CHECK_EQUAL(chunks[i].files.size(), 0);
    }
}

BOOST_AUTO_TEST_CASE(get_max_allocate_size)
{
    StorageHeap heap;
    std::vector<uint64_t> sizes = {40ull * 1024 * 1024 * 1024,      // 40 Gb
                                   60ull * 1024 * 1024 * 1024};     // 60 Gb
    std::vector<std::string> paths = {"/path/to/some/directory/1",
                                      "/path/to/some/directory/2"};
    for(unsigned int i = 0; i < paths.size(); ++i) {
        heap.AddChunk(paths[i], sizes[i]);
    }

    uint64_t maxAllocateSize = heap.MaxAllocateSize();

    BOOST_CHECK_EQUAL(maxAllocateSize, 60ull * 1024 * 1024 * 1024); // 60 Gb
}

BOOST_AUTO_TEST_CASE(allocate_file)
{
    StorageHeap heap;
    auto *pTestHeap = static_cast<TestStorageHeap*>(&heap);
    uint64_t size = 100ull * 1024 * 1024 * 1024;        // 100 Gb
    std::string path = "/path/to/some/directory";
    std::string strFileURI = "0xFF...some hash...FF";
    uint64_t nFileSize = 10ull * 1024 * 1024 * 1024;    // 10 Gb
    heap.AddChunk(path, size);

    heap.AllocateFile(strFileURI, nFileSize);

    BOOST_CHECK_EQUAL(pTestHeap->_chunks().size(), 1);
    StorageChunk& chunk = pTestHeap->_chunks()[0];
    BOOST_CHECK_EQUAL(chunk.files.size(), 1);
    std::shared_ptr<AllocatedFile>& file = chunk.files[0];
    BOOST_CHECK_EQUAL(file->uri, strFileURI);
    BOOST_CHECK_EQUAL(file->size, nFileSize);
    BOOST_CHECK_EQUAL(file->filename, "");
    BOOST_CHECK_EQUAL(file->pubkey, "");
    BOOST_CHECK_EQUAL(pTestHeap->_files().size(), 1);
    auto file_it = pTestHeap->_files().find(strFileURI);
    BOOST_CHECK(file_it != pTestHeap->_files().end());
    BOOST_CHECK_EQUAL(file_it->second->uri, strFileURI);
    BOOST_CHECK_EQUAL(file_it->second->size, nFileSize);
    BOOST_CHECK_EQUAL(file_it->second->filename, "");
    BOOST_CHECK_EQUAL(file_it->second->pubkey, "");
}

BOOST_AUTO_TEST_CASE(find_best_chunk)
{
    StorageHeap heap;
    auto *pTestHeap = static_cast<TestStorageHeap*>(&heap);
    std::vector<uint64_t> sizes = {40ull * 1024 * 1024 * 1024,      // 40 Gb
                                   60ull * 1024 * 1024 * 1024};     // 60 Gb
    std::vector<std::string> paths = {"/path/to/some/directory/1",
                                      "/path/to/some/directory/2"};
    std::string strFileURI = "0xFF...some hash...FF";
    uint64_t nFileSize = 10ull * 1024 * 1024 * 1024;                // 10 Gb
    for(unsigned int i = 0; i < paths.size(); ++i) {
        heap.AddChunk(paths[i], sizes[i]);
    }

    heap.AllocateFile(strFileURI, nFileSize);

    uint64_t maxAllocateSize = heap.MaxAllocateSize();
    BOOST_CHECK_EQUAL(maxAllocateSize, 60ull * 1024 * 1024 * 1024); // 60 Gb
    BOOST_CHECK_EQUAL(pTestHeap->_files().size(), 1);
    auto file_it = pTestHeap->_files().find(strFileURI);
    BOOST_CHECK(file_it != pTestHeap->_files().end());
    BOOST_CHECK_EQUAL(file_it->second->uri, strFileURI);
    BOOST_CHECK_EQUAL(file_it->second->size, nFileSize);
    BOOST_CHECK_EQUAL(file_it->second->filename, "");
    BOOST_CHECK_EQUAL(file_it->second->pubkey, "");
}

BOOST_AUTO_TEST_CASE(doesnt_allocate_oversize_file)
{
    StorageHeap heap;
    auto *pTestHeap = static_cast<TestStorageHeap*>(&heap);
    uint64_t size = 100ull * 1024 * 1024 * 1024;        // 100 Gb
    std::string path = "/path/to/some/directory";
    std::string strFileURI = "0xFF...some hash...FF";
    uint64_t nFileSize = 101ull * 1024 * 1024 * 1024;   // 101 Gb
    heap.AddChunk(path, size);

    heap.AllocateFile(strFileURI, nFileSize);

    BOOST_CHECK_EQUAL(pTestHeap->_chunks().size(), 1);
    StorageChunk& chunk = pTestHeap->_chunks()[0];
    BOOST_CHECK_EQUAL(chunk.files.size(), 0);
    BOOST_CHECK_EQUAL(pTestHeap->_files().size(), 0);
}

BOOST_AUTO_TEST_CASE(free_file)
{
    StorageHeap heap;
    auto *pTestHeap = static_cast<TestStorageHeap*>(&heap);
    uint64_t size = 100ull * 1024 * 1024 * 1024;        // 100 Gb
    std::string path = "/path/to/some/directory";
    std::string strFileURI = "0xFF...some hash...FF";
    uint64_t nFileSize = 10ull * 1024 * 1024 * 1024;    // 10 Gb
    heap.AddChunk(path, size);
    heap.AllocateFile(strFileURI, nFileSize);
    BOOST_CHECK_EQUAL(pTestHeap->_chunks().size(), 1);
    StorageChunk& chunk = pTestHeap->_chunks()[0];
    BOOST_CHECK_EQUAL(chunk.files.size(), 1);
    BOOST_CHECK_EQUAL(heap.MaxAllocateSize(), size - nFileSize);
    BOOST_CHECK_EQUAL(pTestHeap->_files().size(), 1);

    heap.FreeFile(strFileURI);

    BOOST_CHECK_EQUAL(chunk.files.size(), 0);
    BOOST_CHECK_EQUAL(heap.MaxAllocateSize(), size);
    BOOST_CHECK_EQUAL(pTestHeap->_files().size(), 0);
}

BOOST_AUTO_TEST_CASE(get_file)
{
    StorageHeap heap;
    uint64_t size = 100ull * 1024 * 1024 * 1024;        // 100 Gb
    std::string path = "/path/to/some/directory";
    std::string strFileURI = "0xFF...some hash...FF";
    uint64_t nFileSize = 10ull * 1024 * 1024 * 1024;    // 10 Gb
    heap.AddChunk(path, size);
    heap.AllocateFile(strFileURI, nFileSize);

    auto file = heap.GetFile(strFileURI);

    BOOST_CHECK_EQUAL(file->uri, strFileURI);
    BOOST_CHECK_EQUAL(file->size, nFileSize);
    BOOST_CHECK_EQUAL(file->filename, "");
    BOOST_CHECK_EQUAL(file->pubkey, "");
}

BOOST_AUTO_TEST_CASE(file_not_found)
{
    StorageHeap heap;
    uint64_t size = 100ull * 1024 * 1024 * 1024;        // 100 Gb
    std::string path = "/path/to/some/directory";
    std::string strOtherFileURI = "0xFF...wrong hash...FF";
    heap.AddChunk(path, size);

    auto file = heap.GetFile(strOtherFileURI);

    BOOST_CHECK(!file);
}

BOOST_AUTO_TEST_CASE(set_public_key_for_file)
{
    StorageHeap heap;
    uint64_t size = 100ull * 1024    * 1024 * 1024;     // 100 Gb
    std::string path = "/path/to/some/directory";
    std::string strFileURI = "0xFF...some hash...FF";
    std::string strPubKey = "0xFF...some public key hash...FF";
    uint64_t nFileSize = 10ull * 1024 * 1024 * 1024;    // 10 Gb
    heap.AddChunk(path, size);
    heap.AllocateFile(strFileURI, nFileSize);

    heap.SetPubKey(strFileURI, strPubKey);

    auto file = heap.GetFile(strFileURI);
    BOOST_CHECK_EQUAL(file->uri, strFileURI);
    BOOST_CHECK_EQUAL(file->size, nFileSize);
    BOOST_CHECK_EQUAL(file->filename, "");
    BOOST_CHECK_EQUAL(file->pubkey, strPubKey);
}

BOOST_AUTO_TEST_SUITE_END()
