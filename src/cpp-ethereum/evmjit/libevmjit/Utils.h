#pragma once

#include <cstdint>
#include <iostream>

namespace dev {
namespace evmjit {

void keccak(uint8_t const *_data, uint64_t _size, uint8_t *o_hash);

// The same as assert, but expression is always evaluated and result returned
#define CHECK(expr) (assert(expr), expr)

#if !defined(NDEBUG) // Debug

std::ostream &getLogStream(char const *_channel);

#define DLOG(CHANNEL) ::dev::evmjit::getLogStream(#CHANNEL)

#else // Release

struct Voider
{
    void operator=(std::ostream const&) {}
};

#define DLOG(CHANNEL) true ? (void)0 : ::dev::evmjit::Voider{} = std::cerr

#endif

}
}
