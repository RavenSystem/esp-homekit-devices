/* config-Crypt.h
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


// <<< Use Configuration Wizard in Context Menu >>>

// <h> wolfCrypt Configuration

//  <h>Cert/Key Strage
//        <o>Cert Storage <0=> SD Card <1=> Mem Buff (1024bytes) <2=> Mem Buff (2048bytes)
#define MDK_CONF_CERT_BUFF 0
#if MDK_CONF_CERT_BUFF== 1
#define USE_CERT_BUFFERS_1024
#elif MDK_CONF_CERT_BUFF == 2
#define USE_CERT_BUFFERS_2048
#endif
//</h>

//  <h>Crypt Algrithm

//      <e>MD2
#define MDK_CONF_MD2 1
#if MDK_CONF_MD2 == 1
#define WOLFSSL_MD2
#endif
//  </e>
//      <e>MD4
#define MDK_CONF_MD4 1
#if MDK_CONF_MD4 == 0
#define NO_MD4
#endif
//  </e>
//      <e>MD5
#define MDK_CONF_MD5 1
#if MDK_CONF_MD5 == 0
#define NO_MD5
#endif
//  </e>
//      <e>SHA
#define MDK_CONF_SHA  1
#if MDK_CONF_SHA == 0
#define NO_SHA
#endif
//  </e>
//      <e>SHA-256
#define MDK_CONF_SHA256 1
#if MDK_CONF_SHA256 == 0
#define NO_SHA256
#endif
//  </e>
//      <e>SHA-384
#define MDK_CONF_SHA384 1
#if MDK_CONF_SHA384 == 1
#define WOLFSSL_SHA384
#endif
//  </e>
//      <e>SHA-512
#define MDK_CONF_SHA512     1
#if MDK_CONF_SHA512     == 1
#define WOLFSSL_SHA512
#endif
//  </e>
//      <e>RIPEMD
#define MDK_CONF_RIPEMD 1
#if MDK_CONF_RIPEMD == 1
#define WOLFSSL_RIPEMD
#endif
//  </e>
//      <e>BLAKE2
#define MDK_CONF_BLAKE2 0
#if MDK_CONF_BLAKE2 == 1
#define HAVE_BLAKE2
#endif
//  </e>
//      <e>HMAC
#define MDK_CONF_HMAC 1
#if MDK_CONF_HMAC == 0
#define NO_HMAC
#endif
//  </e>
//      <e>HMAC KDF
#define MDK_CONF_HKDF 1
#if MDK_CONF_HKDF == 1
#define HAVE_HKDF
#endif
//  </e>

//      <e>AES CCM
#define MDK_CONF_AESCCM 1
#if MDK_CONF_AESCCM == 1
#define HAVE_AESCCM
#endif
//  </e>
//      <e>AES GCM
#define MDK_CONF_AESGCM 1
#if MDK_CONF_AESGCM == 1
#define HAVE_AESGCM
#endif
//  </e>

//      <e>RC4
#define MDK_CONF_RC4 1
#if MDK_CONF_RC4 == 0
#define NO_RC4
#endif
//  </e>

//      <e>HC128
#define MDK_CONF_HC128 1
#if MDK_CONF_AESGCM == 0
#define NO_HC128
#endif
//  </e>

//      <e>RABBIT
#define MDK_CONF_RABBIT 1
#if MDK_CONF_RABBIT == 0
#define NO_RABBIT
#endif
//  </e>

//      <e>CHACHA
#define MDK_CONF_CHACHA 1
#if MDK_CONF_CHACHA == 1
#define HAVE_CHACHA
#endif
//  </e>

//      <e>POLY1305
#define MDK_CONF_POLY1305 1
#if MDK_CONF_POLY1305 == 1
#define HAVE_POLY1305
#define HAVE_ONE_TIME_AUTH
#endif
//  </e>

//      <e>DES3
#define MDK_CONF_DES3 1
#if MDK_CONF_DES3 == 0
#define NO_DES3
#endif
//  </e>

//      <e>AES
#define MDK_CONF_AES 1
#if MDK_CONF_AES == 0
#define NO_AES
#endif
//  </e>

//      <e>CAMELLIA
#define MDK_CONF_CAMELLIA 1
#if MDK_CONF_CAMELLIA == 1
#define HAVE_CAMELLIA
#endif
//  </e>

//      <e>DH
#define MDK_CONF_DH 1
#if MDK_CONF_DH == 0
#define NO_DH
#endif
//  </e>
//      <e>DSA
#define MDK_CONF_DSA 1
#if MDK_CONF_DSA == 0
#define NO_DSA
#endif
//  </e>

//      <e>SRP
#define MDK_CONF_SRP 1
#if MDK_CONF_SRP == 1
#define HAVE_SRP
#endif
//  </e>

//      <e>PWDBASED
#define MDK_CONF_PWDBASED 1
#if MDK_CONF_PWDBASED == 0
#define NO_PWDBASED
#endif
//  </e>

//      <e>ECC
#define MDK_CONF_ECC 1
#if MDK_CONF_ECC == 1
#define HAVE_ECC
#endif
//  </e>

//      <e>CURVE25519
#define MDK_CONF_CURVE25519 1
#if MDK_CONF_CURVE25519 == 1
#define HAVE_CURVE25519
#define CURVED25519_SMALL
//#define TFM_ECC256
#endif
//  </e>

//      <e>ED25519
#define MDK_CONF_ED25519 1
#if MDK_CONF_ED25519 == 1
#define HAVE_ED25519
#endif
//  </e>

//      <e>PKCS7
#define MDK_CONF_PKCS7 0
#if MDK_CONF_PKCS7 == 1
#define HAVE_PKCS7
#endif
//  </e>

//      <e>NTRU (need License, "crypto_ntru.h")
#define MDK_CONF_NTRU 0
#if MDK_CONF_NTRU == 1
#define HAVE_NTRU
#endif
//  </e>
//  </h>

//  <h>Hardware Crypt (See document for usage)
//      <e>Hardware RNG
#define MDK_CONF_STM32F2_RNG 0
#if MDK_CONF_STM32F2_RNG == 1
#define STM32F2_RNG
#else

#endif
//  </e>
//      <e>Hardware Crypt
#define MDK_CONF_STM32F2_CRYPTO 0
#if MDK_CONF_STM32F2_CRYPTO == 1
#define STM32F2_CRYPTO
#endif
//  </e>

// </h>

//  <h>Other Settings

//      <e>Use Fast Math
#define MDK_CONF_FASTMATH 1
#if MDK_CONF_FASTMATH == 1
#define USE_FAST_MATH
#define TFM_TIMING_RESISTANT
#endif
//  </e>
//      <e>Small Stack
#define MDK_CONF_SmallStack 0
#if MDK_CONF_SmallStack == 0
#define NO_WOLFSSL_SMALL_STACK
#endif
//  </e>
//      <e>ErrNo.h
#define MDK_CONF_ErrNo 1
#if MDK_CONF_ErrNo == 1
#define HAVE_ERRNO
#endif
//  </e>
//      <e>Error Strings
#define MDK_CONF_ErrorStrings 1
#if MDK_CONF_ErrorStrings == 0
#define NO_ERROR_STRINGS
#endif
//  </e>
//      <e>zlib (need "zlib.h")
#define MDK_CONF_LIBZ 0
#if MDK_CONF_LIBZ == 1
#define HAVE_LIBZ
#endif
//  </e>
//      <e>CAVIUM (need CAVIUM headers)
#define MDK_CONF_CAVIUM 0
#if MDK_CONF_CAVIUM == 1
#define HAVE_CAVIUM
#endif
//  </e>

//  </h>



//</h>
// <<< end of configuration section >>>
