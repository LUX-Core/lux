#include "RandomX/src/bytecode_machine.hpp"
#include "RandomX/src/dataset.hpp"
#include "RandomX/src/blake2/endian.h"
#include "RandomX/src/blake2/blake2.h"
#include "RandomX/src/blake2_generator.hpp"
#include "RandomX/src/superscalar.hpp"
#include "RandomX/src/reciprocal.h"
#include "RandomX/src/intrin_portable.h"
#include "RandomX/src/jit_compiler.hpp"
#include "RandomX/src/aes_hash.hpp"
#include "RandomX/src/randomx.h"
#include <openssl/sha.h>
#include "main.h"
#include <cassert>

//! barrystyle 03032020
bool is_vm_init = false;
bool is_cache_init = false;
uint256 oldCache;
char keyCache[32];
unsigned int seedHeight;
randomx_flags flags;
randomx_cache *cache = nullptr;
randomx_dataset *dataset = nullptr;
randomx_vm *vm = nullptr;

void randomx_init()
{
    randomx_initseed();
    randomx_initcache();
    randomx_initdataset();
    randomx_initvm();
}

void randomx_initseed()
{
    seedHash(oldCache,keyCache,1);
}

void randomx_initcache()
{
    flags = randomx_get_flags();
    cache = randomx_alloc_cache(flags | RANDOMX_FLAG_LARGE_PAGES);
    if (!cache)
        cache = randomx_alloc_cache(flags);
    randomx_init_cache(cache, &keyCache, 32);
    is_cache_init = true;
}

void randomx_initdataset()
{
    dataset = randomx_alloc_dataset(RANDOMX_FLAG_LARGE_PAGES);
    if (!dataset)
        dataset = randomx_alloc_dataset(RANDOMX_FLAG_DEFAULT);
    randomx_init_dataset(dataset, cache, 0, randomx_dataset_item_count());
}

void randomx_initvm()
{
    if (vm)
        randomx_destroy_vm(vm);
    vm = randomx_create_vm(flags, cache, nullptr);
    is_vm_init = true;
}

void randomx_shutoff()
{
    if (vm)
        randomx_destroy_vm(vm);
    vm = nullptr;
    randomx_release_cache(cache);
}

void seedNow(int nHeight)
{
    uint256 tempCache;
    char tempStr[64];
    seedHash(tempCache, tempStr, nHeight);
    if (!memcmp(&tempCache,keyCache,32)) {
        LogPrintf("* changed seed at height %d\n", nHeight);
        memcpy(keyCache,&tempCache,32);
    }
}

void seedHash(uint256 &seed, char *seedStr, int nHeight)
{
    char seedHalf[32] = {0};
    int seedInt = (((nHeight+99)/100)+100);
    sprintf(seedHalf,"%d",seedInt);
    SHA256((const unsigned char*)seedHalf,32,(unsigned char*)seedHalf);
    memcpy(&seed,seedHalf,32);
    for (unsigned int i=0; i<32; i++)
        sprintf(seedStr+(i*2),"%02hhx", seedHalf[i]);
}

void randomxhash(const char* input, char* output, int len)
{
    char hash[32] = {0};
    if (!is_cache_init)
        randomx_initcache();
    if (!is_vm_init)
        randomx_initvm();
    randomx_calculate_hash(vm, input, len, hash);
    memcpy(output, hash, 32);
}

