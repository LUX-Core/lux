/**
* @file       SerialNumberSignatureOfKnowledge.h
*
* @brief      SerialNumberSignatureOfKnowledge class for the Zerocoin library.
*
* @author     Ian Miers, Christina Garman and Matthew Green
* @date       June 2013
*
* @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
* @license    This project is released under the MIT license.
**/
// Copyright (c) 2017 The LUX developers

#ifndef SERIALNUMBERPROOF_H_
#define SERIALNUMBERPROOF_H_

#include <list>
#include <vector>
#include <bitset>
#include "Params.h"
#include "Coin.h"
#include "Commitment.h"
#include "bignum.h"
#include "serialize.h"
#include "Accumulator.h"
#include "hash.h"

using namespace std;
namespace libzerocoin {

/**A Signature of knowledge on the hash of metadata attesting that the signer knows the values
 *  necessary to open a commitment which contains a coin(which it self is of course a commitment)
 * with a given serial number.
 */
class SerialNumberSignatureOfKnowledge {
public:
	SerialNumberSignatureOfKnowledge(const ZerocoinParams* p);
	/** Creates a Signature of knowledge object that a commitment to a coin contains a coin with serial number x
	 *
	 * @param p params
	 * @param coin the coin we are going to prove the serial number of.
	 * @param commitmentToCoin the commitment to the coin
	 * @param msghash hash of meta data to create a signature of knowledge on.
	 */
	SerialNumberSignatureOfKnowledge(const ZerocoinParams* p, const PrivateCoin& coin, const Commitment& commitmentToCoin, uint256 msghash);

	/** Verifies the Signature of knowledge.
	 *
	 * @param msghash hash of meta data to create a signature of knowledge on.
	 * @return
	 */
	bool Verify(const CBigNum& coinSerialNumber, const CBigNum& valueOfCommitmentToCoin,const uint256 msghash) const;
	ADD_SERIALIZE_METHODS;
  template <typename Stream, typename Operation>  inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
	    READWRITE(s_notprime);
	    READWRITE(sprime);
	    READWRITE(hash);
	}
private:
	const ZerocoinParams* params;
	// challenge hash
	uint256 hash; //TODO For efficiency, should this be a bitset where Templates define params?

	// challenge response values
	// this is s_notprime instead of s
	// because the serialization macros
	// define something named s and it conflicts
	vector<CBigNum> s_notprime;
	vector<CBigNum> sprime;
	inline CBigNum challengeCalculation(const CBigNum& a_exp, const CBigNum& b_exp,
	                                   const CBigNum& h_exp) const;
};

} /* namespace libzerocoin */
#endif /* SERIALNUMBERPROOF_H_ */
