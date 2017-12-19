/**
 * @file       AccumulatorProofOfKnowledge.cpp
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
#include "AccumulatorProofOfKnowledge.h"
#include "hash.h"

namespace libzerocoin {

AccumulatorProofOfKnowledge::AccumulatorProofOfKnowledge(const AccumulatorAndProofParams* p): params(p) {}

AccumulatorProofOfKnowledge::AccumulatorProofOfKnowledge(const AccumulatorAndProofParams* p,
        const Commitment& commitmentToCoin, const AccumulatorWitness& witness,
        Accumulator& a): params(p) {

	CBigNum sg = params->accumulatorPoKCommitmentGroup.g;
	CBigNum sh = params->accumulatorPoKCommitmentGroup.h;

	CBigNum g_n = params->accumulatorQRNCommitmentGroup.g;
	CBigNum h_n = params->accumulatorQRNCommitmentGroup.h;

	CBigNum e = commitmentToCoin.getContents();
	CBigNum r = commitmentToCoin.getRandomness();

	CBigNum aM_4 = params->accumulatorModulus/CBigNum((long)4);
	
	CBigNum r_1 = CBigNum::randBignum(params->accumulatorModulus/4);
	CBigNum r_2 = CBigNum::randBignum(params->accumulatorModulus/4);
	CBigNum r_3 = CBigNum::randBignum(params->accumulatorModulus/4);

	this->C_e = g_n.pow_mod(e, params->accumulatorModulus) * h_n.pow_mod(r_1, params->accumulatorModulus);
	this->C_u = witness.getValue() * h_n.pow_mod(r_2, params->accumulatorModulus);
	this->C_r = g_n.pow_mod(r_2, params->accumulatorModulus) * h_n.pow_mod(r_3, params->accumulatorModulus);

	CBigNum r_alpha = CBigNum::randBignum(params->maxCoinValue * CBigNum(2).pow(params->k_prime + params->k_dprime));
	if(!(CBigNum::randBignum(CBigNum(3)) % 2)) {
		r_alpha = 0-r_alpha;
	}

	CBigNum r_gamma = CBigNum::randBignum(params->accumulatorPoKCommitmentGroup.modulus);
	CBigNum r_phi = CBigNum::randBignum(params->accumulatorPoKCommitmentGroup.modulus);
	CBigNum r_psi = CBigNum::randBignum(params->accumulatorPoKCommitmentGroup.modulus);
	CBigNum r_sigma = CBigNum::randBignum(params->accumulatorPoKCommitmentGroup.modulus);
	CBigNum r_xi = CBigNum::randBignum(params->accumulatorPoKCommitmentGroup.modulus);

	CBigNum r_epsilon =  CBigNum::randBignum((params->accumulatorModulus/4) * CBigNum(2).pow(params->k_prime + params->k_dprime));
	if(!(CBigNum::randBignum(CBigNum(3)) % 2)) {
		r_epsilon = 0-r_epsilon;
	}
	CBigNum r_eta = CBigNum::randBignum((params->accumulatorModulus/4) * CBigNum(2).pow(params->k_prime + params->k_dprime));
	if(!(CBigNum::randBignum(CBigNum(3)) % 2)) {
		r_eta = 0-r_eta;
	}
	CBigNum r_zeta = CBigNum::randBignum((params->accumulatorModulus/4) * CBigNum(2).pow(params->k_prime + params->k_dprime));
	if(!(CBigNum::randBignum(CBigNum(3)) % 2)) {
		r_zeta = 0-r_zeta;
	}

	CBigNum r_beta = CBigNum::randBignum((params->accumulatorModulus/4) * params->accumulatorPoKCommitmentGroup.modulus * CBigNum(2).pow(params->k_prime + params->k_dprime));
	if(!(CBigNum::randBignum(CBigNum(3)) % 2)) {
		r_beta = 0-r_beta;
	}
	CBigNum r_delta = CBigNum::randBignum((params->accumulatorModulus/4) * params->accumulatorPoKCommitmentGroup.modulus * CBigNum(2).pow(params->k_prime + params->k_dprime));
	if(!(CBigNum::randBignum(CBigNum(3)) % 2)) {
		r_delta = 0-r_delta;
	}

	this->st_1 = (sg.pow_mod(r_alpha, params->accumulatorPoKCommitmentGroup.modulus) * sh.pow_mod(r_phi, params->accumulatorPoKCommitmentGroup.modulus)) % params->accumulatorPoKCommitmentGroup.modulus;
	this->st_2 = (((commitmentToCoin.getCommitmentValue() * sg.inverse(params->accumulatorPoKCommitmentGroup.modulus)).pow_mod(r_gamma, params->accumulatorPoKCommitmentGroup.modulus)) * sh.pow_mod(r_psi, params->accumulatorPoKCommitmentGroup.modulus)) % params->accumulatorPoKCommitmentGroup.modulus;
	this->st_3 = ((sg * commitmentToCoin.getCommitmentValue()).pow_mod(r_sigma, params->accumulatorPoKCommitmentGroup.modulus) * sh.pow_mod(r_xi, params->accumulatorPoKCommitmentGroup.modulus)) % params->accumulatorPoKCommitmentGroup.modulus;

	this->t_1 = (h_n.pow_mod(r_zeta, params->accumulatorModulus) * g_n.pow_mod(r_epsilon, params->accumulatorModulus)) % params->accumulatorModulus;
	this->t_2 = (h_n.pow_mod(r_eta, params->accumulatorModulus) * g_n.pow_mod(r_alpha, params->accumulatorModulus)) % params->accumulatorModulus;
	this->t_3 = (C_u.pow_mod(r_alpha, params->accumulatorModulus) * ((h_n.inverse(params->accumulatorModulus)).pow_mod(r_beta, params->accumulatorModulus))) % params->accumulatorModulus;
	this->t_4 = (C_r.pow_mod(r_alpha, params->accumulatorModulus) * ((h_n.inverse(params->accumulatorModulus)).pow_mod(r_delta, params->accumulatorModulus)) * ((g_n.inverse(params->accumulatorModulus)).pow_mod(r_beta, params->accumulatorModulus))) % params->accumulatorModulus;

	CHashWriter hasher(0,0);
	hasher << *params << sg << sh << g_n << h_n << commitmentToCoin.getCommitmentValue() << C_e << C_u << C_r << st_1 << st_2 << st_3 << t_1 << t_2 << t_3 << t_4;

	//According to the proof, this hash should be of length k_prime bits.  It is currently greater than that, which should not be a problem, but we should check this.
	CBigNum c = CBigNum(hasher.GetHash());

	this->s_alpha = r_alpha - c*e;
	this->s_beta = r_beta - c*r_2*e;
	this->s_zeta = r_zeta - c*r_3;
	this->s_sigma = r_sigma - c*((e+1).inverse(params->accumulatorPoKCommitmentGroup.groupOrder));
	this->s_eta = r_eta - c*r_1;
	this->s_epsilon = r_epsilon - c*r_2;
	this->s_delta = r_delta - c*r_3*e;
	this->s_xi = r_xi + c*r*((e+1).inverse(params->accumulatorPoKCommitmentGroup.groupOrder));
	this->s_phi = (r_phi - c*r) % params->accumulatorPoKCommitmentGroup.groupOrder;
	this->s_gamma = r_gamma - c*((e-1).inverse(params->accumulatorPoKCommitmentGroup.groupOrder));
	this->s_psi = r_psi + c*r*((e-1).inverse(params->accumulatorPoKCommitmentGroup.groupOrder));
}

/** Verifies that a commitment c is accumulated in accumulator a
 */
bool AccumulatorProofOfKnowledge:: Verify(const Accumulator& a, const CBigNum& valueOfCommitmentToCoin) const {
	CBigNum sg = params->accumulatorPoKCommitmentGroup.g;
	CBigNum sh = params->accumulatorPoKCommitmentGroup.h;

	CBigNum g_n = params->accumulatorQRNCommitmentGroup.g;
	CBigNum h_n = params->accumulatorQRNCommitmentGroup.h;

	//According to the proof, this hash should be of length k_prime bits.  It is currently greater than that, which should not be a problem, but we should check this.
	CHashWriter hasher(0,0);
	hasher << *params << sg << sh << g_n << h_n << valueOfCommitmentToCoin << C_e << C_u << C_r << st_1 << st_2 << st_3 << t_1 << t_2 << t_3 << t_4;

	CBigNum c = CBigNum(hasher.GetHash()); //this hash should be of length k_prime bits

	CBigNum st_1_prime = (valueOfCommitmentToCoin.pow_mod(c, params->accumulatorPoKCommitmentGroup.modulus) * sg.pow_mod(s_alpha, params->accumulatorPoKCommitmentGroup.modulus) * sh.pow_mod(s_phi, params->accumulatorPoKCommitmentGroup.modulus)) % params->accumulatorPoKCommitmentGroup.modulus;
	CBigNum st_2_prime = (sg.pow_mod(c, params->accumulatorPoKCommitmentGroup.modulus) * ((valueOfCommitmentToCoin * sg.inverse(params->accumulatorPoKCommitmentGroup.modulus)).pow_mod(s_gamma, params->accumulatorPoKCommitmentGroup.modulus)) * sh.pow_mod(s_psi, params->accumulatorPoKCommitmentGroup.modulus)) % params->accumulatorPoKCommitmentGroup.modulus;
	CBigNum st_3_prime = (sg.pow_mod(c, params->accumulatorPoKCommitmentGroup.modulus) * (sg * valueOfCommitmentToCoin).pow_mod(s_sigma, params->accumulatorPoKCommitmentGroup.modulus) * sh.pow_mod(s_xi, params->accumulatorPoKCommitmentGroup.modulus)) % params->accumulatorPoKCommitmentGroup.modulus;

	CBigNum t_1_prime = (C_r.pow_mod(c, params->accumulatorModulus) * h_n.pow_mod(s_zeta, params->accumulatorModulus) * g_n.pow_mod(s_epsilon, params->accumulatorModulus)) % params->accumulatorModulus;
	CBigNum t_2_prime = (C_e.pow_mod(c, params->accumulatorModulus) * h_n.pow_mod(s_eta, params->accumulatorModulus) * g_n.pow_mod(s_alpha, params->accumulatorModulus)) % params->accumulatorModulus;
	CBigNum t_3_prime = ((a.getValue()).pow_mod(c, params->accumulatorModulus) * C_u.pow_mod(s_alpha, params->accumulatorModulus) * ((h_n.inverse(params->accumulatorModulus)).pow_mod(s_beta, params->accumulatorModulus))) % params->accumulatorModulus;
	CBigNum t_4_prime = (C_r.pow_mod(s_alpha, params->accumulatorModulus) * ((h_n.inverse(params->accumulatorModulus)).pow_mod(s_delta, params->accumulatorModulus)) * ((g_n.inverse(params->accumulatorModulus)).pow_mod(s_beta, params->accumulatorModulus))) % params->accumulatorModulus;

	bool result = false;

	bool result_st1 = (st_1 == st_1_prime);
	bool result_st2 = (st_2 == st_2_prime);
	bool result_st3 = (st_3 == st_3_prime);

	bool result_t1 = (t_1 == t_1_prime);
	bool result_t2 = (t_2 == t_2_prime);
	bool result_t3 = (t_3 == t_3_prime);
	bool result_t4 = (t_4 == t_4_prime);

	bool result_range = ((s_alpha >= -(params->maxCoinValue * CBigNum(2).pow(params->k_prime + params->k_dprime + 1))) && (s_alpha <= (params->maxCoinValue * CBigNum(2).pow(params->k_prime + params->k_dprime + 1))));

	result = result_st1 && result_st2 && result_st3 && result_t1 && result_t2 && result_t3 && result_t4 && result_range;

	return result;
}

} /* namespace libzerocoin */
