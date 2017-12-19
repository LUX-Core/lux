// Copyright (c) 2017 The LUX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUX_CSPORKDB_H
#define LUX_CSPORKDB_H

#include <boost/filesystem/path.hpp>
#include "leveldbwrapper.h"
#include "spork.h"

class CSporkDB : public CLevelDBWrapper
{
public:
    CSporkDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

private:
    CSporkDB(const CSporkDB&);
    void operator=(const CSporkDB&);

public:
    bool WriteSpork(const int nSporkId, const CSporkMessage& spork);
    bool ReadSpork(const int nSporkId, CSporkMessage& spork);
    bool SporkExists(const int nSporkId);
};


#endif //LUX_CSPORKDB_H
