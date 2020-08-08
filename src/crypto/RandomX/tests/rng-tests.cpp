/*
  cd ~
  wget http://simul.iro.umontreal.ca/testu01/TestU01.zip
  unzip TestU01.zip
  mkdir TestU01
  cd TestU01-1.2.3
  ./configure --prefix=`pwd`/../TestU01
  make -j8
  make install
  cd ~/RandomX
  g++ -O3 src/tests/rng-tests.cpp -lm -I ~/TestU01/include -L ~/TestU01/lib -L bin/ -l:libtestu01.a -l:libmylib.a -l:libprobdist.a -lrandomx -o bin/rng-tests -DRANDOMX_GEN=4R -DRANDOMX_TESTU01=Crush
  bin/rng-tests 0
*/

extern "C" {
  #include "unif01.h"
  #include "bbattery.h"
}

#include "../aes_hash.hpp"
#include "../blake2/blake2.h"
#include "utility.hpp"
#include <cstdint>

#ifndef RANDOMX_GEN
#error Please define RANDOMX_GEN with a value of 1R or 4R
#endif

#ifndef RANDOMX_TESTU01
#error Please define RANDOMX_TESTU01 with a value of SmallCrush, Crush or BigCrush
#endif

#define STR(x) #x
#define CONCAT(a,b,c) a ## b ## c
#define GEN_NAME(x) "AesGenerator" STR(x)
#define GEN_FUNC(x) CONCAT(fillAes, x, x4)
#define TEST_SUITE(x) CONCAT(bbattery_, x,)

constexpr int GeneratorStateSize = 64;
constexpr int GeneratorCapacity = GeneratorStateSize / sizeof(uint32_t);

static unsigned long aesGenBits(void *param, void *state) {
  uint32_t* statePtr = (uint32_t*)state;
  int* indexPtr = (int*)param;
  int stateIndex = *indexPtr;
  if(stateIndex >= GeneratorCapacity) {
    GEN_FUNC(RANDOMX_GEN)<false>(statePtr, GeneratorStateSize, statePtr);
    stateIndex = 0;
  }
  uint32_t next = statePtr[stateIndex];
  *indexPtr = stateIndex + 1;
  return next;
}

static double aesGenDouble(void *param, void *state) {
  return aesGenBits (param, state) / unif01_NORM32;
}

static void aesWriteState(void* state) {
  char* statePtr = (char*)state;
  for(int i = 0; i < 4; ++i) {
    std::cout << "state" << i << " = ";
    outputHex(std::cout, statePtr + (i * 16), 16);
    std::cout << std::endl;
  }
}

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cout << argv[0] << " <seed>" << std::endl;
    return 1;
  }
  uint32_t state[GeneratorCapacity] = { 0 };
  int stateIndex = GeneratorCapacity;
  char name[] = GEN_NAME(RANDOMX_GEN);
  uint64_t seed = strtoull(argv[1], nullptr, 0);
  if(seed) {
    blake2b(&state, sizeof(state), &seed, sizeof(seed), nullptr, 0);
  }
  unif01_Gen gen;
  gen.state = &state;
  gen.param = &stateIndex;
  gen.Write = &aesWriteState;
  gen.GetU01 = &aesGenDouble;
  gen.GetBits = &aesGenBits;
  gen.name = (char*)name;

  gen.Write(gen.state);
  std::cout << std::endl;

  TEST_SUITE(RANDOMX_TESTU01)(&gen);
  return 0;
}