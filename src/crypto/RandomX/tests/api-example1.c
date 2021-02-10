#include "../randomx.h"
#include <stdio.h>

int main() {
	const char myKey[] = "RandomX example key";
	const char myInput[] = "RandomX example input";
	char hash[RANDOMX_HASH_SIZE];

	randomx_flags flags = randomx_get_flags();
	randomx_cache *myCache = randomx_alloc_cache(flags);
	randomx_init_cache(myCache, &myKey, sizeof myKey);
	randomx_vm *myMachine = randomx_create_vm(flags, myCache, NULL);

	randomx_calculate_hash(myMachine, &myInput, sizeof myInput, hash);

	randomx_destroy_vm(myMachine);
	randomx_release_cache(myCache);

	for (unsigned i = 0; i < RANDOMX_HASH_SIZE; ++i)
		printf("%02x", hash[i] & 0xff);

	printf("\n");

	return 0;
}
