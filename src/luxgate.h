#ifndef LUXGATE_H
#define LUXGATE_H

#include "main.h"
#include "uint256.h"
#include "util.h"
#include "blockchainclient.h"
#include "serialize.h"

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
