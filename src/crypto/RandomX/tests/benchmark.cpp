/*
Copyright (c) 2018-2019, tevador <tevador@gmail.com>

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
	* Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.
	* Neither the name of the copyright holder nor the
	  names of its contributors may be used to endorse or promote products
	  derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <fstream>
#include <iostream>
#include <iomanip>
#include <exception>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "stopwatch.hpp"
#include "utility.hpp"
#include "../randomx.h"
#include "../dataset.hpp"
#include "../blake2/endian.h"
#include "../common.hpp"
#include "../jit_compiler.hpp"
#ifdef _WIN32
#include <windows.h>
#include <versionhelpers.h>
#endif
#include "affinity.hpp"

const uint8_t blockTemplate_[] = {
		0x07, 0x07, 0xf7, 0xa4, 0xf0, 0xd6, 0x05, 0xb3, 0x03, 0x26, 0x08, 0x16, 0xba, 0x3f, 0x10, 0x90, 0x2e, 0x1a, 0x14,
		0x5a, 0xc5, 0xfa, 0xd3, 0xaa, 0x3a, 0xf6, 0xea, 0x44, 0xc1, 0x18, 0x69, 0xdc, 0x4f, 0x85, 0x3f, 0x00, 0x2b, 0x2e,
		0xea, 0x00, 0x00, 0x00, 0x00, 0x77, 0xb2, 0x06, 0xa0, 0x2c, 0xa5, 0xb1, 0xd4, 0xce, 0x6b, 0xbf, 0xdf, 0x0a, 0xca,
		0xc3, 0x8b, 0xde, 0xd3, 0x4d, 0x2d, 0xcd, 0xee, 0xf9, 0x5c, 0xd2, 0x0c, 0xef, 0xc1, 0x2f, 0x61, 0xd5, 0x61, 0x09
};

class AtomicHash {
public:
	AtomicHash() {
		for (int i = 0; i < 4; ++i)
			hash[i].store(0);
	}
	void xorWith(uint64_t update[4]) {
		for (int i = 0; i < 4; ++i)
			hash[i].fetch_xor(update[i]);
	}
	void print(std::ostream& os) {
		for (int i = 0; i < 4; ++i)
			print(hash[i], os);
		os << std::endl;
	}
private:
	static void print(std::atomic<uint64_t>& hash, std::ostream& os) {
		auto h = hash.load();
		outputHex(std::cout, (char*)&h, sizeof(h));
	}
	std::atomic<uint64_t> hash[4];
};

void printUsage(const char* executable) {
	std::cout << "Usage: " << executable << " [OPTIONS]" << std::endl;
	std::cout << "Supported options:" << std::endl;
	std::cout << "  --help        shows this message" << std::endl;
	std::cout << "  --mine        mining mode: 2080 MiB" << std::endl;
	std::cout << "  --verify      verification mode: 256 MiB" << std::endl;
	std::cout << "  --jit         JIT compiled mode (default: interpreter)" << std::endl;
	std::cout << "  --secure      W^X policy for JIT pages (default: off)" << std::endl;
	std::cout << "  --largePages  use large pages (default: small pages)" << std::endl;
	std::cout << "  --softAes     use software AES (default: hardware AES)" << std::endl;
	std::cout << "  --threads T   use T threads (default: 1)" << std::endl;
	std::cout << "  --affinity A  thread affinity bitmask (default: 0)" << std::endl;
	std::cout << "  --init Q      initialize dataset with Q threads (default: 1)" << std::endl;
	std::cout << "  --nonces N    run N nonces (default: 1000)" << std::endl;
	std::cout << "  --seed S      seed for cache initialization (default: 0)" << std::endl;
	std::cout << "  --ssse3       use optimized Argon2 for SSSE3 CPUs" << std::endl;
	std::cout << "  --avx2        use optimized Argon2 for AVX2 CPUs" << std::endl;
	std::cout << "  --auto        select the best options for the current CPU" << std::endl;
	std::cout << "  --noBatch     calculate hashes one by one (default: batch)" << std::endl;
}

struct MemoryException : public std::exception {
};
struct CacheAllocException : public MemoryException {
	const char * what() const throw () {
		return "Cache allocation failed";
	}
};
struct DatasetAllocException : public MemoryException {
	const char * what() const throw () {
		return "Dataset allocation failed";
	}
};

using MineFunc = void(randomx_vm * vm, std::atomic<uint32_t> & atomicNonce, AtomicHash & result, uint32_t noncesCount, int thread, int cpuid);

template<bool batch>
void mine(randomx_vm* vm, std::atomic<uint32_t>& atomicNonce, AtomicHash& result, uint32_t noncesCount, int thread, int cpuid = -1) {
	if (cpuid >= 0) {
		int rc = set_thread_affinity(cpuid);
		if (rc) {
			std::cerr << "Failed to set thread affinity for thread " << thread << " (error=" << rc << ")" << std::endl;
		}
	}
	uint64_t hash[RANDOMX_HASH_SIZE / sizeof(uint64_t)];
	uint8_t blockTemplate[sizeof(blockTemplate_)];
	memcpy(blockTemplate, blockTemplate_, sizeof(blockTemplate));
	void* noncePtr = blockTemplate + 39;
	auto nonce = atomicNonce.fetch_add(1);

	if (batch) {
		store32(noncePtr, nonce);
		randomx_calculate_hash_first(vm, blockTemplate, sizeof(blockTemplate));
	}

	while (nonce < noncesCount) {
		if (batch) {
			nonce = atomicNonce.fetch_add(1);
		}
		store32(noncePtr, nonce);
		(batch ? randomx_calculate_hash_next : randomx_calculate_hash)(vm, blockTemplate, sizeof(blockTemplate), &hash);
		result.xorWith(hash);
		if (!batch) {
			nonce = atomicNonce.fetch_add(1);
		}
	}
}

int main(int argc, char** argv) {
	bool softAes, miningMode, verificationMode, help, largePages, jit, secure;
	bool ssse3, avx2, autoFlags, noBatch;
	int noncesCount, threadCount, initThreadCount;
	uint64_t threadAffinity;
	int32_t seedValue;
	char seed[4];

	readOption("--softAes", argc, argv, softAes);
	readOption("--mine", argc, argv, miningMode);
	readOption("--verify", argc, argv, verificationMode);
	readIntOption("--threads", argc, argv, threadCount, 1);
	readUInt64Option("--affinity", argc, argv, threadAffinity, 0);
	readIntOption("--nonces", argc, argv, noncesCount, 1000);
	readIntOption("--init", argc, argv, initThreadCount, 1);
	readIntOption("--seed", argc, argv, seedValue, 0);
	readOption("--largePages", argc, argv, largePages);
	if (!largePages) {
		readOption("--largepages", argc, argv, largePages);
	}
	readOption("--jit", argc, argv, jit);
	readOption("--help", argc, argv, help);
	readOption("--secure", argc, argv, secure);
	readOption("--ssse3", argc, argv, ssse3);
	readOption("--avx2", argc, argv, avx2);
	readOption("--auto", argc, argv, autoFlags);
	readOption("--noBatch", argc, argv, noBatch);

	store32(&seed, seedValue);

	std::cout << "RandomX benchmark v1.1.8" << std::endl;

	if (help) {
		printUsage(argv[0]);
		return 0;
	}

	if (!miningMode && !verificationMode) {
		std::cout << "Please select either the fast mode (--mine) or the slow mode (--verify)" << std::endl;
		std::cout << "Run '" << argv[0] << " --help' to see all supported options" << std::endl;
		return 0;
	}

	std::atomic<uint32_t> atomicNonce(0);
	AtomicHash result;
	std::vector<randomx_vm*> vms;
	std::vector<std::thread> threads;
	randomx_dataset* dataset;
	randomx_cache* cache;
	randomx_flags flags;

	if (autoFlags) {
		initThreadCount = std::thread::hardware_concurrency();
		flags = randomx_get_flags();
	}
	else {
		flags = RANDOMX_FLAG_DEFAULT;
		if (ssse3) {
			flags |= RANDOMX_FLAG_ARGON2_SSSE3;
		}
		if (avx2) {
			flags |= RANDOMX_FLAG_ARGON2_AVX2;
		}
		if (!softAes) {
			flags |= RANDOMX_FLAG_HARD_AES;
		}
		if (jit) {
			flags |= RANDOMX_FLAG_JIT;
#ifdef RANDOMX_FORCE_SECURE
			flags |= RANDOMX_FLAG_SECURE;
#endif
		}
	}

	if (largePages) {
		flags |= RANDOMX_FLAG_LARGE_PAGES;
	}
	if (miningMode) {
		flags |= RANDOMX_FLAG_FULL_MEM;
	}
#ifndef RANDOMX_FORCE_SECURE
	if (secure) {
		flags |= RANDOMX_FLAG_SECURE;
	}
#endif

	if (flags & RANDOMX_FLAG_ARGON2_AVX2) {
		std::cout << " - Argon2 implementation: AVX2" << std::endl;
	}
	else if (flags & RANDOMX_FLAG_ARGON2_SSSE3) {
		std::cout << " - Argon2 implementation: SSSE3" << std::endl;
	}
	else {
		std::cout << " - Argon2 implementation: reference" << std::endl;
	}

	if (flags & RANDOMX_FLAG_FULL_MEM) {
		std::cout << " - full memory mode (2080 MiB)" << std::endl;
	}
	else {
		std::cout << " - light memory mode (256 MiB)" << std::endl;
	}

	if (flags & RANDOMX_FLAG_JIT) {
		std::cout << " - JIT compiled mode ";
		if (flags & RANDOMX_FLAG_SECURE) {
			std::cout << "(secure)";
		}
		std::cout << std::endl;
	}
	else {
		std::cout << " - interpreted mode" << std::endl;
	}

	if (flags & RANDOMX_FLAG_HARD_AES) {
		std::cout << " - hardware AES mode" << std::endl;
	}
	else {
		std::cout << " - software AES mode" << std::endl;
	}

	if (flags & RANDOMX_FLAG_LARGE_PAGES) {
		std::cout << " - large pages mode" << std::endl;
	}
	else {
		std::cout << " - small pages mode" << std::endl;
	}

	if (threadAffinity) {
		std::cout << " - thread affinity (" << mask_to_string(threadAffinity) << ")" << std::endl;
	}

	MineFunc* func;

	if (noBatch) {
		func = &mine<false>;
	}
	else {
		func = &mine<true>;
		std::cout << " - batch mode" << std::endl;
	}

	std::cout << "Initializing";
	if (miningMode)
		std::cout << " (" << initThreadCount << " thread" << (initThreadCount > 1 ? "s)" : ")");
	std::cout << " ..." << std::endl;

	try {
		if (nullptr == randomx::selectArgonImpl(flags)) {
			throw std::runtime_error("Unsupported Argon2 implementation");
		}
		if ((flags & RANDOMX_FLAG_JIT) && !RANDOMX_HAVE_COMPILER) {
			throw std::runtime_error("JIT compilation is not supported on this platform. Try without --jit");
		}
		if (!(flags & RANDOMX_FLAG_JIT) && RANDOMX_HAVE_COMPILER) {
			std::cout << "WARNING: You are using the interpreter mode. Use --jit for optimal performance." << std::endl;
		}

		Stopwatch sw(true);
		cache = randomx_alloc_cache(flags);
		if (cache == nullptr) {
			throw CacheAllocException();
		}
		randomx_init_cache(cache, &seed, sizeof(seed));
		if (miningMode) {
			dataset = randomx_alloc_dataset(flags);
			if (dataset == nullptr) {
				throw DatasetAllocException();
			}
			uint32_t datasetItemCount = randomx_dataset_item_count();
			if (initThreadCount > 1) {
				auto perThread = datasetItemCount / initThreadCount;
				auto remainder = datasetItemCount % initThreadCount;
				uint32_t startItem = 0;
				for (int i = 0; i < initThreadCount; ++i) {
					auto count = perThread + (i == initThreadCount - 1 ? remainder : 0);
					threads.push_back(std::thread(&randomx_init_dataset, dataset, cache, startItem, count));
					startItem += count;
				}
				for (unsigned i = 0; i < threads.size(); ++i) {
					threads[i].join();
				}
			}
			else {
				randomx_init_dataset(dataset, cache, 0, datasetItemCount);
			}
			randomx_release_cache(cache);
			cache = nullptr;
			threads.clear();
		}
		std::cout << "Memory initialized in " << sw.getElapsed() << " s" << std::endl;
		std::cout << "Initializing " << threadCount << " virtual machine(s) ..." << std::endl;
		for (int i = 0; i < threadCount; ++i) {
			randomx_vm *vm = randomx_create_vm(flags, cache, dataset);
			if (vm == nullptr) {
				if ((flags & RANDOMX_FLAG_HARD_AES)) {
					throw std::runtime_error("Cannot create VM with the selected options. Try using --softAes");
				}
				if (largePages) {
					throw std::runtime_error("Cannot create VM with the selected options. Try without --largePages");
				}
				throw std::runtime_error("Cannot create VM");
			}
			vms.push_back(vm);
		}
		std::cout << "Running benchmark (" << noncesCount << " nonces) ..." << std::endl;
		sw.restart();
		if (threadCount > 1) {
			for (unsigned i = 0; i < vms.size(); ++i) {
				int cpuid = -1;
				if (threadAffinity)
					cpuid = cpuid_from_mask(threadAffinity, i);
				threads.push_back(std::thread(func, vms[i], std::ref(atomicNonce), std::ref(result), noncesCount, i, cpuid));
			}
			for (unsigned i = 0; i < threads.size(); ++i) {
				threads[i].join();
			}
		}
		else {
			func(vms[0], std::ref(atomicNonce), std::ref(result), noncesCount, 0, -1);
		}

		double elapsed = sw.getElapsed();
		for (unsigned i = 0; i < vms.size(); ++i)
			randomx_destroy_vm(vms[i]);
		if (miningMode)
			randomx_release_dataset(dataset);
		else
			randomx_release_cache(cache);
		std::cout << "Calculated result: ";
		result.print(std::cout);
		if (noncesCount == 1000 && seedValue == 0)
			std::cout << "Reference result:  10b649a3f15c7c7f88277812f2e74b337a0f20ce909af09199cccb960771cfa1" << std::endl;
		if (!miningMode) {
			std::cout << "Performance: " << 1000 * elapsed / noncesCount << " ms per hash" << std::endl;
		}
		else {
			std::cout << "Performance: " << noncesCount / elapsed << " hashes per second" << std::endl;
		}
	}
	catch (MemoryException& e) {
		std::cout << "ERROR: " << e.what() << std::endl;
		if (largePages) {
#ifdef _WIN32
			std::cout << "To use large pages, please enable the \"Lock Pages in Memory\" policy and reboot." << std::endl;
			if (!IsWindows8OrGreater()) {
				std::cout << "Additionally, you have to run the benchmark from elevated command prompt." << std::endl;
			}
#else
			std::cout << "To use large pages, please run: sudo sysctl -w vm.nr_hugepages=1250" << std::endl;
#endif
		}
		return 1;
	}
	catch (std::exception& e) {
		std::cout << "ERROR: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}
