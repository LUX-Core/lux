// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_UTIL_H
#define BITCOIN_RPC_UTIL_H

#include <string>
#include <vector>

class CKeyStore;
class CPubKey;
class CScript;

CPubKey HexToPubKey(const std::string& hex_in);
CPubKey AddrToPubKey(CKeyStore* const keystore, const std::string& addr_in);
CScript CreateMultisigRedeemscript(const int required, const std::vector<CPubKey>& pubkeys);

#endif // BITCOIN_RPC_UTIL_H
