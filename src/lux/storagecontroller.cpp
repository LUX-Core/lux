#include "storagecontroller.h"

#include <fstream>
#include <boost/filesystem.hpp>

#include "main.h"
#include "streams.h"
#include "util.h"
#include "replicabuilder.h"

StorageController storageController;

StorageController::StorageController() : rate(STORAGE_MIN_RATE), address(CService("127.0.0.1")) {
//    namespace fs = boost::filesystem;
//
//    fs::path defaultDfsPath = GetDataDir() / "dfs";
//    if (!fs::exists(defaultDfsPath)) {
//        fs::create_directories(defaultDfsPath);
//    }
//    storageHeap.AddChunk(defaultDfsPath.string(), DEFAULT_STORAGE_SIZE);
//    fs::path defaultDfsTempPath = GetDataDir() / "dfstemp";
//    if (!fs::exists(defaultDfsTempPath)) {
//        fs::create_directories(defaultDfsTempPath);
//    }
//    tempStorageHeap.AddChunk(defaultDfsTempPath.string(), DEFAULT_STORAGE_SIZE);
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
            if (storageHeap.MaxAllocateSize() > order.fileSize && tempStorageHeap.MaxAllocateSize() > order.fileSize) {
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
                pNode->PushMessage("dfsproposal", proposal);
            }
        }
    } else if (strCommand == "dfsproposal") {
        isStorageCommand = true;
        StorageProposal proposal;
        vRecv >> proposal;
        auto itAnnounce = mapAnnouncements.find(proposal.orderHash);
        if (itAnnounce != mapAnnouncements.end()) {
            std::vector<uint256> vListenProposals = proposalsAgent.GetListenProposals();
            if (std::find(vListenProposals.begin(), vListenProposals.end(), proposal.orderHash) != vListenProposals.end()) {
                if (itAnnounce->second.maxRate > proposal.rate) {
                    proposalsAgent.AddProposal(proposal);
                }
            }
            CNode* pNode = FindNode(proposal.address);
            if (pNode) {
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
            if (storageHeap.MaxAllocateSize() > order.fileSize && tempStorageHeap.MaxAllocateSize() > order.fileSize) { // Change to exist(handshake.proposalHash) (SS)
                StorageHandshake requestReplica;
                requestReplica.time = std::time(0);
                requestReplica.orderHash = handshake.orderHash;
                requestReplica.proposalHash = handshake.proposalHash;
                requestReplica.port = DEFAULT_DFS_PORT;

                mapReceivedHandshakes[handshake.orderHash] = handshake;
                CNode* pNode = FindNode(order.address);
                if (!pNode) {
                    LogPrint("dfs", "\"dfshandshake\" message handler have not connection to order sender");
                    return ;
                }
                pNode->PushMessage("dfsrequestreplica", requestReplica);
            }
        } else {
            // DoS prevention
//            CNode* pNode = FindNode(proposal.address);
//            if (pNode) {
//                CNodeState *state = State(pNode); // CNodeState was declared in main.cpp (SS)
//                state->nMisbehavior += 10;
//            }
        }
    } else if (strCommand == "dfsrequestreplica") {
        isStorageCommand = true;
        StorageHandshake handshake;
        vRecv >> handshake;
        auto itAnnounce = mapAnnouncements.find(handshake.orderHash);
        if (itAnnounce != mapAnnouncements.end()) {
            StorageOrder order = itAnnounce->second;
            auto it = mapLocalFiles.find(handshake.orderHash);
            if (it != mapLocalFiles.end()) {
                mapReceivedHandshakes[handshake.orderHash] = handshake;
                auto proposal = proposalsAgent.GetProposal(handshake.orderHash, handshake.proposalHash);
                CreateReplica(it->second, order, proposal);
            } else {
                // DoS prevention
//            CNode* pNode = FindNode(proposal.address);
//            if (pNode) {
//                CNodeState *state = State(pNode); // CNodeState was declared in main.cpp (SS)
//                state->nMisbehavior += 10;
//            }
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
            LOCK(pNode->cs_filter);
            if (pNode->pfilter == nullptr) {
                pNode->PushMessage("inv", vInv);
            }
        }
    }
}

void StorageController::AnnounceOrder(const StorageOrder &order, const boost::filesystem::path &path)
{
    AnnounceOrder(order);
    mapLocalFiles[order.GetHash()] = path;
}

void StorageController::StartHandshake(const StorageProposal &proposal)
{
    StorageHandshake handshake;
    handshake.time = std::time(0);
    handshake.orderHash = proposal.orderHash;
    handshake.proposalHash = proposal.GetHash();
    handshake.port = DEFAULT_DFS_PORT;

    CNode* pNode = FindNode(proposal.address);
    if (!pNode) {
        CAddress addr;
        OpenNetworkConnection(addr, false, NULL, proposal.address.ToStringIPPort().c_str());
        MilliSleep(500);
        pNode = FindNode(proposal.address);
    }
    pNode->PushMessage("dfshandshake", handshake);
}

std::vector<StorageProposal> StorageController::SortProposals(const StorageOrder &order)
{
    std::vector<StorageProposal> proposals = proposalsAgent.GetProposals(order.GetHash());
    if (proposals.size()) {
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

void StorageController::ListenProposal(const uint256 &orderHash)
{
    proposalsAgent.ListenProposal(orderHash);
}

void StorageController::StopListenProposal(const uint256 &orderHash)
{
    proposalsAgent.StopListenProposal(orderHash);
}

std::vector<StorageProposal> StorageController::GetProposals(const uint256 &orderHash)
{
    return proposalsAgent.GetProposals(orderHash);
}

void StorageController::FindReplicaKeepers(const StorageOrder &order, const int countReplica)
{
    std::vector<StorageProposal> proposals = SortProposals(order);
    int numReplica = 0;
    for (auto &&proposal : proposals) {
        StartHandshake(proposal);
        auto it = mapReceivedHandshakes.find(proposal.orderHash);
        for (int times = 0; times < 300 || it != mapReceivedHandshakes.end(); ++times) {
            MilliSleep(100);
            it = mapReceivedHandshakes.find(proposal.orderHash);
        }
        if (it != mapReceivedHandshakes.end()) {
            ++numReplica;
            if (numReplica == countReplica) {
                break;
            }
        } else {
            CNode* pNode = FindNode(proposal.address);
            if (pNode) {
                pNode->CloseSocketDisconnect();
            }
        }
    }
}

void StorageController::CreateReplica(const boost::filesystem::path &filename, const StorageOrder &order, const StorageProposal &proposal)
{
//    void EncryptData(const byte *src, uint64_t offset, size_t srcSize, byte *cipherText,
//                     const AESKey &aesKey, RSA *rsa);
    namespace fs = boost::filesystem;

    std::ifstream filein;
    filein.open(filename.string().c_str(), std::ios::binary);
    if (!filein.is_open()) {
        LogPrint("dfs", "file %s cannot be opened", filename.string());
        return ;
    }

    filein.seekg (0, ios::end);
    uint64_t length = filein.tellg();
    filein.seekg (0, ios::beg);

    int BUFFER_SIZE = 128;
    std::shared_ptr<AllocatedFile> tempFile = tempStorageHeap.AllocateFile(order.fileURI.ToString(), length + BUFFER_SIZE); // TODO: size = (length / BUFFER_SIZE + (length % BUFFER_SIZE != 0)) * BUFFER_SIZE (SS)

    std::ofstream outfile;
    outfile.open(tempFile->filename, std::ios::binary);

    char *buffer = new char[BUFFER_SIZE];
    while(filein.read(buffer, sizeof(buffer)))
    {
        outfile.write(buffer, sizeof(buffer));
    }
    outfile.write(buffer, filein.gcount());

    filein.close();
    outfile.close();

    delete[] buffer;
}
