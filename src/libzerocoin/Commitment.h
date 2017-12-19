/**
 * @file       Commitment.h
 *
 * @brief      Commitment and CommitmentProof classes for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017 The LUX developers

#ifndef COMMITMENT_H_
#define COMMITMENT_H_

#include "Params.h"
#include "serialize.h"

// We use a SHA256 hash for our PoK challenges. Update the following
// if we ever change hash functions.
#define COMMITMENT_EQUALITY_CHALLENGE_SIZE  256

// A 512-bit security parameter for the statistical ZK PoK.
#define COMMITMENT_EQUALITY_SECMARGIN       512

namespace libzerocoin {

/**
 * A commitment, complete with contents and opening randomness.
 * These should remain secret. Publish only the commitment value.
 */
class Commitment {
public:
	/**Generates a Pedersen commitment to the given value.
	 *
	 * @param p the group parameters for the coin
	 * @param value the value to commit to
	 */
	Commitment(const IntegerGroupParams* p, const CBigNum& value);
	const CBigNum& getCommitmentValue() const;
	const CBigNum& getRandomness() const;
	const CBigNum& getContents() const;
private:
	const IntegerGroupParams *params;
	CBigNum commitmentValue;
	CBigNum randomness;
	const CBigNum contents;
	ADD_SERIALIZE_METHODS;
  template <typename Stream, typename Operation>  inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
	    READWRITE(commitmentValue);
	    READWRITE(randomness);
	    READWRITE(contents);
	}
};

/**Proof that two commitments open to the same value.
 *
 */
class CommitmentProofOfKnowledge {
public:
	CommitmentProofOfKnowledge(const IntegerGroupParams* ap, const IntegerGroupParams* bp);
	/** Generates a proof that two commitments, a and b, open to the same value.
	 *
	 * @param ap the IntegerGroup for commitment a
	 * @param bp the IntegerGroup for commitment b
	 * @param a the first commitment
	 * @param b the second commitment
	 */
	CommitmentProofOfKnowledge(const IntegerGroupParams* aParams, const IntegerGroupParams* bParams, const Commitment& a, const Commitment& b);
	//FIXME: is it best practice that this is here?
	template<typename Stream>
	CommitmentProofOfKnowledge(const IntegerGroupParams* aParams,
	                           const IntegerGroupParams* bParams, Stream& strm): ap(aParams), bp(bParams)
	{
		strm >> *this;
	}

	const CBigNum calculateChallenge(const CBigNum& a, const CBigNum& b, const CBigNum &commitOne, const CBigNum &commitTwo) const;

	/**Verifies the proof
	 *
	 * @return true if the proof is valid.
	 */
	/**Verifies the proof of equality of the two commitments
	 *
	 * @param A value of commitment one
	 * @param B value of commitment two
	 * @return
	 */
	bool Verify(const CBigNum& A, const CBigNum& B) const;
	ADD_SERIALIZE_METHODS;
  template <typename Stream, typename Operation>  inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
	    READWRITE(S1);
	    READWRITE(S2);
	    READWRITE(S3);
	    READWRITE(challenge);
	}
private:
	const IntegerGroupParams *ap, *bp;

	CBigNum S1, S2, S3, challenge;
};

} /* namespace libzerocoin */
#endif /* COMMITMENT_H_ */
