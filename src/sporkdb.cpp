// Copyright (c) 2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sporkdb.h"
#include "spork.h"

CSporkDB::CSporkDB(size_t nCacheSize, bool fMemory, bool fWipe) : CLevelDBWrapper(GetDataDir() / "sporks", nCacheSize, fMemory, fWipe) {}

bool CSporkDB::WriteSpork(const int nSporkId, const CSporkMessage& spork)
{
    LogPrintf("Wrote spork %s to database\n", sporkManager.GetSporkNameByID(nSporkId));
    return Write(nSporkId, spork);

}

bool CSporkDB::ReadSpork(const int nSporkId, CSporkMessage& spork)
{
    return Read(nSporkId, spork);
}

bool CSporkDB::SporkExists(const int nSporkId)
{
    return Exists(nSporkId);
}
