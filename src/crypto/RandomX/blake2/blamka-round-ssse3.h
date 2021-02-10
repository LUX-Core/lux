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

/* Original code from Argon2 reference source code package used under CC0 Licence
 * https://github.com/P-H-C/phc-winner-argon2
 * Copyright 2015
 * Daniel Dinu, Dmitry Khovratovich, Jean-Philippe Aumasson, and Samuel Neves
*/

#ifndef BLAKE_ROUND_MKA_OPT_H
#define BLAKE_ROUND_MKA_OPT_H

#include "blake2-impl.h"

#ifdef __GNUC__
#include <x86intrin.h>
#else
#include <intrin.h>
#endif

#ifdef _mm_roti_epi64 //clang defines it using the XOP instruction set
#undef _mm_roti_epi64
#endif

#define r16                                                                    \
    (_mm_setr_epi8(2, 3, 4, 5, 6, 7, 0, 1, 10, 11, 12, 13, 14, 15, 8, 9))
#define r24                                                                    \
    (_mm_setr_epi8(3, 4, 5, 6, 7, 0, 1, 2, 11, 12, 13, 14, 15, 8, 9, 10))
#define _mm_roti_epi64(x, c)                                                   \
    (-(c) == 32)                                                               \
        ? _mm_shuffle_epi32((x), _MM_SHUFFLE(2, 3, 0, 1))                      \
        : (-(c) == 24)                                                         \
              ? _mm_shuffle_epi8((x), r24)                                     \
              : (-(c) == 16)                                                   \
                    ? _mm_shuffle_epi8((x), r16)                               \
                    : (-(c) == 63)                                             \
                          ? _mm_xor_si128(_mm_srli_epi64((x), -(c)),           \
                                          _mm_add_epi64((x), (x)))             \
                          : _mm_xor_si128(_mm_srli_epi64((x), -(c)),           \
                                          _mm_slli_epi64((x), 64 - (-(c))))

static FORCE_INLINE __m128i fBlaMka(__m128i x, __m128i y) {
    const __m128i z = _mm_mul_epu32(x, y);
    return _mm_add_epi64(_mm_add_epi64(x, y), _mm_add_epi64(z, z));
}

#define G1(A0, B0, C0, D0, A1, B1, C1, D1)                                     \
    do {                                                                       \
        A0 = fBlaMka(A0, B0);                                                  \
        A1 = fBlaMka(A1, B1);                                                  \
                                                                               \
        D0 = _mm_xor_si128(D0, A0);                                            \
        D1 = _mm_xor_si128(D1, A1);                                            \
                                                                               \
        D0 = _mm_roti_epi64(D0, -32);                                          \
        D1 = _mm_roti_epi64(D1, -32);                                          \
                                                                               \
        C0 = fBlaMka(C0, D0);                                                  \
        C1 = fBlaMka(C1, D1);                                                  \
                                                                               \
        B0 = _mm_xor_si128(B0, C0);                                            \
        B1 = _mm_xor_si128(B1, C1);                                            \
                                                                               \
        B0 = _mm_roti_epi64(B0, -24);                                          \
        B1 = _mm_roti_epi64(B1, -24);                                          \
    } while ((void)0, 0)

#define G2(A0, B0, C0, D0, A1, B1, C1, D1)                                     \
    do {                                                                       \
        A0 = fBlaMka(A0, B0);                                                  \
        A1 = fBlaMka(A1, B1);                                                  \
                                                                               \
        D0 = _mm_xor_si128(D0, A0);                                            \
        D1 = _mm_xor_si128(D1, A1);                                            \
                                                                               \
        D0 = _mm_roti_epi64(D0, -16);                                          \
        D1 = _mm_roti_epi64(D1, -16);                                          \
                                                                               \
        C0 = fBlaMka(C0, D0);                                                  \
        C1 = fBlaMka(C1, D1);                                                  \
                                                                               \
        B0 = _mm_xor_si128(B0, C0);                                            \
        B1 = _mm_xor_si128(B1, C1);                                            \
                                                                               \
        B0 = _mm_roti_epi64(B0, -63);                                          \
        B1 = _mm_roti_epi64(B1, -63);                                          \
    } while ((void)0, 0)

#define DIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1)                            \
    do {                                                                       \
        __m128i t0 = _mm_alignr_epi8(B1, B0, 8);                               \
        __m128i t1 = _mm_alignr_epi8(B0, B1, 8);                               \
        B0 = t0;                                                               \
        B1 = t1;                                                               \
                                                                               \
        t0 = C0;                                                               \
        C0 = C1;                                                               \
        C1 = t0;                                                               \
                                                                               \
        t0 = _mm_alignr_epi8(D1, D0, 8);                                       \
        t1 = _mm_alignr_epi8(D0, D1, 8);                                       \
        D0 = t1;                                                               \
        D1 = t0;                                                               \
    } while ((void)0, 0)

#define UNDIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1)                          \
    do {                                                                       \
        __m128i t0 = _mm_alignr_epi8(B0, B1, 8);                               \
        __m128i t1 = _mm_alignr_epi8(B1, B0, 8);                               \
        B0 = t0;                                                               \
        B1 = t1;                                                               \
                                                                               \
        t0 = C0;                                                               \
        C0 = C1;                                                               \
        C1 = t0;                                                               \
                                                                               \
        t0 = _mm_alignr_epi8(D0, D1, 8);                                       \
        t1 = _mm_alignr_epi8(D1, D0, 8);                                       \
        D0 = t1;                                                               \
        D1 = t0;                                                               \
    } while ((void)0, 0)

#define BLAKE2_ROUND(A0, A1, B0, B1, C0, C1, D0, D1)                           \
    do {                                                                       \
        G1(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
        G2(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
                                                                               \
        DIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1);                           \
                                                                               \
        G1(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
        G2(A0, B0, C0, D0, A1, B1, C1, D1);                                    \
                                                                               \
        UNDIAGONALIZE(A0, B0, C0, D0, A1, B1, C1, D1);                         \
    } while ((void)0, 0)


#endif /* BLAKE_ROUND_MKA_OPT_H */
