/**
 * @file       Coin.cpp
 *
 * @brief      PublicCoin and PrivateCoin classes for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/

// Copyright (c) 2017 The LUX developers
#include <stdexcept>
#include <iostream>
#include "Coin.h"
#include "Commitment.h"
#include "Denominations.h"

namespace libzerocoin {

//PublicCoin class
PublicCoin::PublicCoin(const ZerocoinParams* p):
	params(p) {
	if (this->params->initialized == false) {
		throw std::runtime_error("Params are not initialized");
	}
    // Assume this will get set by another method later
    denomination = ZQ_ERROR;
};

PublicCoin::PublicCoin(const ZerocoinParams* p, const CBigNum& coin, const CoinDenomination d):
	params(p), value(coin) {
	if (this->params->initialized == false) {
		throw std::runtime_error("Params are not initialized");
	}

	denomination = d;
	for(const CoinDenomination denom : zerocoinDenomList) {
		if(denom == d)
			denomination = d;
	}
    if(denomination == 0){
		std::cout << "denom does not exist\n";
		throw std::runtime_error("Denomination does not exist");
	}
};

//PrivateCoin class
PrivateCoin::PrivateCoin(const ZerocoinParams* p, const CoinDenomination denomination): params(p), publicCoin(p) {
	// Verify that the parameters are valid
	if(this->params->initialized == false) {
		throw std::runtime_error("Params are not initialized");
	}

#ifdef ZEROCOIN_FAST_MINT
	// Mint a new coin with a random serial number using the fast process.
	// This is more vulnerable to timing attacks so don't mint coins when
	// somebody could be timing you.
	this->mintCoinFast(denomination);
#else
	// Mint a new coin with a random serial number using the standard process.
	this->mintCoin(denomination);
#endif
	
}

void PrivateCoin::mintCoin(const CoinDenomination denomination) {
	// Repeat this process up to MAX_COINMINT_ATTEMPTS times until
	// we obtain a prime number
	for(uint32_t attempt = 0; attempt < MAX_COINMINT_ATTEMPTS; attempt++) {

		// Generate a random serial number in the range 0...{q-1} where
		// "q" is the order of the commitment group.
		CBigNum s = CBigNum::randBignum(this->params->coinCommitmentGroup.groupOrder);

		// Generate a Pedersen commitment to the serial number "s"
		Commitment coin(&params->coinCommitmentGroup, s);

		// Now verify that the commitment is a prime number
		// in the appropriate range. If not, we'll throw this coin
		// away and generate a new one.
		if (coin.getCommitmentValue().isPrime(ZEROCOIN_MINT_PRIME_PARAM) &&
		        coin.getCommitmentValue() >= params->accumulatorParams.minCoinValue &&
		        coin.getCommitmentValue() <= params->accumulatorParams.maxCoinValue) {
			// Found a valid coin. Store it.
			this->serialNumber = s;
			this->randomness = coin.getRandomness();
			this->publicCoin = PublicCoin(params,coin.getCommitmentValue(), denomination);

			// Success! We're done.
			return;
		}
	}

	// We only get here if we did not find a coin within
	// MAX_COINMINT_ATTEMPTS. Throw an exception.
	throw std::runtime_error("Unable to mint a new Zerocoin (too many attempts)");
}

void PrivateCoin::mintCoinFast(const CoinDenomination denomination) {
	
	// Generate a random serial number in the range 0...{q-1} where
	// "q" is the order of the commitment group.
	CBigNum s = CBigNum::randBignum(this->params->coinCommitmentGroup.groupOrder);
	
	// Generate a random number "r" in the range 0...{q-1}
	CBigNum r = CBigNum::randBignum(this->params->coinCommitmentGroup.groupOrder);
	
	// Manually compute a Pedersen commitment to the serial number "s" under randomness "r"
	// C = g^s * h^r mod p
	CBigNum commitmentValue = this->params->coinCommitmentGroup.g.pow_mod(s, this->params->coinCommitmentGroup.modulus).mul_mod(this->params->coinCommitmentGroup.h.pow_mod(r, this->params->coinCommitmentGroup.modulus), this->params->coinCommitmentGroup.modulus);
	
	// Repeat this process up to MAX_COINMINT_ATTEMPTS times until
	// we obtain a prime number
	for (uint32_t attempt = 0; attempt < MAX_COINMINT_ATTEMPTS; attempt++) {
		// First verify that the commitment is a prime number
		// in the appropriate range. If not, we'll throw this coin
		// away and generate a new one.
		if (commitmentValue.isPrime(ZEROCOIN_MINT_PRIME_PARAM) &&
			commitmentValue >= params->accumulatorParams.minCoinValue &&
			commitmentValue <= params->accumulatorParams.maxCoinValue) {
			// Found a valid coin. Store it.
			this->serialNumber = s;
			this->randomness = r;
			this->publicCoin = PublicCoin(params, commitmentValue, denomination);
				
			// Success! We're done.
			return;
		}
		
		// Generate a new random "r_delta" in 0...{q-1}
		CBigNum r_delta = CBigNum::randBignum(this->params->coinCommitmentGroup.groupOrder);

		// The commitment was not prime. Increment "r" and recalculate "C":
		// r = r + r_delta mod q
		// C = C * h mod p
		r = (r + r_delta) % this->params->coinCommitmentGroup.groupOrder;
		commitmentValue = commitmentValue.mul_mod(this->params->coinCommitmentGroup.h.pow_mod(r_delta, this->params->coinCommitmentGroup.modulus), this->params->coinCommitmentGroup.modulus);
	}
		
	// We only get here if we did not find a coin within
	// MAX_COINMINT_ATTEMPTS. Throw an exception.
	throw std::runtime_error("Unable to mint a new Zerocoin (too many attempts)");
}
	
} /* namespace libzerocoin */
