#ifndef BITCOIN_BIP38_H
#define BITCOIN_BIP38_H

#include "pubkey.h"
#include "uint256.h"

#include <string>


/** 39 bytes - 78 characters
 * 1) Prefix - 2 bytes - 4 chars - strKey[0..3]
 * 2) Flagbyte - 1 byte - 2 chars - strKey[4..5]
 * 3) addresshash - 4 bytes - 8 chars - strKey[6..13]
 * 4) Owner Entropy - 8 bytes - 16 chars - strKey[14..29]
 * 5) Encrypted Part 1 - 8 bytes - 16 chars - strKey[30..45]
 * 6) Encrypted Part 2 - 16 bytes - 32 chars - strKey[46..77]
 */

void DecryptAES(uint256 encryptedIn, uint256 decryptionKey, uint256& output);

void ComputePreFactor(std::string strPassphrase, std::string strSalt, uint256& prefactor);

void ComputePassfactor(std::string ownersalt, uint256 prefactor, uint256& passfactor);

bool ComputePasspoint(uint256 passfactor, CPubKey& passpoint);


void ComputeSeedBPass(CPubKey passpoint, std::string strAddressHash, std::string strOwnerSalt, uint512& seedBPass);


void ComputeFactorB(uint256 seedB, uint256& factorB);

std::string BIP38_Encrypt(std::string strAddress, std::string strPassphrase, uint256 privKey);
bool BIP38_Decrypt(std::string strPassphrase, std::string strEncryptedKey, uint256& privKey, bool& fCompressed);

std::string AddressToBip38Hash(std::string address);

#endif // BIP38_H
