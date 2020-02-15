#include "uint256.h"

void randomx_init();
void randomx_initseed();
void randomx_initcache();
void randomx_initdataset();
void randomx_initvm();
void randomx_shutoff();
void seedNow(int nHeight);
void seedHash(uint256 &seed, char *seedStr, int nHeight);
void randomxhash(const char* input, char* output, int len);

