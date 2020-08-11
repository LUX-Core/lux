#include "uint256.h"
#include <string.h>
#include "util.h"
#include "randomx.h"
#include "RandomX/randomx.h"


//    bool is_init = false;
//   static char randomx_seed[64]; //={0}; // this should be 64 and not 32

//    randomx_flags flags;
//    randomx_vm* rx_vm = nullptr;
//    randomx_cache* cache = nullptr;

//#define KEY_CHANGE 2048
//#define SWITCH_KEY 64

uint256 GetRandomXSeed(const uint32_t& nHeight)
{  
        printf("%s\n", __func__);
    static uint256 current_key_block;
    uint32_t SeedStartingHeight = Params().GetConsensus().RdxSeedHeight;
    uint32_t SeedInterval = Params().GetConsensus().RdxSeedInterval;
    uint32_t SwitchKey = SeedStartingHeight % SeedInterval;


    auto remainer = nHeight % SeedInterval;

    auto first_check = nHeight - remainer;
    auto second_check = nHeight - SeedInterval - remainer;
    
//    std::cout << "first_check-SeedStartingHeight " << first_check-SeedStartingHeight << std::endl;
//    std::cout << "second_check-SeedStartingHeight " << second_check-SeedStartingHeight << std::endl;
    if (nHeight > nHeight - remainer + SwitchKey) {
        if ( nHeight  > first_check)
            current_key_block = uint256("0xaf8367c7285833f5036ae884aadcc64a277b7e20bfb160cf57eaf7ece0e00c68size");/*chainActive[first_check-SeedStartingHeight]->GetBlockHash();*/
    } else {
        if ( nHeight  > second_check)
            current_key_block = uint256("0xaf8367c7285833f5036ae884aadcc64a277b7e20bfb160cf57eaf7ece0e00c68size");/*chainActive[second_check-SeedStartingHeight]->GetBlockHash();*/
    }

    if (current_key_block == uint256())
        current_key_block = chainActive.Genesis()->GetBlockHash();

       printf("%s\n", __func__);
    return current_key_block;
}

/*
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
*/
/*
uint256 seed_set2(uint256 newseed)
{    
    printf("%s\n", __func__);
    if (!newseed) {
        uint256 random_loc = uint256();
        return random_loc;
//        char randomx_loc[64]={0};
//        strncpy(randomx_seed, randomx_loc, strlen(randomx_loc));
    } else {
//        strncpy(randomx_seed, newseed.GetHex().c_str(), strlen(newseed.GetHex().c_str()));
        return new
    }
    return random_seed;
}
*/
/*
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
*/
void rx_slow_hash(const char* data, char* hash, int length, uint256 seedhash)
{
//    printf("%s\n", __func__);
    static bool is_init; // = false;
//    static char randomx_seed[64]; //={0}; // this should be 64 and not 32
    static uint256 randomx_seed2;
    static randomx_flags flags;
    static randomx_vm *rx_vm; // = nullptr;
    static randomx_cache *cache; // = nullptr;

 
    if (!is_init) {
//        randomx_init();
//    seed_set(0);
        randomx_seed2 = uint256(0);
    if (!cache) {
        flags = randomx_get_flags();
        cache = randomx_alloc_cache(flags);
        randomx_init_cache(cache, randomx_seed2.GetHex().c_str(), 64);
    }
 
    if (!rx_vm)
        rx_vm = randomx_create_vm(flags, cache, nullptr);

    is_init = true;
    }
    if (/*seed_changed(seedhash)*/ randomx_seed2!=seedhash) {
        randomx_seed2 = seedhash;
//        randomx_reinit();
    randomx_destroy_vm(rx_vm);
    randomx_release_cache(cache);

    cache = randomx_alloc_cache(flags);
    randomx_init_cache(cache, randomx_seed2.GetHex().c_str(), 64);
    rx_vm = randomx_create_vm(flags, cache, nullptr);

    }   
//    printf("before calcultae hash %s\n", __func__);
    randomx_calculate_hash(rx_vm, data, length, hash);
//    printf("after calcultae hash %s\n", __func__); 
}

void rx_slow_hash2(const char* data, char* hash, int length, uint256 seedhash)
{
    printf("%s\n", __func__);
    bool is_init = false;
//    static char randomx_seed[64]; //={0}; // this should be 64 and not 32
    static uint256 randomx_seed2;
    static randomx_flags flags;
    static randomx_vm *rx_vm = nullptr;
    static randomx_cache *cache = nullptr;

 
    if (!is_init) {
//        randomx_init();
//    seed_set(0);
        randomx_seed2 = seedhash;
    if (!cache) {
        flags = randomx_get_flags();
        cache = randomx_alloc_cache(flags);
        randomx_init_cache(cache, randomx_seed2.GetHex().c_str(), 64);
    }
 
    if (!rx_vm)
        rx_vm = randomx_create_vm(flags, cache, nullptr);

    is_init = true;
    }
    if (/*seed_changed(seedhash)*/ randomx_seed2!=seedhash) {
        randomx_seed2 = seedhash;
//        randomx_reinit();
    randomx_destroy_vm(rx_vm);
    randomx_release_cache(cache);

    cache = randomx_alloc_cache(flags);
    randomx_init_cache(cache, randomx_seed2.GetHex().c_str(), 64);
    rx_vm = randomx_create_vm(flags, cache, nullptr);

    }   
    printf("before calcultae hash %s\n", __func__);
    randomx_calculate_hash(rx_vm, data, length, hash);
    printf("after calcultae hash %s\n", __func__); 
}