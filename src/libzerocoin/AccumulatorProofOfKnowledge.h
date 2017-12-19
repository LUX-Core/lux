/**
 * @file       AccumulatorProofOfKnowledge.h
 *
 * @brief      AccumulatorProofOfKnowledge class for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017 The LUX developers

#ifndef ACCUMULATEPROOF_H_
#define ACCUMULATEPROOF_H_

#include "Accumulator.h"
#include "Commitment.h"

namespace libzerocoin {

/**A prove that a value insde the commitment commitmentToCoin is in an accumulator a.
 *
 */
class AccumulatorProofOfKnowledge {
public:
	AccumulatorProofOfKnowledge(const AccumulatorAndProofParams* p);

	/** Generates a proof that a commitment to a coin c was accumulated
	 * @param p  Cryptographic parameters
	 * @param commitmentToCoin commitment containing the coin we want to prove is accumulated
	 * @param witness The witness to the accumulation of the coin
	 * @param a
	 */
	AccumulatorProofOfKnowledge(const AccumulatorAndProofParams* p, const Commitment& commitmentToCoin, const AccumulatorWitness& witness, Accumulator& a);
	/** Verifies that  a commitment c is accumulated in accumulated a
	 */
	bool Verify(const Accumulator& a,const CBigNum& valueOfCommitmentToCoin) const;
	
	ADD_SERIALIZE_METHODS;
  template <typename Stream, typename Operation>  inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
	    READWRITE(C_e);
	    READWRITE(C_u);
	    READWRITE(C_r);
	    READWRITE(st_1);
	    READWRITE(st_2);
	    READWRITE(st_3);
	    READWRITE(t_1);
	    READWRITE(t_2);
	    READWRITE(t_3);
	    READWRITE(t_4);
	    READWRITE(s_alpha);
	    READWRITE(s_beta);
	    READWRITE(s_zeta);
	    READWRITE(s_sigma);
	    READWRITE(s_eta);
	    READWRITE(s_epsilon);
	    READWRITE(s_delta);
	    READWRITE(s_xi);
	    READWRITE(s_phi);
	    READWRITE(s_gamma);
	    READWRITE(s_psi);
  }	
private:
	const AccumulatorAndProofParams* params;

	/* Return values for proof */
	CBigNum C_e;
	CBigNum C_u;
	CBigNum C_r;

	CBigNum st_1;
	CBigNum st_2;
	CBigNum st_3;

	CBigNum t_1;
	CBigNum t_2;
	CBigNum t_3;
	CBigNum t_4;

	CBigNum s_alpha;
	CBigNum s_beta;
	CBigNum s_zeta;
	CBigNum s_sigma;
	CBigNum s_eta;
	CBigNum s_epsilon;
	CBigNum s_delta;
	CBigNum s_xi;
	CBigNum s_phi;
	CBigNum s_gamma;
	CBigNum s_psi;
};

} /* namespace libzerocoin */
#endif /* ACCUMULATEPROOF_H_ */
