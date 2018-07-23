// Copyright (c) 2012 Pieter Wuille
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "addrman.h"

#include "hash.h"
#include "serialize.h"
#include "streams.h"

using namespace std;

int CAddrInfo::GetTriedBucket(const uint256& nKey) const
{
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0) << nKey << GetKey()).GetHash().GetLow64();
    uint64_t hash2 = (CHashWriter(SER_GETHASH, 0) << nKey << GetGroup() << (hash1 % ADDRMAN_TRIED_BUCKETS_PER_GROUP)).GetHash().GetLow64();
    return hash2 % ADDRMAN_TRIED_BUCKET_COUNT;
}

int CAddrInfo::GetNewBucket(const uint256& nKey, const CNetAddr& src) const
{
    std::vector<unsigned char> vchSourceGroupKey = src.GetGroup();
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0) << nKey << GetGroup() << vchSourceGroupKey).GetHash().GetLow64();
    uint64_t hash2 = (CHashWriter(SER_GETHASH, 0) << nKey << vchSourceGroupKey << (hash1 % ADDRMAN_NEW_BUCKETS_PER_SOURCE_GROUP)).GetHash().GetLow64();
    return hash2 % ADDRMAN_NEW_BUCKET_COUNT;
}

int CAddrInfo::GetBucketPosition(const uint256& nKey, bool fNew, int nBucket) const
{
    uint64_t hash1 = (CHashWriter(SER_GETHASH, 0) << nKey << (fNew ? 'N' : 'K') << nBucket << GetKey()).GetHash().GetLow64();
    return hash1 % ADDRMAN_BUCKET_SIZE;
}

bool CAddrInfo::IsTerrible(int64_t nNow) const
{
    if (nLastTry && nLastTry >= nNow - 60) // never remove things tried in the last minute
        return false;

    if (nTime > nNow + 10 * 60) // came in a flying DeLorean
        return true;

    if (nTime == 0 || nNow - nTime > ADDRMAN_HORIZON_DAYS * 24 * 60 * 60) // not seen in recent history
        return true;

    if (nLastSuccess == 0 && nAttempts >= ADDRMAN_RETRIES) // tried N times and never a success
        return true;

    if (nNow - nLastSuccess > ADDRMAN_MIN_FAIL_DAYS * 24 * 60 * 60 && nAttempts >= ADDRMAN_MAX_FAILURES) // N successive failures in the last week
        return true;

    return false;
}

double CAddrInfo::GetChance(int64_t nNow) const
{
    double fChance = 1.0;

    int64_t nSinceLastSeen = nNow - nTime;
    int64_t nSinceLastTry = nNow - nLastTry;

    if (nSinceLastSeen < 0)
        nSinceLastSeen = 0;
    if (nSinceLastTry < 0)
        nSinceLastTry = 0;

    // deprioritize very recent attempts away
    if (nSinceLastTry < 60 * 10)
        fChance *= 0.01;

    // deprioritize 66% after each failed attempt, but at most 1/28th to avoid the search taking forever or overly penalizing outages.
    fChance *= pow(0.66, min(nAttempts, 8));

    return fChance;
}

CAddrInfo* CAddrMan::Find(const CNetAddr& addr, int* pnId)
{
    std::map<CNetAddr, int>::iterator it = mapAddr.find(addr);
    if (it == mapAddr.end())
        return NULL;
    if (pnId)
        *pnId = (*it).second;
    std::map<int, CAddrInfo>::iterator it2 = mapInfo.find((*it).second);
    if (it2 != mapInfo.end())
        return &(*it2).second;
    return NULL;
}

#ifdef I2PADDRMAN_EXTENSIONS
/** \brief We sometimes have a vast reservoir of i2p destinations, b32.i2p addresses are simply the hash
of those addresses, so we should be able to look them up here, if we already have the base64 string,
we're done and never have to ask the router to lookup the destination.
 *
 * \param sB32addr const string&
 * \return CAddrInfo*
 *
 */
CAddrInfo* CAddrMan::LookupB32addr(const std::string& sB32addr)
{
    // Convert the string back into a hash, and look it up in the AddrMan map
    if( isValidI2pB32(sB32addr) ) {
        bool fValid;
        std::vector<unsigned char> vchHash;
        std::string sHash = sB32addr;
        sHash.resize( sB32addr.size() - 8 );
        vchHash = DecodeBase32( sHash.c_str(), &fValid );
        uint256 uintHash( vchHash );        // Hate wasting time copying object, but vectors, strings and uint256 values dont always pass compiler checks otherwise
        if( fValid ) {                      // Lookup the hash for a match, if found we have the CAddrInfo id
            std::map<uint256, int>::iterator it = mapI2pHashes.find( uintHash );
            if( it != mapI2pHashes.end() ) {
                std::map<int, CAddrInfo>::iterator it2 = mapInfo.find((*it).second);
                if (it2 != mapInfo.end())
                    return &(*it2).second;
            }
        }
    }
    return NULL;
}

/** \brief Simply looks up the hash, if the id is found returns a pointer to the
            CAddrInfo(CAddress(CService(CNetAddr()))) class object where the base64 string is stored
 *
 * \param sB32addr const string&
 * \return string, null if not found or the Base64 destination string of give b32.i2p address
 *
 */
std::string CAddrMan::GetI2pBase64Destination(const std::string& sB32addr)
{
    CAddrInfo* paddr = LookupB32addr(sB32addr);
    return  paddr && paddr->IsI2P() ? paddr->GetI2pDestination() : std::string();
}

// Returns the number of entries processed
int CAddrMan::CopyDestinationStats( std::vector<CDestinationStats>& vStats )
{
    int nSize = 0;
    vStats.clear();
    vStats.reserve( mapI2pHashes.size() );
    for( std::map<uint256, int>::iterator it = mapI2pHashes.begin(); it != mapI2pHashes.end(); it++) {
        CDestinationStats stats;
        std::map<int, CAddrInfo>::iterator it2 = mapInfo.find((*it).second);
        if (it2 != mapInfo.end()) {
            CAddrInfo* paddr = &(*it2).second;
            stats.sAddress = paddr->ToString();
            stats.fInTried = paddr->fInTried;
            stats.uPort = paddr->GetPort();
            stats.nServices = paddr->nServices;
            stats.nAttempts = paddr->nAttempts;
            stats.nLastTry = paddr->nLastTry;
            stats.nSuccessTime = paddr->nLastSuccess;
            stats.sSource = paddr->source.ToString();
            stats.sBase64 = paddr->GetI2pDestination();
            nSize++;
            vStats.push_back( stats );
        }
    }
    assert( mapI2pHashes.size() == static_cast<unsigned long>(nSize) );
    return nSize;
}

#endif // I2PADDRMAN_EXTENSIONS

CAddrInfo* CAddrMan::Create(const CAddress& addr, const CNetAddr& addrSource, int* pnId)
{
    int nId = nIdCount++;
    mapInfo[nId] = CAddrInfo(addr, addrSource);
    mapAddr[addr] = nId;
    mapInfo[nId].nRandomPos = vRandom.size();
    vRandom.push_back(nId);
    if (pnId)
        *pnId = nId;
#ifdef I2PADDRMAN_EXTENSIONS
    if( addr.IsI2P() ) {
        assert( addr.IsNativeI2P() );
        uint256 b32hash = GetI2pDestinationHash( addr.GetI2pDestination() );
        if( mapI2pHashes.count( b32hash ) == 0 )
            mapI2pHashes[b32hash] = nId;
        else
            LogPrintf( "ERROR - Can't create base32 Hash in AddrMan for one that already exists\n");
    }
#endif
    return &mapInfo[nId];
}

void CAddrMan::SwapRandom(unsigned int nRndPos1, unsigned int nRndPos2)
{
    if (nRndPos1 == nRndPos2)
        return;

    assert(nRndPos1 < vRandom.size() && nRndPos2 < vRandom.size());

    int nId1 = vRandom[nRndPos1];
    int nId2 = vRandom[nRndPos2];

    assert(mapInfo.count(nId1) == 1);
    assert(mapInfo.count(nId2) == 1);

    mapInfo[nId1].nRandomPos = nRndPos2;
    mapInfo[nId2].nRandomPos = nRndPos1;

    vRandom[nRndPos1] = nId2;
    vRandom[nRndPos2] = nId1;
}

void CAddrMan::Delete(int nId)
{
    assert(mapInfo.count(nId) != 0);
    CAddrInfo& info = mapInfo[nId];
    assert(!info.fInTried);
    assert(info.nRefCount == 0);

    SwapRandom(info.nRandomPos, vRandom.size() - 1);
    vRandom.pop_back();
    mapAddr.erase(info);
    mapInfo.erase(nId);
    nNew--;
}

#ifdef I2PADDRMAN_EXTENSIONS
void CAddrMan::CheckAndDeleteB32Hash( const int nID, const CAddrInfo& aTerrible )
{
    if( aTerrible.IsI2P() ) {
        uint256 b32hash = GetI2pDestinationHash( aTerrible.GetI2pDestination() );
        if( mapI2pHashes.count( b32hash ) == 1 ) {
            int nID2 = mapI2pHashes[ b32hash ];
            if( nID == nID2 )            // Yap this is the one they want to delete, and it exists
                mapI2pHashes.erase( b32hash );
            else {
                LogPrint( "addrman", "While attempting to erase base32 hash %s, it was unexpected that the ids differ id1=%d != id2=%d\n", b32hash.GetHex(), nID, nID2 );
                // CAddrInfo& info2 = mapInfo[nID2];
                // aTerrible.print();
                // info2.print();
            }
        }
        else {
            LogPrint( "addrman", "While attempting to remove base32 hash %s, it was found to not exist.\n", b32hash.GetHex() );
            // aTerrible.print();
        }
    }
}
#endif // I2PADDRMAN_EXTENSIONS

void CAddrMan::ClearNew(int nUBucket, int nUBucketPos)
{
    // if there is an entry in the specified bucket, delete it.
    if (vvNew[nUBucket][nUBucketPos] != -1) {
    int nIdDelete = vvNew[nUBucket][nUBucketPos];
    CAddrInfo& infoDelete = mapInfo[nIdDelete];
    assert(infoDelete.nRefCount > 0);
    infoDelete.nRefCount--;
    vvNew[nUBucket][nUBucketPos] = -1;
    if (infoDelete.nRefCount == 0) {
#ifdef I2PADDRMAN_EXTENSIONS
    CheckAndDeleteB32Hash( nIdDelete, infoDelete);
#endif
    Delete(nIdDelete);
        }
    }
}

void CAddrMan::MakeTried(CAddrInfo& info, int nId)
{
    // remove the entry from all new buckets
    for (int bucket = 0; bucket < ADDRMAN_NEW_BUCKET_COUNT; bucket++) {
        int pos = info.GetBucketPosition(nKey, true, bucket);
        if (vvNew[bucket][pos] == nId) {
            vvNew[bucket][pos] = -1;
            info.nRefCount--;
        }
    }
    nNew--;

    assert(info.nRefCount == 0);

    // which tried bucket to move the entry to
    int nKBucket = info.GetTriedBucket(nKey);
    int nKBucketPos = info.GetBucketPosition(nKey, false, nKBucket);

    // first make space to add it (the existing tried entry there is moved to new, deleting whatever is there).
    if (vvTried[nKBucket][nKBucketPos] != -1) {
        // find an item to evict
        int nIdEvict = vvTried[nKBucket][nKBucketPos];
        assert(mapInfo.count(nIdEvict) == 1);
        CAddrInfo& infoOld = mapInfo[nIdEvict];

        // Remove the to-be-evicted item from the tried set.
        infoOld.fInTried = false;
        vvTried[nKBucket][nKBucketPos] = -1;
        nTried--;

        // find which new bucket it belongs to
        int nUBucket = infoOld.GetNewBucket(nKey);
        int nUBucketPos = infoOld.GetBucketPosition(nKey, true, nUBucket);
        ClearNew(nUBucket, nUBucketPos);
        assert(vvNew[nUBucket][nUBucketPos] == -1);

        // Enter it into the new set again.
        infoOld.nRefCount = 1;
        vvNew[nUBucket][nUBucketPos] = nIdEvict;
        nNew++;
    }
    assert(vvTried[nKBucket][nKBucketPos] == -1);

    vvTried[nKBucket][nKBucketPos] = nId;
    nTried++;
    info.fInTried = true;
}

void CAddrMan::Good_(const CService& addr, int64_t nTime)
{
    int nId;
    CAddrInfo* pinfo = Find(addr, &nId);

    // if not found, bail out
    if (!pinfo)
        return;

    CAddrInfo& info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    info.nLastSuccess = nTime;
    info.nLastTry = nTime;
    info.nAttempts = 0;
    // nTime is not updated here, to avoid leaking information about
    // currently-connected peers.

    // if it is already in the tried set, don't do anything else
    if (info.fInTried)
        return;

    // find a bucket it is in now
    int nRnd = GetRandInt(ADDRMAN_NEW_BUCKET_COUNT);
    int nUBucket = -1;
    for (unsigned int n = 0; n < ADDRMAN_NEW_BUCKET_COUNT; n++) {
        int nB = (n + nRnd) % ADDRMAN_NEW_BUCKET_COUNT;
        int nBpos = info.GetBucketPosition(nKey, true, nB);
        if (vvNew[nB][nBpos] == nId) {
            nUBucket = nB;
            break;
        }
    }

    // if no bucket is found, something bad happened;
    // TODO: maybe re-add the node, but for now, just bail out
    if (nUBucket == -1)
        return;

    LogPrint("addrman", "Moving %s to tried\n", addr.ToString());

    // move nId to the tried tables
    MakeTried(info, nId);
}

bool CAddrMan::Add_(const CAddress& addrIn, const CNetAddr& source, int64_t nTimePenalty)
{
#ifdef I2PADDRMAN_EXTENSIONS
    //! We now need to check for an possibly modify the CAddress object for the garliccat field, so we make a local copy
    CAddress addr = addrIn;
    /**
     * Before we can add an address, even before we can test if its Routable, or use the Find command to match correctly,
     * we need to make sure that any I2P addresses have the GarlicCat field setup correctly in the IP area of the
     * CNetAddr portion of a given CAddress->CService->CNetAddr object, this should have already been done, but
     * double checking it here also insures we do not get a polluted b32 hash map
     */
    if( addr.CheckAndSetGarlicCat() )
        LogPrint( "addrman", "While adding an i2p destination, did not expect to need the garliccat fixed for %s\n", addr.ToString() );
#endif
    if( !addr.IsRoutable() ) {
        LogPrint( "addrman", "While adding an address, did not expect to find it unroutable: %s\n", addr.ToString() );
        return false;
    }

    bool fNew = false;
    int nId;
    CAddrInfo* pinfo = Find(addr, &nId);

    if (pinfo) {
        // periodically update nTime
        bool fCurrentlyOnline = (GetAdjustedTime() - addr.nTime < 24 * 60 * 60);
        int64_t nUpdateInterval = (fCurrentlyOnline ? 60 * 60 : 24 * 60 * 60);
        if (addr.nTime && (!pinfo->nTime || pinfo->nTime < addr.nTime - nUpdateInterval - nTimePenalty))
            pinfo->nTime = max((int64_t)0, addr.nTime - nTimePenalty);

        // add services
        pinfo->nServices = ServiceFlags(pinfo->nServices | addr.nServices);

        // do not update if no new information is present
        if (!addr.nTime || (pinfo->nTime && addr.nTime <= pinfo->nTime))
            return false;

        // do not update if the entry was already in the "tried" table
        if (pinfo->fInTried)
            return false;

        // do not update if the max reference count is reached
        if (pinfo->nRefCount == ADDRMAN_NEW_BUCKETS_PER_ADDRESS)
            return false;

        // stochastic test: previous nRefCount == N: 2^N times harder to increase it
        int nFactor = 1;
        for (int n = 0; n < pinfo->nRefCount; n++)
            nFactor *= 2;
        if (nFactor > 1 && (GetRandInt(nFactor) != 0))
            return false;
    } else {
        pinfo = Create(addr, source, &nId);
        pinfo->nTime = max((int64_t)0, (int64_t)pinfo->nTime - nTimePenalty);
        nNew++;
        fNew = true;
    }

    int nUBucket = pinfo->GetNewBucket(nKey, source);
    int nUBucketPos = pinfo->GetBucketPosition(nKey, true, nUBucket);
    if (vvNew[nUBucket][nUBucketPos] != nId) {
        bool fInsert = vvNew[nUBucket][nUBucketPos] == -1;
        if (!fInsert) {
            CAddrInfo& infoExisting = mapInfo[vvNew[nUBucket][nUBucketPos]];
            if (infoExisting.IsTerrible() || (infoExisting.nRefCount > 1 && pinfo->nRefCount == 0)) {
                // Overwrite the existing new table entry.
                fInsert = true;
            }
        }
        if (fInsert) {
            ClearNew(nUBucket, nUBucketPos);
            pinfo->nRefCount++;
            vvNew[nUBucket][nUBucketPos] = nId;
        } else {
            if (pinfo->nRefCount == 0) {
                Delete(nId);
            }
        }
    }
    return fNew;
}

void CAddrMan::Attempt_(const CService& addr, int64_t nTime)
{
    CAddrInfo* pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    CAddrInfo& info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    info.nLastTry = nTime;
    info.nAttempts++;
}

CAddress CAddrMan::Select_()
{
    if (size() == 0)
        return CAddress();

    // Use a 50% chance for choosing between tried and new table entries.
    if (nTried > 0 && (nNew == 0 || GetRandInt(2) == 0)) {
        // use a tried node
        double fChanceFactor = 1.0;
        while (1) {
            int nKBucket = GetRandInt(ADDRMAN_TRIED_BUCKET_COUNT);
            int nKBucketPos = GetRandInt(ADDRMAN_BUCKET_SIZE);
            if (vvTried[nKBucket][nKBucketPos] == -1)
                continue;
            int nId = vvTried[nKBucket][nKBucketPos];
            assert(mapInfo.count(nId) == 1);
            CAddrInfo& info = mapInfo[nId];
            if (GetRandInt(1 << 30) < fChanceFactor * info.GetChance() * (1 << 30))
                return info;
            fChanceFactor *= 1.2;
        }
    } else {
        // use a new node
        double fChanceFactor = 1.0;
        while (1) {
            int nUBucket = GetRandInt(ADDRMAN_NEW_BUCKET_COUNT);
            int nUBucketPos = GetRandInt(ADDRMAN_BUCKET_SIZE);
            if (vvNew[nUBucket][nUBucketPos] == -1)
                continue;
            int nId = vvNew[nUBucket][nUBucketPos];
            assert(mapInfo.count(nId) == 1);
            CAddrInfo& info = mapInfo[nId];
            if (GetRandInt(1 << 30) < fChanceFactor * info.GetChance() * (1 << 30))
                return info;
            fChanceFactor *= 1.2;
        }
    }
}

#ifdef DEBUG_ADDRMAN
int CAddrMan::Check_()
{
    std::set<int> setTried;
    std::map<int, int> mapNew;

    if (vRandom.size() != nTried + nNew)
        return -7;

    for (std::map<int, CAddrInfo>::iterator it = mapInfo.begin(); it != mapInfo.end(); it++) {
        int n = (*it).first;
        CAddrInfo& info = (*it).second;
        if (info.fInTried) {
            if (!info.nLastSuccess)
                return -1;
            if (info.nRefCount)
                return -2;
            setTried.insert(n);
        } else {
            if (info.nRefCount < 0 || info.nRefCount > ADDRMAN_NEW_BUCKETS_PER_ADDRESS)
                return -3;
            if (!info.nRefCount)
                return -4;
            mapNew[n] = info.nRefCount;
        }
        if (mapAddr[info] != n)
            return -5;
        if (info.nRandomPos < 0 || info.nRandomPos >= vRandom.size() || vRandom[info.nRandomPos] != n)
            return -14;
        if (info.nLastTry < 0)
            return -6;
        if (info.nLastSuccess < 0)
            return -8;
    }

    if (setTried.size() != nTried)
        return -9;
    if (mapNew.size() != nNew)
        return -10;

    for (int n = 0; n < ADDRMAN_TRIED_BUCKET_COUNT; n++) {
        for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
            if (vvTried[n][i] != -1) {
                if (!setTried.count(vvTried[n][i]))
                    return -11;
                if (mapInfo[vvTried[n][i]].GetTriedBucket(nKey) != n)
                    return -17;
                if (mapInfo[vvTried[n][i]].GetBucketPosition(nKey, false, n) != i)
                    return -18;
                setTried.erase(vvTried[n][i]);
            }
        }
    }

    for (int n = 0; n < ADDRMAN_NEW_BUCKET_COUNT; n++) {
        for (int i = 0; i < ADDRMAN_BUCKET_SIZE; i++) {
            if (vvNew[n][i] != -1) {
                if (!mapNew.count(vvNew[n][i]))
                    return -12;
                if (mapInfo[vvNew[n][i]].GetBucketPosition(nKey, true, n) != i)
                    return -19;
                if (--mapNew[vvNew[n][i]] == 0)
                    mapNew.erase(vvNew[n][i]);
            }
        }
    }

    if (setTried.size())
        return -13;
    if (mapNew.size())
        return -15;
    if (nKey.IsNull())
        return -16;

    return 0;
}
#endif

#ifdef I2PADDRMAN_EXTENSIONS
void CAddrMan::GetAddr_(std::vector<CAddress>& vAddr, const bool fIpOnly, const bool fI2pOnly)
#else
void CAddrMan::GetAddr_(std::vector<CAddress>& vAddr)
#endif
{
    unsigned int nNodes = ADDRMAN_GETADDR_MAX_PCT * vRandom.size() / 100;
    if (nNodes > ADDRMAN_GETADDR_MAX)
        nNodes = ADDRMAN_GETADDR_MAX;

    // gather a list of random nodes, skipping those of low quality
    for (unsigned int n = 0; n < vRandom.size(); n++) {
        if (vAddr.size() >= nNodes)
            break;

        int nRndPos = GetRandInt(vRandom.size() - n) + n;
        SwapRandom(n, nRndPos);
        assert(mapInfo.count(vRandom[n]) == 1);

        const CAddrInfo& ai = mapInfo[vRandom[n]];
        //! Don't send terrible addresses in response to GetAddr requests
//#ifdef I2PADDRMAN_EXTENSIONS
        //! Additional checks, don't send addresses to nodes that cant process them or don't care about them.
        //! Inclusion of a check for RFC1918() addresses, means any local whitelisted peers that have made
        //! it into the address manager and are RFC1918 IPs will not be globally shared with peers on the
        //! Anoncoin network, originally done for software testing, now seems like a good idea to leave it.
        //! CSlave: There is a logical error in the following conditional string, (!fIpOnly || !ai.IsI2P())
        //! always return false and false so the addresses were never shared to I2P peers. Probably the
        //! fIpOnly check is flawed as it always return true even for I2P only peers. Furthermore I think
        //! there is no harm to share both IP and I2P address to all peers, hence those conditions are removed.
        //if( !ai.IsTerrible() && !ai.IsRFC1918() && (!fIpOnly || !ai.IsI2P()) && (!fI2pOnly || ai.IsI2P()) )
        if (!ai.IsTerrible() && !ai.IsRFC1918() )
//#else
        //if(!ai.IsTerrible())
//#endif
            vAddr.push_back(ai);
    }
}

void CAddrMan::Connected_(const CService& addr, int64_t nTime)
{
    CAddrInfo* pinfo = Find(addr);

    // if not found, bail out
    if (!pinfo)
        return;

    CAddrInfo& info = *pinfo;

    // check whether we are talking about the exact same CService (including same port)
    if (info != addr)
        return;

    // update info
    int64_t nUpdateInterval = 20 * 60;
    if (nTime - info.nTime > nUpdateInterval)
        info.nTime = nTime;
}
