// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once
#include "arith_uint256.h"
#include "uint256.h"

/** 512-bit unsigned big integer. */
class uint512 : public base_blob<512>
{
public:
    uint512() {}
    uint512(const base_blob<512>& b) : base_blob<512>(b) {}
    //explicit uint512(const std::vector<unsigned char>& vch) : base_uint<512>(vch) {}
    explicit uint512(const std::vector<unsigned char>& vch) : base_blob<512>(vch) {}
    //explicit uint512(const std::string& str) : base_blob<512>(str) {}

    uint256 trim256() const
    {
        std::vector<unsigned char> vch;
        const unsigned char* p = this->begin();
        for (unsigned int i = 0; i < 32; i++) {
            vch.push_back(*p++);
        }
        uint256 retval(vch);
        return retval;
    }
};


/* uint256 from const char *.
 * This is a separate function because the constructor uint256(const char*) can result
 * in dangerously catching uint256(0).
 */
inline uint512 uint512S(const char* str)
{
    uint512 rv;
    rv.SetHex(str);
    return rv;
}
