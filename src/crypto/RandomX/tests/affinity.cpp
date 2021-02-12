/*
Copyright (c) 2019, jtgrassie
Copyright (c) 2019, tevador <tevador@gmail.com>

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

#include <sstream>

#if defined(_WIN32) || defined(__CYGWIN__)
  #include <windows.h>
#else
  #ifdef __APPLE__
    #include <mach/thread_act.h>
    #include <mach/thread_policy.h>
  #endif
  #include <pthread.h>
#endif
#include "affinity.hpp"

int
set_thread_affinity(const unsigned &cpuid)
{
    std::thread::native_handle_type thread;
#if defined(_WIN32) || defined(__CYGWIN__)
    thread = reinterpret_cast<std::thread::native_handle_type>(GetCurrentThread());
#else
    thread = static_cast<std::thread::native_handle_type>(pthread_self());
#endif
    return set_thread_affinity(thread, cpuid);
}

int
set_thread_affinity(std::thread::native_handle_type thread,
        const unsigned &cpuid)
{
    int rc = -1;
#ifdef __APPLE__
    thread_port_t mach_thread;
    thread_affinity_policy_data_t policy = { static_cast<integer_t>(cpuid) };
    mach_thread = pthread_mach_thread_np(thread);
    rc = thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY,
            (thread_policy_t)&policy, 1);
#elif defined(_WIN32) || defined(__CYGWIN__)
    rc = SetThreadAffinityMask(reinterpret_cast<HANDLE>(thread), 1ULL << cpuid) == 0 ? -2 : 0;
#elif !defined(__OpenBSD__) && !defined(__FreeBSD__) && !defined(__ANDROID__) && !defined(__NetBSD__)
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(cpuid, &cs);
    rc = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cs);
#endif
    return rc;
}

unsigned
cpuid_from_mask(uint64_t mask, const unsigned &thread_index)
{
    static unsigned lookup[64];
    static bool init = false;
    if (init)
        return lookup[thread_index];
    unsigned count_found = 0;
    for (unsigned i=0; i<64; i++)
    {
        if (1ULL & mask)
        {
            lookup[count_found] = i;
            count_found++;
        }
        mask >>= 1;
    }
    init = true;
    return lookup[thread_index];
}

std::string
mask_to_string(uint64_t mask)
{
    std::ostringstream ss;
    unsigned len = 0;
    unsigned v = 0;
    unsigned i = 64;
    while (i--)
    {
        v = mask >> i;
        if (1ULL & v)
        {
            if (len == 0) len = i + 1;
            ss << '1';
        }
        else
            if (len > 0) ss << '0';
    }
    return ss.str();
}
