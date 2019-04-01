#include "storagecontroller.h"

#include "cancelingsettimeout.h"
#include "compat.h"
#include "main.h"
#include "merkler.h"
#include "primitives/transaction.h"
#include "replicabuilder.h"
#include "serialize.h"
#include "streams.h"
#include "txdb.h"
#include "util.h"

#include <fstream>
#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

std::unique_ptr<StorageController> storageController;

struct ReplicaStream
{
    const uint64_t BUFFER_SIZE = 4 * 1024;
    std::fstream filestream;
    DecryptionKeys keys;
    uint256 currentOrderHash;
    uint256 merkleRootHash;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        std::vector<char> buf(BUFFER_SIZE);

        READWRITE(currentOrderHash);
        READWRITE(merkleRootHash);
        READWRITE(keys);
        const auto *order = storageController->GetOrder(currentOrderHash);
        if (order) {
            const auto fileSize = GetCryptoReplicaSize(order->fileSize);
            if (!ser_action.ForRead()) {
                for (auto i = 0u; i < fileSize;) {
                    uint64_t n = std::min(BUFFER_SIZE, fileSize - i);
                    buf.resize(n);
                    filestream.read(&buf[0], n);  // TODO: change to loop of readsome
                    if (buf.empty()) {
                        break;
                    }
                    READWRITE(buf);
                    i += buf.size();
                }
            } else {
                for (auto i = 0u; i < fileSize;) {
                    READWRITE(buf);
                    filestream.write(&buf[0], buf.size());
                    i += buf.size();
                }
            }
        }
    }
};

void StorageController::InitStorages(const boost::filesystem::path &dataDir, const boost::filesystem::path &tempDataDir)
{
    namespace fs = boost::filesystem;

    if (!fs::exists(dataDir)) {
        fs::create_directories(dataDir);
    }
    boost::lock_guard<boost::mutex> lock(mutex);
    storageHeap.AddChunk(dataDir, DEFAULT_STORAGE_SIZE);
    if (!fs::exists(tempDataDir)) {
        fs::create_directories(tempDataDir);
    }
    tempStorageHeap.AddChunk(tempDataDir, DEFAULT_STORAGE_SIZE);
}

void StorageController::InitDB(size_t nCacheSize, bool fMemory, bool fWipe)
{
    if (db) delete db;
    db = new CFileStorageDB(nCacheSize, fMemory, fWipe);
}

void StorageController::ProcessStorageMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isStorageCommand)
{
    namespace fs = boost::filesystem;

    if (strCommand == "dfsannounce") {
        isStorageCommand = true;
        StorageOrder order;
        vRecv >> order;
        uint256 hash = order.GetHash();
        if (!GetOrder(hash)) {
            AnnounceOrder(order);
            uint64_t storageHeapSize = 0;
            uint64_t tempStorageHeapSize = 0;
            {
                boost::lock_guard<boost::mutex> lock(mutex);
                storageHeapSize = storageHeap.MaxAllocateSize();
                tempStorageHeapSize = tempStorageHeap.MaxAllocateSize();
            }
            if (storageHeapSize > order.fileSize &&
                tempStorageHeapSize > order.fileSize &&
                order.maxRate >= rate &&
                order.maxGap >= maxblocksgap) {
                StorageProposal proposal { std::time(nullptr), hash, rate, address };

                CNode* pNode = TryConnectToNode(order.address, 2);
                if (pNode) {
                    pNode->PushMessage("dfsproposal", proposal);
                } else {
                    pfrom->PushMessage("dfsproposal", proposal);
                }

            }
        }
    } else if (strCommand == "dfshandshake") {
        isStorageCommand = true;
        StorageHandshake handshake;
        vRecv >> handshake;
        const auto *order = GetOrder(handshake.orderHash);
        if (order) {
            uint64_t storageHeapSize = 0;
            uint64_t tempStorageHeapSize = 0;
            {
                boost::lock_guard<boost::mutex> lock(mutex);
                storageHeapSize = storageHeap.MaxAllocateSize();
                tempStorageHeapSize = tempStorageHeap.MaxAllocateSize();
            }
            if (storageHeapSize > order->fileSize && tempStorageHeapSize > order->fileSize) {
                StorageHandshake requestReplica { std::time(nullptr), handshake.orderHash, handshake.proposalHash, DEFAULT_DFS_PORT };
                handshakeAgent.Add(handshake);
                CNode* pNode = FindNode(order->address);

                if (pNode) {
                    pNode->PushMessage("dfsrr", requestReplica);
                } else {
                    LogPrint("dfs", "\"dfshandshake\" message handler have not connection to order sender");
                    pfrom->PushMessage("dfsrr", requestReplica);
                }
            }
        } else {
            // DoS prevention
//            CNode* pNode = FindNode(proposal.address);
//            if (pNode) {
//                CNodeState *state = State(pNode); // CNodeState was declared in main.cpp (SS)
//                state->nMisbehavior += 10;
//            }
        }
    } else if (strCommand == "dfsalr") { // dfs announcements list request
        std::vector <CInv> vInv;
        size_t counter = 0;
        if (pfrom->nVersion < ActiveProtocol()) {
            LogPrint("dfs", "protocol version \"%s\" is not active", pfrom->nVersion);
            return ;
        }
        for (auto &&pair : mapAnnouncements) {
            CInv inv(MSG_STORAGE_ORDER_ANNOUNCE, pair.first);
            vInv.push_back(inv);
            if (counter >= MAX_ANNOUNCEMETS_SIZE) {
                pfrom->PushMessage("inv", vInv);
                vInv.clear();
                counter = 0;
            } else {
                counter++;
            }
        }
        if (counter > 0) {
            pfrom->PushMessage("inv", vInv);
        }
    } else if (strCommand == "dfsping") {
        isStorageCommand = true;
        pfrom->PushMessage("dfspong", pfrom->addr);
    } else if (strCommand == "dfspong") {
        isStorageCommand = true;
        vRecv >> address;
        address.SetPort(GetListenPort());
        lastCheckIp = std::time(nullptr);
        pfrom->PushMessage("dfsalr");
    } else if (strCommand == "dfsproposal") {
        isStorageCommand = true;
        StorageProposal proposal;
        vRecv >> proposal;
        const auto *order = GetOrder(proposal.orderHash);
        if (order) {
            std::vector<ProposalHash> vListenProposals;
            {
                boost::lock_guard <boost::mutex> lock(mutex);
                vListenProposals = proposalsAgent.GetListenProposals();
            }
            if (std::find(vListenProposals.begin(), vListenProposals.end(), proposal.orderHash) != vListenProposals.end()) {
                if (order->maxRate > proposal.rate) {
                    boost::lock_guard <boost::mutex> lock(mutex);
                    proposalsAgent.AddProposal(proposal);
                }
            }
            CNode* pNode = FindNode(proposal.address);
            if (pNode && vNodes.size() > 5) {
                pNode->CloseSocketDisconnect();
            }
        } else {
            // DoS prevention
//            CNode* pNode = FindNode(proposal.address);
//            if (pNode) {
//                CNodeState *state = State(pNode); // CNodeState was declared in main.cpp (SS)
//                state->nMisbehavior += 10;
//            }
        }
    } else if (strCommand == "dfsresv") {
        isStorageCommand = true;
        OrderHash orderHash;
        vRecv >> orderHash;
        const auto *order = GetOrder(orderHash);
        if (order) {
            while (qProposals.size() > 0) { // TODO: all copies must be saved (SS)
                auto proposal = qProposals.pop();
                if (proposal.orderHash != orderHash) {
                    qProposals.push(proposal);
                    break;
                }
            }
        }
        Notify(BackgroundJobs::ACCEPT_PROPOSAL);
    } else if (strCommand == "dfsrr") { // dfs request replica
        isStorageCommand = true;
        StorageHandshake handshake;
        vRecv >> handshake;
        if (GetOrder(handshake.orderHash)) {
            auto it = mapLocalFiles.find(handshake.orderHash);
            if (it != mapLocalFiles.end()) {
                handshakeAgent.CancelWait(handshake.orderHash);
                handshakeAgent.Add(handshake);
                PushHandshake(handshake);
                // TODO: add timer for file sending (SS)
            } else {
                // DoS prevention
//            CNode* pNode = FindNode(proposal.address);
//            if (pNode) {
//                CNodeState *state = State(pNode); // CNodeState was declared in main.cpp (SS)
//                state->nMisbehavior += 10;
//            }
            }
        }
    } else if (strCommand == "dfssend") {
        isStorageCommand = true;
        ReplicaStream replicaStream;
        fs::path tempFile;
        {
            boost::lock_guard<boost::mutex> lock(mutex);
            tempFile = tempStorageHeap.GetChunks().back()->path; // TODO: temp usage (SS)
        }
        tempFile /= (std::to_string(std::time(nullptr)) + ".luxfs");
        replicaStream.filestream.open(tempFile.string(), std::ios::binary|std::ios::out);
        if (!replicaStream.filestream.is_open()) {
            LogPrint("dfs", "File \"%s\" cannot be opened", tempFile);
            return ;
        }
        vRecv >> replicaStream;
        replicaStream.filestream.close();
        OrderHash orderHash = replicaStream.currentOrderHash;
        MerkleRootHash receivedMerkleRootHash = replicaStream.merkleRootHash;
        DecryptionKeys keys = replicaStream.keys;
        if (!CheckReceivedReplica(orderHash, receivedMerkleRootHash, tempFile)) {
            fs::remove(tempFile);
            return ;
        }
        const auto *order = GetOrder(orderHash);
        if (order) {
            if (!handshakeAgent.Exist(orderHash)) {
                LogPrint("dfs", "Handshake \"%s\" not found", orderHash.ToString());
                fs::remove(tempFile);
                return ;
            }
            {
                boost::lock_guard<boost::mutex> lock(mutex);
                std::shared_ptr<AllocatedFile> file = storageHeap.AllocateFile(order->fileURI, GetCryptoReplicaSize(order->fileSize));
                storageHeap.SetDecryptionKeys(file->uri, keys.rsaKey, keys.aesKey);
                storageHeap.SetMerkleRootHash(file->uri, receivedMerkleRootHash);
                fs::rename(tempFile, file->fullpath);
            }
            LogPrint("dfs", "File \"%s\" was uploaded", order->filename);
            CNode* pNode = FindNode(order->address);
            if (pNode) {
                pNode->PushMessage("dfsresv", orderHash);
            } else {
                LogPrint("dfs", "\"dfssend\" message handler have not connection to order sender");
                pfrom->PushMessage("dfsresv", orderHash);
            }
        }
    }
}

void StorageController::AnnounceOrder(const StorageOrder &order)
{
    OrderHash hash = order.GetHash();
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        mapAnnouncements[hash] = order;
    }

    CInv inv(MSG_STORAGE_ORDER_ANNOUNCE, hash);
    std::vector <CInv> vInv;
    vInv.push_back(inv);

    std::vector<CNode*> vNodesCopy;
    {
        LOCK(cs_vNodes);
        vNodesCopy = vNodes;
    }
    for (auto *pNode : vNodesCopy) {
        if (!pNode) {
            continue;
        }
        if (pNode->nVersion >= ActiveProtocol()) {
            pNode->PushMessage("inv", vInv);
        }
    }
}

void StorageController::AnnounceNewOrder(const StorageOrder &order, const boost::filesystem::path &path)
{
    AnnounceOrder(order);

    {
        boost::lock_guard<boost::mutex> lock(mutex);
        mapLocalFiles[order.GetHash()] = path;
        proposalsAgent.ListenProposals(order.GetHash());
    }
    mapTimers[order.GetHash()] = std::make_shared<CancelingSetTimeout>(std::chrono::milliseconds(60000),
                                                                       nullptr,
                                                                       [this](){ Notify(BackgroundJobs::CHECK_PROPOSALS); });
}

bool StorageController::CancelOrder(const uint256 &orderHash)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    if (!mapAnnouncements.count(orderHash)) {
        return false;
    }

    proposalsAgent.StopListenProposals(orderHash);
    proposalsAgent.EraseOrdersProposals(orderHash);
    mapLocalFiles.erase(orderHash);
    mapAnnouncements.erase(orderHash);
    return true;
}

void StorageController::ClearOldAnnouncments(std::time_t timestamp)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    for (auto &&it = mapAnnouncements.begin(); it != mapAnnouncements.end(); ) {
        StorageOrder order = it->second;
        if (order.time < timestamp) {
            std::vector<ProposalHash> vListenProposals = proposalsAgent.GetListenProposals();
            OrderHash orderHash = order.GetHash();
            if (std::find(vListenProposals.begin(), vListenProposals.end(), orderHash) != vListenProposals.end()) {
                proposalsAgent.StopListenProposals(orderHash);
            }
            std::vector<StorageProposal> vProposals = proposalsAgent.GetProposals(orderHash);
            if (vProposals.size()) {
                proposalsAgent.EraseOrdersProposals(orderHash);
            }
            mapLocalFiles.erase(orderHash);
            it = mapAnnouncements.erase(it);
        } else {
            ++it;
        }
    }
}

bool StorageController::AcceptProposal(const StorageProposal &proposal) {
    CNode* pNode = TryConnectToNode(proposal.address);
    handshakeAgent.StartHandshake(proposal, pNode);
}

void StorageController::DecryptReplica(const uint256 &orderHash, const boost::filesystem::path &decryptedFile)
{
    namespace fs = boost::filesystem;

    const auto *order = GetOrder(orderHash);
    if (!order) {
        return ;
    }
    std::shared_ptr<AllocatedFile> pAllocatedFile;
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        pAllocatedFile = storageHeap.GetFile(order->fileURI);
    }
    RSA *rsa = DecryptionKeys::CreatePublicRSA(DecryptionKeys::ToString(pAllocatedFile->keys.rsaKey));
    std::ifstream filein(pAllocatedFile->fullpath.string(), std::ios::binary);
    if (!filein.is_open()) {
        LogPrint("dfs", "file %s cannot be opened", pAllocatedFile->fullpath.string());
        return ;
    }
    auto fileSize = fs::file_size(pAllocatedFile->fullpath);
    auto bytesSize = order->fileSize;
    std::ofstream outfile;
    outfile.open(decryptedFile.string().c_str(), std::ios::binary);
    uint64_t sizeBuffer = nBlockSizeRSA - 2;
    byte *buffer = new byte[sizeBuffer];
    byte *replica = new byte[nBlockSizeRSA];

    for (auto i = 0u; i < fileSize; i+= nBlockSizeRSA)
    {
        filein.read((char *)replica, nBlockSizeRSA); // TODO: change to loop of readsome

        DecryptData(replica, 0, sizeBuffer, buffer, pAllocatedFile->keys.aesKey, rsa);
        outfile.write((char *) buffer, std::min(sizeBuffer, bytesSize));
        bytesSize -= sizeBuffer;
        // TODO: check write (SS)
    }

    delete[] buffer;
    delete[] replica;
    filein.close();
    outfile.close();
    return ;
}

void StorageController::LoadOrders()
{
    db->LoadOrders([this] (const MerkleRootHash &merkleRootHash, const StorageOrderDB &orderDB) {
        mapOrders.insert(std::make_pair(merkleRootHash, orderDB));
    });
}

void StorageController::AddOrder(const StorageOrderDB &orderDB)
{
    mapOrders.insert(std::make_pair(orderDB.merkleRootHash, orderDB));
}

void StorageController::SaveOrder(const StorageOrderDB &orderDB)
{
    db->WriteOrder(orderDB.merkleRootHash, orderDB);
}

void StorageController::AddProof(const StorageProofDB &proof)
{
    mapProofs[proof.merkleRootHash].push_back(proof);
}

void StorageController::LoadProofs()
{
    db->LoadProofs([this] (const uint256 &proofHash, const StorageProofDB &proof) {
        mapProofs[proof.merkleRootHash].push_back(proof);
    });
}

void StorageController::SaveProof(const StorageProofDB &proof)
{
    db->WriteProof(proof.GetHash(), proof);
}

std::map<uint256, StorageOrder> StorageController::GetAnnouncements()
{
    boost::lock_guard<boost::mutex> lock(mutex);
    return mapAnnouncements;
}

const StorageOrder *StorageController::GetOrder(const uint256 &hash)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    return mapAnnouncements.find(hash) != mapAnnouncements.end()? &(mapAnnouncements[hash]) : nullptr;
}

std::vector<std::shared_ptr<StorageChunk>> StorageController::GetChunks(const bool tempChunk)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    return tempChunk?
           tempStorageHeap.GetChunks() :
           storageHeap.GetChunks();
}

void StorageController::MoveChunk(size_t chunkIndex, const boost::filesystem::path &newpath, const bool tempChunk)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    tempChunk?
    tempStorageHeap.MoveChunk(chunkIndex, newpath) :
    storageHeap.MoveChunk(chunkIndex, newpath);
}

std::vector<StorageProposal> StorageController::GetProposals(const uint256 &orderHash)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    return proposalsAgent.GetProposals(orderHash);
}

StorageProposal StorageController::GetProposal(const uint256 &orderHash, const uint256 &proposalHash)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    return proposalsAgent.GetProposal(orderHash, proposalHash);
}

std::pair<uint256, DecryptionKeys> StorageController::GetMetadata(const uint256 &orderHash, const uint256 &proposalHash)
{
    if (mapMetadata.find(orderHash) != mapMetadata.end() &&
        mapMetadata[orderHash].find(proposalHash) != mapMetadata[orderHash].end()) {
        return mapMetadata[orderHash][proposalHash];
    }
    return {};
}

const StorageOrderDB *StorageController::GetOrderDB(const uint256 &merkleRootHash)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    auto it = std::find_if(mapOrders.begin(), mapOrders.end(), [&merkleRootHash](const std::pair<OrderHashDB, StorageOrderDB> &entity) {
        return entity.second.merkleRootHash == merkleRootHash;
    });
    return it != mapOrders.end()? &(it->second) : nullptr;
}

const std::vector<StorageProofDB> StorageController::GetProofs(const uint256 &merkleRootHash)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    if (mapProofs.find(merkleRootHash) != mapProofs.end()) {
        return mapProofs[merkleRootHash];
    }
    return {};
}

const std::list<uint256> StorageController::ConstructMerklePath(const uint256 &fileURI, const size_t position)
{
    std::shared_ptr<AllocatedFile> pAllocatedFile = storageHeap.GetFile(fileURI);

    if (!pAllocatedFile) {
        return {};
    }

    std::shared_ptr<AllocatedFile> pMerleTreeFile;
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        pMerleTreeFile = tempStorageHeap.AllocateFile(uint256{}, pAllocatedFile->size);
    }
    auto merkleRootHash = Merkler::ConstructMerkleTree(pAllocatedFile->fullpath, pMerleTreeFile->fullpath);

    std::list<uint256> ret = Merkler::ConstructMerklePath(pAllocatedFile->fullpath, pMerleTreeFile->fullpath, position);

    boost::filesystem::remove(pMerleTreeFile->fullpath);
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        tempStorageHeap.FreeFile(pMerleTreeFile->uri);
    }

    return ret;
}

void StorageController::StopThreads()
{
    {
        boost::lock_guard<boost::mutex> lockJob(jobsMutex);
        boost::lock_guard<boost::mutex> lockHandshake(handshakesMutex);
        shutdownThreads = true;
    }
    jobsHandler.notify_one();
    handshakesHandler.notify_one();

    proposalsManagerThread.join();
    handshakesManagerThread.join();
    pingThread.join();
}

std::shared_ptr<AllocatedFile> StorageController::CreateReplica(const boost::filesystem::path &sourcePath,
                                                                const StorageOrder &order,
                                                                const DecryptionKeys &keys,
                                                                RSA *rsa)
{
    namespace fs = boost::filesystem;

    std::ifstream filein;
    filein.open(sourcePath.string().c_str(), std::ios::binary);
    if (!filein.is_open()) {
        LogPrint("dfs", "file %s cannot be opened", sourcePath.string());
        return {};
    }

    auto length = fs::file_size(sourcePath);

    std::shared_ptr<AllocatedFile> tempFile;
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        tempFile = tempStorageHeap.AllocateFile(order.fileURI, GetCryptoReplicaSize(length));
    }
    std::ofstream outfile;
    outfile.open(tempFile->fullpath.string(), std::ios::binary);

    uint64_t sizeBuffer = nBlockSizeRSA - 2;
    byte *buffer = new byte[sizeBuffer];
    byte *replica = new byte[nBlockSizeRSA];
    for (auto i = 0u; i < length; i+= sizeBuffer)
    {
        uint64_t n = std::min(sizeBuffer, order.fileSize - i);

        filein.read((char *)buffer, n); // TODO: change to loop of readsome
        EncryptData(buffer, 0, n, replica, keys.aesKey, rsa);
        outfile.write((char *) replica, nBlockSizeRSA);
        // TODO: check write (SS)
    }
    {
        boost::lock_guard <boost::mutex> lock(mutex);
        tempStorageHeap.SetDecryptionKeys(tempFile->uri, keys.rsaKey, keys.aesKey);
    }
    filein.close();
    outfile.close();
    delete[] buffer;
    delete[] replica;

    return tempFile;
}

bool StorageController::SendReplica(const StorageOrder &order,
                                    const uint256 merkleRootHash,
                                    std::shared_ptr<AllocatedFile> pAllocatedFile,
                                    const DecryptionKeys &keys,
                                    CNode* pNode)
{
    namespace fs = boost::filesystem;
    if (!pNode) {
        LogPrint("dfs", "Node does not found");
        return false;
    }
    ReplicaStream replicaStream;
    replicaStream.currentOrderHash = order.GetHash();
    replicaStream.merkleRootHash = merkleRootHash;
    replicaStream.keys = keys;
    replicaStream.filestream.open(pAllocatedFile->fullpath.string(), std::ios::binary|std::ios::in);
    if (!replicaStream.filestream.is_open()) {
        LogPrint("dfs", "file %s cannot be opened", pAllocatedFile->fullpath.string());
        return false;
    }
    pNode->PushMessage("dfssend", replicaStream);

    replicaStream.filestream.close();
    {
        boost::lock_guard<boost::mutex> lock(mutex);
        tempStorageHeap.FreeFile(pAllocatedFile->uri);
    }
    fs::remove(pAllocatedFile->fullpath);
    return true;
}

bool StorageController::CheckReceivedReplica(const uint256 &orderHash, const uint256 &receivedMerkleRootHash, const boost::filesystem::path &replica)
{
    namespace fs = boost::filesystem;

    const auto *order = GetOrder(orderHash);
    if (!order) {
        return false;
    }
    size_t size = fs::file_size(replica);
    if (size != GetCryptoReplicaSize(order->fileSize)) {
        LogPrint("dfs", "Wrong file \"%s\" size. real size: %d not equal order size: %d ", order->filename, size,  GetCryptoReplicaSize(order->fileSize));
        return false;
    }
    { // check merkle root hash
        std::shared_ptr<AllocatedFile> pMerleTreeFile;
        {
            boost::lock_guard<boost::mutex> lock(mutex);
            pMerleTreeFile = tempStorageHeap.AllocateFile(uint256{}, size);
        }
        MerkleRootHash merkleRootHash = Merkler::ConstructMerkleTree(replica, pMerleTreeFile->fullpath);
        fs::remove(pMerleTreeFile->fullpath);
        {
            boost::lock_guard<boost::mutex> lock(mutex);
            tempStorageHeap.FreeFile(pMerleTreeFile->uri);
        }
        if (merkleRootHash.ToString() != receivedMerkleRootHash.ToString()) {
            LogPrint("dfs", "Wrong merkle root hash. real hash: \"%s\" != \"%s\"(received)", merkleRootHash.ToString(), receivedMerkleRootHash.ToString());
            return false;
        }
    }
    return true;
}

CNode *StorageController::TryConnectToNode(const CService &address, int maxAttempt)
{
    CNode* pNode = FindNode(address);
    if (pNode == nullptr) {
        for (size_t nLoop = 0; nLoop < maxAttempt && pNode == nullptr; ++nLoop) {
            CAddress addr;
            OpenNetworkConnection(addr, false, NULL, address.ToStringIPPort().c_str());
            for (size_t i = 0; i < 10 && i < nLoop; ++i) {
                MilliSleep(500);
            }
            pNode = FindNode(address);
        }
    }
    return pNode;
}

void StorageController::Notify(const StorageController::BackgroundJobs job)
{
    {
        boost::lock_guard<boost::mutex> lock(jobsMutex);
        qJobs.push(job);
    }
    jobsHandler.notify_one();
}

void StorageController::PushHandshake(const StorageHandshake &handshake,const bool status)
{
    {
        boost::lock_guard<boost::mutex> lock(handshakesMutex);
        qHandshakes.push(std::make_pair(status, handshake));
    }
    handshakesHandler.notify_one();
}

void StorageController::FoundMyIP()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    lastCheckIp = std::time(nullptr);

    while (true) {
        boost::this_thread::interruption_point();

        if (shutdownThreads) {
            return ;
        }

        if (!address.IsValid() || !(std::time(nullptr) - lastCheckIp < 3600)) {
            std::vector<CNode*> vNodesCopy;
            {
                LOCK(cs_vNodes);
                vNodesCopy = vNodes;
            }

            for (const auto &node : vNodesCopy) {
                node->PushMessage("dfsping");
            }
        }

        boost::this_thread::sleep(boost::posix_time::seconds(1));

    }
}

void StorageController::ProcessProposalsMessages()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);
    bool getNextProposal = false;

    while (true) {
        boost::this_thread::interruption_point();

        if(!qJobs.size() && !shutdownThreads) {
            boost::unique_lock<boost::mutex> lock(jobsMutex);
            jobsHandler.wait(lock, [this]() { return qJobs.size() || shutdownThreads; });
        }

        if (shutdownThreads) {
            return ;
        }

        BackgroundJobs job = qJobs.pop();

        if (job == BackgroundJobs::CHECK_PROPOSALS) {
            std::vector<OrderHash> orderHashes;
            {
                boost::lock_guard<boost::mutex> lock(mutex);
                orderHashes = proposalsAgent.GetListenProposals();
            }
            for (auto &&orderHash : orderHashes) {
                boost::lock_guard<boost::mutex> lock(mutex);
                std::vector <StorageProposal> proposals = proposalsAgent.GetSortedProposals(orderHash);
                if (proposals.size() > 0) {
                    for (auto &&proposal : proposals) {
                        qProposals.push(proposal);
                    }
                    getNextProposal = true;
                } else {
                    proposalsAgent.StopListenProposals(orderHash);
                }
            }
        } else if (job == BackgroundJobs::ACCEPT_PROPOSAL) {
            getNextProposal = true;
        } else if (job == BackgroundJobs::FAIL_HANDSHAKE) {
            getNextProposal = true;
        }
        if (qProposals.size() && getNextProposal) {
            auto proposal = qProposals.pop();
            AcceptProposal(proposal);
        }
        getNextProposal = false;
    }
}

void StorageController::ProcessHandshakesMessages()
{
    SetThreadPriority(THREAD_PRIORITY_BELOW_NORMAL);

    while (true) {
        boost::this_thread::interruption_point();

        if(!qHandshakes.size() && !shutdownThreads) {
            boost::unique_lock<boost::mutex> lock(handshakesMutex);
            handshakesHandler.wait(lock, [this]() { return qHandshakes.size() != 0 || shutdownThreads; });
        }

        if (shutdownThreads) {
            return ;
        }

        auto item = qHandshakes.pop();
        bool status = item.first;
        StorageHandshake handshake = item.second;
        auto order = GetOrder(handshake.orderHash);
        if (!order) {
            continue ;
        }
        StorageProposal proposal;
        {
            boost::lock_guard<boost::mutex> lock(mutex);
            proposal = proposalsAgent.GetProposal(handshake.orderHash, handshake.proposalHash);
        }
        CNode *pNode = FindNode(proposal.address);
        if (!status) {
            if (pNode && vNodes.size() > 5) {
                pNode->CloseSocketDisconnect();
            }
            Notify(BackgroundJobs::FAIL_HANDSHAKE);
            continue ;
        }

        RSA *rsa;
        DecryptionKeys keys = DecryptionKeys::GenerateKeys(&rsa);
        boost::filesystem::path filePath;
        {
            boost::lock_guard<boost::mutex> lock(mutex);
            auto itFile = mapLocalFiles.find(proposal.orderHash);
            filePath = itFile->second;
        }
        auto pAllocatedFile = CreateReplica(filePath, *order, keys, rsa);
        std::shared_ptr<AllocatedFile> pMerleTreeFile;
        {
            boost::lock_guard<boost::mutex> lock(mutex);
            pMerleTreeFile = tempStorageHeap.AllocateFile(uint256{}, pAllocatedFile->size);
        }
        auto merkleRootHash = Merkler::ConstructMerkleTree(pAllocatedFile->fullpath, pMerleTreeFile->fullpath);
        if (pNode) {
            if (!SendReplica(*order, merkleRootHash, pAllocatedFile, keys, pNode)) {
                Notify(BackgroundJobs::FAIL_HANDSHAKE);
            }
            boost::filesystem::remove(pMerleTreeFile->fullpath);
            {
                boost::lock_guard<boost::mutex> lock(mutex);
                tempStorageHeap.FreeFile(pMerleTreeFile->uri);
            }
        }

        mapMetadata[handshake.orderHash][handshake.proposalHash] = std::make_pair(merkleRootHash, keys);
        RSA_free(rsa);
    }
}

