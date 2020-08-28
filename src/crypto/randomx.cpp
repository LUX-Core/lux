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
//    printf("the seed function \n");
    static uint256 current_key_block;
    uint32_t SeedStartingHeight = Params().GetConsensus().RdxSeedHeight;
    uint32_t SeedInterval = Params().GetConsensus().RdxSeedInterval;
    uint32_t SwitchKey = SeedStartingHeight % SeedInterval;

    if (current_key_block == uint256())
        current_key_block = chainActive.Genesis()->GetBlockHash();

    uint32_t remainer = nHeight % SeedInterval;

    uint32_t first_check = nHeight - remainer;
    uint32_t second_check = nHeight - SeedInterval - remainer;
//    printf("the seed function aftergenesis first height = %d second height = %d \n",first_check,second_check);
    if (nHeight > nHeight - remainer + SwitchKey) {
        
        if ( nHeight  > first_check)
            current_key_block = /*uint256("0xaf8367c7285833f5036ae884aadcc64a277b7e20bfb160cf57eaf7ece0e00c68size"); */ chainActive[first_check-SeedStartingHeight]->GetBlockHash();
    } else {
           
        if ( nHeight  > second_check) {
            
            current_key_block = /*uint256("0xaf8367c7285833f5036ae884aadcc64a277b7e20bfb160cf57eaf7ece0e00c68size"); */ chainActive[second_check-SeedStartingHeight]->GetBlockHash();
        }
    }

    return current_key_block;
}


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

        randomx_seed2 = seedhash;
    if (!cache) {
        flags = randomx_get_flags();
        cache = randomx_alloc_cache(flags);
       
        randomx_init_cache(cache, randomx_seed2.GetHex().c_str(), randomx_seed2.GetHex().size());
    }
 
    if (!rx_vm)
        rx_vm = randomx_create_vm(flags, cache, nullptr);

    is_init = true;
    }
    if (randomx_seed2!=seedhash) {
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
//    printf("%s\n", __func__);
    static bool is_init; // = false;
//    static char randomx_seed[64]; //={0}; // this should be 64 and not 32
    static uint256 randomx_seed2;
    static randomx_flags flags;
    static randomx_vm *rx_vm; // = nullptr;
    static randomx_cache *cache; // = nullptr;

 
    if (!is_init) {

        randomx_seed2 = seedhash;
    if (!cache) {
        flags = randomx_get_flags();
        cache = randomx_alloc_cache(flags);
       
        randomx_init_cache(cache, randomx_seed2.GetHex().c_str(), randomx_seed2.GetHex().size());
    }
 
    if (!rx_vm)
        rx_vm = randomx_create_vm(flags, cache, nullptr);

    is_init = true;
    }
    if (randomx_seed2!=seedhash) {
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


void rx_slow_hash2_old(const char* data, char* hash, int length, uint256 seedhash)
{
 
    bool is_init = false;
//    static char randomx_seed[64]; //={0}; // this should be 64 and not 32
     uint256 randomx_seed2;
     
        randomx_seed2 = seedhash;
 
        randomx_flags flags = randomx_get_flags();
        randomx_cache * cache = randomx_alloc_cache(flags);
        randomx_init_cache(cache, randomx_seed2.GetHex().c_str(), 64);
        randomx_vm *rx_vm = randomx_create_vm(flags, cache, nullptr);

   
 //   printf("before calcultae hash %s\n", __func__);
    randomx_calculate_hash(rx_vm, data, length, hash);
 //   printf("after calcultae hash %s %s\n", __func__,hash); 

    randomx_destroy_vm(rx_vm);
    randomx_release_cache(cache);


}