#include "storagecontroller.h"

#include <fstream>
#include <boost/filesystem.hpp>

#include "main.h"
#include "streams.h"
#include "util.h"
#include "replicabuilder.h"
#include "serialize.h"

std::unique_ptr<StorageController> storageController;

struct FileStream
{
    const std::size_t BUFFER_SIZE = 4 * 1024;
    std::fstream filestream;
    uint256 currenOrderHash;

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        std::vector<char> buf(BUFFER_SIZE);

        if (!ser_action.ForRead()) {
            READWRITE(currenOrderHash);
            while (!filestream.eof()) {
                buf.resize(BUFFER_SIZE);
                buf.resize(filestream.readsome(&buf[0], buf.size()));

                if (buf.empty()) {
                    break;
                }
                READWRITE(buf);
            }
        } else {
            READWRITE(currenOrderHash);

            if (storageController->mapAnnouncements.count(currenOrderHash) != 0) {
                const auto order = storageController->mapAnnouncements[currenOrderHash];
                const auto fileSize = GetCryptoReplicaSize(order.fileSize);
                const auto tailSize = fileSize % BUFFER_SIZE;

                for (auto i = 0u; i < fileSize; i += BUFFER_SIZE) {
                    READWRITE(buf);
                    filestream.write(&buf[0], buf.size());
                }

                if (tailSize > 0) {
                    buf.resize(tailSize);
                    READWRITE(buf);
                    filestream.write(&buf[0], buf.size());
                }
            }
        }
    }
};


#if OPENSSL_VERSION_NUMBER < 0x10100005L
static void RSA_get0_key(const RSA *r, const BIGNUM **n, const BIGNUM **e, const BIGNUM **d)
{
    if(n != NULL)
        *n = r->n;

    if(e != NULL)
        *e = r->e;

    if(d != NULL)
        *d = r->d;
}
#endif

StorageController::StorageController() : background(boost::bind(&StorageController::BackgroundJob, this)),
                                         rate(STORAGE_MIN_RATE),
                                         maxblocksgap(DEFAULT_STORAGE_MAX_BLOCK_GAP)
{
    namespace fs = boost::filesystem;

    fs::path defaultDfsPath = GetDataDir() / "dfs";
    if (!fs::exists(defaultDfsPath)) {
        fs::create_directories(defaultDfsPath);
    }
    storageHeap.AddChunk(defaultDfsPath.string(), DEFAULT_STORAGE_SIZE);
    fs::path defaultDfsTempPath = GetDataDir() / "dfstemp";
    if (!fs::exists(defaultDfsTempPath)) {
        fs::create_directories(defaultDfsTempPath);
    }
    tempStorageHeap.AddChunk(defaultDfsTempPath.string(), DEFAULT_STORAGE_SIZE);
}

void StorageController::ProcessStorageMessage(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isStorageCommand)
{
    if (strCommand == "dfsannounce") {
        isStorageCommand = true;
        StorageOrder order;
        vRecv >> order;

        uint256 hash = order.GetHash();
        if (mapAnnouncements.find(hash) == mapAnnouncements.end()) {
            AnnounceOrder(order); // TODO: Is need remove "pfrom" node from announcement? (SS)
            if (storageHeap.MaxAllocateSize() > order.fileSize &&
                tempStorageHeap.MaxAllocateSize() > order.fileSize &&
                order.maxRate >= rate &&
                order.maxGap >= maxblocksgap) {
                StorageProposal proposal;
                proposal.time = std::time(0);
                proposal.orderHash = hash;
                proposal.rate = rate;
                proposal.address = address;

                CNode* pNode = FindNode(order.address);
                if (!pNode) {
                    CAddress addr;
                    OpenNetworkConnection(addr, false, NULL, order.address.ToStringIPPort().c_str());
                    MilliSleep(500);
                    pNode = FindNode(order.address);
                }

                if (pNode) {
                    pNode->PushMessage("dfsproposal", proposal);
                } else {
                    pfrom->PushMessage("dfsproposal", proposal);
                }

            }
        }
    } else if (strCommand == "dfsproposal") {
        isStorageCommand = true;
        StorageProposal proposal;
        vRecv >> proposal;
        auto itAnnounce = mapAnnouncements.find(proposal.orderHash);
        if (itAnnounce != mapAnnouncements.end()) {
            std::vector<uint256> vListenProposals;
            {
                boost::lock_guard <boost::mutex> lock(mutex);
                vListenProposals = proposalsAgent.GetListenProposals();
            }

            if (std::find(vListenProposals.begin(), vListenProposals.end(), proposal.orderHash) != vListenProposals.end()) {
                if (itAnnounce->second.maxRate > proposal.rate) {
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
    } else if (strCommand == "dfshandshake") {
        isStorageCommand = true;
        StorageHandshake handshake;
        vRecv >> handshake;
        auto itAnnounce = mapAnnouncements.find(handshake.orderHash);
        if (itAnnounce != mapAnnouncements.end()) {
            StorageOrder &order = itAnnounce->second;
            if (storageHeap.MaxAllocateSize() > order.fileSize && tempStorageHeap.MaxAllocateSize() > order.fileSize) { // TODO: Change to exist(handshake.proposalHash) (SS)
                StorageHandshake requestReplica;
                requestReplica.time = std::time(0);
                requestReplica.orderHash = handshake.orderHash;
                requestReplica.proposalHash = handshake.proposalHash;
                requestReplica.port = DEFAULT_DFS_PORT;

                mapReceivedHandshakes[handshake.orderHash] = handshake;
                CNode* pNode = FindNode(order.address);

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
    } else if (strCommand == "dfsrr") {
        isStorageCommand = true;
        StorageHandshake handshake;
        vRecv >> handshake;
        auto itAnnounce = mapAnnouncements.find(handshake.orderHash);
        if (itAnnounce != mapAnnouncements.end()) {
            auto it = mapLocalFiles.find(handshake.orderHash);
            if (it != mapLocalFiles.end()) {
                mapReceivedHandshakes[handshake.orderHash] = handshake;
            } else {
                // DoS prevention
//            CNode* pNode = FindNode(proposal.address);
//            if (pNode) {
//                CNodeState *state = State(pNode); // CNodeState was declared in main.cpp (SS)
//                state->nMisbehavior += 10;
//            }
            }
        }
    } else if (strCommand == "dfssendfile") {
        isStorageCommand = true;
        FileStream fileStream;
        boost::filesystem::path fullFilename = storageHeap.GetChunks().back()->path;
        fullFilename /= (std::to_string(std::time(nullptr)) + ".luxfs");
        fileStream.filestream.open(fullFilename.string(), std::ios::binary|std::ios::out);
        if (!fileStream.filestream.is_open()) {
            LogPrint("dfs", "file %s cannot be opened", fullFilename);
            return ;
        }
        vRecv >> fileStream;
    } else if (strCommand == "dfsping") {
        isStorageCommand = true;
        pfrom->PushMessage("dfspong", pfrom->addr);
    } else if (strCommand == "dfspong") {
        isStorageCommand = true;
        vRecv >> address;
        address.SetPort(GetListenPort());
    }
}

void StorageController::BackgroundJob()
{
    auto lastCheckIp = std::time(nullptr);

    while (1) {
        boost::this_thread::interruption_point();
        boost::this_thread::sleep(boost::posix_time::seconds(1));

        std::vector<CNode*> vNodesCopy;
        {
            LOCK(cs_vNodes);
            vNodesCopy = vNodes;
        }

        if (!address.IsValid() || (std::time(nullptr) - lastCheckIp > 3600))
        {
            for (const auto &node : vNodesCopy) {
                node->PushMessage("dfsping");
            }
        }

        std::vector<uint256> orderHashes;
        {
            boost::lock_guard<boost::mutex> lock(mutex);
            orderHashes = proposalsAgent.GetListenProposals();
        }
        for(auto &&orderHash : orderHashes) {
            auto orderIt = mapAnnouncements.find(orderHash);
            if (orderIt != mapAnnouncements.end()) {
                auto order = orderIt->second;
                if(std::time(nullptr) > order.time + 60) {
                    if (FindReplicaKeepers(order, 1)) {
                        boost::lock_guard<boost::mutex> lock(mutex);
                        proposalsAgent.StopListenProposal(orderHash);
                    }
                }
            }
        }
    }
}

void StorageController::AnnounceOrder(const StorageOrder &order)
{
    uint256 hash = order.GetHash();
    mapAnnouncements[hash] = order;

    CInv inv(MSG_STORAGE_ORDER_ANNOUNCE, hash);
    vector <CInv> vInv;
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

void StorageController::AnnounceOrder(const StorageOrder &order, const boost::filesystem::path &path)
{
    AnnounceOrder(order);
    boost::lock_guard<boost::mutex> lock(mutex);
    mapLocalFiles[order.GetHash()] = path;
    proposalsAgent.ListenProposal(order.GetHash());
}

bool StorageController::CancelOrder(const uint256 &orderHash)
{
    if (!mapAnnouncements.count(orderHash)) {
        return false;
    }

    proposalsAgent.StopListenProposal(orderHash);
    proposalsAgent.EraseOrdersProposals(orderHash);
    mapLocalFiles.erase(orderHash);
    mapAnnouncements.erase(orderHash);
    return true;
}

bool StorageController::AcceptProposal(const StorageProposal &proposal)
{
    if (!mapAnnouncements.count(proposal.orderHash)) {
        return false;
    }

    auto order = mapAnnouncements[proposal.orderHash];
    std::pair<DecryptionKeys, RSA> keys = GenerateKeys();
    if (!StartHandshake(proposal, keys.first)) {
        return false;
    }
    auto it = mapReceivedHandshakes.find(proposal.orderHash);
    for (int times = 0; times < 300 && it == mapReceivedHandshakes.end(); ++times) {
        MilliSleep(100);
        it = mapReceivedHandshakes.find(proposal.orderHash);
    }
    CNode* pNode = FindNode(proposal.address);
    if (it != mapReceivedHandshakes.end()) {
        auto itFile = mapLocalFiles.find(proposal.orderHash);
        auto pAllocatedFile = CreateReplica(itFile->second, order.fileURI, keys.first, &(keys.second));
        if (pNode) {
            if (!SendReplica(order, pAllocatedFile, pNode)) {
                return false;
            }
        }
    } else {
        if (pNode) {
            pNode->CloseSocketDisconnect();
        }
    }
    return false;
}

bool StorageController::StartHandshake(const StorageProposal &proposal, const DecryptionKeys &keys)
{
    StorageHandshake handshake;
    handshake.time = std::time(0);
    handshake.orderHash = proposal.orderHash;
    handshake.proposalHash = proposal.GetHash();
    handshake.port = DEFAULT_DFS_PORT;
    handshake.keys = keys;

    CNode* pNode = FindNode(proposal.address);
    if (pNode == nullptr) {
        for (int64_t nLoop = 0; nLoop < 100 && pNode == nullptr; nLoop++) {
         //   ProcessOneShot();
            CAddress addr;
            OpenNetworkConnection(addr, false, NULL, proposal.address.ToStringIPPort().c_str());
            for (int i = 0; i < 10 && i < nLoop; i++) {
                MilliSleep(500);
            }
            pNode = FindNode(proposal.address);
            MilliSleep(500);
        }
    }

    if (pNode != nullptr) {
        pNode->PushMessage("dfshandshake", handshake);
        return true;
    }
    return false;
}

std::vector<StorageProposal> StorageController::SortProposals(const StorageOrder &order)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    std::vector<StorageProposal> proposals = proposalsAgent.GetProposals(order.GetHash());
    if (!proposals.size()) {
        return {};
    }

    std::list<StorageProposal> sortedProposals = { *(proposals.begin()) };
    for (auto itProposal = ++(proposals.begin()); itProposal != proposals.end(); ++itProposal) {
        for (auto it = sortedProposals.begin(); it != sortedProposals.end(); ++it) {
            if (itProposal->rate < it->rate) { // TODO: add check last save file time, free space, etc. (SS)
                sortedProposals.insert(it, *itProposal);
                break;
            }
        }
    }

    std::vector<StorageProposal> bestProposals(sortedProposals.begin(), sortedProposals.end());

    return bestProposals;
}

void StorageController::ClearOldAnnouncments(std::time_t timestamp)
{
    boost::lock_guard<boost::mutex> lock(mutex);
    for (auto &&it = mapAnnouncements.begin(); it != mapAnnouncements.end(); ) {
        StorageOrder order = it->second;
        if (order.time < timestamp) {
            std::vector<uint256> vListenProposals = proposalsAgent.GetListenProposals();
            uint256 orderHash = order.GetHash();
            if (std::find(vListenProposals.begin(), vListenProposals.end(), orderHash) != vListenProposals.end()) {
                proposalsAgent.StopListenProposal(orderHash);
            }
            std::vector<StorageProposal> vProposals = proposalsAgent.GetProposals(orderHash);
            if (vProposals.size()) {
                proposalsAgent.EraseOrdersProposals(orderHash);
            }
            mapLocalFiles.erase(orderHash);
            mapAnnouncements.erase(it++);
        } else {
            ++it;
        }
    }
}

//void StorageController::ListenProposal(const uint256 &orderHash)
//{
//    boost::lock_guard<boost::mutex> lock(mutex);
//    proposalsAgent.ListenProposal(orderHash);
//}
//
//void StorageController::StopListenProposal(const uint256 &orderHash)
//{
//    proposalsAgent.StopListenProposal(orderHash);
//}
//
//std::vector<StorageProposal> StorageController::GetProposals(const uint256 &orderHash)
//{
//    boost::lock_guard<boost::mutex> lock(mutex);
//    return proposalsAgent.GetProposals(orderHash);
//}

bool StorageController::FindReplicaKeepers(const StorageOrder &order, const int countReplica)
{
    std::vector<StorageProposal> proposals = SortProposals(order);
    int numReplica = 0;
    for (StorageProposal proposal : proposals) {
        if (AcceptProposal(proposal)) {
            if (++numReplica == countReplica) {
                boost::lock_guard<boost::mutex> lock(mutex);
                proposalsAgent.StopListenProposal(order.GetHash());
                return true;
            }
        }
    }
    return false;
}

std::pair<DecryptionKeys, RSA> StorageController::GenerateKeys()
{
    RSA *rsa;
    const BIGNUM *rsa_n;
    // search for rsa->n > 0x0000ff...126 bytes...ff
    {
        rsa = RSA_generate_key(nBlockSizeRSA * 8, 3, nullptr, nullptr);
        BIGNUM *minModulus = GetMinModulus();
        RSA_get0_key(rsa, &rsa_n, nullptr, nullptr);
        while (BN_ucmp(minModulus, rsa_n) >= 0) {
            RSA_free(rsa);
            rsa = RSA_generate_key(nBlockSizeRSA * 8, 3, nullptr, nullptr);
        }
        BN_free(minModulus);
    }
    const char chAESKey[] = "1234567890123456"; // TODO: generate unique 16 bytes (SS)
    const AESKey aesKey(chAESKey, chAESKey + sizeof(chAESKey)/sizeof(*chAESKey));

    BIO *pub = BIO_new(BIO_s_mem());
    PEM_write_bio_RSAPublicKey(pub, rsa);
    size_t publicKeyLength = BIO_pending(pub);
    char *rsaPubKey = new char[publicKeyLength + 1];
    BIO_read(pub, rsaPubKey, publicKeyLength);
    rsaPubKey[publicKeyLength] = '\0';

    DecryptionKeys decryptionKeys = {DecryptionKeys::ToBytes(std::string(rsaPubKey)), aesKey};
    std::pair<DecryptionKeys, RSA> keys = {decryptionKeys, *rsa};
    return keys;
}

std::shared_ptr<AllocatedFile> StorageController::CreateReplica(const boost::filesystem::path &filename,
                                                                const uint256 &fileURI,
                                                                const DecryptionKeys &keys,
                                                                RSA *rsa)
{
    namespace fs = boost::filesystem;

    std::ifstream filein;
    filein.open(filename.string().c_str(), std::ios::binary);
    if (!filein.is_open()) {
        LogPrint("dfs", "file %s cannot be opened", filename.string());
        return {};
    }

    auto length = fs::file_size(filename);

    std::shared_ptr<AllocatedFile> tempFile = tempStorageHeap.AllocateFile(fileURI.ToString(), GetCryptoReplicaSize(length));

    std::ofstream outfile;
    outfile.open(tempFile->filename, std::ios::binary);

    size_t sizeBuffer = nBlockSizeRSA - 2;
    byte *buffer = new byte[sizeBuffer];
    byte *replica = new byte[nBlockSizeRSA];
    while(!filein.eof())
    {
        auto n = filein.readsome((char *)buffer, sizeBuffer);
        if (n <= 0) {
            break;
        }
        EncryptData(buffer, 0, n, replica, keys.aesKey, rsa);
        outfile.write((char *) replica, nBlockSizeRSA);
        // TODO: check write (SS)
    }
    tempStorageHeap.SetDecryptionKeys(tempFile->uri, keys.rsaKey, keys.aesKey);
    filein.close();
    outfile.close();
    RSA_free(rsa);
    delete[] buffer;
    delete[] replica;

    return tempFile;
}

bool StorageController::SendReplica(const StorageOrder &order, std::shared_ptr<AllocatedFile> pAllocatedFile, CNode* pNode)
{
    if (!pNode) {
        LogPrint("dfs", "Node does not found");
        return false;
    }
    FileStream fileStream;
    fileStream.currenOrderHash = order.GetHash();
    fileStream.filestream.open(pAllocatedFile->filename, std::ios::binary|std::ios::in);
    if (!fileStream.filestream.is_open()) {
        LogPrint("dfs", "file %s cannot be opened", pAllocatedFile->filename);
        return false;
    }
    pNode->PushMessage("dfssendfile", fileStream);

    return true;
}
