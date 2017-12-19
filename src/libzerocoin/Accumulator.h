/**
 * @file       Accumulator.h
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
#ifndef ACCUMULATOR_H_
#define ACCUMULATOR_H_

#include "Coin.h"

namespace libzerocoin {
/**
 * \brief Implementation of the RSA-based accumulator.
 **/

class Accumulator {
public:

	/**
	 * @brief      Construct an Accumulator from a stream.
	 * @param p    An AccumulatorAndProofParams object containing global parameters
	 * @param d    the denomination of coins we are accumulating
	 * @throw      Zerocoin exception in case of invalid parameters
	 **/
	template<typename Stream>
	Accumulator(const AccumulatorAndProofParams* p, Stream& strm): params(p) {
		strm >> *this;
	}

	template<typename Stream>
	Accumulator(const ZerocoinParams* p, Stream& strm) {
		strm >> *this;
		this->params = &(p->accumulatorParams);
	}

	/**
	 * @brief      Construct an Accumulator from a Params object.
	 * @param p    A Params object containing global parameters
	 * @param d the denomination of coins we are accumulating
	 * @throw     Zerocoin exception in case of invalid parameters
	 **/
	Accumulator(const AccumulatorAndProofParams* p, const CoinDenomination d);

	Accumulator(const ZerocoinParams* p, const CoinDenomination d, Bignum bnValue = 0);

	/**
	 * Accumulate a coin into the accumulator. Validates
	 * the coin prior to accumulation.
	 *
	 * @param coin	A PublicCoin to accumulate.
	 *
	 * @throw		Zerocoin exception if the coin is not valid.
	 *
	 **/
	void accumulate(const PublicCoin &coin);
    void increment(const CBigNum& bnValue);

	CoinDenomination getDenomination() const;
	/** Get the accumulator result
	 *
	 * @return a CBigNum containing the result.
	 */
	const CBigNum& getValue() const;

	void setValue(CBigNum bnValue);


	// /**
	//  * Used to set the accumulator value
	//  *
	//  * Use this to handle accumulator checkpoints
	//  * @param b the value to set the accumulator to.
	//  * @throw  A ZerocoinException if the accumulator value is invalid.
	//  */
	// void setValue(CBigNum &b); // shouldn't this be a constructor?

	/** Used to accumulate a coin
	 *
	 * @param c the coin to accumulate
	 * @return a refrence to the updated accumulator.
	 */
	Accumulator& operator +=(const PublicCoin& c);
  Accumulator& operator =(Accumulator rhs);
	bool operator==(const Accumulator rhs) const;
	ADD_SERIALIZE_METHODS;
  template <typename Stream, typename Operation>  inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
	    READWRITE(value);
      READWRITE(denomination);
	}
private:
	const AccumulatorAndProofParams* params;
	CBigNum value;
  CoinDenomination denomination;
};

/**A witness that a PublicCoin is in the accumulation of a set of coins
 *
 */
class AccumulatorWitness {
public:
	template<typename Stream>
	AccumulatorWitness(const ZerocoinParams* p, Stream& strm) {
		strm >> *this;
	}

	/**  Construct's a witness.  You must add all elements after the witness
	 * @param p pointer to params
	 * @param checkpoint the last known accumulator value before the element was added
	 * @param coin the coin we want a witness to
	 */
	AccumulatorWitness(const ZerocoinParams* p, const Accumulator& checkpoint, const PublicCoin coin);

	/** Adds element to the set whose's accumulation we are proving coin is a member of
	 *
	 * @param c the coin to add
	 */
	void AddElement(const PublicCoin& c);

    /** Adds element to the set whose's accumulation we are proving coin is a member of. No checks performed!
	 *
	 * @param bnValue the coin's value to add
	 */
    void addRawValue(const CBigNum& bnValue);

	/**
	 *
	 * @return the value of the witness
	 */
	const CBigNum& getValue() const;
    void resetValue(const Accumulator& checkpoint, const PublicCoin coin);

	/** Checks that this is a witness to the accumulation of coin
	 * @param a             the accumulator we are checking against.
	 * @param publicCoin    the coin we're providing a witness for
	 * @return True if the witness computation validates
	 */
	bool VerifyWitness(const Accumulator& a, const PublicCoin &publicCoin) const;

	/**
	 * Adds rhs to the set whose's accumulation ware proving coin is a member of
	 * @param rhs the PublicCoin to add
	 * @return
	 */
	AccumulatorWitness& operator +=(const PublicCoin& rhs);

  AccumulatorWitness& operator =(AccumulatorWitness rhs);
private:
	Accumulator witness;
  PublicCoin element; // was const but changed to use setting in assignment
};

} /* namespace libzerocoin */
#endif /* ACCUMULATOR_H_ */
