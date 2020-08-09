#include "uint256.h"
#include <string.h>
#include "util.h"
#include "randomx.h"
#include "RandomX/randomx.h"


    bool is_init = false;
   static char randomx_seed[64]; //={0}; // this should be 64 and not 32

    randomx_flags flags;
    randomx_vm* rx_vm = nullptr;
    randomx_cache* cache = nullptr;

//#define KEY_CHANGE 2048
//#define SWITCH_KEY 64

uint256 GetRandomXSeed(const uint32_t& nHeight)
{  
    static uint256 current_key_block;
    uint32_t SeedStartingHeight = Params().GetConsensus().RdxSeedHeight;
    uint32_t SeedInterval = Params().GetConsensus().RdxSeedInterval;
    uint32_t SwitchKey = SeedStartingHeight % SeedInterval;


    auto remainer = nHeight % SeedInterval;

    auto first_check = nHeight - remainer;
    auto second_check = nHeight - SeedInterval - remainer;

    if (nHeight > nHeight - remainer + SwitchKey) {
        if ( nHeight  > first_check)
            current_key_block = chainActive[first_check-SeedStartingHeight]->GetBlockHash();
    } else {
        if ( nHeight  > second_check)
            current_key_block = chainActive[second_check-SeedStartingHeight]->GetBlockHash();
    }

    if (current_key_block == uint256())
        current_key_block = chainActive.Genesis()->GetBlockHash();

    return current_key_block;
}


void seed_set(uint256 newseed)
{    
    printf("%s\n", __func__);
    if (!newseed) {
        char randomx_loc[64]={0};
        strncpy(randomx_seed, randomx_loc, strlen(randomx_loc));
    } else {
        strncpy(randomx_seed, newseed.GetHex().c_str(), strlen(newseed.GetHex().c_str()));
    }
}

bool seed_changed(uint256 newseed)
{
    bool changed = false;
    if (strcmp(newseed.GetHex().c_str(),randomx_seed)!=0) {
        seed_set(newseed);
        changed = true;
    }
    printf("%s returning %s\n", __func__, changed ? "true" : "false");
    return changed;
}

void randomx_init()
{
    printf("%s\n", __func__);

    seed_set(0);

    if (!cache) {
        flags = randomx_get_flags();
        cache = randomx_alloc_cache(flags);
        randomx_init_cache(cache, &randomx_seed, 64);
    }
 
    if (!rx_vm)
        rx_vm = randomx_create_vm(flags, cache, nullptr);

    is_init = true;
}

void randomx_reinit()
{
    printf("%s\n", __func__);
    
    randomx_destroy_vm(rx_vm);
    randomx_release_cache(cache);

    cache = randomx_alloc_cache(flags);
    randomx_init_cache(cache, &randomx_seed, 64);
    rx_vm = randomx_create_vm(flags, cache, nullptr);
}

void rx_slow_hash(const char* data, char* hash, int length, uint256 seedhash)
{
    printf("%s\n", __func__);
   

 
    if (!is_init)
        randomx_init();

    if (seed_changed(seedhash)) {
        randomx_reinit();
    }   
    
    randomx_calculate_hash(rx_vm, data, length, hash);
}

