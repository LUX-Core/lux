
#include "chain.h"

extern CChain chainActive;

//bool seed_changed(uint256 newseed);
//void seed_set(uint256 newseed);
//void randomx_init();
//void randomx_reinit();
void rx_slow_hash(const char* data, char* hash, int length, uint256 seedhash);
void rx_slow_hash2(const char* data, char* hash, int length, uint256 seedhash);
uint256 GetRandomXSeed(const uint32_t& nHeight);
