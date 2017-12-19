/**
* @file       Params.cpp
*
* @brief      Parameter class for Zerocoin.
*
* @author     Ian Miers, Christina Garman and Matthew Green
* @date       June 2013
*
* @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
* @license    This project is released under the MIT license.
**/
// Copyright (c) 2017 The LUX developers
#include "Params.h"
#include "ParamGeneration.h"

namespace libzerocoin {

ZerocoinParams::ZerocoinParams(CBigNum N, uint32_t securityLevel) {
	this->zkp_hash_len = securityLevel;
	this->zkp_iterations = securityLevel;

	this->accumulatorParams.k_prime = ACCPROOF_KPRIME;
	this->accumulatorParams.k_dprime = ACCPROOF_KDPRIME;

	// Generate the parameters
	CalculateParams(*this, N, ZEROCOIN_PROTOCOL_VERSION, securityLevel);

	this->accumulatorParams.initialized = true;
	this->initialized = true;
}

AccumulatorAndProofParams::AccumulatorAndProofParams() {
	this->initialized = false;
}

IntegerGroupParams::IntegerGroupParams() {
	this->initialized = false;
}

CBigNum IntegerGroupParams::randomElement() const {
	// The generator of the group raised
	// to a random number less than the order of the group
	// provides us with a uniformly distributed random number.
	return this->g.pow_mod(CBigNum::randBignum(this->groupOrder),this->modulus);
}

} /* namespace libzerocoin */
