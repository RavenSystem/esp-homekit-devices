/* sp.h
 *
 * Copyright (C) 2006-2017 wolfSSL Inc.
 *
 * This file is part of wolfSSL.
 *
 * wolfSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * wolfSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1335, USA
 */


#ifndef WOLF_CRYPT_SP_H
#define WOLF_CRYPT_SP_H

#include <wolfssl/wolfcrypt/types.h>

#if defined(WOLFSSL_HAVE_SP_RSA) || defined(WOLFSSL_HAVE_SP_DH) || \
                                    defined(WOLFSSL_HAVE_SP_ECC)

#include <stdint.h>

#include <wolfssl/wolfcrypt/integer.h>
#include <wolfssl/wolfcrypt/ecc.h>

#if defined(NO_64BIT) || !defined(HAVE___UINT128_T)
#define SP_WORD_SIZE 32
#else
#define SP_WORD_SIZE 64
#endif

#if !defined(WOLFSSL_X86_64_BUILD) || !defined(USE_INTEL_SPEEDUP)
  #if SP_WORD_SIZE == 32
    typedef int32_t sp_digit;
  #elif SP_WORD_SIZE == 64
    typedef int64_t sp_digit;
    typedef long int128_t __attribute__ ((mode(TI)));
  #else
    #error Word size not defined
  #endif
#else
  #if SP_WORD_SIZE == 32
    typedef uint32_t sp_digit;
  #elif SP_WORD_SIZE == 64
    typedef uint64_t sp_digit;
    typedef unsigned long uint128_t __attribute__ ((mode(TI)));
    typedef long int128_t __attribute__ ((mode(TI)));
  #else
    #error Word size not defined
  #endif
#endif

#if defined(_MSC_VER)
    #define SP_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
    #define SP_NOINLINE __attribute__((noinline))
#else
    #define 5P_NOINLINE
#endif

#ifdef __cplusplus
    extern "C" {
#endif

#ifdef WOLFSSL_HAVE_SP_RSA

WOLFSSL_LOCAL int sp_RsaPublic_2048(const byte* in, word32 inLen,
    mp_int* em, mp_int* mm, byte* out, word32* outLen);
WOLFSSL_LOCAL int sp_RsaPrivate_2048(const byte* in, word32 inLen,
    mp_int* dm, mp_int* pm, mp_int* qm, mp_int* dpm, mp_int* dqm, mp_int* qim,
    mp_int* mm, byte* out, word32* outLen);

WOLFSSL_LOCAL int sp_RsaPublic_3072(const byte* in, word32 inLen,
    mp_int* em, mp_int* mm, byte* out, word32* outLen);
WOLFSSL_LOCAL int sp_RsaPrivate_3072(const byte* in, word32 inLen,
    mp_int* dm, mp_int* pm, mp_int* qm, mp_int* dpm, mp_int* dqm, mp_int* qim,
    mp_int* mm, byte* out, word32* outLen);

#endif /* WOLFSSL_HAVE_SP_RSA */

#ifdef WOLFSSL_HAVE_SP_DH

WOLFSSL_LOCAL int sp_DhExp_2048(mp_int* base, const byte* exp, word32 expLen,
    mp_int* mod, byte* out, word32* outLen);
WOLFSSL_LOCAL int sp_DhExp_3072(mp_int* base, const byte* exp, word32 expLen,
    mp_int* mod, byte* out, word32* outLen);

#endif /* WOLFSSL_HAVE_SP_DH */

#ifdef WOLFSSL_HAVE_SP_ECC

int sp_ecc_mulmod_256(mp_int* km, ecc_point* gm, ecc_point* rm, int map,
                      void* heap);
int sp_ecc_mulmod_base_256(mp_int* km, ecc_point* rm, int map, void* heap);

int sp_ecc_make_key_256(WC_RNG* rng, mp_int* priv, ecc_point* pub, void* heap);
int sp_ecc_secret_gen_256(mp_int* priv, ecc_point* pub, byte* out,
                          word32* outlen, void* heap);
int sp_ecc_sign_256(const byte* hash, word32 hashLen, WC_RNG* rng, mp_int* priv,
                    mp_int* rm, mp_int* sm, void* heap);
int sp_ecc_verify_256(const byte* hash, word32 hashLen, mp_int* pX, mp_int* pY,
                      mp_int* pZ, mp_int* r, mp_int* sm, int* res, void* heap);

#endif /*ifdef WOLFSSL_HAVE_SP_ECC */


#ifdef __cplusplus
    } /* extern "C" */
#endif

#endif /* WOLFSSL_HAVE_SP_RSA || WOLFSSL_HAVE_SP_DH || WOLFSSL_HAVE_SP_ECC */

#endif /* WOLF_CRYPT_SP_H */

