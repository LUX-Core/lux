// Copyright (c) 2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LUXGATE_H
#define LUXGATE_H

#include <blockchainclient.h>
#include <main.h>
#include <serialize.h>
#include <uint256.h>
#include <util.h>

typedef uint256 OrderId; 

class COrder;

extern std::map<OrderId, std::shared_ptr<COrder>> orderbook;

void ProcessMessageLuxgate(CNode* pfrom, const std::string& strCommand, CDataStream& vRecv, bool& isLuxgate);


class COrder 
{

public:

    OrderId ComputeId() const;
    CAddress Sender() const { return sender; }
    Ticker Base() const { return base; }
    Ticker Rev() const { return rev; }
    CAmount BaseAmount() const { return baseAmount; }
    CAmount RevAmount() const { return revAmount; }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(base);
        READWRITE(rev);
        READWRITE(baseAmount);
        READWRITE(revAmount);
        READWRITE(sender);
    }

protected:

    Ticker base;
    Ticker rev;
    CAmount baseAmount;
    CAmount revAmount;
    CAddress sender;
};


#endif
