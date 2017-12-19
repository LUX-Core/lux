/**
 * @file       Accumulator.cpp
 *
 * @brief      Accumulator and AccumulatorWitness classes for the Zerocoin library.
 *
 * @author     Ian Miers, Christina Garman and Matthew Green
 * @date       June 2013
 *
 * @copyright  Copyright 2013 Ian Miers, Christina Garman and Matthew Green
 * @license    This project is released under the MIT license.
 **/
// Copyright (c) 2017 The LUX developers

#include <sstream>
#include <iostream>
#include "Accumulator.h"
#include "ZerocoinDefines.h"

namespace libzerocoin {

//Accumulator class
Accumulator::Accumulator(const AccumulatorAndProofParams* p, const CoinDenomination d): params(p) {
	if (!(params->initialized)) {
		throw std::runtime_error("Invalid parameters for accumulator");
	}
    denomination = d;
	this->value = this->params->accumulatorBase;
}

Accumulator::Accumulator(const ZerocoinParams* p, const CoinDenomination d, const Bignum bnValue) {
	this->params = &(p->accumulatorParams);
    denomination = d;

	if (!(params->initialized)) {
		throw std::runtime_error("Invalid parameters for accumulator");
	}

	if(bnValue != 0)
		this->value = bnValue;
	else
		this->value = this->params->accumulatorBase;
}

void Accumulator::increment(const CBigNum& bnValue) {
    // Compute new accumulator = "old accumulator"^{element} mod N
    this->value = this->value.pow_mod(bnValue, this->params->accumulatorModulus);
}

void Accumulator::accumulate(const PublicCoin& coin) {
	// Make sure we're initialized
	if(!(this->value)) {
        std::cout << "Accumulator is not initialized" << "\n";
		throw std::runtime_error("Accumulator is not initialized");
	}

	if(this->denomination != coin.getDenomination()) {
		std::cout << "Wrong denomination for coin. Expected coins of denomination: ";
        std::cout << this->denomination;
        std::cout << ". Instead, got a coin of denomination: ";
        std::cout << coin.getDenomination();
        std::cout << "\n";
		throw std::runtime_error("Wrong denomination for coin");
	}

	if(coin.validate()) {
		increment(coin.getValue());
	} else {
		std::cout << "Coin not valid\n";
        throw std::runtime_error("Coin is not valid");
	}
}

CoinDenomination Accumulator::getDenomination() const {
	return this->denomination;
}

const CBigNum& Accumulator::getValue() const {
	return this->value;
}

//Manually set accumulator value
void Accumulator::setValue(CBigNum bnValue) {
	this->value = bnValue;
}

Accumulator& Accumulator::operator += (const PublicCoin& c) {
	this->accumulate(c);
	return *this;
}

Accumulator& Accumulator::operator = (Accumulator rhs) {
    if (this != &rhs) std::swap(*this, rhs);
    return *this;
}

bool Accumulator::operator == (const Accumulator rhs) const {
	return this->value == rhs.value;
}

//AccumulatorWitness class
AccumulatorWitness::AccumulatorWitness(const ZerocoinParams* p,
                                       const Accumulator& checkpoint, const PublicCoin coin): witness(checkpoint), element(coin) {
}

void AccumulatorWitness::resetValue(const Accumulator& checkpoint, const PublicCoin coin) {
    this->witness.setValue(checkpoint.getValue());
    this->element = coin;
}

void AccumulatorWitness::AddElement(const PublicCoin& c) {
	if(element != c) {
		witness += c;
	}
}

//warning check pubcoin value & denom outside of this function!
void AccumulatorWitness::addRawValue(const CBigNum& bnValue) {
        witness.increment(bnValue);
}

const CBigNum& AccumulatorWitness::getValue() const {
	return this->witness.getValue();
}

bool AccumulatorWitness::VerifyWitness(const Accumulator& a, const PublicCoin &publicCoin) const {
	Accumulator temp(witness);
	temp += element;
	return (temp == a && this->element == publicCoin);
}

AccumulatorWitness& AccumulatorWitness::operator +=(
    const PublicCoin& rhs) {
	this->AddElement(rhs);
	return *this;
}

AccumulatorWitness& AccumulatorWitness::operator =(AccumulatorWitness rhs) {
    // Not pretty, but seems to work (SPOCK)
    if (&witness != &rhs.witness) this->witness = rhs.witness;
    if (&element != &rhs.element) std::swap(element, rhs.element);
	return *this;
}

} /* namespace libzerocoin */
