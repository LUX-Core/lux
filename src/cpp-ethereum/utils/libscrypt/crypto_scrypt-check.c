#ifdef _WIN32
#define strtok_r strtok_s
#else
#define _POSIX_C_SOURCE 200112L /* for strtok_r decl with c99 */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "b64.h"
#include "slowequals.h"
#include "libscrypt.h"

int libscrypt_check(char *mcf, const char *password)
{
	/* Return values:
	* <0 error
	* == 0 password incorrect
	* >0 correct password
	*/

	char *saveptr = NULL;
	uint32_t params;
	uint64_t N;
	uint8_t r, p;
	int retval;
	uint8_t hashbuf[64];
	char outbuf[128];
	uint8_t salt[32];
	char *tok;

	if(memcmp(mcf, SCRYPT_MCF_ID, 3) != 0)
	{
		/* Only version 0 supported */
		return -1;
	}

	tok = strtok_r(mcf, "$", &saveptr);
	if ( !tok )
		return -1;

	tok = strtok_r(NULL, "$", &saveptr);

	if ( !tok )
		return -1;

	params = (uint32_t)strtoul(tok, NULL, 16);
	if ( params == 0 )
		return -1;

	tok = strtok_r(NULL, "$", &saveptr);

	if ( !tok )
		return -1;

	p = params & 0xff;
	r = (params >> 8) & 0xff;
	N = params >> 16;

	if (N > SCRYPT_SAFE_N)
		return -1;

	N = (uint64_t)1 << N;

	/* Useful debugging:
	printf("We've obtained salt 'N' r p of '%s' %d %d %d\n", tok, N,r,p);
	*/

	memset(salt, 0, sizeof(salt)); /* Keeps splint happy */
	retval = libscrypt_b64_decode(tok, (unsigned char*)salt, sizeof(salt));
	if (retval < 1)
		return -1;

	retval = libscrypt_scrypt((uint8_t*)password, strlen(password), salt,
            (uint32_t)retval, N, r, p, hashbuf, sizeof(hashbuf));

	if (retval != 0)
		return -1;

	retval = libscrypt_b64_encode((unsigned char*)hashbuf, sizeof(hashbuf), 
            outbuf, sizeof(outbuf));

	if (retval == 0)
		return -1;

	tok = strtok_r(NULL, "$", &saveptr);

	if ( !tok )
		return -1;

	if(slow_equals(tok, outbuf) == 0)
		return 0;

	return 1; /* This is the "else" condition */
}

