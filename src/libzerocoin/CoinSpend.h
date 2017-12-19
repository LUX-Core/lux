/**
 * @file       CoinSpend.h
 *
 * @brief      CoinSpend class for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017 The LUX developers

#ifndef COINSPEND_H_
#define COINSPEND_H_

#include "Accumulator.h"
#include "AccumulatorProofOfKnowledge.h"
#include "Coin.h"
#include "Commitment.h"
#include "Params.h"
#include "SerialNumberSignatureOfKnowledge.h"
#include "bignum.h"
#include "serialize.h"

namespace libzerocoin
{
/** The complete proof needed to spend a zerocoin.
 * Composes together a proof that a coin is accumulated
 * and that it has a given serial number.
 */
class CoinSpend
{
public:
    template <typename Stream>
    CoinSpend(const ZerocoinParams* p, Stream& strm) : accumulatorPoK(&p->accumulatorParams),
                                                       serialNumberSoK(p),
                                                       commitmentPoK(&p->serialNumberSoKCommitmentGroup, &p->accumulatorParams.accumulatorPoKCommitmentGroup)
    {
        strm >> *this;
    }
    /**Generates a proof spending a zerocoin.
	 *
	 * To use this, provide an unspent PrivateCoin, the latest Accumulator
	 * (e.g from the most recent Bitcoin block) containing the public part
	 * of the coin, a witness to that, and whatever medeta data is needed.
	 *
	 * Once constructed, this proof can be serialized and sent.
	 * It is validated simply be calling validate.
	 * @warning Validation only checks that the proof is correct
	 * @warning for the specified values in this class. These values must be validated
	 *  Clients ought to check that
	 * 1) params is the right params
	 * 2) the accumulator actually is in some block
	 * 3) that the serial number is unspent
	 * 4) that the transaction
	 *
	 * @param p cryptographic parameters
	 * @param coin The coin to be spend
	 * @param a The current accumulator containing the coin
	 * @param witness The witness showing that the accumulator contains the coin
	 * @param a hash of the partial transaction that contains this coin spend
	 * @throw ZerocoinException if the process fails
	 */
    CoinSpend(const ZerocoinParams* p, const PrivateCoin& coin, Accumulator& a, const uint32_t checksum, const AccumulatorWitness& witness, const uint256& ptxHash);

    /** Returns the serial number of the coin spend by this proof.
	 *
	 * @return the coin's serial number
	 */
    const CBigNum& getCoinSerialNumber() const { return this->coinSerialNumber; }

    /**Gets the denomination of the coin spent in this proof.
	 *
	 * @return the denomination
	 */
    CoinDenomination getDenomination() const { return this->denomination; }

    /**Gets the checksum of the accumulator used in this proof.
	 *
	 * @return the checksum
	 */
    uint32_t getAccumulatorChecksum() const { return this->accChecksum; }

    /**Gets the txout hash used in this proof.
	 *
	 * @return the txout hash
	 */
    uint256 getTxOutHash() const { return ptxHash; }
    CBigNum getAccCommitment() const { return accCommitmentToCoinValue; }
    CBigNum getSerialComm() const { return serialCommitmentToCoinValue; }

    bool Verify(const Accumulator& a) const;
    bool HasValidSerial(ZerocoinParams* params) const;
    CBigNum CalculateValidSerial(ZerocoinParams* params);

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(denomination);
        READWRITE(ptxHash);
        READWRITE(accChecksum);
        READWRITE(accCommitmentToCoinValue);
        READWRITE(serialCommitmentToCoinValue);
        READWRITE(coinSerialNumber);
        READWRITE(accumulatorPoK);
        READWRITE(serialNumberSoK);
        READWRITE(commitmentPoK);
    }

private:
    const uint256 signatureHash() const;
    CoinDenomination denomination;
    uint32_t accChecksum;
    uint256 ptxHash;
    CBigNum accCommitmentToCoinValue;
    CBigNum serialCommitmentToCoinValue;
    CBigNum coinSerialNumber;
    AccumulatorProofOfKnowledge accumulatorPoK;
    SerialNumberSignatureOfKnowledge serialNumberSoK;
    CommitmentProofOfKnowledge commitmentPoK;
};

} /* namespace libzerocoin */
#endif /* COINSPEND_H_ */
