#ifndef SCRYPT_H
#define SCRYPT_H
#include <stdlib.h>
#include <stdint.h>
#include <string>

void scrypt(const char* pass, unsigned int pLen, const char* salt, unsigned int sLen, char *output, unsigned int N, unsigned int r, unsigned int p, unsigned int dkLen);

#endif
