/* api.c API unit tests
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


/*----------------------------------------------------------------------------*
 | Includes
 *----------------------------------------------------------------------------*/

#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssl/wolfcrypt/settings.h>

#ifndef FOURK_BUF
    #define FOURK_BUF 4096
#endif
#ifndef TWOK_BUF
    #define TWOK_BUF 2048
#endif
#ifndef ONEK_BUF
    #define ONEK_BUF 1024
#endif
#if defined(WOLFSSL_STATIC_MEMORY)
    #include <wolfssl/wolfcrypt/memory.h>
#endif /* WOLFSSL_STATIC_MEMORY */
#ifndef HEAP_HINT
    #define HEAP_HINT NULL
#endif /* WOLFSSL_STAIC_MEMORY */
#ifdef WOLFSSL_ASNC_CRYPT
    #include <wolfssl/wolfcrypt/async.h>
#endif
#ifdef HAVE_ECC
    #include <wolfssl/wolfcrypt/ecc.h>   /* wc_ecc_fp_free */
    #ifndef ECC_ASN963_MAX_BUF_SZ
        #define ECC_ASN963_MAX_BUF_SZ 133
    #endif
    #ifndef ECC_PRIV_KEY_BUF
        #define ECC_PRIV_KEY_BUF 66  /* For non user defined curves. */
    #endif
    #ifdef HAVE_ALL_CURVES
        /* ecc key sizes: 14, 16, 20, 24, 28, 30, 32, 40, 48, 64*/
        #ifndef KEY14
            #define KEY14 14
        #endif
        #if !defined(KEY16)
            #define KEY16 16
        #endif
        #if !defined(KEY20)
            #define KEY20 20
        #endif
        #if !defined(KEY24)
            #define KEY24 24
        #endif
        #if !defined(KEY28)
            #define KEY28 28
        #endif
        #if !defined(KEY30)
            #define KEY30 30
        #endif
        #if !defined(KEY32)
            #define KEY32 32
        #endif
        #if !defined(KEY40)
            #define KEY40 40
        #endif
        #if !defined(KEY48)
            #define KEY48 48
        #endif
        #if !defined(KEY64)
            #define KEY64 64
        #endif
    #else
        /* ecc key sizes: 14, 16, 20, 24, 28, 30, 32, 40, 48, 64*/
        #ifndef KEY14
            #define KEY14 32
        #endif
        #if !defined(KEY16)
            #define KEY16 32
        #endif
        #if !defined(KEY20)
            #define KEY20 32
        #endif
        #if !defined(KEY24)
            #define KEY24 32
        #endif
        #if !defined(KEY28)
            #define KEY28 32
        #endif
        #if !defined(KEY30)
            #define KEY30 32
        #endif
        #if !defined(KEY32)
            #define KEY32 32
        #endif
        #if !defined(KEY40)
            #define KEY40 32
        #endif
        #if !defined(KEY48)
            #define KEY48 32
        #endif
        #if !defined(KEY64)
            #define KEY64 32
        #endif
    #endif
    #if !defined(HAVE_COMP_KEY)
        #if !defined(NOCOMP)
            #define NOCOMP 0
        #endif
    #else
        #if !defined(COMP)
            #define COMP 1
        #endif
    #endif
    #if !defined(DER_SZ)
        #define DER_SZ (keySz * 2 + 1)
    #endif
#endif
#ifndef NO_ASN
    #include <wolfssl/wolfcrypt/asn_public.h>
#endif
#include <wolfssl/error-ssl.h>

#include <stdlib.h>
#include <wolfssl/ssl.h>  /* compatibility layer */
#include <wolfssl/test.h>
#include <tests/unit.h>
#include "examples/server/server.h"
     /* for testing compatibility layer callbacks */

#ifndef NO_MD5
    #include <wolfssl/wolfcrypt/md5.h>
#endif
#ifndef NO_SHA
    #include <wolfssl/wolfcrypt/sha.h>
#endif
#ifndef NO_SHA256
    #include <wolfssl/wolfcrypt/sha256.h>
#endif
#ifdef WOLFSSL_SHA512
    #include <wolfssl/wolfcrypt/sha512.h>
#endif
#ifdef WOLFSSL_SHA384
    #include <wolfssl/wolfcrypt/sha512.h>
#endif

#ifdef WOLFSSL_SHA3
    #include <wolfssl/wolfcrypt/sha3.h>
    #ifndef HEAP_HINT
        #define HEAP_HINT   NULL
    #endif
#endif

#ifndef NO_AES
    #include <wolfssl/wolfcrypt/aes.h>
    #ifdef HAVE_AES_DECRYPT
        #include <wolfssl/wolfcrypt/wc_encrypt.h>
    #endif
#endif
#ifdef WOLFSSL_RIPEMD
    #include <wolfssl/wolfcrypt/ripemd.h>
#endif
#ifdef HAVE_IDEA
    #include <wolfssl/wolfcrypt/idea.h>
#endif
#ifndef NO_DES3
    #include <wolfssl/wolfcrypt/des3.h>
    #include <wolfssl/wolfcrypt/wc_encrypt.h>
#endif

#ifndef NO_HMAC
    #include <wolfssl/wolfcrypt/hmac.h>
#endif

#ifdef HAVE_CHACHA
    #include <wolfssl/wolfcrypt/chacha.h>
#endif

#ifdef HAVE_POLY1305
    #include <wolfssl/wolfcrypt/poly1305.h>
#endif

#if defined(HAVE_CHACHA) && defined(HAVE_POLY1305)
    #include <wolfssl/wolfcrypt/chacha20_poly1305.h>
#endif

#ifdef HAVE_CAMELLIA
    #include <wolfssl/wolfcrypt/camellia.h>
#endif

#ifndef NO_RABBIT
    #include <wolfssl/wolfcrypt/rabbit.h>
#endif

#ifndef NO_RC4
    #include <wolfssl/wolfcrypt/arc4.h>
#endif

#ifdef HAVE_BLAKE2
    #include <wolfssl/wolfcrypt/blake2.h>
#endif


#ifndef NO_RSA
    #include <wolfssl/wolfcrypt/rsa.h>
    #include <wolfssl/wolfcrypt/hash.h>

    #define FOURK_BUF 4096
    #define GEN_BUF  294

    #ifndef USER_CRYPTO_ERROR
        #define USER_CRYPTO_ERROR -101 /* error returned by IPP lib. */
    #endif
#endif

#ifndef NO_SIG_WRAPPER
    #include <wolfssl/wolfcrypt/signature.h>
#endif


#ifdef HAVE_AESCCM
    #include <wolfssl/wolfcrypt/aes.h>
#endif

#ifdef HAVE_HC128
    #include <wolfssl/wolfcrypt/hc128.h>
#endif

#ifdef HAVE_PKCS7
    #include <wolfssl/wolfcrypt/pkcs7.h>
    #include <wolfssl/wolfcrypt/asn.h>
#endif

#if defined(WOLFSSL_SHA3) || defined(HAVE_PKCS7) || !defined(NO_RSA)
    static int  devId = INVALID_DEVID;
#endif
#ifndef NO_DSA
    #include <wolfssl/wolfcrypt/dsa.h>
    #ifndef ONEK_BUF
        #define ONEK_BUF 1024
    #endif
    #ifndef TWOK_BUF
        #define TWOK_BUF 2048
    #endif
    #ifndef FOURK_BUF
        #define FOURK_BUF 4096
    #endif
    #ifndef DSA_SIG_SIZE
        #define DSA_SIG_SIZE 40
    #endif
    #ifndef MAX_DSA_PARAM_SIZE
        #define MAX_DSA_PARAM_SIZE 256
    #endif
#endif

#ifdef WOLFSSL_CMAC
    #include <wolfssl/wolfcrypt/cmac.h>
#endif

#ifdef HAVE_ED25519
    #include <wolfssl/wolfcrypt/ed25519.h>
#endif

#ifdef HAVE_CURVE25519
    #include <wolfssl/wolfcrypt/curve25519.h>
#endif

#if (defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL))
    #include <wolfssl/openssl/ssl.h>
    #ifndef NO_ASN
    /* for ASN_COMMON_NAME DN_tags enum */
    #include <wolfssl/wolfcrypt/asn.h>
    #endif
#endif
#ifdef OPENSSL_EXTRA
    #include <wolfssl/openssl/asn1.h>
    #include <wolfssl/openssl/crypto.h>
    #include <wolfssl/openssl/pkcs12.h>
    #include <wolfssl/openssl/evp.h>
    #include <wolfssl/openssl/dh.h>
    #include <wolfssl/openssl/bn.h>
    #include <wolfssl/openssl/buffer.h>
    #include <wolfssl/openssl/pem.h>
    #include <wolfssl/openssl/ec.h>
    #include <wolfssl/openssl/engine.h>
    #include <wolfssl/openssl/crypto.h>
    #include <wolfssl/openssl/hmac.h>
    #include <wolfssl/openssl/objects.h>
#ifndef NO_AES
    #include <wolfssl/openssl/aes.h>
#endif
#ifndef NO_DES3
    #include <wolfssl/openssl/des.h>
#endif
#endif /* OPENSSL_EXTRA */

#if defined(OPENSSL_EXTRA) && defined(WOLFCRYPT_HAVE_SRP) \
    && !defined(NO_SHA256) && !defined(RC_NO_RNG)
        #include <wolfssl/wolfcrypt/srp.h>
#endif

#if defined(SESSION_CERTS) && defined(TEST_PEER_CERT_CHAIN)
#include "wolfssl/internal.h" /* for testing SSL_get_peer_cert_chain */
#endif

/* force enable test buffers */
#ifndef USE_CERT_BUFFERS_2048
    #define USE_CERT_BUFFERS_2048
#endif
#ifndef USE_CERT_BUFFERS_256
    #define USE_CERT_BUFFERS_256
#endif
#include <wolfssl/certs_test.h>

typedef struct testVector {
    const char* input;
    const char* output;
    size_t inLen;
    size_t outLen;

} testVector;

#if defined(HAVE_PKCS7)
    typedef struct {
        const byte* content;
        word32      contentSz;
        int         contentOID;
        int         encryptOID;
        int         keyWrapOID;
        int         keyAgreeOID;
        byte*       cert;
        size_t      certSz;
        byte*       privateKey;
        word32      privateKeySz;
    } pkcs7EnvelopedVector;

    #ifndef NO_PKCS7_ENCRYPTED_DATA
        typedef struct {
            const byte*     content;
            word32          contentSz;
            int             contentOID;
            int             encryptOID;
            byte*           encryptionKey;
            word32          encryptionKeySz;
        } pkcs7EncryptedVector;
    #endif
#endif /* HAVE_PKCS7 */


/*----------------------------------------------------------------------------*
 | Constants
 *----------------------------------------------------------------------------*/

#define TEST_SUCCESS    (1)
#define TEST_FAIL       (0)

#define testingFmt "   %s:"
#define resultFmt  " %s\n"
static const char* passed = "passed";
static const char* failed = "failed";

#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && \
    (!defined(NO_WOLFSSL_SERVER) || !defined(NO_WOLFSSL_CLIENT))
    static const char* bogusFile  =
    #ifdef _WIN32
        "NUL"
    #else
        "/dev/null"
    #endif
    ;
#endif /* !NO_FILESYSTEM && !NO_CERTS && (!NO_WOLFSSL_SERVER || !NO_WOLFSSL_CLIENT) */

enum {
    TESTING_RSA = 1,
    TESTING_ECC = 2
};


/*----------------------------------------------------------------------------*
 | Setup
 *----------------------------------------------------------------------------*/

static int test_wolfSSL_Init(void)
{
    int result;

    printf(testingFmt, "wolfSSL_Init()");
    result = wolfSSL_Init();
    printf(resultFmt, result == WOLFSSL_SUCCESS ? passed : failed);

    return result;
}


static int test_wolfSSL_Cleanup(void)
{
    int result;

    printf(testingFmt, "wolfSSL_Cleanup()");
    result = wolfSSL_Cleanup();
    printf(resultFmt, result == WOLFSSL_SUCCESS ? passed : failed);

    return result;
}


/*  Initialize the wolfCrypt state.
 *  POST: 0 success.
 */
static int test_wolfCrypt_Init(void)
{
    int result;

    printf(testingFmt, "wolfCrypt_Init()");
    result = wolfCrypt_Init();
    printf(resultFmt, result == 0 ? passed : failed);

    return result;

} /* END test_wolfCrypt_Init */

/*----------------------------------------------------------------------------*
 | Method Allocators
 *----------------------------------------------------------------------------*/

static void test_wolfSSL_Method_Allocators(void)
{
    #define TEST_METHOD_ALLOCATOR(allocator, condition) \
        do {                                            \
            WOLFSSL_METHOD *method;                      \
            condition(method = allocator());            \
            XFREE(method, 0, DYNAMIC_TYPE_METHOD);      \
        } while(0)

    #define TEST_VALID_METHOD_ALLOCATOR(a) \
            TEST_METHOD_ALLOCATOR(a, AssertNotNull)

    #define TEST_INVALID_METHOD_ALLOCATOR(a) \
            TEST_METHOD_ALLOCATOR(a, AssertNull)

#ifndef NO_OLD_TLS
    #ifdef WOLFSSL_ALLOW_SSLV3
        TEST_VALID_METHOD_ALLOCATOR(wolfSSLv3_server_method);
        TEST_VALID_METHOD_ALLOCATOR(wolfSSLv3_client_method);
    #endif
    #ifdef WOLFSL_ALLOW_TLSV10
        TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_server_method);
        TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_client_method);
    #endif
    #ifndef NO_WOLFSSL_SERVER
        TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_1_server_method);
    #endif
    #ifndef NO_WOLFSSL_CLIENT
        TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_1_client_method);
    #endif
#endif
#ifndef WOLFSSL_NO_TLS12
    #ifndef NO_WOLFSSL_SERVER
        TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_2_server_method);
    #endif
    #ifndef NO_WOLFSSL_CLIENT
        TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_2_client_method);
    #endif
#endif
#ifdef WOLFSSL_TLS13
    #ifndef NO_WOLFSSL_SERVER
        TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_3_server_method);
    #endif
    #ifndef NO_WOLFSSL_CLIENT
        TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_3_client_method);
    #endif
#endif
#ifndef NO_WOLFSSL_SERVER
    TEST_VALID_METHOD_ALLOCATOR(wolfSSLv23_server_method);
#endif
#ifndef NO_WOLFSSL_CLIENT
    TEST_VALID_METHOD_ALLOCATOR(wolfSSLv23_client_method);
#endif
#ifdef WOLFSSL_DTLS
    #ifndef NO_OLD_TLS
        TEST_VALID_METHOD_ALLOCATOR(wolfDTLSv1_server_method);
        TEST_VALID_METHOD_ALLOCATOR(wolfDTLSv1_client_method);
    #endif
    TEST_VALID_METHOD_ALLOCATOR(wolfDTLSv1_2_server_method);
    TEST_VALID_METHOD_ALLOCATOR(wolfDTLSv1_2_client_method);
#endif

#ifdef OPENSSL_EXTRA
    TEST_INVALID_METHOD_ALLOCATOR(wolfSSLv2_server_method);
    TEST_INVALID_METHOD_ALLOCATOR(wolfSSLv2_client_method);
#endif
}

/*----------------------------------------------------------------------------*
 | Context
 *----------------------------------------------------------------------------*/
#ifndef NO_WOLFSSL_SERVER
static void test_wolfSSL_CTX_new(WOLFSSL_METHOD *method)
{
    WOLFSSL_CTX *ctx;

    AssertNull(ctx = wolfSSL_CTX_new(NULL));

    AssertNotNull(method);
    AssertNotNull(ctx = wolfSSL_CTX_new(method));
    wolfSSL_CTX_free(ctx);
}
#endif


static void test_wolfSSL_CTX_use_certificate_file(void)
{
#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && !defined(NO_WOLFSSL_SERVER)
    WOLFSSL_CTX *ctx;

    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_server_method()));

    /* invalid context */
    AssertFalse(wolfSSL_CTX_use_certificate_file(NULL, svrCertFile,
                                                             WOLFSSL_FILETYPE_PEM));
    /* invalid cert file */
    AssertFalse(wolfSSL_CTX_use_certificate_file(ctx, bogusFile,
                                                             WOLFSSL_FILETYPE_PEM));
    /* invalid cert type */
    AssertFalse(wolfSSL_CTX_use_certificate_file(ctx, svrCertFile, 9999));

#ifdef NO_RSA
    /* rsa needed */
    AssertFalse(wolfSSL_CTX_use_certificate_file(ctx, svrCertFile,WOLFSSL_FILETYPE_PEM));
#else
    /* success */
    AssertTrue(wolfSSL_CTX_use_certificate_file(ctx, svrCertFile, WOLFSSL_FILETYPE_PEM));
#endif

    wolfSSL_CTX_free(ctx);
#endif
}

/*  Test function for wolfSSL_CTX_use_certificate_buffer. Load cert into
 *  context using buffer.
 *  PRE: NO_CERTS not defined; USE_CERT_BUFFERS_2048 defined; compile with
 *  --enable-testcert flag.
 */
static int test_wolfSSL_CTX_use_certificate_buffer(void)
{
    #if !defined(NO_CERTS) && defined(USE_CERT_BUFFERS_2048) && \
            !defined(NO_RSA) && !defined(NO_WOLFSSL_SERVER)
        WOLFSSL_CTX*            ctx;
        int                     ret;

        printf(testingFmt, "wolfSSL_CTX_use_certificate_buffer()");
        AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_server_method()));

        ret = wolfSSL_CTX_use_certificate_buffer(ctx, server_cert_der_2048,
                    sizeof_server_cert_der_2048, WOLFSSL_FILETYPE_ASN1);

        printf(resultFmt, ret == WOLFSSL_SUCCESS ? passed : failed);
        wolfSSL_CTX_free(ctx);

        return ret;
    #else
        return WOLFSSL_SUCCESS;
    #endif

} /*END test_wolfSSL_CTX_use_certificate_buffer*/

static void test_wolfSSL_CTX_use_PrivateKey_file(void)
{
#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && !defined(NO_WOLFSSL_SERVER)
    WOLFSSL_CTX *ctx;

    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_server_method()));

    /* invalid context */
    AssertFalse(wolfSSL_CTX_use_PrivateKey_file(NULL, svrKeyFile,
                                                             WOLFSSL_FILETYPE_PEM));
    /* invalid key file */
    AssertFalse(wolfSSL_CTX_use_PrivateKey_file(ctx, bogusFile,
                                                             WOLFSSL_FILETYPE_PEM));
    /* invalid key type */
    AssertFalse(wolfSSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, 9999));

    /* success */
#ifdef NO_RSA
    /* rsa needed */
    AssertFalse(wolfSSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, WOLFSSL_FILETYPE_PEM));
#else
    /* success */
    AssertTrue(wolfSSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, WOLFSSL_FILETYPE_PEM));
#endif

    wolfSSL_CTX_free(ctx);
#endif
}


/* test both file and buffer versions along with unloading trusted peer certs */
static void test_wolfSSL_CTX_trust_peer_cert(void)
{
#if !defined(NO_CERTS) && defined(WOLFSSL_TRUST_PEER_CERT) && !defined(NO_WOLFSSL_CLIENT)
    WOLFSSL_CTX *ctx;

    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_client_method()));

#if !defined(NO_FILESYSTEM)
    /* invalid file */
    assert(wolfSSL_CTX_trust_peer_cert(ctx, NULL,
                                              WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS);
    assert(wolfSSL_CTX_trust_peer_cert(ctx, bogusFile,
                                              WOLFSSL_FILETYPE_PEM) != WOLFSSL_SUCCESS);
    assert(wolfSSL_CTX_trust_peer_cert(ctx, cliCertFile,
                                             WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS);

    /* success */
    assert(wolfSSL_CTX_trust_peer_cert(ctx, cliCertFile, WOLFSSL_FILETYPE_PEM)
                                                                == WOLFSSL_SUCCESS);

    /* unload cert */
    assert(wolfSSL_CTX_Unload_trust_peers(NULL) != WOLFSSL_SUCCESS);
    assert(wolfSSL_CTX_Unload_trust_peers(ctx) == WOLFSSL_SUCCESS);
#endif

    /* Test of loading certs from buffers */

    /* invalid buffer */
    assert(wolfSSL_CTX_trust_peer_buffer(ctx, NULL, -1,
                                             WOLFSSL_FILETYPE_ASN1) != WOLFSSL_SUCCESS);

    /* success */
#ifdef USE_CERT_BUFFERS_1024
    assert(wolfSSL_CTX_trust_peer_buffer(ctx, client_cert_der_1024,
                sizeof_client_cert_der_1024, WOLFSSL_FILETYPE_ASN1) == WOLFSSL_SUCCESS);
#endif
#ifdef USE_CERT_BUFFERS_2048
    assert(wolfSSL_CTX_trust_peer_buffer(ctx, client_cert_der_2048,
                sizeof_client_cert_der_2048, WOLFSSL_FILETYPE_ASN1) == WOLFSSL_SUCCESS);
#endif

    /* unload cert */
    assert(wolfSSL_CTX_Unload_trust_peers(NULL) != WOLFSSL_SUCCESS);
    assert(wolfSSL_CTX_Unload_trust_peers(ctx) == WOLFSSL_SUCCESS);

    wolfSSL_CTX_free(ctx);
#endif
}


static void test_wolfSSL_CTX_load_verify_locations(void)
{
#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && !defined(NO_WOLFSSL_CLIENT)
    WOLFSSL_CTX *ctx;
    WOLFSSL_CERT_MANAGER* cm;
#ifdef PERSIST_CERT_CACHE
    int cacheSz;
#endif

    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_client_method()));

    /* invalid context */
    AssertFalse(wolfSSL_CTX_load_verify_locations(NULL, caCertFile, 0));

    /* invalid ca file */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_load_verify_locations(ctx, NULL,      0));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_load_verify_locations(ctx, bogusFile, 0));


#ifndef WOLFSSL_TIRTOS
    /* invalid path */
    /* not working... investigate! */
    /* AssertFalse(wolfSSL_CTX_load_verify_locations(ctx, caCertFile, bogusFile)); */
#endif

    /* load ca cert */
    AssertTrue(wolfSSL_CTX_load_verify_locations(ctx, caCertFile, 0));

#ifdef PERSIST_CERT_CACHE
    /* Get cert cache size */
    cacheSz = wolfSSL_CTX_get_cert_cache_memsize(ctx);
#endif
    /* Test unloading CA's */
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_UnloadCAs(ctx));

#ifdef PERSIST_CERT_CACHE
    /* Verify no certs (result is less than cacheSz) */
    AssertIntGT(cacheSz, wolfSSL_CTX_get_cert_cache_memsize(ctx));
#endif

    /* load ca cert again */
    AssertTrue(wolfSSL_CTX_load_verify_locations(ctx, caCertFile, 0));

    /* Test getting CERT_MANAGER */
    AssertNotNull(cm = wolfSSL_CTX_GetCertManager(ctx));

    /* Test unloading CA's using CM */
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CertManagerUnloadCAs(cm));

#ifdef PERSIST_CERT_CACHE
    /* Verify no certs (result is less than cacheSz) */
    AssertIntGT(cacheSz, wolfSSL_CTX_get_cert_cache_memsize(ctx));
#endif

    wolfSSL_CTX_free(ctx);
#endif
}

static void test_wolfSSL_CTX_SetTmpDH_file(void)
{
#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && !defined(NO_DH) && \
        !defined(NO_WOLFSSL_CLIENT)
    WOLFSSL_CTX *ctx;

    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_client_method()));

    /* invalid context */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_SetTmpDH_file(NULL,
                dhParamFile, WOLFSSL_FILETYPE_PEM));

    /* invalid dhParamFile file */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_SetTmpDH_file(ctx,
                NULL, WOLFSSL_FILETYPE_PEM));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_SetTmpDH_file(ctx,
                bogusFile, WOLFSSL_FILETYPE_PEM));

    /* success */
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_SetTmpDH_file(ctx, dhParamFile,
                WOLFSSL_FILETYPE_PEM));

    wolfSSL_CTX_free(ctx);
#endif
}

static void test_wolfSSL_CTX_SetTmpDH_buffer(void)
{
#if !defined(NO_CERTS) && !defined(NO_DH) && !defined(NO_WOLFSSL_CLIENT)
    WOLFSSL_CTX *ctx;

    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_client_method()));

    /* invalid context */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_SetTmpDH_buffer(NULL, dh_key_der_2048,
                sizeof_dh_key_der_2048, WOLFSSL_FILETYPE_ASN1));

    /* invalid dhParamFile file */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_SetTmpDH_buffer(NULL, NULL,
                0, WOLFSSL_FILETYPE_ASN1));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_SetTmpDH_buffer(ctx, dsa_key_der_2048,
                sizeof_dsa_key_der_2048, WOLFSSL_FILETYPE_ASN1));

    /* success */
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_SetTmpDH_buffer(ctx, dh_key_der_2048,
                sizeof_dh_key_der_2048, WOLFSSL_FILETYPE_ASN1));

    wolfSSL_CTX_free(ctx);
#endif
}

/*----------------------------------------------------------------------------*
 | SSL
 *----------------------------------------------------------------------------*/

static void test_server_wolfSSL_new(void)
{
#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && !defined(NO_RSA) && \
        !defined(NO_WOLFSSL_SERVER)
    WOLFSSL_CTX *ctx;
    WOLFSSL_CTX *ctx_nocert;
    WOLFSSL *ssl;

    AssertNotNull(ctx_nocert = wolfSSL_CTX_new(wolfSSLv23_server_method()));
    AssertNotNull(ctx        = wolfSSL_CTX_new(wolfSSLv23_server_method()));

    AssertTrue(wolfSSL_CTX_use_certificate_file(ctx, svrCertFile, WOLFSSL_FILETYPE_PEM));
    AssertTrue(wolfSSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, WOLFSSL_FILETYPE_PEM));

    /* invalid context */
    AssertNull(ssl = wolfSSL_new(NULL));
#ifndef WOLFSSL_SESSION_EXPORT
    AssertNull(ssl = wolfSSL_new(ctx_nocert));
#endif

    /* success */
    AssertNotNull(ssl = wolfSSL_new(ctx));

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
    wolfSSL_CTX_free(ctx_nocert);
#endif
}


static void test_client_wolfSSL_new(void)
{
#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && !defined(NO_RSA) && \
        !defined(NO_WOLFSSL_CLIENT)
    WOLFSSL_CTX *ctx;
    WOLFSSL_CTX *ctx_nocert;
    WOLFSSL *ssl;

    AssertNotNull(ctx_nocert = wolfSSL_CTX_new(wolfSSLv23_client_method()));
    AssertNotNull(ctx        = wolfSSL_CTX_new(wolfSSLv23_client_method()));

    AssertTrue(wolfSSL_CTX_load_verify_locations(ctx, caCertFile, 0));

    /* invalid context */
    AssertNull(ssl = wolfSSL_new(NULL));

    /* success */
    AssertNotNull(ssl = wolfSSL_new(ctx_nocert));
    wolfSSL_free(ssl);

    /* success */
    AssertNotNull(ssl = wolfSSL_new(ctx));
    wolfSSL_free(ssl);

    wolfSSL_CTX_free(ctx);
    wolfSSL_CTX_free(ctx_nocert);
#endif
}

static void test_wolfSSL_SetTmpDH_file(void)
{
#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && !defined(NO_DH) && \
        !defined(NO_WOLFSSL_SERVER)
    WOLFSSL_CTX *ctx;
    WOLFSSL *ssl;

    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_server_method()));
#ifndef NO_RSA
    AssertTrue(wolfSSL_CTX_use_certificate_file(ctx, svrCertFile,
                WOLFSSL_FILETYPE_PEM));
    AssertTrue(wolfSSL_CTX_use_PrivateKey_file(ctx, svrKeyFile,
                WOLFSSL_FILETYPE_PEM));
#else
    AssertTrue(wolfSSL_CTX_use_certificate_file(ctx, eccCertFile,
                WOLFSSL_FILETYPE_PEM));
    AssertTrue(wolfSSL_CTX_use_PrivateKey_file(ctx, eccKeyFile,
                WOLFSSL_FILETYPE_PEM));
#endif
    AssertNotNull(ssl = wolfSSL_new(ctx));

    /* invalid ssl */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_SetTmpDH_file(NULL,
                dhParamFile, WOLFSSL_FILETYPE_PEM));

    /* invalid dhParamFile file */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_SetTmpDH_file(ssl,
                NULL, WOLFSSL_FILETYPE_PEM));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_SetTmpDH_file(ssl,
                bogusFile, WOLFSSL_FILETYPE_PEM));

    /* success */
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_SetTmpDH_file(ssl, dhParamFile,
                WOLFSSL_FILETYPE_PEM));

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
#endif
}

static void test_wolfSSL_SetTmpDH_buffer(void)
{
#if !defined(NO_CERTS) && !defined(NO_DH) && !defined(NO_WOLFSSL_SERVER)
    WOLFSSL_CTX *ctx;
    WOLFSSL *ssl;

    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_server_method()));
    AssertTrue(wolfSSL_CTX_use_certificate_buffer(ctx, server_cert_der_2048,
                sizeof_server_cert_der_2048, WOLFSSL_FILETYPE_ASN1));
    AssertTrue(wolfSSL_CTX_use_PrivateKey_buffer(ctx, server_key_der_2048,
                sizeof_server_key_der_2048, WOLFSSL_FILETYPE_ASN1));
    AssertNotNull(ssl = wolfSSL_new(ctx));

    /* invalid ssl */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_SetTmpDH_buffer(NULL, dh_key_der_2048,
                sizeof_dh_key_der_2048, WOLFSSL_FILETYPE_ASN1));

    /* invalid dhParamFile file */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_SetTmpDH_buffer(NULL, NULL,
                0, WOLFSSL_FILETYPE_ASN1));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_SetTmpDH_buffer(ssl, dsa_key_der_2048,
                sizeof_dsa_key_der_2048, WOLFSSL_FILETYPE_ASN1));

    /* success */
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_SetTmpDH_buffer(ssl, dh_key_der_2048,
                sizeof_dh_key_der_2048, WOLFSSL_FILETYPE_ASN1));

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
#endif
}


/* Test function for wolfSSL_SetMinVersion. Sets the minimum downgrade version
 * allowed.
 * POST: return 1 on success.
 */
static int test_wolfSSL_SetMinVersion(void)
{
    int                 failFlag = WOLFSSL_SUCCESS;
#ifndef NO_WOLFSSL_CLIENT
    WOLFSSL_CTX*        ctx;
    WOLFSSL*            ssl;
    int                 itr;

    #ifndef NO_OLD_TLS
        const int versions[]  =  { WOLFSSL_TLSV1, WOLFSSL_TLSV1_1,
                                  WOLFSSL_TLSV1_2};
    #elif !defined(WOLFSSL_NO_TLS12)
        const int versions[]  =  { WOLFSSL_TLSV1_2 };
    #else
        const int versions[]  =  { WOLFSSL_TLSV1_3 };
    #endif

    AssertTrue(wolfSSL_Init());
    #ifndef WOLFSSL_NO_TLS12
        ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    #else
        ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
    #endif
    ssl = wolfSSL_new(ctx);

    printf(testingFmt, "wolfSSL_SetMinVersion()");

    for (itr = 0; itr < (int)(sizeof(versions)/sizeof(int)); itr++){
       if(wolfSSL_SetMinVersion(ssl, *(versions + itr)) != WOLFSSL_SUCCESS){
            failFlag = WOLFSSL_FAILURE;
        }
    }

    printf(resultFmt, failFlag == WOLFSSL_SUCCESS ? passed : failed);

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
    AssertTrue(wolfSSL_Cleanup());
#endif
    return failFlag;

} /* END test_wolfSSL_SetMinVersion */


/*----------------------------------------------------------------------------*
 | EC
 *----------------------------------------------------------------------------*/

/* Test function for EC_POINT_new, EC_POINT_mul, EC_POINT_free,
    EC_GROUP_new_by_curve_name
 */

# if defined(OPENSSL_EXTRA)
static void test_wolfSSL_EC(void)
{
#ifdef HAVE_ECC
    BN_CTX *ctx;
    EC_GROUP *group;
    EC_POINT *Gxy, *new_point;
    BIGNUM *k = NULL, *Gx = NULL, *Gy = NULL, *Gz = NULL;
    BIGNUM *X, *Y;
#if defined(WOLFSSL_KEY_GEN) || defined(HAVE_COMP_KEY) || defined(DEBUG_WOLFSSL)
    char* hexStr;
#endif
    const char* kTest = "F4F8338AFCC562C5C3F3E1E46A7EFECD17AF381913FF7A96314EA47055EA0FD0";
    /* NISTP256R1 Gx/Gy */
    const char* kGx   = "6B17D1F2E12C4247F8BCE6E563A440F277037D812DEB33A0F4A13945D898C296";
    const char* kGy   = "4FE342E2FE1A7F9B8EE7EB4A7C0F9E162BCE33576B315ECECBB6406837BF51F5";

    AssertNotNull(ctx = BN_CTX_new());
    AssertNotNull(group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    AssertNotNull(Gxy = EC_POINT_new(group));
    AssertNotNull(new_point = EC_POINT_new(group));
    AssertNotNull(X = BN_new());
    AssertNotNull(Y = BN_new());

    /* load test values */
    AssertIntEQ(BN_hex2bn(&k,  kTest), WOLFSSL_SUCCESS);
    AssertIntEQ(BN_hex2bn(&Gx, kGx),   WOLFSSL_SUCCESS);
    AssertIntEQ(BN_hex2bn(&Gy, kGy),   WOLFSSL_SUCCESS);
    AssertIntEQ(BN_hex2bn(&Gz, "1"),   WOLFSSL_SUCCESS);

    /* populate coordinates for input point */
    Gxy->X = Gx;
    Gxy->Y = Gy;
    Gxy->Z = Gz;

    /* perform point multiplication */
    AssertIntEQ(EC_POINT_mul(group, new_point, NULL, Gxy, k, ctx), WOLFSSL_SUCCESS);

    /* check if point X coordinate is zero */
    AssertIntEQ(BN_is_zero(new_point->X), WOLFSSL_FAILURE);

    /* extract the coordinates from point */
    AssertIntEQ(EC_POINT_get_affine_coordinates_GFp(group, new_point, X, Y, ctx), WOLFSSL_SUCCESS);

    /* check if point X coordinate is zero */
    AssertIntEQ(BN_is_zero(X), WOLFSSL_FAILURE);

    /* check bx2hex */
#if defined(WOLFSSL_KEY_GEN) || defined(HAVE_COMP_KEY) || defined(DEBUG_WOLFSSL)
    hexStr = BN_bn2hex(k);
    AssertStrEQ(hexStr, kTest);
    XFREE(hexStr, NULL, DYNAMIC_TYPE_ECC);

    hexStr = BN_bn2hex(Gx);
    AssertStrEQ(hexStr, kGx);
    XFREE(hexStr, NULL, DYNAMIC_TYPE_ECC);

    hexStr = BN_bn2hex(Gy);
    AssertStrEQ(hexStr, kGy);
    XFREE(hexStr, NULL, DYNAMIC_TYPE_ECC);
#endif

    /* cleanup */
    BN_free(X);
    BN_free(Y);
    BN_free(k);
    EC_POINT_free(new_point);
    EC_POINT_free(Gxy);
    EC_GROUP_free(group);
    BN_CTX_free(ctx);
#endif /* HAVE_ECC */
}
#endif


#include <wolfssl/openssl/pem.h>
/*----------------------------------------------------------------------------*
 | EVP
 *----------------------------------------------------------------------------*/

/* Test function for wolfSSL_EVP_get_cipherbynid.
 */

# if defined(OPENSSL_EXTRA)
static void test_wolfSSL_EVP_get_cipherbynid(void)
{
#ifndef NO_AES
    const WOLFSSL_EVP_CIPHER* c;

    c = wolfSSL_EVP_get_cipherbynid(419);
    #if defined(HAVE_AES_CBC) && defined(WOLFSSL_AES_128)
        AssertNotNull(c);
        AssertNotNull(strcmp("EVP_AES_128_CBC", c));
    #else
        AssertNull(c);
    #endif

    c = wolfSSL_EVP_get_cipherbynid(423);
    #if defined(HAVE_AES_CBC) && defined(WOLFSSL_AES_192)
        AssertNotNull(c);
        AssertNotNull(strcmp("EVP_AES_192_CBC", c));
    #else
        AssertNull(c);
    #endif

    c = wolfSSL_EVP_get_cipherbynid(427);
    #if defined(HAVE_AES_CBC) && defined(WOLFSSL_AES_256)
        AssertNotNull(c);
        AssertNotNull(strcmp("EVP_AES_256_CBC", c));
    #else
        AssertNull(c);
    #endif

    c = wolfSSL_EVP_get_cipherbynid(904);
    #if defined(WOLFSSL_AES_COUNTER) && defined(WOLFSSL_AES_128)
        AssertNotNull(c);
        AssertNotNull(strcmp("EVP_AES_128_CTR", c));
    #else
        AssertNull(c);
    #endif

    c = wolfSSL_EVP_get_cipherbynid(905);
    #if defined(WOLFSSL_AES_COUNTER) && defined(WOLFSSL_AES_192)
        AssertNotNull(c);
        AssertNotNull(strcmp("EVP_AES_192_CTR", c));
    #else
        AssertNull(c);
    #endif

    c = wolfSSL_EVP_get_cipherbynid(906);
    #if defined(WOLFSSL_AES_COUNTER) && defined(WOLFSSL_AES_256)
        AssertNotNull(c);
        AssertNotNull(strcmp("EVP_AES_256_CTR", c));
    #else
        AssertNull(c);
    #endif

    c = wolfSSL_EVP_get_cipherbynid(418);
    #if defined(HAVE_AES_ECB) && defined(WOLFSSL_AES_128)
        AssertNotNull(c);
        AssertNotNull(strcmp("EVP_AES_128_ECB", c));
    #else
        AssertNull(c);
    #endif

    c = wolfSSL_EVP_get_cipherbynid(422);
    #if defined(HAVE_AES_ECB) && defined(WOLFSSL_AES_192)
        AssertNotNull(c);
        AssertNotNull(strcmp("EVP_AES_192_ECB", c));
    #else
        AssertNull(c);
    #endif

    c = wolfSSL_EVP_get_cipherbynid(426);
    #if defined(HAVE_AES_ECB) && defined(WOLFSSL_AES_256)
        AssertNotNull(c);
        AssertNotNull(strcmp("EVP_AES_256_ECB", c));
    #else
        AssertNull(c);
    #endif
#endif

#ifndef NO_DES3
    AssertNotNull(strcmp("EVP_DES_CBC", wolfSSL_EVP_get_cipherbynid(31)));
#ifdef WOLFSSL_DES_ECB
    AssertNotNull(strcmp("EVP_DES_ECB", wolfSSL_EVP_get_cipherbynid(29)));
#endif
    AssertNotNull(strcmp("EVP_DES_EDE3_CBC", wolfSSL_EVP_get_cipherbynid(44)));
#ifdef WOLFSSL_DES_ECB
    AssertNotNull(strcmp("EVP_DES_EDE3_ECB", wolfSSL_EVP_get_cipherbynid(33)));
#endif
#endif /*NO_DES3*/

#ifdef HAVE_IDEA
    AssertNotNull(strcmp("EVP_IDEA_CBC", wolfSSL_EVP_get_cipherbynid(34)));
#endif

  /* test for nid is out of range */
  AssertNull(wolfSSL_EVP_get_cipherbynid(1));

}
#endif

/*----------------------------------------------------------------------------*
 | IO
 *----------------------------------------------------------------------------*/
#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && \
    !defined(NO_RSA)        && !defined(SINGLE_THREADED) && \
    (!defined(NO_WOLFSSL_SERVER) || !defined(NO_WOLFSSL_CLIENT))
#define HAVE_IO_TESTS_DEPENDENCIES
#endif

/* helper functions */
#ifdef HAVE_IO_TESTS_DEPENDENCIES
#ifdef WOLFSSL_SESSION_EXPORT
/* set up function for sending session information */
static int test_export(WOLFSSL* inSsl, byte* buf, word32 sz, void* userCtx)
{
    WOLFSSL_CTX* ctx;
    WOLFSSL*     ssl;

    AssertNotNull(inSsl);
    AssertNotNull(buf);
    AssertIntNE(0, sz);

    /* Set ctx to DTLS 1.2 */
    ctx = wolfSSL_CTX_new(wolfDTLSv1_2_server_method());
    AssertNotNull(ctx);

    ssl = wolfSSL_new(ctx);
    AssertNotNull(ssl);

    AssertIntGE(wolfSSL_dtls_import(ssl, buf, sz), 0);

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
    (void)userCtx;
    return WOLFSSL_SUCCESS;
}
#endif

#if !defined(NO_WOLFSSL_CLIENT) && !defined(NO_WOLFSSL_SERVER)
static THREAD_RETURN WOLFSSL_THREAD test_server_nofail(void* args)
{
    SOCKET_T sockfd = 0;
    SOCKET_T clientfd = 0;
    word16 port;

    callback_functions* cbf = NULL;
    WOLFSSL_METHOD* method = 0;
    WOLFSSL_CTX* ctx = 0;
    WOLFSSL* ssl = 0;

    char msg[] = "I hear you fa shizzle!";
    char input[1024];
    int idx;
    int ret, err = 0;

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    ((func_args*)args)->return_code = TEST_FAIL;
    cbf = ((func_args*)args)->callbacks;
    if (cbf != NULL && cbf->method != NULL) {
        method = cbf->method();
    }
    else {
        method = wolfSSLv23_server_method();
    }
    ctx = wolfSSL_CTX_new(method);

#if defined(USE_WINDOWS_API)
    port = ((func_args*)args)->signal->port;
#elif defined(NO_MAIN_DRIVER) && !defined(WOLFSSL_SNIFFER) && \
     !defined(WOLFSSL_MDK_SHELL) && !defined(WOLFSSL_TIRTOS)
    /* Let tcp_listen assign port */
    port = 0;
#else
    /* Use default port */
    port = wolfSSLPort;
#endif

    wolfSSL_CTX_set_verify(ctx,
                          WOLFSSL_VERIFY_PEER | WOLFSSL_VERIFY_FAIL_IF_NO_PEER_CERT, 0);

#ifdef WOLFSSL_ENCRYPTED_KEYS
    wolfSSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif

    if (wolfSSL_CTX_load_verify_locations(ctx, cliCertFile, 0) != WOLFSSL_SUCCESS)
    {
        /*err_sys("can't load ca file, Please run from wolfSSL home dir");*/
        goto done;
    }
    if (wolfSSL_CTX_use_certificate_file(ctx, svrCertFile, WOLFSSL_FILETYPE_PEM)
            != WOLFSSL_SUCCESS)
    {
        /*err_sys("can't load server cert chain file, "
                "Please run from wolfSSL home dir");*/
        goto done;
    }
    if (wolfSSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, WOLFSSL_FILETYPE_PEM)
            != WOLFSSL_SUCCESS)
    {
        /*err_sys("can't load server key file, "
                "Please run from wolfSSL home dir");*/
        goto done;
    }

    /* call ctx setup callback */
    if (cbf != NULL && cbf->ctx_ready != NULL) {
        cbf->ctx_ready(ctx);
    }

    ssl = wolfSSL_new(ctx);
    tcp_accept(&sockfd, &clientfd, (func_args*)args, port, 0, 0, 0, 0, 1);
    CloseSocket(sockfd);

    if (wolfSSL_set_fd(ssl, clientfd) != WOLFSSL_SUCCESS) {
        /*err_sys("SSL_set_fd failed");*/
        goto done;
    }

    #if !defined(NO_FILESYSTEM) && !defined(NO_DH)
        wolfSSL_SetTmpDH_file(ssl, dhParamFile, WOLFSSL_FILETYPE_PEM);
    #elif !defined(NO_DH)
        SetDH(ssl);  /* will repick suites with DHE, higher priority than PSK */
    #endif

    /* call ssl setup callback */
    if (cbf != NULL && cbf->ssl_ready != NULL) {
        cbf->ssl_ready(ssl);
    }

    do {
#ifdef WOLFSSL_ASYNC_CRYPT
        if (err == WC_PENDING_E) {
            ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
            if (ret < 0) { break; } else if (ret == 0) { continue; }
        }
#endif

        err = 0; /* Reset error */
        ret = wolfSSL_accept(ssl);
        if (ret != WOLFSSL_SUCCESS) {
            err = wolfSSL_get_error(ssl, 0);
        }
    } while (ret != WOLFSSL_SUCCESS && err == WC_PENDING_E);

    if (ret != WOLFSSL_SUCCESS) {
        char buff[WOLFSSL_MAX_ERROR_SZ];
        printf("error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buff));
        /*err_sys("SSL_accept failed");*/
        goto done;
    }

    idx = wolfSSL_read(ssl, input, sizeof(input)-1);
    if (idx > 0) {
        input[idx] = 0;
        printf("Client message: %s\n", input);
    }

    if (wolfSSL_write(ssl, msg, sizeof(msg)) != sizeof(msg))
    {
        /*err_sys("SSL_write failed");*/
#ifdef WOLFSSL_TIRTOS
        return;
#else
        return 0;
#endif
    }

#ifdef WOLFSSL_TIRTOS
    Task_yield();
#endif

    ((func_args*)args)->return_code = TEST_SUCCESS;

done:
    wolfSSL_shutdown(ssl);
    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);

    CloseSocket(clientfd);

#ifdef WOLFSSL_TIRTOS
    fdCloseSession(Task_self());
#endif

#if defined(NO_MAIN_DRIVER) && defined(HAVE_ECC) && defined(FP_ECC) \
                            && defined(HAVE_THREAD_LS)
    wc_ecc_fp_free();  /* free per thread cache */
#endif

#ifndef WOLFSSL_TIRTOS
    return 0;
#endif
}

typedef int (*cbType)(WOLFSSL_CTX *ctx, WOLFSSL *ssl);

static void test_client_nofail(void* args, void *cb)
{
    SOCKET_T sockfd = 0;
    callback_functions* cbf = NULL;

    WOLFSSL_METHOD*  method  = 0;
    WOLFSSL_CTX*     ctx     = 0;
    WOLFSSL*         ssl     = 0;
    WOLFSSL_CIPHER*  cipher;

    char msg[64] = "hello wolfssl!";
    char reply[1024];
    int  input;
    int  msgSz = (int)XSTRLEN(msg);
    int  ret, err = 0;
    int  cipherSuite;
    const char* cipherName1, *cipherName2;

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif
    if (((func_args*)args)->callbacks != NULL) {
        cbf = ((func_args*)args)->callbacks;
    }

    ((func_args*)args)->return_code = TEST_FAIL;
    if (cbf != NULL && cbf->method != NULL) {
        method = cbf->method();
    }
    else {
        method = wolfSSLv23_client_method();
    }
    ctx = wolfSSL_CTX_new(method);

#ifdef WOLFSSL_ENCRYPTED_KEYS
    wolfSSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif

    if (wolfSSL_CTX_load_verify_locations(ctx, caCertFile, 0) != WOLFSSL_SUCCESS)
    {
        /* err_sys("can't load ca file, Please run from wolfSSL home dir");*/
        goto done2;
    }
    if (wolfSSL_CTX_use_certificate_file(ctx, cliCertFile, WOLFSSL_FILETYPE_PEM)
            != WOLFSSL_SUCCESS)
    {
        /*err_sys("can't load client cert file, "
                "Please run from wolfSSL home dir");*/
        goto done2;
    }
    if (wolfSSL_CTX_use_PrivateKey_file(ctx, cliKeyFile, WOLFSSL_FILETYPE_PEM)
            != WOLFSSL_SUCCESS)
    {
        /*err_sys("can't load client key file, "
                "Please run from wolfSSL home dir");*/
        goto done2;
    }

    /* call ctx setup callback */
    if (cbf != NULL && cbf->ctx_ready != NULL) {
        cbf->ctx_ready(ctx);
    }

    ssl = wolfSSL_new(ctx);
    tcp_connect(&sockfd, wolfSSLIP, ((func_args*)args)->signal->port,
                0, 0, ssl);
    if (wolfSSL_set_fd(ssl, sockfd) != WOLFSSL_SUCCESS) {
        /*err_sys("SSL_set_fd failed");*/
        goto done2;
    }

    /* call ssl setup callback */
    if (cbf != NULL && cbf->ssl_ready != NULL) {
        cbf->ssl_ready(ssl);
    }

    do {
#ifdef WOLFSSL_ASYNC_CRYPT
        if (err == WC_PENDING_E) {
            ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
            if (ret < 0) { break; } else if (ret == 0) { continue; }
        }
#endif

        err = 0; /* Reset error */
        ret = wolfSSL_connect(ssl);
        if (ret != WOLFSSL_SUCCESS) {
            err = wolfSSL_get_error(ssl, 0);
        }
    } while (ret != WOLFSSL_SUCCESS && err == WC_PENDING_E);

    if (ret != WOLFSSL_SUCCESS) {
        char buff[WOLFSSL_MAX_ERROR_SZ];
        printf("error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buff));
        /*err_sys("SSL_connect failed");*/
        goto done2;
    }

    /* test the various get cipher methods */
    cipherSuite = wolfSSL_get_current_cipher_suite(ssl);
    cipherName1 = wolfSSL_get_cipher_name(ssl);
    cipherName2 = wolfSSL_get_cipher_name_from_suite(
        (cipherSuite >> 8), cipherSuite & 0xFF);
    AssertStrEQ(cipherName1, cipherName2);

    cipher = wolfSSL_get_current_cipher(ssl);
    cipherName1 = wolfSSL_CIPHER_get_name(cipher);
    cipherName2 = wolfSSL_get_cipher(ssl);
#ifdef NO_ERROR_STRINGS
    AssertNull(cipherName1);
    AssertNull(cipherName2);
#else
    AssertStrEQ(cipherName1, cipherName2);
#endif


    if(cb != NULL)((cbType)cb)(ctx, ssl);

    if (wolfSSL_write(ssl, msg, msgSz) != msgSz)
    {
        /*err_sys("SSL_write failed");*/
        goto done2;
    }

    input = wolfSSL_read(ssl, reply, sizeof(reply)-1);
    if (input > 0)
    {
        reply[input] = 0;
        printf("Server response: %s\n", reply);
    }

    ((func_args*)args)->return_code = TEST_SUCCESS;

done2:
    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);

    CloseSocket(sockfd);

#ifdef WOLFSSL_TIRTOS
    fdCloseSession(Task_self());
#endif

    return;
}
#endif /* !NO_WOLFSSL_CLIENT && !NO_WOLFSSL_SERVER */

/* SNI / ALPN / session export helper functions */
#if defined(HAVE_SNI) || defined(HAVE_ALPN) || defined(WOLFSSL_SESSION_EXPORT)

static THREAD_RETURN WOLFSSL_THREAD run_wolfssl_server(void* args)
{
    callback_functions* callbacks = ((func_args*)args)->callbacks;

    WOLFSSL_CTX* ctx = wolfSSL_CTX_new(callbacks->method());
    WOLFSSL*     ssl = NULL;
    SOCKET_T    sfd = 0;
    SOCKET_T    cfd = 0;
    word16      port;

    char msg[] = "I hear you fa shizzle!";
    int  len   = (int) XSTRLEN(msg);
    char input[1024];
    int  idx;
    int  ret, err = 0;

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif
    ((func_args*)args)->return_code = TEST_FAIL;

#if defined(USE_WINDOWS_API)
    port = ((func_args*)args)->signal->port;
#elif defined(NO_MAIN_DRIVER) && !defined(WOLFSSL_SNIFFER) && \
     !defined(WOLFSSL_MDK_SHELL) && !defined(WOLFSSL_TIRTOS)
    /* Let tcp_listen assign port */
    port = 0;
#else
    /* Use default port */
    port = wolfSSLPort;
#endif

    wolfSSL_CTX_set_verify(ctx,
                          WOLFSSL_VERIFY_PEER | WOLFSSL_VERIFY_FAIL_IF_NO_PEER_CERT, 0);

#ifdef WOLFSSL_ENCRYPTED_KEYS
    wolfSSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif
#ifdef WOLFSSL_SESSION_EXPORT
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_dtls_set_export(ctx, test_export));
#endif


    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_load_verify_locations(ctx, cliCertFile, 0));

    AssertIntEQ(WOLFSSL_SUCCESS,
               wolfSSL_CTX_use_certificate_file(ctx, svrCertFile, WOLFSSL_FILETYPE_PEM));

    AssertIntEQ(WOLFSSL_SUCCESS,
                 wolfSSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, WOLFSSL_FILETYPE_PEM));

    if (callbacks->ctx_ready)
        callbacks->ctx_ready(ctx);

    ssl = wolfSSL_new(ctx);
    if (wolfSSL_dtls(ssl)) {
        SOCKADDR_IN_T cliAddr;
        socklen_t     cliLen;

        cliLen = sizeof(cliAddr);
        tcp_accept(&sfd, &cfd, (func_args*)args, port, 0, 1, 0, 0, 0);
        idx = (int)recvfrom(sfd, input, sizeof(input), MSG_PEEK,
                (struct sockaddr*)&cliAddr, &cliLen);
        AssertIntGT(idx, 0);
        wolfSSL_dtls_set_peer(ssl, &cliAddr, cliLen);
    }
    else {
        tcp_accept(&sfd, &cfd, (func_args*)args, port, 0, 0, 0, 0, 1);
        CloseSocket(sfd);
    }

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_set_fd(ssl, cfd));

#ifdef NO_PSK
    #if !defined(NO_FILESYSTEM) && !defined(NO_DH)
        wolfSSL_SetTmpDH_file(ssl, dhParamFile, WOLFSSL_FILETYPE_PEM);
    #elif !defined(NO_DH)
        SetDH(ssl);  /* will repick suites with DHE, higher priority than PSK */
    #endif
#endif

    if (callbacks->ssl_ready)
        callbacks->ssl_ready(ssl);

    do {
#ifdef WOLFSSL_ASYNC_CRYPT
        if (err == WC_PENDING_E) {
            ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
            if (ret < 0) { break; } else if (ret == 0) { continue; }
        }
#endif

        err = 0; /* Reset error */
        ret = wolfSSL_accept(ssl);
        if (ret != WOLFSSL_SUCCESS) {
            err = wolfSSL_get_error(ssl, 0);
        }
    } while (ret != WOLFSSL_SUCCESS && err == WC_PENDING_E);

    if (ret != WOLFSSL_SUCCESS) {
        char buff[WOLFSSL_MAX_ERROR_SZ];
        printf("error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buff));
        /*err_sys("SSL_accept failed");*/
    }
    else {
        if (0 < (idx = wolfSSL_read(ssl, input, sizeof(input)-1))) {
            input[idx] = 0;
            printf("Client message: %s\n", input);
        }

        AssertIntEQ(len, wolfSSL_write(ssl, msg, len));
#if defined(WOLFSSL_SESSION_EXPORT) && !defined(HAVE_IO_POOL)
        if (wolfSSL_dtls(ssl)) {
            byte*  import;
            word32 sz;

            wolfSSL_dtls_export(ssl, NULL, &sz);
            import = (byte*)XMALLOC(sz, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            AssertNotNull(import);
            idx = wolfSSL_dtls_export(ssl, import, &sz);
            AssertIntGE(idx, 0);
            AssertIntGE(wolfSSL_dtls_import(ssl, import, idx), 0);
            XFREE(import, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        }
#endif
#ifdef WOLFSSL_TIRTOS
        Task_yield();
#endif
        ((func_args*)args)->return_code = TEST_SUCCESS;
    }

    if (callbacks->on_result)
        callbacks->on_result(ssl);

    wolfSSL_shutdown(ssl);
    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
    CloseSocket(cfd);


#ifdef WOLFSSL_TIRTOS
    fdCloseSession(Task_self());
#endif

#if defined(NO_MAIN_DRIVER) && defined(HAVE_ECC) && defined(FP_ECC) \
                            && defined(HAVE_THREAD_LS)
    wc_ecc_fp_free();  /* free per thread cache */
#endif

#ifndef WOLFSSL_TIRTOS
    return 0;
#endif
}


static void run_wolfssl_client(void* args)
{
    callback_functions* callbacks = ((func_args*)args)->callbacks;

    WOLFSSL_CTX* ctx = wolfSSL_CTX_new(callbacks->method());
    WOLFSSL*     ssl = NULL;
    SOCKET_T    sfd = 0;

    char msg[] = "hello wolfssl server!";
    int  len   = (int) XSTRLEN(msg);
    char input[1024];
    int  idx;
    int  ret, err = 0;

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    ((func_args*)args)->return_code = TEST_FAIL;

#ifdef WOLFSSL_ENCRYPTED_KEYS
    wolfSSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_load_verify_locations(ctx, caCertFile, 0));

    AssertIntEQ(WOLFSSL_SUCCESS,
               wolfSSL_CTX_use_certificate_file(ctx, cliCertFile, WOLFSSL_FILETYPE_PEM));

    AssertIntEQ(WOLFSSL_SUCCESS,
                 wolfSSL_CTX_use_PrivateKey_file(ctx, cliKeyFile, WOLFSSL_FILETYPE_PEM));

    if (callbacks->ctx_ready)
        callbacks->ctx_ready(ctx);

    ssl = wolfSSL_new(ctx);
    if (wolfSSL_dtls(ssl)) {
        tcp_connect(&sfd, wolfSSLIP, ((func_args*)args)->signal->port,
                    1, 0, ssl);
    }
    else {
        tcp_connect(&sfd, wolfSSLIP, ((func_args*)args)->signal->port,
                    0, 0, ssl);
    }
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_set_fd(ssl, sfd));

    if (callbacks->ssl_ready)
        callbacks->ssl_ready(ssl);

    do {
#ifdef WOLFSSL_ASYNC_CRYPT
        if (err == WC_PENDING_E) {
            ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
            if (ret < 0) { break; } else if (ret == 0) { continue; }
        }
#endif

        err = 0; /* Reset error */
        ret = wolfSSL_connect(ssl);
        if (ret != WOLFSSL_SUCCESS) {
            err = wolfSSL_get_error(ssl, 0);
        }
    } while (ret != WOLFSSL_SUCCESS && err == WC_PENDING_E);

    if (ret != WOLFSSL_SUCCESS) {
        char buff[WOLFSSL_MAX_ERROR_SZ];
        printf("error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buff));
        /*err_sys("SSL_connect failed");*/
    }
    else {
        AssertIntEQ(len, wolfSSL_write(ssl, msg, len));

        if (0 < (idx = wolfSSL_read(ssl, input, sizeof(input)-1))) {
            input[idx] = 0;
            printf("Server response: %s\n", input);
        }
        ((func_args*)args)->return_code = TEST_SUCCESS;
    }

    if (callbacks->on_result)
        callbacks->on_result(ssl);

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
    CloseSocket(sfd);

#ifdef WOLFSSL_TIRTOS
    fdCloseSession(Task_self());
#endif
}

#endif /* defined(HAVE_SNI) || defined(HAVE_ALPN) ||
          defined(WOLFSSL_SESSION_EXPORT) */
#endif /* io tests dependencies */

#if !defined(NO_WOLFSSL_CLIENT) && !defined(NO_WOLFSSL_SERVER)
static void test_wolfSSL_read_write(void)
{
#ifdef HAVE_IO_TESTS_DEPENDENCIES
    /* The unit testing for read and write shall happen simutaneously, since
     * one can't do anything with one without the other. (Except for a failure
     * test case.) This function will call all the others that will set up,
     * execute, and report their test findings.
     *
     * Set up the success case first. This function will become the template
     * for the other tests. This should eventually be renamed
     *
     * The success case isn't interesting, how can this fail?
     * - Do not give the client context a CA certificate. The connect should
     *   fail. Do not need server for this?
     * - Using NULL for the ssl object on server. Do not need client for this.
     * - Using NULL for the ssl object on client. Do not need server for this.
     * - Good ssl objects for client and server. Client write() without server
     *   read().
     * - Good ssl objects for client and server. Server write() without client
     *   read().
     * - Forgetting the password callback?
    */
    tcp_ready ready;
    func_args client_args;
    func_args server_args;
    THREAD_TYPE serverThread;

    XMEMSET(&client_args, 0, sizeof(func_args));
    XMEMSET(&server_args, 0, sizeof(func_args));
#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    StartTCP();
    InitTcpReady(&ready);

#if defined(USE_WINDOWS_API)
    /* use RNG to get random port if using windows */
    ready.port = GetRandomPort();
#endif

    server_args.signal = &ready;
    client_args.signal = &ready;

    start_thread(test_server_nofail, &server_args, &serverThread);
    wait_tcp_ready(&server_args);
    test_client_nofail(&client_args, NULL);
    join_thread(serverThread);

    AssertTrue(client_args.return_code);
    AssertTrue(server_args.return_code);

    FreeTcpReady(&ready);

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

#endif
}
#endif /* !NO_WOLFSSL_CLIENT && !NO_WOLFSSL_SERVER */


#if defined(HAVE_IO_TESTS_DEPENDENCIES) && defined(WOLFSSL_DTLS) && \
    defined(WOLFSSL_SESSION_EXPORT)
/* canned export of a session using older version 3 */
static unsigned char version_3[] = {
    0xA5, 0xA3, 0x01, 0x87, 0x00, 0x39, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x80,
    0x00, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xC0, 0x30, 0x05, 0x09, 0x0A,
    0x01, 0x01, 0x00, 0x0D, 0x05, 0xFE, 0xFD, 0x01,
    0x25, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
    0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00,
    0x01, 0x00, 0x07, 0x00, 0x00, 0x00, 0x30, 0x00,
    0x00, 0x00, 0x10, 0x01, 0x01, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x20, 0x05, 0x12, 0xCF, 0x22,
    0xA1, 0x9F, 0x1C, 0x39, 0x1D, 0x31, 0x11, 0x12,
    0x1D, 0x11, 0x18, 0x0D, 0x0B, 0xF3, 0xE1, 0x4D,
    0xDC, 0xB1, 0xF1, 0x39, 0x98, 0x91, 0x6C, 0x48,
    0xE5, 0xED, 0x11, 0x12, 0xA0, 0x00, 0xF2, 0x25,
    0x4C, 0x09, 0x26, 0xD1, 0x74, 0xDF, 0x23, 0x40,
    0x15, 0x6A, 0x42, 0x2A, 0x26, 0xA5, 0xAC, 0x56,
    0xD5, 0x4A, 0x20, 0xB7, 0xE9, 0xEF, 0xEB, 0xAF,
    0xA8, 0x1E, 0x23, 0x7C, 0x04, 0xAA, 0xA1, 0x6D,
    0x92, 0x79, 0x7B, 0xFA, 0x80, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x0C, 0x79, 0x7B,
    0xFA, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xAA, 0xA1, 0x6D, 0x92, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
    0x00, 0x20, 0x00, 0x04, 0x00, 0x10, 0x00, 0x10,
    0x08, 0x02, 0x05, 0x08, 0x01, 0x30, 0x28, 0x00,
    0x00, 0x0F, 0x00, 0x02, 0x00, 0x09, 0x31, 0x32,
    0x37, 0x2E, 0x30, 0x2E, 0x30, 0x2E, 0x31, 0xED,
    0x4F
};
#endif /* defined(HAVE_IO_TESTS_DEPENDENCIES) && defined(WOLFSSL_DTLS) && \
          defined(WOLFSSL_SESSION_EXPORT) */

static void test_wolfSSL_dtls_export(void)
{
#if defined(HAVE_IO_TESTS_DEPENDENCIES) && defined(WOLFSSL_DTLS) && \
    defined(WOLFSSL_SESSION_EXPORT)
    tcp_ready ready;
    func_args client_args;
    func_args server_args;
    THREAD_TYPE serverThread;
    callback_functions server_cbf;
    callback_functions client_cbf;
#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    InitTcpReady(&ready);

#if defined(USE_WINDOWS_API)
    /* use RNG to get random port if using windows */
    ready.port = GetRandomPort();
#endif

    /* set using dtls */
    XMEMSET(&client_args, 0, sizeof(func_args));
    XMEMSET(&server_args, 0, sizeof(func_args));
    XMEMSET(&server_cbf, 0, sizeof(callback_functions));
    XMEMSET(&client_cbf, 0, sizeof(callback_functions));
    server_cbf.method = wolfDTLSv1_2_server_method;
    client_cbf.method = wolfDTLSv1_2_client_method;
    server_args.callbacks = &server_cbf;
    client_args.callbacks = &client_cbf;

    server_args.signal = &ready;
    client_args.signal = &ready;

    start_thread(run_wolfssl_server, &server_args, &serverThread);
    wait_tcp_ready(&server_args);
    run_wolfssl_client(&client_args);
    join_thread(serverThread);

    AssertTrue(client_args.return_code);
    AssertTrue(server_args.return_code);

    FreeTcpReady(&ready);

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    {
        WOLFSSL_CTX* ctx;
        WOLFSSL*     ssl;

        /* Set ctx to DTLS 1.2 */
        AssertNotNull(ctx = wolfSSL_CTX_new(wolfDTLSv1_2_server_method()));
        AssertNotNull(ssl = wolfSSL_new(ctx));

        /* test importing version 3 */
        AssertIntGE(wolfSSL_dtls_import(ssl, version_3, sizeof(version_3)), 0);

        /* test importing bad length and bad version */
        version_3[2] += 1;
        AssertIntLT(wolfSSL_dtls_import(ssl, version_3, sizeof(version_3)), 0);
        version_3[2] -= 1; version_3[1] = 0XA0;
        AssertIntLT(wolfSSL_dtls_import(ssl, version_3, sizeof(version_3)), 0);
        wolfSSL_free(ssl);
        wolfSSL_CTX_free(ctx);
    }

    printf(testingFmt, "wolfSSL_dtls_export()");
    printf(resultFmt, passed);
#endif
}

/*----------------------------------------------------------------------------*
 | TLS extensions tests
 *----------------------------------------------------------------------------*/

#if defined(HAVE_SNI) || defined(HAVE_ALPN)
/* connection test runner */
static void test_wolfSSL_client_server(callback_functions* client_callbacks,
                                       callback_functions* server_callbacks)
{
#ifdef HAVE_IO_TESTS_DEPENDENCIES
    tcp_ready ready;
    func_args client_args;
    func_args server_args;
    THREAD_TYPE serverThread;

    XMEMSET(&client_args, 0, sizeof(func_args));
    XMEMSET(&server_args, 0, sizeof(func_args));

    StartTCP();

    client_args.callbacks = client_callbacks;
    server_args.callbacks = server_callbacks;

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    /* RUN Server side */
    InitTcpReady(&ready);

#if defined(USE_WINDOWS_API)
    /* use RNG to get random port if using windows */
    ready.port = GetRandomPort();
#endif

    server_args.signal = &ready;
    client_args.signal = &ready;
    start_thread(run_wolfssl_server, &server_args, &serverThread);
    wait_tcp_ready(&server_args);

    /* RUN Client side */
    run_wolfssl_client(&client_args);
    join_thread(serverThread);

    FreeTcpReady(&ready);
#ifdef WOLFSSL_TIRTOS
    fdCloseSession(Task_self());
#endif

#else
    (void)client_callbacks;
    (void)server_callbacks;
#endif
}

#endif /* defined(HAVE_SNI) || defined(HAVE_ALPN) */


#ifdef HAVE_SNI
static void test_wolfSSL_UseSNI_params(void)
{
    WOLFSSL_CTX *ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    WOLFSSL     *ssl = wolfSSL_new(ctx);

    AssertNotNull(ctx);
    AssertNotNull(ssl);

    /* invalid [ctx|ssl] */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_UseSNI(NULL, 0, "ctx", 3));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_UseSNI(    NULL, 0, "ssl", 3));
    /* invalid type */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_UseSNI(ctx, -1, "ctx", 3));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_UseSNI(    ssl, -1, "ssl", 3));
    /* invalid data */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_UseSNI(ctx,  0, NULL,  3));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_UseSNI(    ssl,  0, NULL,  3));
    /* success case */
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_UseSNI(ctx,  0, "ctx", 3));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseSNI(    ssl,  0, "ssl", 3));

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
}

/* BEGIN of connection tests callbacks */
static void use_SNI_at_ctx(WOLFSSL_CTX* ctx)
{
    AssertIntEQ(WOLFSSL_SUCCESS,
        wolfSSL_CTX_UseSNI(ctx, WOLFSSL_SNI_HOST_NAME, "www.wolfssl.com", 15));
}

static void use_SNI_at_ssl(WOLFSSL* ssl)
{
    AssertIntEQ(WOLFSSL_SUCCESS,
             wolfSSL_UseSNI(ssl, WOLFSSL_SNI_HOST_NAME, "www.wolfssl.com", 15));
}

static void different_SNI_at_ssl(WOLFSSL* ssl)
{
    AssertIntEQ(WOLFSSL_SUCCESS,
             wolfSSL_UseSNI(ssl, WOLFSSL_SNI_HOST_NAME, "ww2.wolfssl.com", 15));
}

static void use_SNI_WITH_CONTINUE_at_ssl(WOLFSSL* ssl)
{
    use_SNI_at_ssl(ssl);
    wolfSSL_SNI_SetOptions(ssl, WOLFSSL_SNI_HOST_NAME,
                                              WOLFSSL_SNI_CONTINUE_ON_MISMATCH);
}

static void use_SNI_WITH_FAKE_ANSWER_at_ssl(WOLFSSL* ssl)
{
    use_SNI_at_ssl(ssl);
    wolfSSL_SNI_SetOptions(ssl, WOLFSSL_SNI_HOST_NAME,
                                                WOLFSSL_SNI_ANSWER_ON_MISMATCH);
}

static void use_MANDATORY_SNI_at_ctx(WOLFSSL_CTX* ctx)
{
    use_SNI_at_ctx(ctx);
    wolfSSL_CTX_SNI_SetOptions(ctx, WOLFSSL_SNI_HOST_NAME,
                                                  WOLFSSL_SNI_ABORT_ON_ABSENCE);
}

static void use_MANDATORY_SNI_at_ssl(WOLFSSL* ssl)
{
    use_SNI_at_ssl(ssl);
    wolfSSL_SNI_SetOptions(ssl, WOLFSSL_SNI_HOST_NAME,
                                                  WOLFSSL_SNI_ABORT_ON_ABSENCE);
}

static void use_PSEUDO_MANDATORY_SNI_at_ctx(WOLFSSL_CTX* ctx)
{
    use_SNI_at_ctx(ctx);
    wolfSSL_CTX_SNI_SetOptions(ctx, WOLFSSL_SNI_HOST_NAME,
                 WOLFSSL_SNI_ANSWER_ON_MISMATCH | WOLFSSL_SNI_ABORT_ON_ABSENCE);
}

static void verify_UNKNOWN_SNI_on_server(WOLFSSL* ssl)
{
    AssertIntEQ(UNKNOWN_SNI_HOST_NAME_E, wolfSSL_get_error(ssl, 0));
}

static void verify_SNI_ABSENT_on_server(WOLFSSL* ssl)
{
    AssertIntEQ(SNI_ABSENT_ERROR, wolfSSL_get_error(ssl, 0));
}

static void verify_SNI_no_matching(WOLFSSL* ssl)
{
    byte type = WOLFSSL_SNI_HOST_NAME;
    char* request = (char*) &type; /* to be overwriten */

    AssertIntEQ(WOLFSSL_SNI_NO_MATCH, wolfSSL_SNI_Status(ssl, type));
    AssertNotNull(request);
    AssertIntEQ(0, wolfSSL_SNI_GetRequest(ssl, type, (void**) &request));
    AssertNull(request);
}

static void verify_SNI_real_matching(WOLFSSL* ssl)
{
    byte type = WOLFSSL_SNI_HOST_NAME;
    char* request = NULL;

    AssertIntEQ(WOLFSSL_SNI_REAL_MATCH, wolfSSL_SNI_Status(ssl, type));
    AssertIntEQ(15, wolfSSL_SNI_GetRequest(ssl, type, (void**) &request));
    AssertNotNull(request);
    AssertStrEQ("www.wolfssl.com", request);
}

static void verify_SNI_fake_matching(WOLFSSL* ssl)
{
    byte type = WOLFSSL_SNI_HOST_NAME;
    char* request = NULL;

    AssertIntEQ(WOLFSSL_SNI_FAKE_MATCH, wolfSSL_SNI_Status(ssl, type));
    AssertIntEQ(15, wolfSSL_SNI_GetRequest(ssl, type, (void**) &request));
    AssertNotNull(request);
    AssertStrEQ("ww2.wolfssl.com", request);
}

static void verify_FATAL_ERROR_on_client(WOLFSSL* ssl)
{
    AssertIntEQ(FATAL_ERROR, wolfSSL_get_error(ssl, 0));
}
/* END of connection tests callbacks */

static void test_wolfSSL_UseSNI_connection(void)
{
    unsigned long i;
    callback_functions callbacks[] = {
        /* success case at ctx */
        {0, use_SNI_at_ctx, 0, 0},
        {0, use_SNI_at_ctx, 0, verify_SNI_real_matching},

        /* success case at ssl */
        {0, 0, use_SNI_at_ssl, verify_SNI_real_matching},
        {0, 0, use_SNI_at_ssl, verify_SNI_real_matching},

        /* default missmatch behavior */
        {0, 0, different_SNI_at_ssl, verify_FATAL_ERROR_on_client},
        {0, 0, use_SNI_at_ssl,       verify_UNKNOWN_SNI_on_server},

        /* continue on missmatch */
        {0, 0, different_SNI_at_ssl,         0},
        {0, 0, use_SNI_WITH_CONTINUE_at_ssl, verify_SNI_no_matching},

        /* fake answer on missmatch */
        {0, 0, different_SNI_at_ssl,            0},
        {0, 0, use_SNI_WITH_FAKE_ANSWER_at_ssl, verify_SNI_fake_matching},

        /* sni abort - success */
        {0, use_SNI_at_ctx,           0, 0},
        {0, use_MANDATORY_SNI_at_ctx, 0, verify_SNI_real_matching},

        /* sni abort - abort when absent (ctx) */
        {0, 0,                        0, verify_FATAL_ERROR_on_client},
        {0, use_MANDATORY_SNI_at_ctx, 0, verify_SNI_ABSENT_on_server},

        /* sni abort - abort when absent (ssl) */
        {0, 0, 0,                        verify_FATAL_ERROR_on_client},
        {0, 0, use_MANDATORY_SNI_at_ssl, verify_SNI_ABSENT_on_server},

        /* sni abort - success when overwriten */
        {0, 0, 0, 0},
        {0, use_MANDATORY_SNI_at_ctx, use_SNI_at_ssl, verify_SNI_no_matching},

        /* sni abort - success when allowing missmatches */
        {0, 0, different_SNI_at_ssl, 0},
        {0, use_PSEUDO_MANDATORY_SNI_at_ctx, 0, verify_SNI_fake_matching},
    };

    for (i = 0; i < sizeof(callbacks) / sizeof(callback_functions); i += 2) {
        callbacks[i    ].method = wolfSSLv23_client_method;
        callbacks[i + 1].method = wolfSSLv23_server_method;
        test_wolfSSL_client_server(&callbacks[i], &callbacks[i + 1]);
    }
}

static void test_wolfSSL_SNI_GetFromBuffer(void)
{
    byte buffer[] = { /* www.paypal.com */
        0x00, 0x00, 0x00, 0x00, 0xff, 0x01, 0x00, 0x00, 0x60, 0x03, 0x03, 0x5c,
        0xc4, 0xb3, 0x8c, 0x87, 0xef, 0xa4, 0x09, 0xe0, 0x02, 0xab, 0x86, 0xca,
        0x76, 0xf0, 0x9e, 0x01, 0x65, 0xf6, 0xa6, 0x06, 0x13, 0x1d, 0x0f, 0xa5,
        0x79, 0xb0, 0xd4, 0x77, 0x22, 0xeb, 0x1a, 0x00, 0x00, 0x16, 0x00, 0x6b,
        0x00, 0x67, 0x00, 0x39, 0x00, 0x33, 0x00, 0x3d, 0x00, 0x3c, 0x00, 0x35,
        0x00, 0x2f, 0x00, 0x05, 0x00, 0x04, 0x00, 0x0a, 0x01, 0x00, 0x00, 0x21,
        0x00, 0x00, 0x00, 0x13, 0x00, 0x11, 0x00, 0x00, 0x0e, 0x77, 0x77, 0x77,
        0x2e, 0x70, 0x61, 0x79, 0x70, 0x61, 0x6c, 0x2e, 0x63, 0x6f, 0x6d, 0x00,
        0x0d, 0x00, 0x06, 0x00, 0x04, 0x04, 0x01, 0x02, 0x01
    };

    byte buffer2[] = { /* api.textmate.org */
        0x16, 0x03, 0x01, 0x00, 0xc6, 0x01, 0x00, 0x00, 0xc2, 0x03, 0x03, 0x52,
        0x8b, 0x7b, 0xca, 0x69, 0xec, 0x97, 0xd5, 0x08, 0x03, 0x50, 0xfe, 0x3b,
        0x99, 0xc3, 0x20, 0xce, 0xa5, 0xf6, 0x99, 0xa5, 0x71, 0xf9, 0x57, 0x7f,
        0x04, 0x38, 0xf6, 0x11, 0x0b, 0xb8, 0xd3, 0x00, 0x00, 0x5e, 0x00, 0xff,
        0xc0, 0x24, 0xc0, 0x23, 0xc0, 0x0a, 0xc0, 0x09, 0xc0, 0x07, 0xc0, 0x08,
        0xc0, 0x28, 0xc0, 0x27, 0xc0, 0x14, 0xc0, 0x13, 0xc0, 0x11, 0xc0, 0x12,
        0xc0, 0x26, 0xc0, 0x25, 0xc0, 0x2a, 0xc0, 0x29, 0xc0, 0x05, 0xc0, 0x04,
        0xc0, 0x02, 0xc0, 0x03, 0xc0, 0x0f, 0xc0, 0x0e, 0xc0, 0x0c, 0xc0, 0x0d,
        0x00, 0x3d, 0x00, 0x3c, 0x00, 0x2f, 0x00, 0x05, 0x00, 0x04, 0x00, 0x35,
        0x00, 0x0a, 0x00, 0x67, 0x00, 0x6b, 0x00, 0x33, 0x00, 0x39, 0x00, 0x16,
        0x00, 0xaf, 0x00, 0xae, 0x00, 0x8d, 0x00, 0x8c, 0x00, 0x8a, 0x00, 0x8b,
        0x00, 0xb1, 0x00, 0xb0, 0x00, 0x2c, 0x00, 0x3b, 0x01, 0x00, 0x00, 0x3b,
        0x00, 0x00, 0x00, 0x15, 0x00, 0x13, 0x00, 0x00, 0x10, 0x61, 0x70, 0x69,
        0x2e, 0x74, 0x65, 0x78, 0x74, 0x6d, 0x61, 0x74, 0x65, 0x2e, 0x6f, 0x72,
        0x67, 0x00, 0x0a, 0x00, 0x08, 0x00, 0x06, 0x00, 0x17, 0x00, 0x18, 0x00,
        0x19, 0x00, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x00, 0x0d, 0x00, 0x0c, 0x00,
        0x0a, 0x05, 0x01, 0x04, 0x01, 0x02, 0x01, 0x04, 0x03, 0x02, 0x03
    };

    byte buffer3[] = { /* no sni extension */
        0x16, 0x03, 0x03, 0x00, 0x4d, 0x01, 0x00, 0x00, 0x49, 0x03, 0x03, 0xea,
        0xa1, 0x9f, 0x60, 0xdd, 0x52, 0x12, 0x13, 0xbd, 0x84, 0x34, 0xd5, 0x1c,
        0x38, 0x25, 0xa8, 0x97, 0xd2, 0xd5, 0xc6, 0x45, 0xaf, 0x1b, 0x08, 0xe4,
        0x1e, 0xbb, 0xdf, 0x9d, 0x39, 0xf0, 0x65, 0x00, 0x00, 0x16, 0x00, 0x6b,
        0x00, 0x67, 0x00, 0x39, 0x00, 0x33, 0x00, 0x3d, 0x00, 0x3c, 0x00, 0x35,
        0x00, 0x2f, 0x00, 0x05, 0x00, 0x04, 0x00, 0x0a, 0x01, 0x00, 0x00, 0x0a,
        0x00, 0x0d, 0x00, 0x06, 0x00, 0x04, 0x04, 0x01, 0x02, 0x01
    };

    byte buffer4[] = { /* last extension has zero size */
        0x16, 0x03, 0x01, 0x00, 0xba, 0x01, 0x00, 0x00,
        0xb6, 0x03, 0x03, 0x83, 0xa3, 0xe6, 0xdc, 0x16, 0xa1, 0x43, 0xe9, 0x45,
        0x15, 0xbd, 0x64, 0xa9, 0xb6, 0x07, 0xb4, 0x50, 0xc6, 0xdd, 0xff, 0xc2,
        0xd3, 0x0d, 0x4f, 0x36, 0xb4, 0x41, 0x51, 0x61, 0xc1, 0xa5, 0x9e, 0x00,
        0x00, 0x28, 0xcc, 0x14, 0xcc, 0x13, 0xc0, 0x2b, 0xc0, 0x2f, 0x00, 0x9e,
        0xc0, 0x0a, 0xc0, 0x09, 0xc0, 0x13, 0xc0, 0x14, 0xc0, 0x07, 0xc0, 0x11,
        0x00, 0x33, 0x00, 0x32, 0x00, 0x39, 0x00, 0x9c, 0x00, 0x2f, 0x00, 0x35,
        0x00, 0x0a, 0x00, 0x05, 0x00, 0x04, 0x01, 0x00, 0x00, 0x65, 0xff, 0x01,
        0x00, 0x01, 0x00, 0x00, 0x0a, 0x00, 0x08, 0x00, 0x06, 0x00, 0x17, 0x00,
        0x18, 0x00, 0x19, 0x00, 0x0b, 0x00, 0x02, 0x01, 0x00, 0x00, 0x23, 0x00,
        0x00, 0x33, 0x74, 0x00, 0x00, 0x00, 0x10, 0x00, 0x1b, 0x00, 0x19, 0x06,
        0x73, 0x70, 0x64, 0x79, 0x2f, 0x33, 0x08, 0x73, 0x70, 0x64, 0x79, 0x2f,
        0x33, 0x2e, 0x31, 0x08, 0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31,
        0x75, 0x50, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x0d, 0x00, 0x12, 0x00, 0x10, 0x04, 0x01, 0x05, 0x01, 0x02,
        0x01, 0x04, 0x03, 0x05, 0x03, 0x02, 0x03, 0x04, 0x02, 0x02, 0x02, 0x00,
        0x12, 0x00, 0x00
    };

    byte buffer5[] = { /* SSL v2.0 client hello */
        0x00, 0x2b, 0x01, 0x03, 0x01, 0x00, 0x09, 0x00, 0x00,
        /* dummy bytes bellow, just to pass size check */
        0xb6, 0x03, 0x03, 0x83, 0xa3, 0xe6, 0xdc, 0x16, 0xa1, 0x43, 0xe9, 0x45,
        0x15, 0xbd, 0x64, 0xa9, 0xb6, 0x07, 0xb4, 0x50, 0xc6, 0xdd, 0xff, 0xc2,
        0xd3, 0x0d, 0x4f, 0x36, 0xb4, 0x41, 0x51, 0x61, 0xc1, 0xa5, 0x9e, 0x00,
    };

    byte result[32] = {0};
    word32 length   = 32;

    AssertIntEQ(0, wolfSSL_SNI_GetFromBuffer(buffer4, sizeof(buffer4),
                                                           0, result, &length));

    AssertIntEQ(0, wolfSSL_SNI_GetFromBuffer(buffer3, sizeof(buffer3),
                                                           0, result, &length));

    AssertIntEQ(0, wolfSSL_SNI_GetFromBuffer(buffer2, sizeof(buffer2),
                                                           1, result, &length));

    AssertIntEQ(BUFFER_ERROR, wolfSSL_SNI_GetFromBuffer(buffer, sizeof(buffer),
                                                           0, result, &length));
    buffer[0] = 0x16;

    AssertIntEQ(BUFFER_ERROR, wolfSSL_SNI_GetFromBuffer(buffer, sizeof(buffer),
                                                           0, result, &length));
    buffer[1] = 0x03;

    AssertIntEQ(SNI_UNSUPPORTED, wolfSSL_SNI_GetFromBuffer(buffer,
                                           sizeof(buffer), 0, result, &length));
    buffer[2] = 0x03;

    AssertIntEQ(INCOMPLETE_DATA, wolfSSL_SNI_GetFromBuffer(buffer,
                                           sizeof(buffer), 0, result, &length));
    buffer[4] = 0x64;

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_SNI_GetFromBuffer(buffer, sizeof(buffer),
                                                           0, result, &length));
    result[length] = 0;
    AssertStrEQ("www.paypal.com", (const char*) result);

    length = 32;

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_SNI_GetFromBuffer(buffer2, sizeof(buffer2),
                                                           0, result, &length));
    result[length] = 0;
    AssertStrEQ("api.textmate.org", (const char*) result);

    /* SSL v2.0 tests */
    AssertIntEQ(SNI_UNSUPPORTED, wolfSSL_SNI_GetFromBuffer(buffer5,
                                          sizeof(buffer5), 0, result, &length));

    buffer5[2] = 0x02;
    AssertIntEQ(BUFFER_ERROR, wolfSSL_SNI_GetFromBuffer(buffer5,
                                          sizeof(buffer5), 0, result, &length));

    buffer5[2] = 0x01; buffer5[6] = 0x08;
    AssertIntEQ(BUFFER_ERROR, wolfSSL_SNI_GetFromBuffer(buffer5,
                                          sizeof(buffer5), 0, result, &length));

    buffer5[6] = 0x09; buffer5[8] = 0x01;
    AssertIntEQ(BUFFER_ERROR, wolfSSL_SNI_GetFromBuffer(buffer5,
                                          sizeof(buffer5), 0, result, &length));
}

#endif /* HAVE_SNI */

static void test_wolfSSL_UseSNI(void)
{
#ifdef HAVE_SNI
    test_wolfSSL_UseSNI_params();
    test_wolfSSL_UseSNI_connection();

    test_wolfSSL_SNI_GetFromBuffer();
#endif
}

static void test_wolfSSL_UseMaxFragment(void)
{
#if defined(HAVE_MAX_FRAGMENT) && !defined(NO_WOLFSSL_CLIENT)
    WOLFSSL_CTX *ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    WOLFSSL     *ssl = wolfSSL_new(ctx);

    AssertNotNull(ctx);
    AssertNotNull(ssl);

    /* error cases */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_UseMaxFragment(NULL, WOLFSSL_MFL_2_9));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_UseMaxFragment(    NULL, WOLFSSL_MFL_2_9));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_UseMaxFragment(ctx, 0));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_UseMaxFragment(ctx, 6));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_UseMaxFragment(ssl, 0));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_UseMaxFragment(ssl, 6));

    /* success case */
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_UseMaxFragment(ctx,  WOLFSSL_MFL_2_9));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_UseMaxFragment(ctx,  WOLFSSL_MFL_2_10));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_UseMaxFragment(ctx,  WOLFSSL_MFL_2_11));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_UseMaxFragment(ctx,  WOLFSSL_MFL_2_12));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_UseMaxFragment(ctx,  WOLFSSL_MFL_2_13));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseMaxFragment(    ssl,  WOLFSSL_MFL_2_9));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseMaxFragment(    ssl,  WOLFSSL_MFL_2_10));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseMaxFragment(    ssl,  WOLFSSL_MFL_2_11));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseMaxFragment(    ssl,  WOLFSSL_MFL_2_12));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseMaxFragment(    ssl,  WOLFSSL_MFL_2_13));

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
#endif
}

static void test_wolfSSL_UseTruncatedHMAC(void)
{
#if defined(HAVE_TRUNCATED_HMAC) && !defined(NO_WOLFSSL_CLIENT)
    WOLFSSL_CTX *ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    WOLFSSL     *ssl = wolfSSL_new(ctx);

    AssertNotNull(ctx);
    AssertNotNull(ssl);

    /* error cases */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_UseTruncatedHMAC(NULL));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_UseTruncatedHMAC(NULL));

    /* success case */
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_UseTruncatedHMAC(ctx));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseTruncatedHMAC(ssl));

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
#endif
}

static void test_wolfSSL_UseSupportedCurve(void)
{
#if defined(HAVE_SUPPORTED_CURVES) && !defined(NO_WOLFSSL_CLIENT)
    WOLFSSL_CTX *ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    WOLFSSL     *ssl = wolfSSL_new(ctx);

    AssertNotNull(ctx);
    AssertNotNull(ssl);

    /* error cases */
    AssertIntNE(WOLFSSL_SUCCESS,
                      wolfSSL_CTX_UseSupportedCurve(NULL, WOLFSSL_ECC_SECP256R1));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_UseSupportedCurve(ctx,  0));

    AssertIntNE(WOLFSSL_SUCCESS,
                          wolfSSL_UseSupportedCurve(NULL, WOLFSSL_ECC_SECP256R1));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_UseSupportedCurve(ssl,  0));

    /* success case */
    AssertIntEQ(WOLFSSL_SUCCESS,
                       wolfSSL_CTX_UseSupportedCurve(ctx, WOLFSSL_ECC_SECP256R1));
    AssertIntEQ(WOLFSSL_SUCCESS,
                           wolfSSL_UseSupportedCurve(ssl, WOLFSSL_ECC_SECP256R1));

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
#endif
}

#ifdef HAVE_ALPN

static void verify_ALPN_FATAL_ERROR_on_client(WOLFSSL* ssl)
{
    AssertIntEQ(UNKNOWN_ALPN_PROTOCOL_NAME_E, wolfSSL_get_error(ssl, 0));
}

static void use_ALPN_all(WOLFSSL* ssl)
{
    /* http/1.1,spdy/1,spdy/2,spdy/3 */
    char alpn_list[] = {0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31, 0x2c,
                        0x73, 0x70, 0x64, 0x79, 0x2f, 0x31, 0x2c,
                        0x73, 0x70, 0x64, 0x79, 0x2f, 0x32, 0x2c,
                        0x73, 0x70, 0x64, 0x79, 0x2f, 0x33};
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseALPN(ssl, alpn_list, sizeof(alpn_list),
                                             WOLFSSL_ALPN_FAILED_ON_MISMATCH));
}

static void use_ALPN_all_continue(WOLFSSL* ssl)
{
    /* http/1.1,spdy/1,spdy/2,spdy/3 */
    char alpn_list[] = {0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31, 0x2c,
        0x73, 0x70, 0x64, 0x79, 0x2f, 0x31, 0x2c,
        0x73, 0x70, 0x64, 0x79, 0x2f, 0x32, 0x2c,
        0x73, 0x70, 0x64, 0x79, 0x2f, 0x33};
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseALPN(ssl, alpn_list, sizeof(alpn_list),
                                             WOLFSSL_ALPN_CONTINUE_ON_MISMATCH));
}

static void use_ALPN_one(WOLFSSL* ssl)
{
    /* spdy/2 */
    char proto[] = {0x73, 0x70, 0x64, 0x79, 0x2f, 0x32};

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseALPN(ssl, proto, sizeof(proto),
                                             WOLFSSL_ALPN_FAILED_ON_MISMATCH));
}

static void use_ALPN_unknown(WOLFSSL* ssl)
{
    /* http/2.0 */
    char proto[] = {0x68, 0x74, 0x74, 0x70, 0x2f, 0x32, 0x2e, 0x30};

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseALPN(ssl, proto, sizeof(proto),
                                             WOLFSSL_ALPN_FAILED_ON_MISMATCH));
}

static void use_ALPN_unknown_continue(WOLFSSL* ssl)
{
    /* http/2.0 */
    char proto[] = {0x68, 0x74, 0x74, 0x70, 0x2f, 0x32, 0x2e, 0x30};

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseALPN(ssl, proto, sizeof(proto),
                                             WOLFSSL_ALPN_CONTINUE_ON_MISMATCH));
}

static void verify_ALPN_not_matching_spdy3(WOLFSSL* ssl)
{
    /* spdy/3 */
    char nego_proto[] = {0x73, 0x70, 0x64, 0x79, 0x2f, 0x33};

    char *proto;
    word16 protoSz = 0;

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_ALPN_GetProtocol(ssl, &proto, &protoSz));

    /* check value */
    AssertIntNE(1, sizeof(nego_proto) == protoSz);
    AssertIntNE(0, XMEMCMP(nego_proto, proto, sizeof(nego_proto)));
}

static void verify_ALPN_not_matching_continue(WOLFSSL* ssl)
{
    char *proto = NULL;
    word16 protoSz = 0;

    AssertIntEQ(WOLFSSL_ALPN_NOT_FOUND,
                wolfSSL_ALPN_GetProtocol(ssl, &proto, &protoSz));

    /* check value */
    AssertIntEQ(1, (0 == protoSz));
    AssertIntEQ(1, (NULL == proto));
}

static void verify_ALPN_matching_http1(WOLFSSL* ssl)
{
    /* http/1.1 */
    char nego_proto[] = {0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31};
    char *proto;
    word16 protoSz = 0;

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_ALPN_GetProtocol(ssl, &proto, &protoSz));

    /* check value */
    AssertIntEQ(1, sizeof(nego_proto) == protoSz);
    AssertIntEQ(0, XMEMCMP(nego_proto, proto, protoSz));
}

static void verify_ALPN_matching_spdy2(WOLFSSL* ssl)
{
    /* spdy/2 */
    char nego_proto[] = {0x73, 0x70, 0x64, 0x79, 0x2f, 0x32};
    char *proto;
    word16 protoSz = 0;

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_ALPN_GetProtocol(ssl, &proto, &protoSz));

    /* check value */
    AssertIntEQ(1, sizeof(nego_proto) == protoSz);
    AssertIntEQ(0, XMEMCMP(nego_proto, proto, protoSz));
}

static void verify_ALPN_client_list(WOLFSSL* ssl)
{
    /* http/1.1,spdy/1,spdy/2,spdy/3 */
    char alpn_list[] = {0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31, 0x2c,
                        0x73, 0x70, 0x64, 0x79, 0x2f, 0x31, 0x2c,
                        0x73, 0x70, 0x64, 0x79, 0x2f, 0x32, 0x2c,
                        0x73, 0x70, 0x64, 0x79, 0x2f, 0x33};
    char    *clist = NULL;
    word16  clistSz = 0;

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_ALPN_GetPeerProtocol(ssl, &clist,
                                                          &clistSz));

    /* check value */
    AssertIntEQ(1, sizeof(alpn_list) == clistSz);
    AssertIntEQ(0, XMEMCMP(alpn_list, clist, clistSz));

    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_ALPN_FreePeerProtocol(ssl, &clist));
}

static void test_wolfSSL_UseALPN_connection(void)
{
    unsigned long i;
    callback_functions callbacks[] = {
        /* success case same list */
        {0, 0, use_ALPN_all, 0},
        {0, 0, use_ALPN_all, verify_ALPN_matching_http1},

        /* success case only one for server */
        {0, 0, use_ALPN_all, 0},
        {0, 0, use_ALPN_one, verify_ALPN_matching_spdy2},

        /* success case only one for client */
        {0, 0, use_ALPN_one, 0},
        {0, 0, use_ALPN_all, verify_ALPN_matching_spdy2},

        /* success case none for client */
        {0, 0, 0, 0},
        {0, 0, use_ALPN_all, 0},

        /* success case missmatch behavior but option 'continue' set */
        {0, 0, use_ALPN_all_continue, verify_ALPN_not_matching_continue},
        {0, 0, use_ALPN_unknown_continue, 0},

        /* success case read protocol send by client */
        {0, 0, use_ALPN_all, 0},
        {0, 0, use_ALPN_one, verify_ALPN_client_list},

        /* missmatch behavior with same list
         * the first and only this one must be taken */
        {0, 0, use_ALPN_all, 0},
        {0, 0, use_ALPN_all, verify_ALPN_not_matching_spdy3},

        /* default missmatch behavior */
        {0, 0, use_ALPN_all, 0},
        {0, 0, use_ALPN_unknown, verify_ALPN_FATAL_ERROR_on_client},
    };

    for (i = 0; i < sizeof(callbacks) / sizeof(callback_functions); i += 2) {
        callbacks[i    ].method = wolfSSLv23_client_method;
        callbacks[i + 1].method = wolfSSLv23_server_method;
        test_wolfSSL_client_server(&callbacks[i], &callbacks[i + 1]);
    }
}

static void test_wolfSSL_UseALPN_params(void)
{
#ifndef NO_WOLFSSL_CLIENT
    /* "http/1.1" */
    char http1[] = {0x68, 0x74, 0x74, 0x70, 0x2f, 0x31, 0x2e, 0x31};
    /* "spdy/1" */
    char spdy1[] = {0x73, 0x70, 0x64, 0x79, 0x2f, 0x31};
    /* "spdy/2" */
    char spdy2[] = {0x73, 0x70, 0x64, 0x79, 0x2f, 0x32};
    /* "spdy/3" */
    char spdy3[] = {0x73, 0x70, 0x64, 0x79, 0x2f, 0x33};
    char buff[256];
    word32 idx;

    WOLFSSL_CTX *ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    WOLFSSL     *ssl = wolfSSL_new(ctx);

    AssertNotNull(ctx);
    AssertNotNull(ssl);

    /* error cases */
    AssertIntNE(WOLFSSL_SUCCESS,
                wolfSSL_UseALPN(NULL, http1, sizeof(http1),
                                WOLFSSL_ALPN_FAILED_ON_MISMATCH));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_UseALPN(ssl, NULL, 0,
                                             WOLFSSL_ALPN_FAILED_ON_MISMATCH));

    /* success case */
    /* http1 only */
    AssertIntEQ(WOLFSSL_SUCCESS,
                wolfSSL_UseALPN(ssl, http1, sizeof(http1),
                                WOLFSSL_ALPN_FAILED_ON_MISMATCH));

    /* http1, spdy1 */
    XMEMCPY(buff, http1, sizeof(http1));
    idx = sizeof(http1);
    buff[idx++] = ',';
    XMEMCPY(buff+idx, spdy1, sizeof(spdy1));
    idx += sizeof(spdy1);
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseALPN(ssl, buff, idx,
                                             WOLFSSL_ALPN_FAILED_ON_MISMATCH));

    /* http1, spdy2, spdy1 */
    XMEMCPY(buff, http1, sizeof(http1));
    idx = sizeof(http1);
    buff[idx++] = ',';
    XMEMCPY(buff+idx, spdy2, sizeof(spdy2));
    idx += sizeof(spdy2);
    buff[idx++] = ',';
    XMEMCPY(buff+idx, spdy1, sizeof(spdy1));
    idx += sizeof(spdy1);
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseALPN(ssl, buff, idx,
                                             WOLFSSL_ALPN_FAILED_ON_MISMATCH));

    /* spdy3, http1, spdy2, spdy1 */
    XMEMCPY(buff, spdy3, sizeof(spdy3));
    idx = sizeof(spdy3);
    buff[idx++] = ',';
    XMEMCPY(buff+idx, http1, sizeof(http1));
    idx += sizeof(http1);
    buff[idx++] = ',';
    XMEMCPY(buff+idx, spdy2, sizeof(spdy2));
    idx += sizeof(spdy2);
    buff[idx++] = ',';
    XMEMCPY(buff+idx, spdy1, sizeof(spdy1));
    idx += sizeof(spdy1);
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_UseALPN(ssl, buff, idx,
                                             WOLFSSL_ALPN_CONTINUE_ON_MISMATCH));

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
#endif
}
#endif /* HAVE_ALPN  */

static void test_wolfSSL_UseALPN(void)
{
#ifdef HAVE_ALPN
    test_wolfSSL_UseALPN_connection();
    test_wolfSSL_UseALPN_params();
#endif
}

static void test_wolfSSL_DisableExtendedMasterSecret(void)
{
#if defined(HAVE_EXTENDED_MASTER) && !defined(NO_WOLFSSL_CLIENT)
    WOLFSSL_CTX *ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    WOLFSSL     *ssl = wolfSSL_new(ctx);

    AssertNotNull(ctx);
    AssertNotNull(ssl);

    /* error cases */
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_CTX_DisableExtendedMasterSecret(NULL));
    AssertIntNE(WOLFSSL_SUCCESS, wolfSSL_DisableExtendedMasterSecret(NULL));

    /* success cases */
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_CTX_DisableExtendedMasterSecret(ctx));
    AssertIntEQ(WOLFSSL_SUCCESS, wolfSSL_DisableExtendedMasterSecret(ssl));

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
#endif
}

/*----------------------------------------------------------------------------*
 | X509 Tests
 *----------------------------------------------------------------------------*/
static void test_wolfSSL_X509_NAME_get_entry(void)
{
#if !defined(NO_CERTS) && !defined(NO_RSA)
#if defined(OPENSSL_ALL) || \
        (defined(OPENSSL_EXTRA) && \
            (defined(KEEP_PEER_CERT) || defined(SESSION_CERTS)))
    printf(testingFmt, "wolfSSL_X509_NAME_get_entry()");

    {
        /* use openssl like name to test mapping */
        X509_NAME_ENTRY* ne = NULL;
        X509_NAME* name = NULL;
        char* subCN = NULL;
        X509* x509;
        ASN1_STRING* asn;
        int idx;

    #ifndef NO_FILESYSTEM
        x509 = wolfSSL_X509_load_certificate_file(cliCertFile, WOLFSSL_FILETYPE_PEM);
        AssertNotNull(x509);

        name = X509_get_subject_name(x509);

        idx = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
        AssertIntGE(idx, 0);

        ne = X509_NAME_get_entry(name, idx);
        AssertNotNull(ne);

        asn = X509_NAME_ENTRY_get_data(ne);
        AssertNotNull(asn);

        subCN = (char*)ASN1_STRING_data(asn);
        AssertNotNull(subCN);

        wolfSSL_FreeX509(x509);
    #endif

    }

    printf(resultFmt, passed);
#endif /* OPENSSL_ALL || (OPENSSL_EXTRA && (KEEP_PEER_CERT || SESSION_CERTS) */
#endif /* !NO_CERTS && !NO_RSA */
}


/* Testing functions dealing with PKCS12 parsing out X509 certs */
static void test_wolfSSL_PKCS12(void)
{
    /* .p12 file is encrypted with DES3 */
#if defined(OPENSSL_EXTRA) && !defined(NO_DES3) && !defined(NO_FILESYSTEM) && \
    !defined(NO_ASN) && !defined(NO_PWDBASED) && !defined(NO_RSA)
    byte buffer[5300];
    char file[] = "./certs/test-servercert.p12";
    char order[] = "./certs/ecc-rsa-server.p12";
    char pass[] = "a password";
    WOLFSSL_X509_NAME* subject;
    FILE *f;
    int  bytes, ret;
    WOLFSSL_BIO      *bio;
    WOLFSSL_EVP_PKEY *pkey;
    WC_PKCS12        *pkcs12;
    WC_PKCS12        *pkcs12_2;
    WOLFSSL_X509     *cert;
    WOLFSSL_X509     *x509;
    WOLFSSL_X509     *tmp;
    WOLF_STACK_OF(WOLFSSL_X509) *ca;

    printf(testingFmt, "wolfSSL_PKCS12()");

    f = fopen(file, "rb");
    AssertNotNull(f);
    bytes = (int)fread(buffer, 1, sizeof(buffer), f);
    fclose(f);

    bio = BIO_new_mem_buf((void*)buffer, bytes);
    AssertNotNull(bio);

    pkcs12 = d2i_PKCS12_bio(bio, NULL);
    AssertNotNull(pkcs12);
    PKCS12_free(pkcs12);

    d2i_PKCS12_bio(bio, &pkcs12);
    AssertNotNull(pkcs12);

    /* check verify MAC fail case */
    ret = PKCS12_parse(pkcs12, "bad", &pkey, &cert, NULL);
    AssertIntEQ(ret, 0);
    AssertNull(pkey);
    AssertNull(cert);

    /* check parse with no extra certs kept */
    ret = PKCS12_parse(pkcs12, "wolfSSL test", &pkey, &cert, NULL);
    AssertIntEQ(ret, 1);
    AssertNotNull(pkey);
    AssertNotNull(cert);

    wolfSSL_EVP_PKEY_free(pkey);
    wolfSSL_X509_free(cert);

    /* check parse with extra certs kept */
    ret = PKCS12_parse(pkcs12, "wolfSSL test", &pkey, &cert, &ca);
    AssertIntEQ(ret, 1);
    AssertNotNull(pkey);
    AssertNotNull(cert);
    AssertNotNull(ca);


    /* should be 2 other certs on stack */
    tmp = sk_X509_pop(ca);
    AssertNotNull(tmp);
    X509_free(tmp);
    tmp = sk_X509_pop(ca);
    AssertNotNull(tmp);
    X509_free(tmp);
    AssertNull(sk_X509_pop(ca));

    EVP_PKEY_free(pkey);
    X509_free(cert);
    sk_X509_pop_free(ca, X509_free);

    /* check PKCS12_create */
    AssertNull(PKCS12_create(pass, NULL, NULL, NULL, NULL, -1, -1, -1, -1,0));
    AssertIntEQ(PKCS12_parse(pkcs12, "wolfSSL test", &pkey, &cert, &ca),
            SSL_SUCCESS);
    AssertNotNull((pkcs12_2 = PKCS12_create(pass, NULL, pkey, cert, ca,
                    -1, -1, 100, -1, 0)));
    EVP_PKEY_free(pkey);
    X509_free(cert);
    sk_X509_free(ca);

    AssertIntEQ(PKCS12_parse(pkcs12_2, "a password", &pkey, &cert, &ca),
            SSL_SUCCESS);
    PKCS12_free(pkcs12_2);
    AssertNotNull((pkcs12_2 = PKCS12_create(pass, NULL, pkey, cert, ca,
             NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
             NID_pbe_WithSHA1And3_Key_TripleDES_CBC,
             2000, 1, 0)));
    EVP_PKEY_free(pkey);
    X509_free(cert);
    sk_X509_free(ca);

    AssertIntEQ(PKCS12_parse(pkcs12_2, "a password", &pkey, &cert, &ca),
            SSL_SUCCESS);

    /* should be 2 other certs on stack */
    tmp = sk_X509_pop(ca);
    AssertNotNull(tmp);
    X509_free(tmp);
    tmp = sk_X509_pop(ca);
    AssertNotNull(tmp);
    X509_free(tmp);
    AssertNull(sk_X509_pop(ca));


#ifndef NO_RC4
    PKCS12_free(pkcs12_2);
    AssertNotNull((pkcs12_2 = PKCS12_create(pass, NULL, pkey, cert, NULL,
             NID_pbe_WithSHA1And128BitRC4,
             NID_pbe_WithSHA1And128BitRC4,
             2000, 1, 0)));
    EVP_PKEY_free(pkey);
    X509_free(cert);
    sk_X509_free(ca);

    AssertIntEQ(PKCS12_parse(pkcs12_2, "a password", &pkey, &cert, &ca),
            SSL_SUCCESS);

#endif /* NO_RC4 */

    EVP_PKEY_free(pkey);
    X509_free(cert);
    BIO_free(bio);
    PKCS12_free(pkcs12);
    PKCS12_free(pkcs12_2);
    sk_X509_free(ca);

#ifdef HAVE_ECC
    /* test order of parsing */
    f = fopen(order, "rb");
    AssertNotNull(f);
    bytes = (int)fread(buffer, 1, sizeof(buffer), f);
    fclose(f);

    AssertNotNull(bio = BIO_new_mem_buf((void*)buffer, bytes));
    AssertNotNull(pkcs12 = d2i_PKCS12_bio(bio, NULL));
    AssertIntEQ((ret = PKCS12_parse(pkcs12, "", &pkey, &cert, &ca)),
            WOLFSSL_SUCCESS);
    AssertNotNull(pkey);
    AssertNotNull(cert);
    AssertNotNull(ca);

    /* compare subject lines of certificates */
    AssertNotNull(subject = wolfSSL_X509_get_subject_name(cert));
    AssertNotNull(x509 = wolfSSL_X509_load_certificate_file(eccRsaCertFile,
                SSL_FILETYPE_PEM));
    AssertIntEQ(wolfSSL_X509_NAME_cmp((const WOLFSSL_X509_NAME*)subject,
            (const WOLFSSL_X509_NAME*)wolfSSL_X509_get_subject_name(x509)), 0);
    X509_free(x509);

    /* test expected fail case */
    AssertNotNull(x509 = wolfSSL_X509_load_certificate_file(eccCertFile,
                SSL_FILETYPE_PEM));
    AssertIntNE(wolfSSL_X509_NAME_cmp((const WOLFSSL_X509_NAME*)subject,
            (const WOLFSSL_X509_NAME*)wolfSSL_X509_get_subject_name(x509)), 0);
    X509_free(x509);
    X509_free(cert);

    /* get subject line from ca stack */
    AssertNotNull(cert = sk_X509_pop(ca));
    AssertNotNull(subject = wolfSSL_X509_get_subject_name(cert));

    /* compare subject from certificate in ca to expected */
    AssertNotNull(x509 = wolfSSL_X509_load_certificate_file(eccCertFile,
                SSL_FILETYPE_PEM));
    AssertIntEQ(wolfSSL_X509_NAME_cmp((const WOLFSSL_X509_NAME*)subject,
            (const WOLFSSL_X509_NAME*)wolfSSL_X509_get_subject_name(x509)), 0);

    EVP_PKEY_free(pkey);
    X509_free(x509);
    X509_free(cert);
    BIO_free(bio);
    PKCS12_free(pkcs12);
    sk_X509_free(ca);
#endif /* HAVE_ECC */

    (void)x509;
    (void)subject;
    (void)order;

    printf(resultFmt, passed);
#endif /* OPENSSL_EXTRA */
}


#if (defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL)) && \
        !defined(NO_DES3) && !defined(NO_FILESYSTEM) && \
        !defined(NO_ASN) && !defined(NO_PWDBASED) && !defined(NO_RSA)
/* for PKCS8 test case */
static INLINE int PKCS8TestCallBack(char* passwd, int sz, int rw, void* userdata)
{
    int flag = 0;

    (void)rw;
    if (userdata != NULL) {
        flag = *((int*)userdata); /* user set data */
    }

    switch (flag) {
        case 1: /* flag set for specific WOLFSSL_CTX structure, note userdata
                 * can be anything the user wishes to be passed to the callback
                 * associated with the WOLFSSL_CTX */
            strncpy(passwd, "yassl123", sz);
            return 8;

       default:
            return BAD_FUNC_ARG;
    }
}
#endif

/* Testing functions dealing with PKCS8 */
static void test_wolfSSL_PKCS8(void)
{
#if (defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL)) && \
        !defined(NO_DES3) && !defined(NO_FILESYSTEM) && \
        !defined(NO_ASN) && !defined(NO_PWDBASED) && !defined(NO_RSA) && \
        defined(WOLFSSL_ENCRYPTED_KEYS)
    byte buffer[FOURK_BUF];
    byte der[FOURK_BUF];
    char file[] = "./certs/server-keyPkcs8Enc.pem";
    FILE *f;
    int  flag = 1;
    int  bytes;
    WOLFSSL_CTX* ctx;

    printf(testingFmt, "wolfSSL_PKCS8()");

    f = fopen(file, "rb");
    AssertNotNull(f);
    bytes = (int)fread(buffer, 1, sizeof(buffer), f);
    fclose(f);

    /* Note that wolfSSL_Init() or wolfCrypt_Init() has been called before these
     * function calls */

#ifndef NO_WOLFSSL_CLIENT
    #ifndef WOLFSSL_NO_TLS12
        AssertNotNull(ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method()));
    #else
        AssertNotNull(ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method()));
    #endif
#else
    #ifndef WOLFSSL_NO_TLS12
        AssertNotNull(ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method()));
    #else
        AssertNotNull(ctx = wolfSSL_CTX_new(wolfTLSv1_3_server_method()));
    #endif
#endif
    wolfSSL_CTX_set_default_passwd_cb(ctx, &PKCS8TestCallBack);
    wolfSSL_CTX_set_default_passwd_cb_userdata(ctx, (void*)&flag);
    AssertIntEQ(wolfSSL_CTX_use_PrivateKey_buffer(ctx, buffer, bytes,
                SSL_FILETYPE_PEM), SSL_SUCCESS);

    /* this next case should fail if setting the user flag to a value other
     * than 1 due to the password callback functions return value */
    flag = 0;
    wolfSSL_CTX_set_default_passwd_cb_userdata(ctx, (void*)&flag);
    AssertIntNE(wolfSSL_CTX_use_PrivateKey_buffer(ctx, buffer, bytes,
                SSL_FILETYPE_PEM), SSL_SUCCESS);

    wolfSSL_CTX_free(ctx);

    /* decrypt PKCS8 PEM to key in DER format with not using WOLFSSL_CTX */
    AssertIntGT(wc_KeyPemToDer(buffer, bytes, der, FOURK_BUF, "yassl123"),
                0);

    /* test that error value is returned with a bad password */
    AssertIntLT(wc_KeyPemToDer(buffer, bytes, der, FOURK_BUF, "bad"), 0);

    printf(resultFmt, passed);
#endif /* OPENSSL_EXTRA */
}

/* Testing functions dealing with PKCS5 */
static void test_wolfSSL_PKCS5(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_SHA) && !defined(NO_PWDBASED)
    const char *passwd = "pass1234";
    const unsigned char *salt = (unsigned char *)"salt1234";
    unsigned char *out = (unsigned char *)XMALLOC(WC_SHA_DIGEST_SIZE, NULL,
                                                  DYNAMIC_TYPE_TMP_BUFFER);
    int ret = 0;

    AssertNotNull(out);
    ret = PKCS5_PBKDF2_HMAC_SHA1(passwd,(int)XSTRLEN(passwd), salt,
                                 (int)XSTRLEN((const char *) salt), 10,
                                 WC_SHA_DIGEST_SIZE,out);
    AssertIntEQ(ret, SSL_SUCCESS);
    XFREE(out, NULL, DYNAMIC_TYPE_TMP_BUFFER);
#endif /* defined(OPENSSL_EXTRA) && !defined(NO_SHA) */
}

/* test parsing URI from certificate */
static void test_wolfSSL_URI(void)
{
#if !defined(NO_CERTS) && !defined(NO_RSA) && !defined(NO_FILESYSTEM) \
    && (defined(KEEP_PEER_CERT) || defined(SESSION_CERTS) || \
    defined(OPENSSL_EXTRA))
    WOLFSSL_X509* x509;
    const char uri[] = "./certs/client-uri-cert.pem";
    const char badUri[] = "./certs/client-relative-uri.pem";

    printf(testingFmt, "wolfSSL URI parse");

    x509 = wolfSSL_X509_load_certificate_file(uri, WOLFSSL_FILETYPE_PEM);
    AssertNotNull(x509);

    wolfSSL_FreeX509(x509);

    x509 = wolfSSL_X509_load_certificate_file(badUri, WOLFSSL_FILETYPE_PEM);
#ifndef IGNORE_NAME_CONSTRAINTS
    AssertNull(x509);
#else
    AssertNotNull(x509);
#endif

    printf(resultFmt, passed);
#endif
}

/* Testing function  wolfSSL_CTX_SetMinVersion; sets the minimum downgrade
 * version allowed.
 * POST: 1 on success.
 */
static int test_wolfSSL_CTX_SetMinVersion(void)
{
    int                     failFlag = WOLFSSL_SUCCESS;
#ifndef NO_WOLFSSL_CLIENT
    WOLFSSL_CTX*            ctx;
    int                     itr;

    #ifndef NO_OLD_TLS
        const int versions[]  = { WOLFSSL_TLSV1, WOLFSSL_TLSV1_1,
                                  WOLFSSL_TLSV1_2 };
    #elif !defined(WOLFSSL_NO_TLS12)
        const int versions[]  = { WOLFSSL_TLSV1_2 };
    #elif defined(WOLFSSL_TLS13)
        const int versions[]  = { WOLFSSL_TLSV1_3 };
    #endif

    failFlag = WOLFSSL_SUCCESS;

    AssertTrue(wolfSSL_Init());
#ifndef WOLFSSL_NO_TLS12
    ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
#else
    ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
#endif

    printf(testingFmt, "wolfSSL_CTX_SetMinVersion()");

    for (itr = 0; itr < (int)(sizeof(versions)/sizeof(int)); itr++){
        if(wolfSSL_CTX_SetMinVersion(ctx, *(versions + itr)) != WOLFSSL_SUCCESS){
            failFlag = WOLFSSL_FAILURE;
        }
    }

    printf(resultFmt, failFlag == WOLFSSL_SUCCESS ? passed : failed);

    wolfSSL_CTX_free(ctx);
    AssertTrue(wolfSSL_Cleanup());
#endif
    return failFlag;

} /* END test_wolfSSL_CTX_SetMinVersion */


/*----------------------------------------------------------------------------*
 | OCSP Stapling
 *----------------------------------------------------------------------------*/


/* Testing wolfSSL_UseOCSPStapling function. OCSP stapling eliminates the need
 * need to contact the CA, lowering the cost of cert revocation checking.
 * PRE: HAVE_OCSP and HAVE_CERTIFICATE_STATUS_REQUEST
 * POST: 1 returned for success.
 */
static int test_wolfSSL_UseOCSPStapling(void)
{
    #if defined(HAVE_CERTIFICATE_STATUS_REQUEST) && defined(HAVE_OCSP) && \
            !defined(NO_WOLFSSL_CLIENT)
        int             ret;
        WOLFSSL_CTX*    ctx;
        WOLFSSL*        ssl;

        wolfSSL_Init();
#ifndef NO_WOLFSSL_CLIENT
    #ifndef WOLFSSL_NO_TLS12
        ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    #else
        ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
    #endif
#else
    #ifndef WOLFSSL_NO_TLS12
        ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method());
    #else
        ctx = wolfSSL_CTX_new(wolfTLSv1_3_server_method());
    #endif
#endif
        ssl = wolfSSL_new(ctx);
        printf(testingFmt, "wolfSSL_UseOCSPStapling()");

        ret = wolfSSL_UseOCSPStapling(ssl, WOLFSSL_CSR2_OCSP,
                                    WOLFSSL_CSR2_OCSP_USE_NONCE);

        printf(resultFmt, ret == WOLFSSL_SUCCESS ? passed : failed);


        wolfSSL_free(ssl);
        wolfSSL_CTX_free(ctx);

        if(ret != WOLFSSL_SUCCESS){
            wolfSSL_Cleanup();
            return WOLFSSL_FAILURE;
        }

        return wolfSSL_Cleanup();
    #else
        return WOLFSSL_SUCCESS;
    #endif

} /*END test_wolfSSL_UseOCSPStapling */


/* Testing OCSP stapling version 2, wolfSSL_UseOCSPStaplingV2 funciton. OCSP
 * stapling eliminates the need ot contact the CA and lowers cert revocation
 * check.
 * PRE: HAVE_CERTIFICATE_STATUS_REQUEST_V2 and HAVE_OCSP defined.
 */
static int test_wolfSSL_UseOCSPStaplingV2 (void)
{
    #if defined(HAVE_CERTIFICATE_STATUS_REQUEST_V2) && defined(HAVE_OCSP) && \
            !defined(NO_WOLFSSL_CLIENT)
        int                 ret;
        WOLFSSL_CTX*        ctx;
        WOLFSSL*            ssl;

        wolfSSL_Init();
#ifndef NO_WOLFSSL_CLIENT
    #ifndef WOLFSSL_NO_TLS12
        ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    #else
        ctx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
    #endif
#else
    #ifndef WOLFSSL_NO_TLS12
        ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method());
    #else
        ctx = wolfSSL_CTX_new(wolfTLSv1_3_server_method());
    #endif
#endif
        ssl = wolfSSL_new(ctx);
        printf(testingFmt, "wolfSSL_UseOCSPStaplingV2()");

        ret = wolfSSL_UseOCSPStaplingV2(ssl, WOLFSSL_CSR2_OCSP,
                                        WOLFSSL_CSR2_OCSP_USE_NONCE );

        printf(resultFmt, ret == WOLFSSL_SUCCESS ? passed : failed);

        wolfSSL_free(ssl);
        wolfSSL_CTX_free(ctx);

        if (ret != WOLFSSL_SUCCESS){
            wolfSSL_Cleanup();
            return WOLFSSL_FAILURE;
        }

        return wolfSSL_Cleanup();
    #else
        return WOLFSSL_SUCCESS;
    #endif

} /*END test_wolfSSL_UseOCSPStaplingV2*/

/*----------------------------------------------------------------------------*
 | Multicast Tests
 *----------------------------------------------------------------------------*/
static void test_wolfSSL_mcast(void)
{
#if defined(WOLFSSL_DTLS) && defined(WOLFSSL_MULTICAST)
    WOLFSSL_CTX* ctx;
    WOLFSSL* ssl;
    int result;
    byte preMasterSecret[512];
    byte clientRandom[32];
    byte serverRandom[32];
    byte suite[2] = {0, 0xfe};  /* WDM_WITH_NULL_SHA256 */
    byte buf[256];
    word16 newId;

    ctx = wolfSSL_CTX_new(wolfDTLSv1_2_client_method());
    AssertNotNull(ctx);

    result = wolfSSL_CTX_mcast_set_member_id(ctx, 0);
    AssertIntEQ(result, WOLFSSL_SUCCESS);

    ssl = wolfSSL_new(ctx);
    AssertNotNull(ssl);

    XMEMSET(preMasterSecret, 0x23, sizeof(preMasterSecret));
    XMEMSET(clientRandom, 0xA5, sizeof(clientRandom));
    XMEMSET(serverRandom, 0x5A, sizeof(serverRandom));
    result = wolfSSL_set_secret(ssl, 23,
                preMasterSecret, sizeof(preMasterSecret),
                clientRandom, serverRandom, suite);
    AssertIntEQ(result, WOLFSSL_SUCCESS);

    result = wolfSSL_mcast_read(ssl, &newId, buf, sizeof(buf));
    AssertIntLE(result, 0);
    AssertIntLE(newId, 100);

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);
#endif /* WOLFSSL_DTLS && WOLFSSL_MULTICAST */
}


/*----------------------------------------------------------------------------*
 |  Wolfcrypt
 *----------------------------------------------------------------------------*/

/* 
 * Unit test for the wc_InitBlake2b()
 */
static int test_wc_InitBlake2b (void)
{
    int ret = 0;
#ifdef HAVE_BLAKE2

    Blake2b blake2;

    printf(testingFmt, "wc_InitBlake2B()");

    /* Test good arg. */
    ret = wc_InitBlake2b(&blake2, 64);
    if (ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    /* Test bad arg. */
    if (!ret) {
        ret = wc_InitBlake2b(NULL, 64);
        if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    if (!ret) {
        ret = wc_InitBlake2b(NULL, 128);
        if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    if (!ret) {
        ret = wc_InitBlake2b(&blake2, 128);
        if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    if (!ret) {
        ret = wc_InitBlake2b(NULL, 0);
        if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    if (!ret) {
        ret = wc_InitBlake2b(&blake2, 0);
        if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
}     /*END test_wc_InitBlake2b*/


/*
 * Unit test for the wc_InitMd5()
 */
static int test_wc_InitMd5 (void)
{
    int flag = 0;
#ifndef NO_MD5

    wc_Md5 md5;
    int ret;

    printf(testingFmt, "wc_InitMd5()");

    /* Test good arg. */
    ret = wc_InitMd5(&md5);
    if (ret != 0) {
        flag = WOLFSSL_FATAL_ERROR;
    }

    /* Test bad arg. */
    if (!flag) {
        ret = wc_InitMd5(NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Md5Free(&md5);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
}     /* END test_wc_InitMd5 */


/*
 * Testing wc_UpdateMd5()
 */
static int test_wc_Md5Update (void)
{

    int flag = 0;
#ifndef NO_MD5
    wc_Md5 md5;
    byte hash[WC_MD5_DIGEST_SIZE];
    testVector a, b, c;
    int ret;

    ret = wc_InitMd5(&md5);
    if (ret != 0) {
        flag = ret;
    }

    printf(testingFmt, "wc_Md5Update()");

    /* Input */
    if (!flag) {
        a.input = "a";
        a.inLen = XSTRLEN(a.input);
    }

    if (!flag){
        ret = wc_Md5Update(&md5, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_Md5Final(&md5, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Update input. */
    if (!flag) {
        a.input = "abc";
        a.output = "\x90\x01\x50\x98\x3c\xd2\x4f\xb0\xd6\x96\x3f\x7d\x28\xe1\x7f"
                    "\x72";
        a.inLen = XSTRLEN(a.input);
        a.outLen = XSTRLEN(a.output);

        ret = wc_Md5Update(&md5, (byte*) a.input, (word32) a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_Md5Final(&md5, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_MD5_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /*Pass in bad values. */
    if (!flag) {
        b.input = NULL;
        b.inLen = 0;

        ret = wc_Md5Update(&md5, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        c.input = NULL;
        c.inLen = WC_MD5_DIGEST_SIZE;

        ret = wc_Md5Update(&md5, (byte*)c.input, (word32)c.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Md5Update(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Md5Free(&md5);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Md5Update()  */


/*
 *  Unit test on wc_Md5Final() in wolfcrypt/src/md5.c
 */
static int test_wc_Md5Final (void)
{

    int flag = 0;
#ifndef NO_MD5
    /* Instantiate */
    wc_Md5 md5;
    byte* hash_test[3];
    byte hash1[WC_MD5_DIGEST_SIZE];
    byte hash2[2*WC_MD5_DIGEST_SIZE];
    byte hash3[5*WC_MD5_DIGEST_SIZE];
    int times, i, ret;

    /* Initialize */
    ret = wc_InitMd5(&md5);
    if (ret != 0)  {
        flag = ret;
    }

    if (!flag) {
        hash_test[0] = hash1;
        hash_test[1] = hash2;
        hash_test[2] = hash3;
    }

    times = sizeof(hash_test)/sizeof(byte*);

    /* Test good args. */
    printf(testingFmt, "wc_Md5Final()");

    for (i = 0; i < times; i++) {
        if (!flag) {
            ret = wc_Md5Final(&md5, hash_test[i]);
            if (ret != 0) {
                flag = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    /* Test bad args. */
    if (!flag) {
        ret = wc_Md5Final(NULL, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Md5Final(NULL, hash1);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Md5Final(&md5, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Md5Free(&md5);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
}


/*
 * Unit test for the wc_InitSha()
 */
static int test_wc_InitSha(void)
{
    int flag = 0;
#ifndef NO_SHA
    wc_Sha sha;
    int ret;

    printf(testingFmt, "wc_InitSha()");

    /* Test good arg. */
    ret = wc_InitSha(&sha);
    if (ret != 0) {
        flag = WOLFSSL_FATAL_ERROR;
    }

    /* Test bad arg. */
    if (!flag) {
        ret = wc_InitSha(NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_ShaFree(&sha);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_InitSha */

/*
 *  Tesing wc_ShaUpdate()
 */
static int test_wc_ShaUpdate (void)
{

    int flag = 0;
#ifndef NO_SHA
    wc_Sha sha;
    byte hash[WC_SHA_DIGEST_SIZE];
    testVector a, b, c;
    int ret;

    ret = wc_InitSha(&sha);
    if (ret != 0) {
        flag = ret;
    }

    printf(testingFmt, "wc_ShaUpdate()");

    /* Input. */
    if (!flag) {
        a.input = "a";
        a.inLen = XSTRLEN(a.input);
    }

    if (!flag) {
        ret = wc_ShaUpdate(&sha, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_ShaFinal(&sha, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Update input. */
    if (!flag) {
        a.input = "abc";
        a.output = "\xA9\x99\x3E\x36\x47\x06\x81\x6A\xBA\x3E\x25\x71\x78\x50\xC2"
                    "\x6C\x9C\xD0\xD8\x9D";
        a.inLen = XSTRLEN(a.input);
        a.outLen = XSTRLEN(a.output);

        ret = wc_ShaUpdate(&sha, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_ShaFinal(&sha, hash);
        if (ret !=0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_SHA_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Try passing in bad values. */
    if (!flag) {
        b.input = NULL;
        b.inLen = 0;

        ret = wc_ShaUpdate(&sha, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        c.input = NULL;
        c.inLen = WC_SHA_DIGEST_SIZE;

        ret = wc_ShaUpdate(&sha, (byte*)c.input, (word32)c.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_ShaUpdate(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_ShaFree(&sha);
    /* If not returned then the unit test passed test vectors. */
    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_ShaUpdate() */


/*
 * Unit test on wc_ShaFinal
 */
static int test_wc_ShaFinal (void)
{
    int flag = 0;
#ifndef NO_SHA
    wc_Sha sha;
    byte* hash_test[3];
    byte hash1[WC_SHA_DIGEST_SIZE];
    byte hash2[2*WC_SHA_DIGEST_SIZE];
    byte hash3[5*WC_SHA_DIGEST_SIZE];
    int times, i, ret;

    /*Initialize*/
    ret = wc_InitSha(&sha);
    if (ret) {
        flag =  ret;
    }

    if (!flag) {
        hash_test[0] = hash1;
        hash_test[1] = hash2;
        hash_test[2] = hash3;
    }

    times = sizeof(hash_test)/sizeof(byte*);

    /* Good test args. */
    printf(testingFmt, "wc_ShaFinal()");

    for (i = 0; i < times; i++) {
        if (!flag) {
            ret = wc_ShaFinal(&sha, hash_test[i]);
            if (ret != 0) {
                flag = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    /* Test bad args. */
    if (!flag) {
        ret = wc_ShaFinal(NULL, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_ShaFinal(NULL, hash1);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_ShaFinal(&sha, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_ShaFree(&sha);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_ShaFinal */


/*
 * Unit test for wc_InitSha256()
 */
static int test_wc_InitSha256 (void)
{
    int flag = 0;
#ifndef NO_SHA256
    wc_Sha256 sha256;
    int ret;

    printf(testingFmt, "wc_InitSha256()");

    /* Test good arg. */
    ret = wc_InitSha256(&sha256);
    if (ret != 0) {
        flag = WOLFSSL_FATAL_ERROR;
    }

    /* Test bad arg. */
    if (!flag) {
        ret = wc_InitSha256(NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha256Free(&sha256);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_InitSha256 */


/*
 * Unit test for wc_Sha256Update()
 */
static int test_wc_Sha256Update (void)
{
    int flag = 0;
#ifndef NO_SHA256
    wc_Sha256 sha256;
    byte hash[WC_SHA256_DIGEST_SIZE];
    testVector a, b, c;
    int ret;

    ret = wc_InitSha256(&sha256);
    if (ret != 0) {
        flag = ret;
    }

    printf(testingFmt, "wc_Sha256Update()");

    /*  Input. */
    if (!flag) {
        a.input = "a";
        a.inLen = XSTRLEN(a.input);
    }

    if (!flag) {
        ret = wc_Sha256Update(&sha256, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_Sha256Final(&sha256, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Update input. */
    if (!flag) {
        a.input = "abc";
        a.output = "\xBA\x78\x16\xBF\x8F\x01\xCF\xEA\x41\x41\x40\xDE\x5D\xAE\x22"
                    "\x23\xB0\x03\x61\xA3\x96\x17\x7A\x9C\xB4\x10\xFF\x61\xF2\x00"
                    "\x15\xAD";
        a.inLen = XSTRLEN(a.input);
        a.outLen = XSTRLEN(a.output);

        ret = wc_Sha256Update(&sha256, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_Sha256Final(&sha256, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_SHA256_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Try passing in bad values */
    if (!flag) {
        b.input = NULL;
        b.inLen = 0;

        ret = wc_Sha256Update(&sha256, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        c.input = NULL;
        c.inLen = WC_SHA256_DIGEST_SIZE;

        ret = wc_Sha256Update(&sha256, (byte*)c.input, (word32)c.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha256Update(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha256Free(&sha256);
    /* If not returned then the unit test passed. */
    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_Sha256Update */


/*
 * Unit test function for wc_Sha256Final()
 */
static int test_wc_Sha256Final (void)
{
    int flag = 0;
#ifndef NO_SHA256
    wc_Sha256 sha256;
    byte* hash_test[3];
    byte hash1[WC_SHA256_DIGEST_SIZE];
    byte hash2[2*WC_SHA256_DIGEST_SIZE];
    byte hash3[5*WC_SHA256_DIGEST_SIZE];
    int times, i, ret;

    /* Initialize */
    ret = wc_InitSha256(&sha256);
    if (ret != 0) {
        flag =  ret;
    }

    if (!flag) {
        hash_test[0] = hash1;
        hash_test[1] = hash2;
        hash_test[2] = hash3;
    }

    times = sizeof(hash_test) / sizeof(byte*);

    /* Good test args. */
    printf(testingFmt, "wc_Sha256Final()");

    for (i = 0; i < times; i++) {
        if (!flag) {
            ret = wc_Sha256Final(&sha256, hash_test[i]);
            if (ret != 0) {
                flag = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    /* Test bad args. */
    if (!flag ) {
        ret = wc_Sha256Final(NULL, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha256Final(NULL, hash1);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha256Final(&sha256, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha256Free(&sha256);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_Sha256Final */



/*
 * Testing wc_InitSha512()
 */
static int test_wc_InitSha512 (void)
{
    int flag = 0;
#ifdef WOLFSSL_SHA512
    wc_Sha512 sha512;
    int ret;

    printf(testingFmt, "wc_InitSha512()");

    /* Test good arg. */
    ret = wc_InitSha512(&sha512);
    if (ret != 0) {
        flag  = WOLFSSL_FATAL_ERROR;
    }

    /* Test bad arg. */
    if (!flag) {
        ret = wc_InitSha512(NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha512Free(&sha512);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_InitSha512 */


/*
 *  wc_Sha512Update() test.
 */
static int test_wc_Sha512Update (void)
{

    int flag = 0;
#ifdef WOLFSSL_SHA512
    wc_Sha512 sha512;
    byte hash[WC_SHA512_DIGEST_SIZE];
    testVector a, b, c;
    int ret;

    ret = wc_InitSha512(&sha512);
    if (ret != 0) {
        flag = ret;
    }

    printf(testingFmt, "wc_Sha512Update()");

    /* Input. */
    if (!flag) {
        a.input = "a";
        a.inLen = XSTRLEN(a.input);
    }

    if (!flag) {
        ret = wc_Sha512Update(&sha512, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_Sha512Final(&sha512, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Update input. */
    if (!flag) {
        a.input = "abc";
        a.output = "\xdd\xaf\x35\xa1\x93\x61\x7a\xba\xcc\x41\x73\x49\xae\x20\x41"
                    "\x31\x12\xe6\xfa\x4e\x89\xa9\x7e\xa2\x0a\x9e\xee\xe6\x4b"
                    "\x55\xd3\x9a\x21\x92\x99\x2a\x27\x4f\xc1\xa8\x36\xba\x3c"
                    "\x23\xa3\xfe\xeb\xbd\x45\x4d\x44\x23\x64\x3c\xe8\x0e\x2a"
                    "\x9a\xc9\x4f\xa5\x4c\xa4\x9f";
        a.inLen = XSTRLEN(a.input);
        a.outLen = XSTRLEN(a.output);

        ret = wc_Sha512Update(&sha512, (byte*) a.input, (word32) a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_Sha512Final(&sha512, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_SHA512_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Try passing in bad values */
    if (!flag) {
        b.input = NULL;
        b.inLen = 0;

        ret = wc_Sha512Update(&sha512, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        c.input = NULL;
        c.inLen = WC_SHA512_DIGEST_SIZE;

        ret = wc_Sha512Update(&sha512, (byte*)c.input, (word32)c.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha512Update(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha512Free(&sha512);

    /* If not returned then the unit test passed test vectors. */
    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_Sha512Update  */


/*
 * Unit test function for wc_Sha512Final()
 */
static int test_wc_Sha512Final (void)
{
    int flag = 0;
#ifdef WOLFSSL_SHA512
    wc_Sha512 sha512;
    byte* hash_test[3];
    byte hash1[WC_SHA512_DIGEST_SIZE];
    byte hash2[2*WC_SHA512_DIGEST_SIZE];
    byte hash3[5*WC_SHA512_DIGEST_SIZE];
    int times, i, ret;

    /* Initialize  */
    ret = wc_InitSha512(&sha512);
    if (ret != 0) {
        flag =  ret;
    }

    if (!flag) {
        hash_test[0] = hash1;
        hash_test[1] = hash2;
        hash_test[2] = hash3;
    }

    times = sizeof(hash_test) / sizeof(byte *);

    /* Good test args. */
    printf(testingFmt, "wc_Sha512Final()");

    for (i = 0; i < times; i++) {
        if (!flag) {
            ret = wc_Sha512Final(&sha512, hash_test[i]);
            if (ret != 0) {
                flag = WOLFSSL_FATAL_ERROR;
            }
        }
    }
    /* Test bad args. */
    if (!flag) {
        ret = wc_Sha512Final(NULL, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }

    if (!flag) {}
        ret = wc_Sha512Final(NULL, hash1);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha512Final(&sha512, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha512Free(&sha512);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha512Final */




/*
 * Testing wc_InitSha384()
 */
static int test_wc_InitSha384 (void)
{
    int flag = 0;
#ifdef WOLFSSL_SHA384
    wc_Sha384 sha384;
    int ret;

    printf(testingFmt, "wc_InitSha384()");

    /* Test good arg. */
    ret = wc_InitSha384(&sha384);
    if (ret != 0) {
        flag = WOLFSSL_FATAL_ERROR;
    }

    /* Test bad arg. */
    if (!flag) {
        ret = wc_InitSha384(NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha384Free(&sha384);
    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_InitSha384 */


/*
 * test wc_Sha384Update()
 */
static int test_wc_Sha384Update (void)
{

    int flag = 0;
#ifdef WOLFSSL_SHA384
    wc_Sha384 sha384;
    byte hash[WC_SHA384_DIGEST_SIZE];
    testVector a, b, c;
    int ret;

    ret = wc_InitSha384(&sha384);
    if (ret != 0) {
        flag = ret;
    }

    printf(testingFmt, "wc_Sha384Update()");

    /* Input */
    if (!flag) {
        a.input = "a";
        a.inLen = XSTRLEN(a.input);
    }

    if (!flag) {
        ret = wc_Sha384Update(&sha384, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_Sha384Final(&sha384, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Update input. */
    if (!flag) {
        a.input = "abc";
        a.output = "\xcb\x00\x75\x3f\x45\xa3\x5e\x8b\xb5\xa0\x3d\x69\x9a\xc6\x50"
                   "\x07\x27\x2c\x32\xab\x0e\xde\xd1\x63\x1a\x8b\x60\x5a\x43\xff"
                   "\x5b\xed\x80\x86\x07\x2b\xa1\xe7\xcc\x23\x58\xba\xec\xa1\x34"
                   "\xc8\x25\xa7";
        a.inLen = XSTRLEN(a.input);
        a.outLen = XSTRLEN(a.output);

        ret = wc_Sha384Update(&sha384, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_Sha384Final(&sha384, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_SHA384_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Pass in bad values. */
    if (!flag) {
        b.input = NULL;
        b.inLen = 0;

        ret = wc_Sha384Update(&sha384, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        c.input = NULL;
        c.inLen = WC_SHA384_DIGEST_SIZE;

        ret = wc_Sha384Update(&sha384, (byte*)c.input, (word32)c.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha384Update(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha384Free(&sha384);

    /* If not returned then the unit test passed test vectors. */
    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha384Update */

/*
 * Unit test function for wc_Sha384Final();
 */
static int test_wc_Sha384Final (void)
{
    int flag = 0;
#ifdef WOLFSSL_SHA384
    wc_Sha384 sha384;
    byte* hash_test[3];
    byte hash1[WC_SHA384_DIGEST_SIZE];
    byte hash2[2*WC_SHA384_DIGEST_SIZE];
    byte hash3[5*WC_SHA384_DIGEST_SIZE];
    int times, i, ret;

    /* Initialize */
    ret = wc_InitSha384(&sha384);
    if (ret) {
        flag = ret;
    }

    if (!flag) {
        hash_test[0] = hash1;
        hash_test[1] = hash2;
        hash_test[2] = hash3;
    }

    times = sizeof(hash_test) / sizeof(byte*);

    /* Good test args. */
    printf(testingFmt, "wc_Sha384Final()");

    for (i = 0; i < times; i++) {
        if (!flag) {
            ret = wc_Sha384Final(&sha384, hash_test[i]);
            if (ret != 0) {
                flag = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    /* Test bad args. */
    if (!flag) {
        ret = wc_Sha384Final(NULL, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha384Final(NULL, hash1);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha384Final(&sha384, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha384Free(&sha384);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_Sha384Final */



/*
 * Testing wc_InitSha224();
 */
static int test_wc_InitSha224 (void)
{
    int flag = 0;
#ifdef WOLFSSL_SHA224
    wc_Sha224 sha224;
    int ret;

    printf(testingFmt, "wc_InitSha224()");

    /* Test good arg. */
    ret = wc_InitSha224(&sha224);
    if (ret != 0) {
        flag = WOLFSSL_FATAL_ERROR;
    }

    /* Test bad arg. */
    if (!flag) {
        ret = wc_InitSha224(NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha224Free(&sha224);
    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_InitSha224 */



/*
 * Unit test on wc_Sha224Update
 */
static int test_wc_Sha224Update (void)
{
    int flag = 0;
#ifdef WOLFSSL_SHA224
    wc_Sha224 sha224;
    byte hash[WC_SHA224_DIGEST_SIZE];
    testVector a, b, c;
    int ret;

    ret = wc_InitSha224(&sha224);
    if (ret != 0) {
        flag = ret;
    }

    printf(testingFmt, "wc_Sha224Update()");

    /* Input. */
    if (!flag) {
        a.input = "a";
        a.inLen = XSTRLEN(a.input);
    }

    if (!flag) {
        ret = wc_Sha224Update(&sha224, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_Sha224Final(&sha224, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Update input. */
    if (!flag) {
        a.input = "abc";
        a.output = "\x23\x09\x7d\x22\x34\x05\xd8\x22\x86\x42\xa4\x77\xbd\xa2"
                    "\x55\xb3\x2a\xad\xbc\xe4\xbd\xa0\xb3\xf7\xe3\x6c\x9d\xa7";
        a.inLen = XSTRLEN(a.input);
        a.outLen = XSTRLEN(a.output);

        ret = wc_Sha224Update(&sha224, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_Sha224Final(&sha224, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_SHA224_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Pass  in bad values. */
    if (!flag) {
        b.input = NULL;
        b.inLen = 0;

        ret = wc_Sha224Update(&sha224, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        c.input = NULL;
        c.inLen = WC_SHA224_DIGEST_SIZE;

        ret = wc_Sha224Update(&sha224, (byte*)c.input, (word32)c.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha224Update(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha224Free(&sha224);

    /* If not returned then the unit test passed test vectors. */
    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_Sha224Update */



/*
 * Unit test for wc_Sha224Final();
 */
static int test_wc_Sha224Final (void)
{
    int flag = 0;
#ifdef WOLFSSL_SHA224
    wc_Sha224 sha224;
    byte* hash_test[3];
    byte hash1[WC_SHA224_DIGEST_SIZE];
    byte hash2[2*WC_SHA224_DIGEST_SIZE];
    byte hash3[5*WC_SHA224_DIGEST_SIZE];
    int times, i, ret;

    /* Initialize */
    ret = wc_InitSha224(&sha224);
    if (ret) {
        flag = ret;
    }

    if (!flag) {
        hash_test[0] = hash1;
        hash_test[1] = hash2;
        hash_test[2] = hash3;
    }

    times = sizeof(hash_test) / sizeof(byte*);

    /* Good test args. */
    printf(testingFmt, "wc_sha224Final()");
    /* Testing oversized buffers. */
    for (i = 0; i < times; i++) {
        if (!flag) {
            ret = wc_Sha224Final(&sha224, hash_test[i]);
            if (ret != 0) {
                flag = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    /* Test bad args. */
    if (!flag) {
        ret = wc_Sha224Final(NULL, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha224Final(NULL, hash1);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_Sha224Final(&sha224, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_Sha224Free(&sha224);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha224Final */




/*
 * Testing wc_InitRipeMd()
 */
static int test_wc_InitRipeMd (void)
{
    int flag = 0;
#ifdef WOLFSSL_RIPEMD
    RipeMd ripemd;
    int ret;

    printf(testingFmt, "wc_InitRipeMd()");

    /* Test good arg. */
    ret = wc_InitRipeMd(&ripemd);
    if (ret != 0) {
        flag = WOLFSSL_FATAL_ERROR;
    }

    /* Test bad arg. */
    if (!flag) {
        ret = wc_InitRipeMd(NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_InitRipeMd */


/*
 * Testing wc_RipeMdUpdate()
 */
static int test_wc_RipeMdUpdate (void)
{

    int flag = 0;
#ifdef WOLFSSL_RIPEMD
    RipeMd ripemd;
    byte hash[RIPEMD_DIGEST_SIZE];
    testVector a, b, c;
    int ret;

    ret = wc_InitRipeMd(&ripemd);
    if (ret != 0) {
        flag = ret;
    }

    printf(testingFmt, "wc_RipeMdUpdate()");

    /* Input */
    if (!flag) {
        a.input = "a";
        a.inLen = XSTRLEN(a.input);
    }

    if (!flag) {
        ret = wc_RipeMdUpdate(&ripemd, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_RipeMdFinal(&ripemd, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Update input. */
    if (!flag) {
        a.input = "abc";
        a.output = "\x8e\xb2\x08\xf7\xe0\x5d\x98\x7a\x9b\x04\x4a\x8e\x98\xc6"
                   "\xb0\x87\xf1\x5a\x0b\xfc";
        a.inLen = XSTRLEN(a.input);
        a.outLen = XSTRLEN(a.output);

        ret = wc_RipeMdUpdate(&ripemd, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_RipeMdFinal(&ripemd, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, RIPEMD_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Pass in bad values. */
    if (!flag) {
        b.input = NULL;
        b.inLen = 0;

        ret = wc_RipeMdUpdate(&ripemd, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        c.input = NULL;
        c.inLen = RIPEMD_DIGEST_SIZE;

        ret = wc_RipeMdUpdate(&ripemd, (byte*)c.input, (word32)c.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_RipeMdUpdate(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_RipeMdUdpate */



/*
 * Unit test function for wc_RipeMdFinal()
 */
static int test_wc_RipeMdFinal (void)
{
    int flag = 0;
#ifdef WOLFSSL_RIPEMD
    RipeMd ripemd;
    byte* hash_test[3];
    byte hash1[RIPEMD_DIGEST_SIZE];
    byte hash2[2*RIPEMD_DIGEST_SIZE];
    byte hash3[5*RIPEMD_DIGEST_SIZE];
    int times, i, ret;

    /* Initialize */
    ret = wc_InitRipeMd(&ripemd);
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        hash_test[0] = hash1;
        hash_test[1] = hash2;
        hash_test[2] = hash3;
    }

    times = sizeof(hash_test) / sizeof(byte*);

    /* Good test args. */
    printf(testingFmt, "wc_RipeMdFinal()");
    /* Testing oversized buffers. */
    for (i = 0; i < times; i++) {
        if (!flag) {
            ret = wc_RipeMdFinal(&ripemd, hash_test[i]);
            if (ret != 0) {
                flag = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    /* Test bad args. */
    if (!flag) {
        ret = wc_RipeMdFinal(NULL, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_RipeMdFinal(NULL, hash1);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_RipeMdFinal(&ripemd, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_RipeMdFinal */





/*
 * Testing wc_InitSha3_224, wc_InitSha3_256, wc_InitSha3_384, and
 * wc_InitSha3_512
 */
static int test_wc_InitSha3 (void)
{
    int             ret = 0;
#if defined(WOLFSSL_SHA3)
    wc_Sha3            sha3;

    #if !defined(WOLFSSL_NOSHA3_224)
        printf(testingFmt, "wc_InitSha3_224()");

        ret = wc_InitSha3_224(&sha3, HEAP_HINT, devId);

        /* Test bad args. */
        if (ret == 0) {
            ret = wc_InitSha3_224(NULL, HEAP_HINT, devId);
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
        wc_Sha3_224_Free(&sha3);
        printf(resultFmt, ret == 0 ? passed : failed);
    #endif /* NOSHA3_224 */
    #if !defined(WOLFSSL_NOSHA3_256)
        if (ret == 0) {
            printf(testingFmt, "wc_InitSha3_256()");

            ret = wc_InitSha3_256(&sha3, HEAP_HINT, devId);
            /* Test bad args. */
            if (ret == 0) {
                ret = wc_InitSha3_256(NULL, HEAP_HINT, devId);
                if (ret == BAD_FUNC_ARG) {
                    ret = 0;
                } else if (ret == 0) {
                    ret = WOLFSSL_FATAL_ERROR;
                }
            }
            wc_Sha3_256_Free(&sha3);
            printf(resultFmt, ret == 0 ? passed : failed);
        } /* END sha3_256 */
    #endif /* NOSHA3_256 */
    #if !defined(WOLFSSL_NOSHA3_384)
        if (ret == 0) {
            printf(testingFmt, "wc_InitSha3_384()");

            ret = wc_InitSha3_384(&sha3, HEAP_HINT, devId);
            /* Test bad args. */
            if (ret == 0) {
                ret = wc_InitSha3_384(NULL, HEAP_HINT, devId);
                if (ret == BAD_FUNC_ARG) {
                    ret = 0;
                } else if (ret == 0) {
                    ret = WOLFSSL_FATAL_ERROR;
                }
            }
            wc_Sha3_384_Free(&sha3);
            printf(resultFmt, ret == 0 ? passed : failed);
        } /* END sha3_384 */
    #endif /* NOSHA3_384 */
    #if !defined(WOLFSSL_NOSHA3_512)
        if (ret == 0) {
            printf(testingFmt, "wc_InitSha3_512()");

            ret = wc_InitSha3_512(&sha3, HEAP_HINT, devId);
            /* Test bad args. */
            if (ret == 0) {
                ret = wc_InitSha3_512(NULL, HEAP_HINT, devId);
                if (ret == BAD_FUNC_ARG) {
                    ret = 0;
                } else if (ret == 0) {
                    ret = WOLFSSL_FATAL_ERROR;
                }
            }
            wc_Sha3_512_Free(&sha3);
            printf(resultFmt, ret == 0 ? passed : failed);
        } /* END sha3_512 */
    #endif /* NOSHA3_512 */
#endif
    return ret;

} /* END test_wc_InitSha3 */


/*
 * Testing wc_Sha3_Update()
 */
static int testing_wc_Sha3_Update (void)
{
    int         ret = 0;

#if defined(WOLFSSL_SHA3)
    wc_Sha3        sha3;
    byte        msg[] = "Everybody's working for the weekend.";
    byte        msg2[] = "Everybody gets Friday off.";
    byte        msgCmp[] = "\x45\x76\x65\x72\x79\x62\x6f\x64\x79\x27\x73\x20"
                        "\x77\x6f\x72\x6b\x69\x6e\x67\x20\x66\x6f\x72\x20\x74"
                        "\x68\x65\x20\x77\x65\x65\x6b\x65\x6e\x64\x2e\x45\x76"
                        "\x65\x72\x79\x62\x6f\x64\x79\x20\x67\x65\x74\x73\x20"
                        "\x46\x72\x69\x64\x61\x79\x20\x6f\x66\x66\x2e";
    word32      msglen = sizeof(msg) - 1;
    word32      msg2len = sizeof(msg2);
    word32      msgCmplen = sizeof(msgCmp);

    #if !defined(WOLFSSL_NOSHA3_224)
        printf(testingFmt, "wc_Sha3_224_Update()");

        ret = wc_InitSha3_224(&sha3, HEAP_HINT, devId);
        if (ret != 0) {
            return ret;
        }

        ret = wc_Sha3_224_Update(&sha3, msg, msglen);
        if (XMEMCMP(msg, sha3.t, msglen) || sha3.i != msglen) {
            ret = WOLFSSL_FATAL_ERROR;
        }
        if (ret == 0) {
            ret = wc_Sha3_224_Update(&sha3, msg2, msg2len);
            if (ret == 0 && XMEMCMP(sha3.t, msgCmp, msgCmplen) != 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
        /* Pass bad args. */
        if (ret == 0) {
            ret = wc_Sha3_224_Update(NULL, msg2, msg2len);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_Sha3_224_Update(&sha3, NULL, 5);
            }
            if (ret == BAD_FUNC_ARG) {
                wc_Sha3_224_Free(&sha3);
                if (wc_InitSha3_224(&sha3, HEAP_HINT, devId)) {
                    return ret;
                }
                ret = wc_Sha3_224_Update(&sha3, NULL, 0);
                if (ret == 0) {
                    ret = wc_Sha3_224_Update(&sha3, msg2, msg2len);
                }
                if (ret == 0 && XMEMCMP(msg2, sha3.t, msg2len) != 0) {
                    ret = WOLFSSL_FATAL_ERROR;
                }
            }
        }
        wc_Sha3_224_Free(&sha3);

        printf(resultFmt, ret == 0 ? passed : failed);
    #endif /* SHA3_224 */

    #if !defined(WOLFSSL_NOSHA3_256)
        if (ret == 0) {
            printf(testingFmt, "wc_Sha3_256_Update()");

            ret = wc_InitSha3_256(&sha3, HEAP_HINT, devId);
            if (ret != 0) {
                return ret;
            }
            ret = wc_Sha3_256_Update(&sha3, msg, msglen);
            if (XMEMCMP(msg, sha3.t, msglen) || sha3.i != msglen) {
                ret = WOLFSSL_FATAL_ERROR;
            }
            if (ret == 0) {
                ret = wc_Sha3_256_Update(&sha3, msg2, msg2len);
                if (XMEMCMP(sha3.t, msgCmp, msgCmplen) != 0) {
                    ret = WOLFSSL_FATAL_ERROR;
                }
            }
            /* Pass bad args. */
            if (ret == 0) {
                ret = wc_Sha3_256_Update(NULL, msg2, msg2len);
                if (ret == BAD_FUNC_ARG) {
                    ret = wc_Sha3_256_Update(&sha3, NULL, 5);
                }
                if (ret == BAD_FUNC_ARG) {
                    wc_Sha3_256_Free(&sha3);
                    if (wc_InitSha3_256(&sha3, HEAP_HINT, devId)) {
                        return ret;
                    }
                    ret = wc_Sha3_256_Update(&sha3, NULL, 0);
                    if (ret == 0) {
                        ret = wc_Sha3_256_Update(&sha3, msg2, msg2len);
                    }
                    if (ret == 0 && XMEMCMP(msg2, sha3.t, msg2len) != 0) {
                        ret = WOLFSSL_FATAL_ERROR;
                    }
                }
            }
            wc_Sha3_256_Free(&sha3);

            printf(resultFmt, ret == 0 ? passed : failed);
        }
    #endif /* SHA3_256 */

    #if !defined(WOLFSSL_NOSHA3_384)
        if (ret == 0) {
            printf(testingFmt, "wc_Sha3_384_Update()");

            ret = wc_InitSha3_384(&sha3, HEAP_HINT, devId);
            if (ret != 0) {
                return ret;
            }
            ret = wc_Sha3_384_Update(&sha3, msg, msglen);
            if (XMEMCMP(msg, sha3.t, msglen) || sha3.i != msglen) {
                ret = WOLFSSL_FATAL_ERROR;
            }
            if (ret == 0) {
                ret = wc_Sha3_384_Update(&sha3, msg2, msg2len);
                if (XMEMCMP(sha3.t, msgCmp, msgCmplen) != 0) {
                    ret = WOLFSSL_FATAL_ERROR;
                }
            }
            /* Pass bad args. */
            if (ret == 0) {
                ret = wc_Sha3_384_Update(NULL, msg2, msg2len);
                if (ret == BAD_FUNC_ARG) {
                    ret = wc_Sha3_384_Update(&sha3, NULL, 5);
                }
                if (ret == BAD_FUNC_ARG) {
                    wc_Sha3_384_Free(&sha3);
                    if (wc_InitSha3_384(&sha3, HEAP_HINT, devId)) {
                        return ret;
                    }
                    ret = wc_Sha3_384_Update(&sha3, NULL, 0);
                    if (ret == 0) {
                        ret = wc_Sha3_384_Update(&sha3, msg2, msg2len);
                    }
                    if (ret == 0 && XMEMCMP(msg2, sha3.t, msg2len) != 0) {
                        ret = WOLFSSL_FATAL_ERROR;
                    }
                }
            }
            wc_Sha3_384_Free(&sha3);

            printf(resultFmt, ret == 0 ? passed : failed);
        }
    #endif /* SHA3_384 */

    #if !defined(WOLFSSL_NOSHA3_512)
        if (ret == 0) {
            printf(testingFmt, "wc_Sha3_512_Update()");

            ret = wc_InitSha3_512(&sha3, HEAP_HINT, devId);
            if (ret != 0) {
                return ret;
            }
            ret = wc_Sha3_512_Update(&sha3, msg, msglen);
            if (XMEMCMP(msg, sha3.t, msglen) || sha3.i != msglen) {
                ret = WOLFSSL_FATAL_ERROR;
            }
            if (ret == 0) {
                ret = wc_Sha3_512_Update(&sha3, msg2, msg2len);
                if (XMEMCMP(sha3.t, msgCmp, msgCmplen) != 0) {
                    ret = WOLFSSL_FATAL_ERROR;
                }
            }
            /* Pass bad args. */
            if (ret == 0) {
                ret = wc_Sha3_512_Update(NULL, msg2, msg2len);
                if (ret == BAD_FUNC_ARG) {
                    ret = wc_Sha3_512_Update(&sha3, NULL, 5);
                }
                if (ret == BAD_FUNC_ARG) {
                    wc_Sha3_512_Free(&sha3);
                    if (wc_InitSha3_512(&sha3, HEAP_HINT, devId)) {
                        return ret;
                    }
                    ret = wc_Sha3_512_Update(&sha3, NULL, 0);
                    if (ret == 0) {
                        ret = wc_Sha3_512_Update(&sha3, msg2, msg2len);
                    }
                    if (ret == 0 && XMEMCMP(msg2, sha3.t, msg2len) != 0) {
                        ret = WOLFSSL_FATAL_ERROR;
                    }
                }
            }
            wc_Sha3_512_Free(&sha3);
            printf(resultFmt, ret == 0 ? passed : failed);
        }
    #endif /* SHA3_512 */
#endif /* WOLFSSL_SHA3 */
    return ret;

} /* END testing_wc_Sha3_Update */

/*
 *  Testing wc_Sha3_224_Final()
 */
static int test_wc_Sha3_224_Final (void)
{
    int         ret = 0;

#if defined(WOLFSSL_SHA3) && !defined(WOLFSSL_NOSHA3_224)
    wc_Sha3        sha3;
    const char* msg    = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnom"
                         "nopnopq";
    const char* expOut = "\x8a\x24\x10\x8b\x15\x4a\xda\x21\xc9\xfd\x55"
                         "\x74\x49\x44\x79\xba\x5c\x7e\x7a\xb7\x6e\xf2"
                         "\x64\xea\xd0\xfc\xce\x33";
    byte        hash[WC_SHA3_224_DIGEST_SIZE];
    byte        hashRet[WC_SHA3_224_DIGEST_SIZE];

    /* Init stack variables. */
    XMEMSET(hash, 0, sizeof(hash));

    printf(testingFmt, "wc_Sha3_224_Final()");

    ret = wc_InitSha3_224(&sha3, HEAP_HINT, devId);
    if (ret != 0) {
        return ret;
    }

    ret= wc_Sha3_224_Update(&sha3, (byte*)msg, (word32)XSTRLEN(msg));
    if (ret == 0) {
        ret = wc_Sha3_224_Final(&sha3, hash);
        if (ret == 0 && XMEMCMP(expOut, hash, WC_SHA3_224_DIGEST_SIZE) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Sha3_224_Final(NULL, hash);
        if (ret == 0) {
            ret = wc_Sha3_224_Final(&sha3, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);

    if (ret == 0) {
        printf(testingFmt, "wc_Sha3_224_GetHash()");

        ret = wc_InitSha3_224(&sha3, HEAP_HINT, devId);
        if (ret != 0) {
            return ret;
        }

        /* Init stack variables. */
        XMEMSET(hash, 0, sizeof(hash));
        XMEMSET(hashRet, 0, sizeof(hashRet));

        ret= wc_Sha3_224_Update(&sha3, (byte*)msg, (word32)XSTRLEN(msg));
        if (ret == 0) {
            ret = wc_Sha3_224_GetHash(&sha3, hashRet);
        }

        if (ret == 0) {
            ret = wc_Sha3_224_Final(&sha3, hash);
            if (ret == 0 && XMEMCMP(hash, hashRet, WC_SHA3_224_DIGEST_SIZE) != 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
        if (ret == 0) {
            /* Test bad args. */
            ret = wc_Sha3_224_GetHash(NULL, hashRet);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_Sha3_224_GetHash(&sha3, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }

        printf(resultFmt, ret == 0 ? passed : failed);
    }

    wc_Sha3_224_Free(&sha3);
#endif
    return ret;
} /* END test_wc_Sha3_224_Final */


/*
 *  Testing wc_Sha3_256_Final()
 */
static int test_wc_Sha3_256_Final (void)
{
    int         ret = 0;

#if defined(WOLFSSL_SHA3) && !defined(WOLFSSL_NOSHA3_256)
    wc_Sha3        sha3;
    const char* msg    = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnom"
                         "nopnopq";
    const char* expOut = "\x41\xc0\xdb\xa2\xa9\xd6\x24\x08\x49\x10\x03\x76\xa8"
                        "\x23\x5e\x2c\x82\xe1\xb9\x99\x8a\x99\x9e\x21\xdb\x32"
                        "\xdd\x97\x49\x6d\x33\x76";
    byte        hash[WC_SHA3_256_DIGEST_SIZE];
    byte        hashRet[WC_SHA3_256_DIGEST_SIZE];

    /* Init stack variables. */
    XMEMSET(hash, 0, sizeof(hash));

    printf(testingFmt, "wc_Sha3_256_Final()");

    ret = wc_InitSha3_256(&sha3, HEAP_HINT, devId);
    if (ret != 0) {
        return ret;
    }

    ret= wc_Sha3_256_Update(&sha3, (byte*)msg, (word32)XSTRLEN(msg));
    if (ret == 0) {
        ret = wc_Sha3_256_Final(&sha3, hash);
        if (ret == 0 && XMEMCMP(expOut, hash, WC_SHA3_256_DIGEST_SIZE) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Sha3_256_Final(NULL, hash);
        if (ret == 0) {
            ret = wc_Sha3_256_Final(&sha3, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);
    if (ret == 0) {
        printf(testingFmt, "wc_Sha3_256_GetHash()");

        ret = wc_InitSha3_256(&sha3, HEAP_HINT, devId);
        if (ret != 0) {
            return ret;
        }
        /* Init stack variables. */
        XMEMSET(hash, 0, sizeof(hash));
        XMEMSET(hashRet, 0, sizeof(hashRet));

        ret= wc_Sha3_256_Update(&sha3, (byte*)msg, (word32)XSTRLEN(msg));
        if (ret == 0) {
            ret = wc_Sha3_256_GetHash(&sha3, hashRet);
        }
        if (ret == 0) {
            ret = wc_Sha3_256_Final(&sha3, hash);
            if (ret == 0 && XMEMCMP(hash, hashRet, WC_SHA3_256_DIGEST_SIZE) != 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
        if (ret == 0) {
            /* Test bad args. */
            ret = wc_Sha3_256_GetHash(NULL, hashRet);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_Sha3_256_GetHash(&sha3, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }

        printf(resultFmt, ret == 0 ? passed : failed);
    }

    wc_Sha3_256_Free(&sha3);
#endif
    return ret;
} /* END test_wc_Sha3_256_Final */


/*
 *  Testing wc_Sha3_384_Final()
 */
static int test_wc_Sha3_384_Final (void)
{
    int         ret = 0;

#if defined(WOLFSSL_SHA3) && !defined(WOLFSSL_NOSHA3_384)
    wc_Sha3        sha3;
    const char* msg    = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnom"
                         "nopnopq";
    const char* expOut = "\x99\x1c\x66\x57\x55\xeb\x3a\x4b\x6b\xbd\xfb\x75\xc7"
                         "\x8a\x49\x2e\x8c\x56\xa2\x2c\x5c\x4d\x7e\x42\x9b\xfd"
                         "\xbc\x32\xb9\xd4\xad\x5a\xa0\x4a\x1f\x07\x6e\x62\xfe"
                         "\xa1\x9e\xef\x51\xac\xd0\x65\x7c\x22";
    byte        hash[WC_SHA3_384_DIGEST_SIZE];
    byte        hashRet[WC_SHA3_384_DIGEST_SIZE];

    /* Init stack variables. */
    XMEMSET(hash, 0, sizeof(hash));

    printf(testingFmt, "wc_Sha3_384_Final()");

    ret = wc_InitSha3_384(&sha3, HEAP_HINT, devId);
    if (ret != 0) {
        return ret;
    }

    ret= wc_Sha3_384_Update(&sha3, (byte*)msg, (word32)XSTRLEN(msg));
    if (ret == 0) {
        ret = wc_Sha3_384_Final(&sha3, hash);
        if (ret == 0 && XMEMCMP(expOut, hash, WC_SHA3_384_DIGEST_SIZE) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Sha3_384_Final(NULL, hash);
        if (ret == 0) {
            ret = wc_Sha3_384_Final(&sha3, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);
    if (ret == 0) {
        printf(testingFmt, "wc_Sha3_384_GetHash()");

        ret = wc_InitSha3_384(&sha3, HEAP_HINT, devId);
        if (ret != 0) {
            return ret;
        }
        /* Init stack variables. */
        XMEMSET(hash, 0, sizeof(hash));
        XMEMSET(hashRet, 0, sizeof(hashRet));

        ret= wc_Sha3_384_Update(&sha3, (byte*)msg, (word32)XSTRLEN(msg));
        if (ret == 0) {
            ret = wc_Sha3_384_GetHash(&sha3, hashRet);
        }
        if (ret == 0) {
            ret = wc_Sha3_384_Final(&sha3, hash);
            if (ret == 0 && XMEMCMP(hash, hashRet, WC_SHA3_384_DIGEST_SIZE) != 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
        if (ret == 0) {
            /* Test bad args. */
            ret = wc_Sha3_384_GetHash(NULL, hashRet);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_Sha3_384_GetHash(&sha3, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }

        printf(resultFmt, ret == 0 ? passed : failed);
    }

    wc_Sha3_384_Free(&sha3);
#endif
    return ret;
} /* END test_wc_Sha3_384_Final */



/*
 *  Testing wc_Sha3_512_Final()
 */
static int test_wc_Sha3_512_Final (void)
{
    int         ret = 0;

#if defined(WOLFSSL_SHA3) && !defined(WOLFSSL_NOSHA3_384)
    wc_Sha3        sha3;
    const char* msg    = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnom"
                         "nopnopq";
    const char* expOut = "\x04\xa3\x71\xe8\x4e\xcf\xb5\xb8\xb7\x7c\xb4\x86\x10"
                         "\xfc\xa8\x18\x2d\xd4\x57\xce\x6f\x32\x6a\x0f\xd3\xd7"
                         "\xec\x2f\x1e\x91\x63\x6d\xee\x69\x1f\xbe\x0c\x98\x53"
                         "\x02\xba\x1b\x0d\x8d\xc7\x8c\x08\x63\x46\xb5\x33\xb4"
                         "\x9c\x03\x0d\x99\xa2\x7d\xaf\x11\x39\xd6\xe7\x5e";
    byte        hash[WC_SHA3_512_DIGEST_SIZE];
    byte        hashRet[WC_SHA3_512_DIGEST_SIZE];

    /* Init stack variables. */
    XMEMSET(hash, 0, sizeof(hash));

    printf(testingFmt, "wc_Sha3_512_Final()");

    ret = wc_InitSha3_512(&sha3, HEAP_HINT, devId);
    if (ret != 0) {
        return ret;
    }

    ret= wc_Sha3_512_Update(&sha3, (byte*)msg, (word32)XSTRLEN(msg));
    if (ret == 0) {
        ret = wc_Sha3_512_Final(&sha3, hash);
        if (ret == 0 && XMEMCMP(expOut, hash, WC_SHA3_512_DIGEST_SIZE) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Sha3_512_Final(NULL, hash);
        if (ret == 0) {
            ret = wc_Sha3_384_Final(&sha3, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);
    if (ret == 0) {
        printf(testingFmt, "wc_Sha3_512_GetHash()");

        ret = wc_InitSha3_512(&sha3, HEAP_HINT, devId);
        if (ret != 0) {
            return ret;
        }
        /* Init stack variables. */
        XMEMSET(hash, 0, sizeof(hash));
        XMEMSET(hashRet, 0, sizeof(hashRet));

        ret= wc_Sha3_512_Update(&sha3, (byte*)msg, (word32)XSTRLEN(msg));
        if (ret == 0) {
            ret = wc_Sha3_512_GetHash(&sha3, hashRet);
        }
        if (ret == 0) {
            ret = wc_Sha3_512_Final(&sha3, hash);
            if (ret == 0 && XMEMCMP(hash, hashRet, WC_SHA3_512_DIGEST_SIZE) != 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
        if (ret == 0) {
            /* Test bad args. */
            ret = wc_Sha3_512_GetHash(NULL, hashRet);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_Sha3_512_GetHash(&sha3, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }

        printf(resultFmt, ret == 0 ? passed : failed);
    }

    wc_Sha3_512_Free(&sha3);
#endif
    return ret;
} /* END test_wc_Sha3_512_Final */


/*
 *  Testing wc_Sha3_224_Copy()
 */
static int test_wc_Sha3_224_Copy (void)
{
    int         ret = 0;
#if defined(WOLFSSL_SHA3) && !defined(WOLFSSL_NOSHA3_224)
    wc_Sha3        sha3, sha3Cpy;
    const char* msg = "Everyone gets Friday off.";
    word32      msglen = (word32)XSTRLEN(msg);
    byte        hash[WC_SHA3_224_DIGEST_SIZE];
    byte        hashCpy[WC_SHA3_224_DIGEST_SIZE];

    XMEMSET(hash, 0, sizeof(hash));
    XMEMSET(hashCpy, 0, sizeof(hashCpy));

    printf(testingFmt, "wc_Sha3_224_Copy()");

    ret = wc_InitSha3_224(&sha3, HEAP_HINT, devId);
    if (ret != 0) {
        return ret;
    }

    ret = wc_InitSha3_224(&sha3Cpy, HEAP_HINT, devId);
    if (ret != 0) {
    wc_Sha3_224_Free(&sha3);
    return ret;
    }

    ret = wc_Sha3_224_Update(&sha3, (byte*)msg, msglen);

    if (ret == 0) {
        ret = wc_Sha3_224_Copy(&sha3Cpy, &sha3);
        if (ret == 0) {
            ret = wc_Sha3_224_Final(&sha3, hash);
            if (ret == 0) {
                ret = wc_Sha3_224_Final(&sha3Cpy, hashCpy);
            }
        }
        if (ret == 0 && XMEMCMP(hash, hashCpy, sizeof(hash)) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Sha3_224_Copy(NULL, &sha3);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Sha3_224_Copy(&sha3Cpy, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_Sha3_224_Copy */



/*
 *  Testing wc_Sha3_256_Copy()
 */
static int test_wc_Sha3_256_Copy (void)
{
    int         ret = 0;
#if defined(WOLFSSL_SHA3) && !defined(WOLFSSL_NOSHA3_256)
    wc_Sha3        sha3, sha3Cpy;
    const char* msg = "Everyone gets Friday off.";
    word32      msglen = (word32)XSTRLEN(msg);
    byte        hash[WC_SHA3_256_DIGEST_SIZE];
    byte        hashCpy[WC_SHA3_256_DIGEST_SIZE];

    XMEMSET(hash, 0, sizeof(hash));
    XMEMSET(hashCpy, 0, sizeof(hashCpy));

    printf(testingFmt, "wc_Sha3_256_Copy()");

    ret = wc_InitSha3_256(&sha3, HEAP_HINT, devId);
    if (ret != 0) {
        return ret;
    }

    ret = wc_InitSha3_256(&sha3Cpy, HEAP_HINT, devId);
    if (ret != 0) {
    wc_Sha3_256_Free(&sha3);
    return ret;
    }

    ret = wc_Sha3_256_Update(&sha3, (byte*)msg, msglen);

    if (ret == 0) {
        ret = wc_Sha3_256_Copy(&sha3Cpy, &sha3);
        if (ret == 0) {
            ret = wc_Sha3_256_Final(&sha3, hash);
            if (ret == 0) {
                ret = wc_Sha3_256_Final(&sha3Cpy, hashCpy);
            }
        }
        if (ret == 0 && XMEMCMP(hash, hashCpy, sizeof(hash)) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Sha3_256_Copy(NULL, &sha3);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Sha3_256_Copy(&sha3Cpy, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_Sha3_256_Copy */



/*
 *  Testing wc_Sha3_384_Copy()
 */
static int test_wc_Sha3_384_Copy (void)
{
    int         ret = 0;
#if defined(WOLFSSL_SHA3) && !defined(WOLFSSL_NOSHA3_384)
    wc_Sha3        sha3, sha3Cpy;
    const char* msg = "Everyone gets Friday off.";
    word32      msglen = (word32)XSTRLEN(msg);
    byte        hash[WC_SHA3_384_DIGEST_SIZE];
    byte        hashCpy[WC_SHA3_384_DIGEST_SIZE];

    XMEMSET(hash, 0, sizeof(hash));
    XMEMSET(hashCpy, 0, sizeof(hashCpy));

    printf(testingFmt, "wc_Sha3_384_Copy()");

    ret = wc_InitSha3_384(&sha3, HEAP_HINT, devId);
    if (ret != 0) {
        return ret;
    }

    ret = wc_InitSha3_384(&sha3Cpy, HEAP_HINT, devId);
    if (ret != 0) {
    wc_Sha3_384_Free(&sha3);
    return ret;
    }

    ret = wc_Sha3_384_Update(&sha3, (byte*)msg, msglen);

    if (ret == 0) {
        ret = wc_Sha3_384_Copy(&sha3Cpy, &sha3);
        if (ret == 0) {
            ret = wc_Sha3_384_Final(&sha3, hash);
            if (ret == 0) {
                ret = wc_Sha3_384_Final(&sha3Cpy, hashCpy);
            }
        }
        if (ret == 0 && XMEMCMP(hash, hashCpy, sizeof(hash)) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Sha3_384_Copy(NULL, &sha3);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Sha3_384_Copy(&sha3Cpy, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_Sha3_384_Copy */


/*
 *  Testing wc_Sha3_512_Copy()
 */
static int test_wc_Sha3_512_Copy (void)
{
    int         ret = 0;

#if defined(WOLFSSL_SHA3) && !defined(WOLFSSL_NOSHA3_512)
    wc_Sha3        sha3, sha3Cpy;
    const char* msg = "Everyone gets Friday off.";
    word32      msglen = (word32)XSTRLEN(msg);
    byte        hash[WC_SHA3_512_DIGEST_SIZE];
    byte        hashCpy[WC_SHA3_512_DIGEST_SIZE];

    XMEMSET(hash, 0, sizeof(hash));
    XMEMSET(hashCpy, 0, sizeof(hashCpy));


    printf(testingFmt, "wc_Sha3_512_Copy()");

    ret = wc_InitSha3_512(&sha3, HEAP_HINT, devId);
    if (ret != 0) {
        return ret;
    }

    ret = wc_InitSha3_512(&sha3Cpy, HEAP_HINT, devId);
    if (ret != 0) {
    wc_Sha3_512_Free(&sha3);
    return ret;
    }

    ret = wc_Sha3_512_Update(&sha3, (byte*)msg, msglen);

    if (ret == 0) {
        ret = wc_Sha3_512_Copy(&sha3Cpy, &sha3);
        if (ret == 0) {
            ret = wc_Sha3_512_Final(&sha3, hash);
            if (ret == 0) {
                ret = wc_Sha3_512_Final(&sha3Cpy, hashCpy);
            }
        }
        if (ret == 0 && XMEMCMP(hash, hashCpy, sizeof(hash)) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Sha3_512_Copy(NULL, &sha3);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Sha3_512_Copy(&sha3Cpy, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_Sha3_512_Copy */




/*
 * unit test for wc_IdeaSetKey()
 */
static int test_wc_IdeaSetKey (void)
{
    int ret = 0;
#ifdef HAVE_IDEA

    Idea        idea;
    const byte  key[] =
    {
        0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37,
        0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37
    };
    int         flag = 0;

    printf(testingFmt, "wc_IdeaSetKey()");
    /*IV can be NULL, default value is 0*/
    ret = wc_IdeaSetKey(&idea, key, IDEA_KEY_SIZE, NULL, IDEA_ENCRYPTION);
    if (ret == 0) {
        ret = wc_IdeaSetKey(&idea, key, IDEA_KEY_SIZE, NULL, IDEA_DECRYPTION);
    }
    /* Bad args. */
    if (ret == 0) {
        ret = wc_IdeaSetKey(NULL, key, IDEA_KEY_SIZE, NULL, IDEA_ENCRYPTION);
        if (ret != BAD_FUNC_ARG) {
            flag = 1;
        }
        ret = wc_IdeaSetKey(&idea, NULL, IDEA_KEY_SIZE, NULL, IDEA_ENCRYPTION);
        if (ret != BAD_FUNC_ARG) {
            flag = 1;
        }
        ret = wc_IdeaSetKey(&idea, key, IDEA_KEY_SIZE - 1,
                                    NULL, IDEA_ENCRYPTION);
        if (ret != BAD_FUNC_ARG) {
            flag = 1;
        }
        ret = wc_IdeaSetKey(&idea, key, IDEA_KEY_SIZE, NULL, -1);
        if (ret != BAD_FUNC_ARG) {
            flag = 1;
        }
        if (flag == 1) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    } /* END Test Bad Args. */

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_IdeaSetKey */

/*
 * Unit test for wc_IdeaSetIV()
 */
static int test_wc_IdeaSetIV (void)
{
    int     ret = 0;
#ifdef HAVE_IDEA
    Idea    idea;

    printf(testingFmt, "wc_IdeaSetIV()");

    ret = wc_IdeaSetIV(&idea, NULL);
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_IdeaSetIV(NULL, NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);
#endif
    return ret;

} /* END test_wc_IdeaSetIV */

/*
 * Unit test for wc_IdeaCipher()
 */
static int test_wc_IdeaCipher (void)
{
    int     ret = 0;
#ifdef HAVE_IDEA
    Idea        idea;
    const byte  key[] =
    {
        0x2B, 0xD6, 0x45, 0x9F, 0x82, 0xC5, 0xB3, 0x00,
        0x95, 0x2C, 0x49, 0x10, 0x48, 0x81, 0xFF, 0x48
    };
    const byte  plain[] =
    {
        0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37
    };
    byte    enc[sizeof(plain)];
    byte    dec[sizeof(enc)];

    printf(testingFmt, "wc_IdeaCipher()");

    ret = wc_IdeaSetKey(&idea, key, IDEA_KEY_SIZE, NULL, IDEA_ENCRYPTION);
    if (ret == 0) {
        ret = wc_IdeaCipher(&idea, enc, plain);
        if (ret != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    if (ret == 0) {
        ret = wc_IdeaSetKey(&idea, key, IDEA_KEY_SIZE, NULL, IDEA_DECRYPTION);
        if (ret == 0) {
            ret = wc_IdeaCipher(&idea, dec, enc);
        }
        if (ret == 0) {
            ret = XMEMCMP(plain, dec, IDEA_BLOCK_SIZE);
        }
        if (ret != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Pass Bad Args. */
    if (ret == 0) {
        ret = wc_IdeaCipher(NULL, enc, dec);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_IdeaCipher(&idea, NULL, dec);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_IdeaCipher(&idea, enc, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
} /* END test_wc_IdeaCipher */

/*
 * Unit test for functions wc_IdeaCbcEncrypt and wc_IdeaCbcDecrypt
 */
static int test_wc_IdeaCbcEncyptDecrypt (void)
{
    int         ret = 0;
#ifdef HAVE_IDEA
    Idea        idea;
    const byte  key[] =
    {
        0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37,
        0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37, 0x37
    };
    const char* message = "International Data Encryption Algorithm";
    byte        msg_enc[40];
    byte        msg_dec[40];

    printf(testingFmt, "wc_IdeaCbcEncrypt()");

    ret = wc_IdeaSetKey(&idea, key, sizeof(key), NULL, IDEA_ENCRYPTION);
    if (ret == 0) {
        ret = wc_IdeaCbcEncrypt(&idea, msg_enc, (byte *)message,
                                        (word32)XSTRLEN(message) + 1);
    }
    if (ret == 0) {
        ret = wc_IdeaSetKey(&idea, key, sizeof(key), NULL, IDEA_DECRYPTION);
    }
    if (ret == 0) {
        ret = wc_IdeaCbcDecrypt(&idea, msg_dec, msg_enc,
                                            (word32)XSTRLEN(message) + 1);
        if (XMEMCMP(message, msg_dec, (word32)XSTRLEN(message))) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Test bad args. Enc */
    if (ret == 0) {
        ret = wc_IdeaCbcEncrypt(NULL, msg_enc, (byte*)message,
                                    (word32)XSTRLEN(message) + 1);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_IdeaCbcEncrypt(&idea, NULL, (byte*)message,
                                    (word32)XSTRLEN(message) + 1);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_IdeaCbcEncrypt(&idea, msg_enc, NULL,
                                    (word32)XSTRLEN(message) + 1);
        }
        if (ret != BAD_FUNC_ARG) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    } /* END test bad args ENC  */

    /* Test bad args DEC */
    if (ret == 0) {
        ret = wc_IdeaCbcDecrypt(NULL, msg_dec, msg_enc,
                                    (word32)XSTRLEN(message) + 1);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_IdeaCbcDecrypt(&idea, NULL, msg_enc,
                                    (word32)XSTRLEN(message) + 1);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_IdeaCbcDecrypt(&idea, msg_dec, NULL,
                                    (word32)XSTRLEN(message) + 1);
        }
        if (ret != BAD_FUNC_ARG) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_IdeaCbcEncryptDecrypt */


/*
 * Test function for wc_HmacSetKey
 */
static int test_wc_Md5HmacSetKey (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && !defined(NO_MD5)
    Hmac hmac;
    int ret,  times, itr;

    const char* keys[]=
    {
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b",
#ifndef HAVE_FIPS
        "Jefe", /* smaller than minumum FIPS key size */
#endif
        "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
    };

    times = sizeof(keys) / sizeof(char*);
    flag = 0;

    printf(testingFmt, "wc_HmacSetKey() with MD5");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    for (itr = 0; itr < times; itr++) {
        ret = wc_HmacSetKey(&hmac, WC_MD5, (byte*)keys[itr],
                            (word32)XSTRLEN(keys[itr]));
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Bad args. */
    if (!flag) {
        ret = wc_HmacSetKey(NULL, WC_MD5, (byte*)keys[0],
                                        (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, WC_MD5, NULL, (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, 20, (byte*)keys[0],
                                        (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, WC_MD5, (byte*)keys[0], 0);
#ifdef HAVE_FIPS
        if (ret != HMAC_MIN_KEYLEN_E) {
            flag = WOLFSSL_FATAL_ERROR;
        }
#else
        if (ret != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
#endif
    }

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Md5HmacSetKey */


/*
 * testing wc_HmacSetKey() on wc_Sha hash.
 */
static int test_wc_ShaHmacSetKey (void)
{

    int flag = 0;
#if !defined(NO_HMAC) && !defined(NO_SHA)
    Hmac hmac;
    int ret, times, itr;

    const char* keys[]=
    {
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                                                                "\x0b\x0b\x0b",
#ifndef HAVE_FIPS
        "Jefe", /* smaller than minumum FIPS key size */
#endif
        "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
                                                                "\xAA\xAA\xAA"
    };

    times = sizeof(keys) / sizeof(char*);
    flag = 0;

    printf(testingFmt, "wc_HmacSetKey() with SHA");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    for (itr = 0; itr < times; itr++) {
        ret = wc_HmacSetKey(&hmac, WC_SHA, (byte*)keys[itr],
                                        (word32)XSTRLEN(keys[itr]));
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Bad args. */
    if (!flag) {
        ret = wc_HmacSetKey(NULL, WC_SHA, (byte*)keys[0],
                                        (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, WC_SHA, NULL, (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, 20, (byte*)keys[0],
                                        (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, WC_SHA, (byte*)keys[0], 0);
#ifdef HAVE_FIPS
        if (ret != HMAC_MIN_KEYLEN_E) {
            flag = WOLFSSL_FATAL_ERROR;
        }
#else
        if (ret != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
#endif
    }

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_ShaHmacSetKey() */

/*
 * testing wc_HmacSetKey() on Sha224 hash.
 */
static int test_wc_Sha224HmacSetKey (void)
{

    int flag = 0;
#if !defined(NO_HMAC) && defined(WOLFSSL_SHA224)
    Hmac hmac;
    int ret, times, itr;

    const char* keys[]=
    {
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                                                                "\x0b\x0b\x0b",
#ifndef HAVE_FIPS
        "Jefe", /* smaller than minumum FIPS key size */
#endif
        "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
                                                                "\xAA\xAA\xAA"
    };

    times = sizeof(keys) / sizeof(char*);
    flag = 0;

    printf(testingFmt, "wc_HmacSetKey() with SHA 224");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    for (itr = 0; itr < times; itr++) {
        ret = wc_HmacSetKey(&hmac, WC_SHA224, (byte*)keys[itr],
                                            (word32)XSTRLEN(keys[itr]));
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Bad args. */
    if (!flag) {
        ret = wc_HmacSetKey(NULL, WC_SHA224, (byte*)keys[0],
                                            (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, WC_SHA224, NULL, (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, 20, (byte*)keys[0],
                                            (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, WC_SHA224, (byte*)keys[0], 0);
#ifdef HAVE_FIPS
        if (ret != HMAC_MIN_KEYLEN_E) {
            flag = WOLFSSL_FATAL_ERROR;
        }
#else
        if (ret != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
#endif
    }

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha224HmacSetKey() */

 /*
  * testing wc_HmacSetKey() on Sha256 hash
  */
static int test_wc_Sha256HmacSetKey (void)
{

    int flag = 0;
#if !defined(NO_HMAC) && !defined(NO_SHA256)
    Hmac hmac;
    int ret, times, itr;

    const char* keys[]=
    {
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                                                                "\x0b\x0b\x0b",
#ifndef HAVE_FIPS
        "Jefe", /* smaller than minumum FIPS key size */
#endif
        "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
                                                                "\xAA\xAA\xAA"
    };

    times = sizeof(keys) / sizeof(char*);
    flag = 0;

    printf(testingFmt, "wc_HmacSetKey() with SHA256");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    for (itr = 0; itr < times; itr++) {
        ret = wc_HmacSetKey(&hmac, WC_SHA256, (byte*)keys[itr],
                                            (word32)XSTRLEN(keys[itr]));
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Bad args. */
    if (!flag) {
        ret = wc_HmacSetKey(NULL, WC_SHA256, (byte*)keys[0],
                                            (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, WC_SHA256, NULL, (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, 20, (byte*)keys[0],
                                            (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, WC_SHA256, (byte*)keys[0], 0);
#ifdef HAVE_FIPS
        if (ret != HMAC_MIN_KEYLEN_E) {
            flag = WOLFSSL_FATAL_ERROR;
        }
#else
        if (ret != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
#endif
    }

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha256HmacSetKey() */


/*
 * testing wc_HmacSetKey on Sha384 hash.
 */
static int test_wc_Sha384HmacSetKey (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && defined(WOLFSSL_SHA384)
    Hmac hmac;
    int ret, times, itr;

    const char* keys[]=
    {
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                                                                "\x0b\x0b\x0b",
#ifndef HAVE_FIPS
        "Jefe", /* smaller than minumum FIPS key size */
#endif
        "\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA"
                                                                "\xAA\xAA\xAA"
    };

    times = sizeof(keys) / sizeof(char*);
    flag = 0;

    printf(testingFmt, "wc_HmacSetKey() with SHA384");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    for (itr = 0; itr < times; itr++) {
        ret = wc_HmacSetKey(&hmac, WC_SHA384, (byte*)keys[itr],
                                            (word32)XSTRLEN(keys[itr]));
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Bad args. */
    if (!flag) {
        ret = wc_HmacSetKey(NULL, WC_SHA384, (byte*)keys[0],
                                            (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, WC_SHA384, NULL, (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, 20, (byte*)keys[0],
                                            (word32)XSTRLEN(keys[0]));
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacSetKey(&hmac, WC_SHA384, (byte*)keys[0], 0);
#ifdef HAVE_FIPS
        if (ret != HMAC_MIN_KEYLEN_E) {
            flag = WOLFSSL_FATAL_ERROR;
        }
#else
        if (ret != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
#endif
    }

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha384HmacSetKey() */


/*
 * testing wc_HmacUpdate on wc_Md5 hash.
 */
static int test_wc_Md5HmacUpdate (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && !defined(NO_MD5)
    Hmac hmac;
    testVector a, b;
    int ret;
#ifdef HAVE_FIPS
    const char* keys =
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
#else
    const char* keys = "Jefe";
#endif

    a.input = "what do ya want for nothing?";
    a.inLen  = XSTRLEN(a.input);

    b.input = "Hi There";
    b.inLen = XSTRLEN(b.input);

    flag = 0;

    printf(testingFmt, "wc_HmacUpdate() with MD5");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_HmacSetKey(&hmac, WC_MD5, (byte*)keys, (word32)XSTRLEN(keys));
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }
    /* Update Hmac. */
    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Test bad args. */
    if (!flag) {
        ret = wc_HmacUpdate(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, NULL, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, 0);
        if (ret != 0) {
            flag = ret;
        }
    }

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Md5HmacUpdate */

/*
 * testing wc_HmacUpdate on SHA hash.
 */
static int test_wc_ShaHmacUpdate (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && !defined(NO_SHA)
    Hmac hmac;
    testVector a, b;
    int ret;
#ifdef HAVE_FIPS
    const char* keys =
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
#else
    const char* keys = "Jefe";
#endif

    a.input = "what do ya want for nothing?";
    a.inLen  = XSTRLEN(a.input);

    b.input = "Hi There";
    b.inLen = XSTRLEN(b.input);

    flag = 0;

    printf(testingFmt, "wc_HmacUpdate() with SHA");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_HmacSetKey(&hmac, WC_SHA, (byte*)keys, (word32)XSTRLEN(keys));
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }
    /* Update Hmac. */
    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Test bad args. */
    if (!flag) {
        ret = wc_HmacUpdate(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, NULL, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, 0);
        if (ret != 0) {
            flag = ret;
        }
    }

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_ShaHmacUpdate */

/*
 * testing wc_HmacUpdate on SHA224 hash.
 */
static int test_wc_Sha224HmacUpdate (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && defined(WOLFSSL_SHA224)
    Hmac hmac;
    testVector a, b;
    int ret;
#ifdef HAVE_FIPS
    const char* keys =
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
#else
    const char* keys = "Jefe";
#endif

    a.input = "what do ya want for nothing?";
    a.inLen  = XSTRLEN(a.input);

    b.input = "Hi There";
    b.inLen = XSTRLEN(b.input);

    flag = 0;

    printf(testingFmt, "wc_HmacUpdate() with SHA224");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_HmacSetKey(&hmac, WC_SHA224, (byte*)keys, (word32)XSTRLEN(keys));
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }
    /* Update Hmac. */
    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Test bad args. */
    if (!flag) {
        ret = wc_HmacUpdate(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, NULL, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, 0);
        if (ret != 0) {
            flag = ret;
        }
    }

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha224HmacUpdate */

/*
 * testing wc_HmacUpdate on SHA256 hash.
 */
static int test_wc_Sha256HmacUpdate (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && !defined(NO_SHA256)
    Hmac hmac;
    testVector a, b;
    int ret;
#ifdef HAVE_FIPS
    const char* keys =
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
#else
    const char* keys = "Jefe";
#endif

    a.input = "what do ya want for nothing?";
    a.inLen  = XSTRLEN(a.input);

    b.input = "Hi There";
    b.inLen = XSTRLEN(b.input);

    flag = 0;

    printf(testingFmt, "wc_HmacUpdate() with WC_SHA256");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_HmacSetKey(&hmac, WC_SHA256, (byte*)keys, (word32)XSTRLEN(keys));
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }
    /* Update Hmac. */
    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Test bad args. */
    if (!flag) {
        ret = wc_HmacUpdate(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, NULL, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, 0);
        if (ret != 0) {
            flag = ret;
        }
    }

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha256HmacUpdate */

/*
 * testing wc_HmacUpdate on SHA384  hash.
 */
static int test_wc_Sha384HmacUpdate (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && defined(WOLFSSL_SHA384)
    Hmac hmac;
    testVector a, b;
    int ret;
#ifdef HAVE_FIPS
    const char* keys =
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
#else
    const char* keys = "Jefe";
#endif

    a.input = "what do ya want for nothing?";
    a.inLen  = XSTRLEN(a.input);

    b.input = "Hi There";
    b.inLen = XSTRLEN(b.input);

    flag = 0;

    printf(testingFmt, "wc_HmacUpdate() with SHA384");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_HmacSetKey(&hmac, WC_SHA384, (byte*)keys, (word32)XSTRLEN(keys));
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)b.input, (word32)b.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }
    /* Update Hmac. */
    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    /* Test bad args. */
    if (!flag) {
        ret = wc_HmacUpdate(NULL, (byte*)a.input, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, NULL, (word32)a.inLen);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, 0);
        if (ret != 0) {
            flag = ret;
        }
    }

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha384HmacUpdate */

/*
 * Testing wc_HmacFinal() with MD5
 */

static int test_wc_Md5HmacFinal (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && !defined(NO_MD5)
    Hmac hmac;
    byte hash[WC_MD5_DIGEST_SIZE];
    testVector a;
    int ret;
    const char* key;

    key = "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
    a.input = "Hi There";
    a.output = "\x92\x94\x72\x7a\x36\x38\xbb\x1c\x13\xf4\x8e\xf8\x15\x8b\xfc"
               "\x9d";
    a.inLen  = XSTRLEN(a.input);
    a.outLen = XSTRLEN(a.output);

    flag = 0;

    printf(testingFmt, "wc_HmacFinal() with MD5");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_HmacSetKey(&hmac, WC_MD5, (byte*)key, (word32)XSTRLEN(key));
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_HmacFinal(&hmac, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_MD5_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Try bad parameters. */
    if (!flag) {
        ret = wc_HmacFinal(NULL, hash);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

#ifndef HAVE_FIPS
    if (!flag) {
        ret = wc_HmacFinal(&hmac, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }
#endif

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_Md5HmacFinal */

/*
 * Testing wc_HmacFinal() with SHA
 */
static int test_wc_ShaHmacFinal (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && !defined(NO_SHA)
    Hmac hmac;
    byte hash[WC_SHA_DIGEST_SIZE];
    testVector a;
    int ret;
    const char* key;

    key = "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                                                                "\x0b\x0b\x0b";
    a.input = "Hi There";
    a.output = "\xb6\x17\x31\x86\x55\x05\x72\x64\xe2\x8b\xc0\xb6\xfb\x37\x8c"
               "\x8e\xf1\x46\xbe\x00";
    a.inLen  = XSTRLEN(a.input);
    a.outLen = XSTRLEN(a.output);

    flag = 0;

    printf(testingFmt, "wc_HmacFinal() with SHA");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_HmacSetKey(&hmac, WC_SHA, (byte*)key, (word32)XSTRLEN(key));
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_HmacFinal(&hmac, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_SHA_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Try bad parameters. */
    if (!flag) {
        ret = wc_HmacFinal(NULL, hash);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

#ifndef HAVE_FIPS
    if (!flag) {
        ret = wc_HmacFinal(&hmac, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }
#endif

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;

} /* END test_wc_ShaHmacFinal */


/*
 * Testing wc_HmacFinal() with SHA224
 */
static int test_wc_Sha224HmacFinal (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && defined(WOLFSSL_SHA224)
    Hmac hmac;
    byte hash[WC_SHA224_DIGEST_SIZE];
    testVector a;
    int ret;
    const char* key;

    key = "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                                                                "\x0b\x0b\x0b";
    a.input = "Hi There";
    a.output = "\x89\x6f\xb1\x12\x8a\xbb\xdf\x19\x68\x32\x10\x7c\xd4\x9d\xf3"
               "\x3f\x47\xb4\xb1\x16\x99\x12\xba\x4f\x53\x68\x4b\x22";
    a.inLen  = XSTRLEN(a.input);
    a.outLen = XSTRLEN(a.output);

    flag = 0;

    printf(testingFmt, "wc_HmacFinal() with SHA224");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_HmacSetKey(&hmac, WC_SHA224, (byte*)key, (word32)XSTRLEN(key));
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_HmacFinal(&hmac, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_SHA224_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Try bad parameters. */
    if (!flag) {
        ret = wc_HmacFinal(NULL, hash);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

#ifndef HAVE_FIPS
    if (!flag) {
        ret = wc_HmacFinal(&hmac, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }
#endif

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha224HmacFinal */

/*
 * Testing wc_HmacFinal() with SHA256
 */
static int test_wc_Sha256HmacFinal (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && !defined(NO_SHA256)
    Hmac hmac;
    byte hash[WC_SHA256_DIGEST_SIZE];
    testVector a;
    int ret;
    const char* key;

    key = "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                                                                "\x0b\x0b\x0b";
    a.input = "Hi There";
    a.output = "\xb0\x34\x4c\x61\xd8\xdb\x38\x53\x5c\xa8\xaf\xce\xaf\x0b\xf1"
               "\x2b\x88\x1d\xc2\x00\xc9\x83\x3d\xa7\x26\xe9\x37\x6c\x2e\x32"
               "\xcf\xf7";
    a.inLen  = XSTRLEN(a.input);
    a.outLen = XSTRLEN(a.output);

    flag = 0;

    printf(testingFmt, "wc_HmacFinal() with WC_SHA256");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_HmacSetKey(&hmac, WC_SHA256, (byte*)key, (word32)XSTRLEN(key));
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_HmacFinal(&hmac, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_SHA256_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Try bad parameters. */
    if (!flag) {
        ret = wc_HmacFinal(NULL, hash);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

#ifndef HAVE_FIPS
    if (!flag) {
        ret = wc_HmacFinal(&hmac, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }
#endif

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha256HmacFinal */

/*
 * Testing wc_HmacFinal() with SHA384
 */
static int test_wc_Sha384HmacFinal (void)
{
    int flag = 0;
#if !defined(NO_HMAC) && defined(WOLFSSL_SHA384)
    Hmac hmac;
    byte hash[WC_SHA384_DIGEST_SIZE];
    testVector a;
    int ret;
    const char* key;

    key = "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                                                                "\x0b\x0b\x0b";
    a.input = "Hi There";
    a.output = "\xaf\xd0\x39\x44\xd8\x48\x95\x62\x6b\x08\x25\xf4\xab\x46\x90"
               "\x7f\x15\xf9\xda\xdb\xe4\x10\x1e\xc6\x82\xaa\x03\x4c\x7c\xeb"
               "\xc5\x9c\xfa\xea\x9e\xa9\x07\x6e\xde\x7f\x4a\xf1\x52\xe8\xb2"
               "\xfa\x9c\xb6";
    a.inLen  = XSTRLEN(a.input);
    a.outLen = XSTRLEN(a.output);

    flag = 0;

    printf(testingFmt, "wc_HmacFinal() with SHA384");

    ret = wc_HmacInit(&hmac, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_HmacSetKey(&hmac, WC_SHA384, (byte*)key, (word32)XSTRLEN(key));
    if (ret != 0) {
        flag = ret;
    }

    if (!flag) {
        ret = wc_HmacUpdate(&hmac, (byte*)a.input, (word32)a.inLen);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        ret = wc_HmacFinal(&hmac, hash);
        if (ret != 0) {
            flag = ret;
        }
    }

    if (!flag) {
        if (XMEMCMP(hash, a.output, WC_SHA384_DIGEST_SIZE) != 0) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Try bad parameters. */
    if (!flag) {
        ret = wc_HmacFinal(NULL, hash);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }
#ifndef HAVE_FIPS
    if (!flag) {
        ret = wc_HmacFinal(&hmac, NULL);
        if (ret != BAD_FUNC_ARG) {
            flag = WOLFSSL_FATAL_ERROR;
        }
    }
#endif

    wc_HmacFree(&hmac);

    printf(resultFmt, flag == 0 ? passed : failed);

#endif
    return flag;
} /* END test_wc_Sha384HmacFinal */



/*
 * Testing wc_InitCmac()
 */
static int test_wc_InitCmac (void)
{
    int         ret = 0;

#if defined(WOLFSSL_CMAC) && !defined(NO_AES)
    Cmac        cmac1, cmac2, cmac3;
    /* AES 128 key. */
    byte        key1[] = "\x01\x02\x03\x04\x05\x06\x07\x08"
                         "\x09\x10\x11\x12\x13\x14\x15\x16";
    /* AES 192 key. */
    byte        key2[] = "\x01\x02\x03\x04\x05\x06\x07\x08"
                         "\x09\x01\x11\x12\x13\x14\x15\x16"
                         "\x01\x02\x03\x04\x05\x06\x07\x08";

    /* AES 256 key. */
    byte        key3[] = "\x01\x02\x03\x04\x05\x06\x07\x08"
                         "\x09\x01\x11\x12\x13\x14\x15\x16"
                         "\x01\x02\x03\x04\x05\x06\x07\x08"
                         "\x09\x01\x11\x12\x13\x14\x15\x16";

    word32      key1Sz = (word32)sizeof(key1) - 1;
    word32      key2Sz = (word32)sizeof(key2) - 1;
    word32      key3Sz = (word32)sizeof(key3) - 1;
    int         type   = WC_CMAC_AES;

    printf(testingFmt, "wc_InitCmac()");

#ifdef WOLFSSL_AES_128
    ret = wc_InitCmac(&cmac1, key1, key1Sz, type, NULL);
#endif
#ifdef WOLFSSL_AES_192
    if (ret == 0)
        ret = wc_InitCmac(&cmac2, key2, key2Sz, type, NULL);
#endif
#ifdef WOLFSSL_AES_256
    if (ret == 0)
        ret = wc_InitCmac(&cmac3, key3, key3Sz, type, NULL);
#endif

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_InitCmac(NULL, key3, key3Sz, type, NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_InitCmac(&cmac3, NULL, key3Sz, type, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_InitCmac(&cmac3, key3, 0, type, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_InitCmac(&cmac3, key3, key3Sz, 0, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = SSL_FATAL_ERROR;
        }
    }

    (void)key1;
    (void)key1Sz;
    (void)key2;
    (void)key2Sz;
    (void)cmac1;
    (void)cmac2;

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_InitCmac */


/*
 * Testing wc_CmacUpdate()
 */
static int test_wc_CmacUpdate (void)
{
    int         ret = 0;

#if defined(WOLFSSL_CMAC) && !defined(NO_AES) && defined(WOLFSSL_AES_128)
    Cmac        cmac;
    byte        key[] =
    {
        0x64, 0x4c, 0xbf, 0x12, 0x85, 0x9d, 0xf0, 0x55,
        0x7e, 0xa9, 0x1f, 0x08, 0xe0, 0x51, 0xff, 0x27
    };
    byte        in[] = "\xe2\xb4\xb6\xf9\x48\x44\x02\x64"
                       "\x5c\x47\x80\x9e\xd5\xa8\x3a\x17"
                       "\xb3\x78\xcf\x85\x22\x41\x74\xd9"
                       "\xa0\x97\x39\x71\x62\xf1\x8e\x8f"
                       "\xf4";

    word32      inSz  = (word32)sizeof(in) - 1;
    word32      keySz = (word32)sizeof(key);
    int         type  = WC_CMAC_AES;

    ret = wc_InitCmac(&cmac, key, keySz, type, NULL);
    if (ret != 0) {
        return ret;
    }

    printf(testingFmt, "wc_CmacUpdate()");

    ret = wc_CmacUpdate(&cmac, in, inSz);

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_CmacUpdate(NULL, in, inSz);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_CmacUpdate(&cmac, NULL, 30);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = SSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_CmacUpdate */


/*
 * Testing wc_CmacFinal()
 */
static int test_wc_CmacFinal (void)
{
    int         ret = 0;

#if defined(WOLFSSL_CMAC) && !defined(NO_AES) && defined(WOLFSSL_AES_128)
    Cmac        cmac;
    byte        key[] =
    {
        0x64, 0x4c, 0xbf, 0x12, 0x85, 0x9d, 0xf0, 0x55,
        0x7e, 0xa9, 0x1f, 0x08, 0xe0, 0x51, 0xff, 0x27
    };
    byte        msg[] =
    {
        0xe2, 0xb4, 0xb6, 0xf9, 0x48, 0x44, 0x02, 0x64,
        0x5c, 0x47, 0x80, 0x9e, 0xd5, 0xa8, 0x3a, 0x17,
        0xb3, 0x78, 0xcf, 0x85, 0x22, 0x41, 0x74, 0xd9,
        0xa0, 0x97, 0x39, 0x71, 0x62, 0xf1, 0x8e, 0x8f,
        0xf4
    };
    /* Test vectors from CMACGenAES128.rsp from
     * http://csrc.nist.gov/groups/STM/cavp/block-cipher-modes.html#cmac
     * Per RFC4493 truncation of lsb is possible.
     */
    byte        expMac[] =
    {
        0x4e, 0x6e, 0xc5, 0x6f, 0xf9, 0x5d, 0x0e, 0xae,
        0x1c, 0xf8, 0x3e, 0xfc, 0xf4, 0x4b, 0xeb
    };
    byte        mac[AES_BLOCK_SIZE];
    word32      msgSz    = (word32)sizeof(msg);
    word32      keySz    = (word32)sizeof(key);
    word32      macSz    = sizeof(mac);
    word32      badMacSz = 17;
    int         expMacSz = sizeof(expMac);
    int         type     = WC_CMAC_AES;

    XMEMSET(mac, 0, macSz);

    ret = wc_InitCmac(&cmac, key, keySz, type, NULL);
    if (ret != 0) {
        return ret;
    }
    ret = wc_CmacUpdate(&cmac, msg, msgSz);

    printf(testingFmt, "wc_CmacFinal()");
    if (ret == 0) {
        ret = wc_CmacFinal(&cmac, mac, &macSz);
        if (ret == 0 && XMEMCMP(mac, expMac, expMacSz) != 0) {
            ret = SSL_FATAL_ERROR;
        }
        /* Pass in bad args. */
        if (ret == 0) {
            ret = wc_CmacFinal(NULL, mac, &macSz);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_CmacFinal(&cmac, NULL, &macSz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_CmacFinal(&cmac, mac, &badMacSz);
                if (ret == BUFFER_E) {
                    ret = 0;
                }
            } else if (ret == 0) {
                ret = SSL_FATAL_ERROR;
            }
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_CmacFinal */


/*
 * Testing wc_AesCmacGenerate() && wc_AesCmacVerify()
 */
static int test_wc_AesCmacGenerate (void)
{
    int         ret = 0;
#if defined(WOLFSSL_CMAC) && !defined(NO_AES) && defined(WOLFSSL_AES_128)
    Cmac        cmac;
    byte        key[] =
    {
        0x26, 0xef, 0x8b, 0x40, 0x34, 0x11, 0x7d, 0x9e,
        0xbe, 0xc0, 0xc7, 0xfc, 0x31, 0x08, 0x54, 0x69
    };
    byte        msg[]    = "\x18\x90\x49\xef\xfd\x7c\xf9\xc8"
                           "\xf3\x59\x65\xbc\xb0\x97\x8f\xd4";
    byte        expMac[] = "\x29\x5f\x2f\x71\xfc\x58\xe6\xf6"
                           "\x3d\x32\x65\x4c\x66\x23\xc5";
    byte        mac[AES_BLOCK_SIZE];
    word32      keySz    = sizeof(key);
    word32      macSz    = sizeof(mac);
    word32      msgSz    = sizeof(msg) - 1;
    word32      expMacSz = sizeof(expMac) - 1;
    int         type     = WC_CMAC_AES;

    XMEMSET(mac, 0, macSz);

    ret = wc_InitCmac(&cmac, key, keySz, type, NULL);
    if (ret != 0) {
        return ret;
    }

    ret = wc_CmacUpdate(&cmac, msg, msgSz);
    if (ret != 0) {
        return ret;
    }

    printf(testingFmt, "wc_AesCmacGenerate()");

    ret = wc_AesCmacGenerate(mac, &macSz, msg, msgSz, key, keySz);
    if (ret == 0 && XMEMCMP(mac, expMac, expMacSz) != 0) {
        ret = SSL_FATAL_ERROR;
    }
    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_AesCmacGenerate(NULL, &macSz, msg, msgSz, key, keySz);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_AesCmacGenerate(mac, &macSz, msg, msgSz, NULL, keySz);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_AesCmacGenerate(mac, &macSz, msg, msgSz, key, 0);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_AesCmacGenerate(mac, &macSz, NULL, msgSz, key, keySz);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = SSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);

    if (ret == 0) {
        printf(testingFmt, "wc_AesCmacVerify()");

        ret = wc_AesCmacVerify(mac, macSz, msg, msgSz, key, keySz);
        /* Test bad args. */
        if (ret == 0) {
            ret = wc_AesCmacVerify(NULL, macSz, msg, msgSz, key, keySz);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_AesCmacVerify(mac, 0, msg, msgSz, key, keySz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_AesCmacVerify(mac, macSz, msg, msgSz, NULL, keySz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_AesCmacVerify(mac, macSz, msg, msgSz, key, 0);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_AesCmacVerify(mac, macSz, NULL, msgSz, key, keySz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = SSL_FATAL_ERROR;
            }
        }

        printf(resultFmt, ret == 0 ? passed : failed);
    }

#endif
    return ret;

} /* END test_wc_AesCmacGenerate */




/*
 * unit test for wc_Des3_SetIV()
 */
static int test_wc_Des3_SetIV (void)
{
    int  ret = 0;
#ifndef NO_DES3
    Des3 des;
    const byte key[] =
    {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xde,0xba,0x98,0x76,0x54,0x32,0x10,
        0x89,0xab,0xcd,0xef,0x01,0x23,0x45,0x67
    };

    const byte iv[] =
    {
        0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0xef,
        0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
        0x11,0x21,0x31,0x41,0x51,0x61,0x71,0x81
    };

    printf(testingFmt, "wc_Des3_SetIV()");

    ret = wc_Des3Init(&des, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    /* DES_ENCRYPTION or DES_DECRYPTION */
    ret = wc_Des3_SetKey(&des, key, iv, DES_ENCRYPTION);

    if (ret == 0) {
        if (XMEMCMP(iv, des.reg, DES_BLOCK_SIZE) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

#ifndef HAVE_FIPS /* no sanity checks with FIPS wrapper */
    /* Test explicitly wc_Des3_SetIV()  */
    if (ret == 0) {
        ret = wc_Des3_SetIV(NULL, iv);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_SetIV(&des, NULL);
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
#endif

    wc_Des3Free(&des);

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_Des3_SetIV */

/*
 * unit test for wc_Des3_SetKey()
 */
static int test_wc_Des3_SetKey (void)
{
    int  ret = 0;
#ifndef NO_DES3
    Des3 des;
    const byte key[] =
    {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xde,0xba,0x98,0x76,0x54,0x32,0x10,
        0x89,0xab,0xcd,0xef,0x01,0x23,0x45,0x67
    };

    const byte iv[] =
    {
        0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0xef,
        0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
        0x11,0x21,0x31,0x41,0x51,0x61,0x71,0x81
    };

    printf(testingFmt, "wc_Des3_SetKey()");

    ret = wc_Des3Init(&des, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    /* DES_ENCRYPTION or DES_DECRYPTION */
    ret = wc_Des3_SetKey(&des, key, iv, DES_ENCRYPTION);
    if (ret == 0) {
        if (XMEMCMP(iv, des.reg, DES_BLOCK_SIZE) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Des3_SetKey(NULL, key, iv, DES_ENCRYPTION);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_SetKey(&des, NULL, iv, DES_ENCRYPTION);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_SetKey(&des, key, iv, -1);
        }
        if (ret == BAD_FUNC_ARG) {
            /* Default case. Should return 0. */
            ret = wc_Des3_SetKey(&des, key, NULL, DES_ENCRYPTION);
        }
    } /* END if ret != 0 */

    wc_Des3Free(&des);

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_Des3_SetKey */
 

/*
 * Test function for wc_Des3_CbcEncrypt and wc_Des3_CbcDecrypt
 */
static int test_wc_Des3_CbcEncryptDecrypt (void)
{
    int ret = 0;
#ifndef NO_DES3
    Des3 des;
    byte cipher[24];
    byte plain[24];

    const byte key[] =
    {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xde,0xba,0x98,0x76,0x54,0x32,0x10,
        0x89,0xab,0xcd,0xef,0x01,0x23,0x45,0x67
    };

    const byte iv[] =
    {
        0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0xef,
        0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
        0x11,0x21,0x31,0x41,0x51,0x61,0x71,0x81
    };

    const byte vector[] = { /* "Now is the time for all " w/o trailing 0 */
        0x4e,0x6f,0x77,0x20,0x69,0x73,0x20,0x74,
        0x68,0x65,0x20,0x74,0x69,0x6d,0x65,0x20,
        0x66,0x6f,0x72,0x20,0x61,0x6c,0x6c,0x20
    };

    printf(testingFmt, "wc_Des3_CbcEncrypt()");

    ret = wc_Des3Init(&des, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_Des3_SetKey(&des, key, iv, DES_ENCRYPTION);

    if (ret == 0) {
        ret = wc_Des3_CbcEncrypt(&des, cipher, vector, 24);

        if (ret == 0) {
            ret = wc_Des3_SetKey(&des, key, iv, DES_DECRYPTION);
        }
        if (ret == 0) {
            ret = wc_Des3_CbcDecrypt(&des, plain, cipher, 24);
        }
    }

    if (ret == 0) {
        if (XMEMCMP(plain, vector, 24) != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_Des3_CbcEncrypt(NULL, cipher, vector, 24);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_CbcEncrypt(&des, NULL, vector, 24);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_CbcEncrypt(&des, cipher, NULL, sizeof(vector));
        }
        if (ret != BAD_FUNC_ARG) {
            ret = WOLFSSL_FATAL_ERROR;;
        } else {
            ret = 0;
        }
    }

    if (ret == 0) {
        ret = wc_Des3_CbcDecrypt(NULL, plain, cipher, 24);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_CbcDecrypt(&des, NULL, cipher, 24);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_CbcDecrypt(&des, plain, NULL, 24);
        }
        if (ret != BAD_FUNC_ARG) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    wc_Des3Free(&des);

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END wc_Des3_CbcEncrypt */

/*
 *  Unit test for wc_Des3_CbcEncryptWithKey and wc_Des3_CbcDecryptWithKey
 */
static int test_wc_Des3_CbcEncryptDecryptWithKey (void)
{
    int ret = 0;
#ifndef NO_DES3

    word32 vectorSz, cipherSz;
    byte cipher[24];
    byte plain[24];

    byte vector[] = /* Now is the time for all w/o trailing 0 */
    {
        0x4e,0x6f,0x77,0x20,0x69,0x73,0x20,0x74,
        0x68,0x65,0x20,0x74,0x69,0x6d,0x65,0x20,
        0x66,0x6f,0x72,0x20,0x61,0x6c,0x6c,0x20
    };

    byte key[] =
    {
        0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
        0xfe,0xde,0xba,0x98,0x76,0x54,0x32,0x10,
        0x89,0xab,0xcd,0xef,0x01,0x23,0x45,0x67
    };

    byte iv[] =
    {
        0x12,0x34,0x56,0x78,0x90,0xab,0xcd,0xef,
        0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
        0x11,0x21,0x31,0x41,0x51,0x61,0x71,0x81
    };


    vectorSz = sizeof(byte) * 24;
    cipherSz = sizeof(byte) * 24;

    printf(testingFmt, "wc_Des3_CbcEncryptWithKey()");

    ret = wc_Des3_CbcEncryptWithKey(cipher, vector, vectorSz, key, iv);
    if (ret == 0) {
        ret = wc_Des3_CbcDecryptWithKey(plain, cipher, cipherSz, key, iv);
        if (ret == 0) {
            if (XMEMCMP(plain, vector, 24) !=  0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    /* pass in bad args. */
    if (ret == 0) {
        ret = wc_Des3_CbcEncryptWithKey(NULL, vector, vectorSz, key, iv);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_CbcEncryptWithKey(cipher, NULL, vectorSz, key, iv);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_CbcEncryptWithKey(cipher, vector, vectorSz, NULL, iv);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_CbcEncryptWithKey(cipher, vector, vectorSz,
                                                                    key, NULL);
        } else {
            /* Return code catch. */
            ret = WOLFSSL_FAILURE;
        }
    }

    if (ret == 0) {
        ret = wc_Des3_CbcDecryptWithKey(NULL, cipher, cipherSz, key, iv);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_CbcDecryptWithKey(plain, NULL, cipherSz, key, iv);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_CbcDecryptWithKey(plain, cipher, cipherSz, NULL, iv);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Des3_CbcDecryptWithKey(plain, cipher, cipherSz, key, NULL);
        } else {
            ret = WOLFSSL_FAILURE;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
} /* END test_wc_Des3_CbcEncryptDecryptWithKey */


/*
 * Testing wc_Chacha_SetKey() and wc_Chacha_SetIV()
 */
static int test_wc_Chacha_SetKey (void)
{
    int         ret = 0;
#ifdef HAVE_CHACHA
    ChaCha      ctx;
    const byte  key[] =
    {
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01
    };
    byte        cipher[128];

    printf(testingFmt, "wc_Chacha_SetKey()");

    ret = wc_Chacha_SetKey(&ctx, key, (word32)(sizeof(key)/sizeof(byte)));
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Chacha_SetKey(NULL, key, (word32)(sizeof(key)/sizeof(byte)));
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Chacha_SetKey(&ctx, key, 18);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);
    if (ret != 0) {
        return ret;
    }

    printf(testingFmt, "wc_Chacha_SetIV");
    ret = wc_Chacha_SetIV(&ctx, cipher, 0);
    if (ret == 0) {
    /* Test bad args. */
        ret = wc_Chacha_SetIV(NULL, cipher, 0);
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FAILURE;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
} /* END test_wc_Chacha_SetKey */

/*
 * unit test for wc_Poly1305SetKey()
 */
static int test_wc_Poly1305SetKey(void)
{
    int ret = 0;
    
#ifdef HAVE_POLY1305
    Poly1305      ctx;
    const byte  key[] =
    {
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01
    };

    printf(testingFmt, "wc_Poly1305_SetKey()");
    
    ret = wc_Poly1305SetKey(&ctx, key, (word32)(sizeof(key)/sizeof(byte))); 
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Poly1305SetKey(NULL, key, (word32)(sizeof(key)/sizeof(byte)));
        if(ret == BAD_FUNC_ARG) {
            ret = wc_Poly1305SetKey(&ctx, NULL, (word32)(sizeof(key)/sizeof(byte)));
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Poly1305SetKey(&ctx, key, 18);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);
    
#endif
    return ret;
} /* END test_wc_Poly1305_SetKey() */

/*
 * Testing wc_Chacha_Process()
 */
static int test_wc_Chacha_Process (void)
{
    int         ret = 0;
#ifdef HAVE_CHACHA
    ChaCha      enc, dec;
    byte        cipher[128];
    byte        plain[128];
    const byte  key[] =
    {
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
         0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01
    };
    const char* input = "Everybody gets Friday off.";
    word32      keySz = sizeof(key)/sizeof(byte);
    unsigned long int inlen = XSTRLEN(input);

    /*Initialize stack varialbes.*/
    XMEMSET(cipher, 0, 128);
    XMEMSET(plain, 0, 128);

    printf(testingFmt, "wc_Chacha_Process()");

    ret = wc_Chacha_SetKey(&enc, key, keySz);
    if (ret == 0) {
        ret = wc_Chacha_SetKey(&dec, key, keySz);
        if (ret == 0) {
            ret = wc_Chacha_SetIV(&enc, cipher, 0);
        }
        if (ret == 0) {
            ret = wc_Chacha_SetIV(&dec, cipher, 0);
        }
    }
    if (ret == 0) {
        ret = wc_Chacha_Process(&enc, cipher, (byte*)input, (word32)inlen);
        if (ret == 0) {
            ret = wc_Chacha_Process(&dec, plain, cipher, (word32)inlen);
            if (ret == 0) {
                ret = XMEMCMP(input, plain, (int)inlen);
            }
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Chacha_Process(NULL, cipher, (byte*)input, (word32)inlen);
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
} /* END test_wc_Chacha_Process */

/*
 * Testing wc_ChaCha20Poly1305_Encrypt() and wc_ChaCha20Poly1305_Decrypt()
 */
static int test_wc_ChaCha20Poly1305_aead (void)
{
    int   ret = 0;
#if defined(HAVE_CHACHA) && defined(HAVE_POLY1305)
    const byte  key[] = {
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f
    };

    const byte  plaintext[] = {
        0x4c, 0x61, 0x64, 0x69, 0x65, 0x73, 0x20, 0x61,
        0x6e, 0x64, 0x20, 0x47, 0x65, 0x6e, 0x74, 0x6c,
        0x65, 0x6d, 0x65, 0x6e, 0x20, 0x6f, 0x66, 0x20,
        0x74, 0x68, 0x65, 0x20, 0x63, 0x6c, 0x61, 0x73,
        0x73, 0x20, 0x6f, 0x66, 0x20, 0x27, 0x39, 0x39,
        0x3a, 0x20, 0x49, 0x66, 0x20, 0x49, 0x20, 0x63,
        0x6f, 0x75, 0x6c, 0x64, 0x20, 0x6f, 0x66, 0x66,
        0x65, 0x72, 0x20, 0x79, 0x6f, 0x75, 0x20, 0x6f,
        0x6e, 0x6c, 0x79, 0x20, 0x6f, 0x6e, 0x65, 0x20,
        0x74, 0x69, 0x70, 0x20, 0x66, 0x6f, 0x72, 0x20,
        0x74, 0x68, 0x65, 0x20, 0x66, 0x75, 0x74, 0x75,
        0x72, 0x65, 0x2c, 0x20, 0x73, 0x75, 0x6e, 0x73,
        0x63, 0x72, 0x65, 0x65, 0x6e, 0x20, 0x77, 0x6f,
        0x75, 0x6c, 0x64, 0x20, 0x62, 0x65, 0x20, 0x69,
        0x74, 0x2e
    };

    const byte  iv[] = {
        0x07, 0x00, 0x00, 0x00, 0x40, 0x41, 0x42, 0x43,
        0x44, 0x45, 0x46, 0x47
    };

    const byte  aad[] = { /* additional data */
        0x50, 0x51, 0x52, 0x53, 0xc0, 0xc1, 0xc2, 0xc3,
        0xc4, 0xc5, 0xc6, 0xc7
    };
    const byte  cipher[] = { /* expected output from operation */
        0xd3, 0x1a, 0x8d, 0x34, 0x64, 0x8e, 0x60, 0xdb,
        0x7b, 0x86, 0xaf, 0xbc, 0x53, 0xef, 0x7e, 0xc2,
        0xa4, 0xad, 0xed, 0x51, 0x29, 0x6e, 0x08, 0xfe,
        0xa9, 0xe2, 0xb5, 0xa7, 0x36, 0xee, 0x62, 0xd6,
        0x3d, 0xbe, 0xa4, 0x5e, 0x8c, 0xa9, 0x67, 0x12,
        0x82, 0xfa, 0xfb, 0x69, 0xda, 0x92, 0x72, 0x8b,
        0x1a, 0x71, 0xde, 0x0a, 0x9e, 0x06, 0x0b, 0x29,
        0x05, 0xd6, 0xa5, 0xb6, 0x7e, 0xcd, 0x3b, 0x36,
        0x92, 0xdd, 0xbd, 0x7f, 0x2d, 0x77, 0x8b, 0x8c,
        0x98, 0x03, 0xae, 0xe3, 0x28, 0x09, 0x1b, 0x58,
        0xfa, 0xb3, 0x24, 0xe4, 0xfa, 0xd6, 0x75, 0x94,
        0x55, 0x85, 0x80, 0x8b, 0x48, 0x31, 0xd7, 0xbc,
        0x3f, 0xf4, 0xde, 0xf0, 0x8e, 0x4b, 0x7a, 0x9d,
        0xe5, 0x76, 0xd2, 0x65, 0x86, 0xce, 0xc6, 0x4b,
        0x61, 0x16
    };
    const byte  authTag[] = { /* expected output from operation */
        0x1a, 0xe1, 0x0b, 0x59, 0x4f, 0x09, 0xe2, 0x6a,
        0x7e, 0x90, 0x2e, 0xcb, 0xd0, 0x60, 0x06, 0x91
    };
    byte        generatedCiphertext[272];
    byte        generatedPlaintext[272];
    byte        generatedAuthTag[CHACHA20_POLY1305_AEAD_AUTHTAG_SIZE];

    /* Initialize stack variables. */
    XMEMSET(generatedCiphertext, 0, 272);
    XMEMSET(generatedPlaintext, 0, 272);

    /* Test Encrypt */
    printf(testingFmt, "wc_ChaCha20Poly1305_Encrypt()");

    ret = wc_ChaCha20Poly1305_Encrypt(key, iv, aad, sizeof(aad), plaintext,
                sizeof(plaintext), generatedCiphertext, generatedAuthTag);
    if (ret == 0) {
        ret = XMEMCMP(generatedCiphertext, cipher, sizeof(cipher)/sizeof(byte));
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ChaCha20Poly1305_Encrypt(NULL, iv, aad, sizeof(aad), plaintext,
                    sizeof(plaintext), generatedCiphertext, generatedAuthTag);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ChaCha20Poly1305_Encrypt(key, NULL, aad, sizeof(aad),
                                        plaintext, sizeof(plaintext),
                                        generatedCiphertext, generatedAuthTag);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ChaCha20Poly1305_Encrypt(key, iv, aad, sizeof(aad), NULL,
                    sizeof(plaintext), generatedCiphertext, generatedAuthTag);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ChaCha20Poly1305_Encrypt(key, iv, aad, sizeof(aad),
                    plaintext, 0, generatedCiphertext, generatedAuthTag);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ChaCha20Poly1305_Encrypt(key, iv, aad, sizeof(aad),
                    plaintext, sizeof(plaintext), NULL, generatedAuthTag);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ChaCha20Poly1305_Encrypt(key, iv, aad, sizeof(aad),
                    plaintext, sizeof(plaintext), generatedCiphertext, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);
    if (ret != 0) {
        return ret;
    }

    printf(testingFmt, "wc_ChaCha20Poly1305_Decrypt()");
    ret = wc_ChaCha20Poly1305_Decrypt(key, iv, aad, sizeof(aad), cipher,
                            sizeof(cipher), authTag, generatedPlaintext);
    if (ret == 0) {
        ret = XMEMCMP(generatedPlaintext, plaintext,
                                        sizeof(plaintext)/sizeof(byte));
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ChaCha20Poly1305_Decrypt(NULL, iv, aad, sizeof(aad), cipher,
                                sizeof(cipher), authTag, generatedPlaintext);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ChaCha20Poly1305_Decrypt(key, NULL, aad, sizeof(aad),
                        cipher, sizeof(cipher), authTag, generatedPlaintext);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ChaCha20Poly1305_Decrypt(key, iv, aad, sizeof(aad), NULL,
                                sizeof(cipher), authTag, generatedPlaintext);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ChaCha20Poly1305_Decrypt(key, iv, aad, sizeof(aad), cipher,
                                    sizeof(cipher), NULL, generatedPlaintext);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ChaCha20Poly1305_Decrypt(key, iv, aad, sizeof(aad), cipher,
                                                sizeof(cipher), authTag, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ChaCha20Poly1305_Decrypt(key, iv, aad, sizeof(aad), cipher,
                                                0, authTag, generatedPlaintext);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test-wc_ChaCha20Poly1305_EncryptDecrypt */


/*
 * Testing function for wc_AesSetIV
 */
static int test_wc_AesSetIV (void)
{
    int     ret = 0;
#if !defined(NO_AES) && defined(WOLFSSL_AES_128)
    Aes     aes;
    byte    key16[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
    byte    iv1[]    = "1234567890abcdef";
    byte    iv2[]    = "0987654321fedcba";

    printf(testingFmt, "wc_AesSetIV()");

    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_AesSetKey(&aes, key16, (word32) sizeof(key16) / sizeof(byte),
                                                     iv1, AES_ENCRYPTION);
    if(ret == 0) {
        ret = wc_AesSetIV(&aes, iv2);
    }
    /* Test bad args. */
    if(ret == 0) {
        ret = wc_AesSetIV(NULL, iv1);
        if(ret == BAD_FUNC_ARG) {
            /* NULL iv should return 0. */
            ret = wc_AesSetIV(&aes, NULL);
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_AesFree(&aes);

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
} /* test_wc_AesSetIV */


/*
 * Testing function for wc_AesSetKey().
 */
static int test_wc_AesSetKey (void)
{
    int     ret = 0;
#ifndef NO_AES
    Aes     aes;
    byte    key16[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
#ifdef WOLFSSL_AES_192
    byte    key24[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
    };
#endif
#ifdef WOLFSSL_AES_256
    byte    key32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
#endif
    byte    badKey16[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65
    };
    byte    iv[]    = "1234567890abcdef";

    printf(testingFmt, "wc_AesSetKey()");

    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

#ifdef WOLFSSL_AES_128
    ret = wc_AesSetKey(&aes, key16, (word32) sizeof(key16) / sizeof(byte),
                                                        iv, AES_ENCRYPTION);
#endif
#ifdef WOLFSSL_AES_192
    if (ret == 0) {
        ret = wc_AesSetKey (&aes, key24, (word32) sizeof(key24) / sizeof(byte),
                                                           iv, AES_ENCRYPTION);
    }
#endif
#ifdef WOLFSSL_AES_256
    if (ret == 0) {
        ret = wc_AesSetKey (&aes, key32, (word32) sizeof(key32) / sizeof(byte),
                                                           iv, AES_ENCRYPTION);
    }
#endif

    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_AesSetKey (NULL, key16, (word32) sizeof(key16) / sizeof(byte),
                                                           iv, AES_ENCRYPTION);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_AesSetKey(&aes, badKey16,
                                    (word32) sizeof(badKey16) / sizeof(byte),
                                                         iv, AES_ENCRYPTION);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_AesFree(&aes);

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
} /* END test_wc_AesSetKey */



/*
 * test function for wc_AesCbcEncrypt(), wc_AesCbcDecrypt(),
 * and wc_AesCbcDecryptWithKey()
 */
static int test_wc_AesCbcEncryptDecrypt (void)
{
    int     ret = 0;
#if !defined(NO_AES) && defined(HAVE_AES_CBC) && defined(HAVE_AES_DECRYPT)&& \
    defined(WOLFSSL_AES_256)
    Aes     aes;
    byte    key32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
    byte    vector[] = /* Now is the time for all w/o trailing 0 */
    {
        0x4e,0x6f,0x77,0x20,0x69,0x73,0x20,0x74,
        0x68,0x65,0x20,0x74,0x69,0x6d,0x65,0x20,
        0x66,0x6f,0x72,0x20,0x61,0x6c,0x6c,0x20
    };
    byte    iv[]    = "1234567890abcdef";
    byte    enc[sizeof(vector)];
    byte    dec[sizeof(vector)];
    int     cbcE    =   WOLFSSL_FATAL_ERROR;
    int     cbcD    =   WOLFSSL_FATAL_ERROR;
    int     cbcDWK  =   WOLFSSL_FATAL_ERROR;
    byte    dec2[sizeof(vector)];

    /* Init stack variables. */
    XMEMSET(enc, 0, sizeof(enc));
    XMEMSET(dec, 0, sizeof(vector));
    XMEMSET(dec2, 0, sizeof(vector));

    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_AesSetKey(&aes, key32, AES_BLOCK_SIZE * 2, iv, AES_ENCRYPTION);
    if (ret == 0) {
        ret = wc_AesCbcEncrypt(&aes, enc, vector, sizeof(vector));
        if (ret == 0) {
            /* Re init for decrypt and set flag. */
            cbcE = 0;
            ret = wc_AesSetKey(&aes, key32, AES_BLOCK_SIZE * 2,
                                                    iv, AES_DECRYPTION);
        }
        if (ret == 0) {
            ret = wc_AesCbcDecrypt(&aes, dec, enc, AES_BLOCK_SIZE);
            if (ret != 0 || XMEMCMP(vector, dec, AES_BLOCK_SIZE) != 0) {
                ret = WOLFSSL_FATAL_ERROR;
            } else {
                /* Set flag. */
                cbcD = 0;
            }
        }
    }
    /* If encrypt succeeds but cbc decrypt fails, we can still test. */
    if (ret == 0 || (ret != 0 && cbcE == 0)) {
        ret = wc_AesCbcDecryptWithKey(dec2, enc, AES_BLOCK_SIZE,
                                     key32, sizeof(key32)/sizeof(byte), iv);
        if (ret == 0 || XMEMCMP(vector, dec2, AES_BLOCK_SIZE) == 0) {
            cbcDWK = 0;
        }
    }

    printf(testingFmt, "wc_AesCbcEncrypt()");
    /* Pass in bad args */
    if (cbcE == 0) {
        cbcE = wc_AesCbcEncrypt(NULL, enc, vector, sizeof(vector));
        if (cbcE == BAD_FUNC_ARG) {
            cbcE = wc_AesCbcEncrypt(&aes, NULL, vector, sizeof(vector));
        }
        if (cbcE == BAD_FUNC_ARG) {
            cbcE = wc_AesCbcEncrypt(&aes, enc, NULL, sizeof(vector));
        }
        if (cbcE == BAD_FUNC_ARG) {
            cbcE = 0;
        } else {
            cbcE = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, cbcE == 0 ? passed : failed);
    if (cbcE != 0) {
        wc_AesFree(&aes);
        return cbcE;
    }

    printf(testingFmt, "wc_AesCbcDecrypt()");
    if (cbcD == 0) {
        cbcD = wc_AesCbcDecrypt(NULL, dec, enc, AES_BLOCK_SIZE);
        if (cbcD == BAD_FUNC_ARG) {
            cbcD = wc_AesCbcDecrypt(&aes, NULL, enc, AES_BLOCK_SIZE);
        }
        if (cbcD == BAD_FUNC_ARG) {
            cbcD = wc_AesCbcDecrypt(&aes, dec, NULL, AES_BLOCK_SIZE);
        }
        if (cbcD == BAD_FUNC_ARG) {
            cbcD = wc_AesCbcDecrypt(&aes, dec, enc, AES_BLOCK_SIZE * 2 - 1);
        }
        if (cbcD == BAD_FUNC_ARG) {
            cbcD = 0;
        } else {
            cbcD = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, cbcD == 0 ? passed : failed);
    if (cbcD != 0) {
        wc_AesFree(&aes);
        return cbcD;
    }

    printf(testingFmt, "wc_AesCbcDecryptWithKey()");
    if (cbcDWK == 0) {
        cbcDWK = wc_AesCbcDecryptWithKey(NULL, enc, AES_BLOCK_SIZE,
                                     key32, sizeof(key32)/sizeof(byte), iv);
        if (cbcDWK == BAD_FUNC_ARG) {
            cbcDWK = wc_AesCbcDecryptWithKey(dec2, NULL, AES_BLOCK_SIZE,
                                     key32, sizeof(key32)/sizeof(byte), iv);
        }
        if (cbcDWK == BAD_FUNC_ARG) {
            cbcDWK = wc_AesCbcDecryptWithKey(dec2, enc, AES_BLOCK_SIZE,
                                     NULL, sizeof(key32)/sizeof(byte), iv);
        }
        if (cbcDWK == BAD_FUNC_ARG) {
            cbcDWK = wc_AesCbcDecryptWithKey(dec2, enc, AES_BLOCK_SIZE,
                                     key32, sizeof(key32)/sizeof(byte), NULL);
        }
        if (cbcDWK == BAD_FUNC_ARG) {
            cbcDWK = 0;
        } else {
            cbcDWK = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_AesFree(&aes);

    printf(resultFmt, cbcDWK == 0 ? passed : failed);

    if (cbcDWK != 0) {
        return cbcDWK;
    }
#endif
    return ret;
} /* END test_wc_AesCbcEncryptDecrypt */

/*
 * Testing wc_AesCtrEncrypt and wc_AesCtrDecrypt
 */
static int test_wc_AesCtrEncryptDecrypt (void)
{
    int     ret = 0;
#if !defined(NO_AES) && defined(WOLFSSL_AES_COUNTER) && defined(WOLFSSL_AES_256)
    Aes     aesEnc, aesDec;
    byte    key32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
    byte    vector[] = /* Now is the time for all w/o trailing 0 */
    {
        0x4e,0x6f,0x77,0x20,0x69,0x73,0x20,0x74,
        0x68,0x65,0x20,0x74,0x69,0x6d,0x65,0x20,
        0x66,0x6f,0x72,0x20,0x61,0x6c,0x6c,0x20
    };
    byte    iv[]    = "1234567890abcdef";
    byte    enc[AES_BLOCK_SIZE * 2];
    byte    dec[AES_BLOCK_SIZE * 2];

    /* Init stack variables. */
    XMEMSET(enc, 0, AES_BLOCK_SIZE * 2);
    XMEMSET(dec, 0, AES_BLOCK_SIZE * 2);

    printf(testingFmt, "wc_AesCtrEncrypt()");

    ret = wc_AesInit(&aesEnc, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;
    ret = wc_AesInit(&aesDec, NULL, INVALID_DEVID);
    if (ret != 0) {
        wc_AesFree(&aesEnc);
        return ret;
    }

    ret = wc_AesSetKey(&aesEnc, key32, AES_BLOCK_SIZE * 2,
                                                    iv, AES_ENCRYPTION);
    if (ret == 0) {
        ret = wc_AesCtrEncrypt(&aesEnc, enc, vector,
                                            sizeof(vector)/sizeof(byte));
        if (ret == 0) {
            /* Decrypt with wc_AesCtrEncrypt() */
            ret = wc_AesSetKey(&aesDec, key32, AES_BLOCK_SIZE * 2,
                                                    iv, AES_ENCRYPTION);
        }
        if (ret == 0) {
            ret = wc_AesCtrEncrypt(&aesDec, dec, enc, sizeof(enc)/sizeof(byte));
            if (ret != 0 || XMEMCMP(vector, dec, sizeof(vector))) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_AesCtrEncrypt(NULL, dec, enc, sizeof(enc)/sizeof(byte));
        if (ret == BAD_FUNC_ARG) {
            ret = wc_AesCtrEncrypt(&aesDec, NULL, enc, sizeof(enc)/sizeof(byte));
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_AesCtrEncrypt(&aesDec, dec, NULL, sizeof(enc)/sizeof(byte));
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_AesFree(&aesEnc);
    wc_AesFree(&aesDec);

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_AesCtrEncryptDecrypt */

/*
 * test function for wc_AesGcmSetKey()
 */
static int test_wc_AesGcmSetKey (void)
{
    int     ret = 0;
#if  !defined(NO_AES) && defined(HAVE_AESGCM)

    Aes     aes;
#ifdef WOLFSSL_AES_128
    byte    key16[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
#endif
#ifdef WOLFSSL_AES_192
    byte    key24[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
    };
#endif
#ifdef WOLFSSL_AES_256
    byte    key32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
#endif
    byte    badKey16[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65
    };
    byte    badKey24[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36
    };
    byte   badKey32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x37, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65
    };

    printf(testingFmt, "wc_AesGcmSetKey()");

    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

#ifdef WOLFSSL_AES_128
    ret = wc_AesGcmSetKey(&aes, key16, sizeof(key16)/sizeof(byte));
#endif
#ifdef WOLFSSL_AES_192
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, key24, sizeof(key24)/sizeof(byte));
    }
#endif
#ifdef WOLFSSL_AES_256
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, key32, sizeof(key32)/sizeof(byte));
    }
#endif

    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, badKey16, sizeof(badKey16)/sizeof(byte));
        if (ret == BAD_FUNC_ARG) {
            ret = wc_AesGcmSetKey(&aes, badKey24, sizeof(badKey24)/sizeof(byte));
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_AesGcmSetKey(&aes, badKey32, sizeof(badKey32)/sizeof(byte));
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_AesFree(&aes);

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
} /* END test_wc_AesGcmSetKey */

/*
 * test function for wc_AesGcmEncrypt and wc_AesGcmDecrypt
 */
static int test_wc_AesGcmEncryptDecrypt (void)
{
    int     ret = 0;
#if !defined(NO_AES) && defined(HAVE_AESGCM) && defined(WOLFSSL_AES_256)

    Aes     aes;
    byte    key32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
    byte    vector[] = /* Now is the time for all w/o trailing 0 */
    {
        0x4e,0x6f,0x77,0x20,0x69,0x73,0x20,0x74,
        0x68,0x65,0x20,0x74,0x69,0x6d,0x65,0x20,
        0x66,0x6f,0x72,0x20,0x61,0x6c,0x6c,0x20
    };
    const byte a[] =
    {
        0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
        0xfe, 0xed, 0xfa, 0xce, 0xde, 0xad, 0xbe, 0xef,
        0xab, 0xad, 0xda, 0xd2
    };
    byte    iv[]   = "1234567890a";
    byte    longIV[]  = "1234567890abcdefghij";
    byte    enc[sizeof(vector)];
    byte    resultT[AES_BLOCK_SIZE];
    byte    dec[sizeof(vector)];
    int     gcmD     =   WOLFSSL_FATAL_ERROR;
    int     gcmE     =   WOLFSSL_FATAL_ERROR;

    /* Init stack variables. */
    XMEMSET(enc, 0, sizeof(vector));
    XMEMSET(dec, 0, sizeof(vector));
    XMEMSET(resultT, 0, AES_BLOCK_SIZE);

    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_AesGcmSetKey(&aes, key32, sizeof(key32)/sizeof(byte));
    if (ret == 0) {
        gcmE = wc_AesGcmEncrypt(&aes, enc, vector, sizeof(vector),
                                        iv, sizeof(iv)/sizeof(byte), resultT,
                                        sizeof(resultT), a, sizeof(a));
    }
    if (gcmE == 0) { /* If encrypt fails, no decrypt. */
        gcmD = wc_AesGcmDecrypt(&aes, dec, enc, sizeof(vector),
                                        iv, sizeof(iv)/sizeof(byte), resultT,
                                        sizeof(resultT), a, sizeof(a));
        if(gcmD == 0 && (XMEMCMP(vector, dec, sizeof(vector)) !=  0)) {
            gcmD = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(testingFmt, "wc_AesGcmEncrypt()");
    /*Test bad args for wc_AesGcmEncrypt and wc_AesGcmDecrypt */
    if (gcmE == 0) {
        gcmE = wc_AesGcmEncrypt(NULL, enc, vector, sizeof(vector),
                        iv, sizeof(iv)/sizeof(byte), resultT, sizeof(resultT),
                        a, sizeof(a));
        if (gcmE == BAD_FUNC_ARG) {
            gcmE = wc_AesGcmEncrypt(&aes, enc, vector,
                    sizeof(vector), iv, sizeof(iv)/sizeof(byte),
                    resultT, sizeof(resultT) + 1, a, sizeof(a));
        }
        if (gcmE == BAD_FUNC_ARG) {
            gcmE = wc_AesGcmEncrypt(&aes, enc, vector,
                    sizeof(vector), iv, sizeof(iv)/sizeof(byte),
                    resultT, sizeof(resultT) - 5, a, sizeof(a));
        }
        if (gcmE == BAD_FUNC_ARG) {
            gcmE = wc_AesGcmEncrypt(&aes, enc, vector, sizeof(vector), longIV,
                            sizeof(longIV)/sizeof(byte), resultT, sizeof(resultT),
                            a, sizeof(a));
        }
        #ifdef HAVE_FIPS
            if (gcmE == BAD_FUNC_ARG) {
                gcmE = 0;
            } else {
                gcmE = WOLFSSL_FATAL_ERROR;
            }
        #endif
    } /* END wc_AesGcmEncrypt */

    printf(resultFmt, gcmE == 0 ? passed : failed);
    if (gcmE != 0) {
        wc_AesFree(&aes);
        return gcmE;
    }

    #ifdef HAVE_AES_DECRYPT
        printf(testingFmt, "wc_AesGcmDecrypt()");

        if (gcmD == 0) {
            gcmD = wc_AesGcmDecrypt(NULL, dec, enc, sizeof(enc)/sizeof(byte),
                                   iv, sizeof(iv)/sizeof(byte), resultT,
                                   sizeof(resultT), a, sizeof(a));
            if (gcmD == BAD_FUNC_ARG) {
                gcmD = wc_AesGcmDecrypt(&aes, NULL, enc, sizeof(enc)/sizeof(byte),
                                   iv, sizeof(iv)/sizeof(byte), resultT,
                                   sizeof(resultT), a, sizeof(a));
            }
            if (gcmD == BAD_FUNC_ARG) {
                gcmD = wc_AesGcmDecrypt(&aes, dec, NULL, sizeof(enc)/sizeof(byte),
                                   iv, sizeof(iv)/sizeof(byte), resultT,
                                   sizeof(resultT), a, sizeof(a));
            }
            if (gcmD == BAD_FUNC_ARG) {
                gcmD = wc_AesGcmDecrypt(&aes, dec, enc, sizeof(enc)/sizeof(byte),
                                   NULL, sizeof(iv)/sizeof(byte), resultT,
                                   sizeof(resultT), a, sizeof(a));
            }
            if (gcmD == BAD_FUNC_ARG) {
                gcmD = wc_AesGcmDecrypt(&aes, dec, enc, sizeof(enc)/sizeof(byte),
                                   iv, sizeof(iv)/sizeof(byte), NULL,
                                   sizeof(resultT), a, sizeof(a));
            }
            if (gcmD == BAD_FUNC_ARG) {
                gcmD = wc_AesGcmDecrypt(&aes, dec, enc, sizeof(enc)/sizeof(byte),
                                   iv, sizeof(iv)/sizeof(byte), resultT,
                                   sizeof(resultT) + 1, a, sizeof(a));
            }
            if (gcmD == BAD_FUNC_ARG) {
                gcmD = 0;
            } else {
                gcmD = WOLFSSL_FATAL_ERROR;
            }
        } /* END wc_AesGcmDecrypt */

        printf(resultFmt, gcmD == 0 ? passed : failed);
    #endif /* HAVE_AES_DECRYPT */

    wc_AesFree(&aes);
#endif

    return ret;

} /* END test_wc_AesGcmEncryptDecrypt */

/*
 * unit test for wc_GmacSetKey()
 */
static int test_wc_GmacSetKey (void)
{
    int     ret = 0;
#if !defined(NO_AES) && defined(HAVE_AESGCM)
    Gmac    gmac;
    byte    key16[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
#ifdef WOLFSSL_AES_192
    byte    key24[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
    };
#endif
#ifdef WOLFSSL_AES_256
    byte    key32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
#endif
    byte    badKey16[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x66
    };
    byte    badKey24[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
    };
    byte    badKey32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };

    printf(testingFmt, "wc_GmacSetKey()");

    ret = wc_AesInit(&gmac.aes, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

#ifdef WOLFSSL_AES_128
    ret = wc_GmacSetKey(&gmac, key16, sizeof(key16)/sizeof(byte));
#endif
#ifdef WOLFSSL_AES_192
    if (ret == 0) {
        ret = wc_GmacSetKey(&gmac, key24, sizeof(key24)/sizeof(byte));
    }
#endif
#ifdef WOLFSSL_AES_256
    if (ret == 0) {
        ret = wc_GmacSetKey(&gmac, key32, sizeof(key32)/sizeof(byte));
    }
#endif

    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_GmacSetKey(NULL, key16, sizeof(key16)/sizeof(byte));
        if (ret == BAD_FUNC_ARG) {
            ret = wc_GmacSetKey(&gmac, NULL, sizeof(key16)/sizeof(byte));
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_GmacSetKey(&gmac, badKey16, sizeof(badKey16)/sizeof(byte));
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_GmacSetKey(&gmac, badKey24, sizeof(badKey24)/sizeof(byte));
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_GmacSetKey(&gmac, badKey32, sizeof(badKey32)/sizeof(byte));
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_AesFree(&gmac.aes);

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_GmacSetKey */

/*
 * unit test for wc_GmacUpdate
 */
static int test_wc_GmacUpdate (void)
{
    int     ret = 0;
#if !defined(NO_AES) && defined(HAVE_AESGCM)
    Gmac    gmac;
#ifdef WOLFSSL_AES_128
    const byte key16[] =
    {
        0x89, 0xc9, 0x49, 0xe9, 0xc8, 0x04, 0xaf, 0x01,
        0x4d, 0x56, 0x04, 0xb3, 0x94, 0x59, 0xf2, 0xc8
    };
#endif
#ifdef WOLFSSL_AES_192
    byte    key24[] =
    {
        0x41, 0xc5, 0xda, 0x86, 0x67, 0xef, 0x72, 0x52,
        0x20, 0xff, 0xe3, 0x9a, 0xe0, 0xac, 0x59, 0x0a,
        0xc9, 0xfc, 0xa7, 0x29, 0xab, 0x60, 0xad, 0xa0
    };
#endif
#ifdef WOLFSSL_AES_256
   byte    key32[] =
    {
        0x78, 0xdc, 0x4e, 0x0a, 0xaf, 0x52, 0xd9, 0x35,
        0xc3, 0xc0, 0x1e, 0xea, 0x57, 0x42, 0x8f, 0x00,
        0xca, 0x1f, 0xd4, 0x75, 0xf5, 0xda, 0x86, 0xa4,
        0x9c, 0x8d, 0xd7, 0x3d, 0x68, 0xc8, 0xe2, 0x23
    };
#endif
#ifdef WOLFSSL_AES_128
    const byte authIn[] =
    {
        0x82, 0xad, 0xcd, 0x63, 0x8d, 0x3f, 0xa9, 0xd9,
        0xf3, 0xe8, 0x41, 0x00, 0xd6, 0x1e, 0x07, 0x77
    };
#endif
#ifdef WOLFSSL_AES_192
    const byte authIn2[] =
    {
       0x8b, 0x5c, 0x12, 0x4b, 0xef, 0x6e, 0x2f, 0x0f,
       0xe4, 0xd8, 0xc9, 0x5c, 0xd5, 0xfa, 0x4c, 0xf1
    };
#endif
    const byte authIn3[] =
    {
        0xb9, 0x6b, 0xaa, 0x8c, 0x1c, 0x75, 0xa6, 0x71,
        0xbf, 0xb2, 0xd0, 0x8d, 0x06, 0xbe, 0x5f, 0x36
    };
#ifdef WOLFSSL_AES_128
    const byte tag1[] = /* Known. */
    {
        0x88, 0xdb, 0x9d, 0x62, 0x17, 0x2e, 0xd0, 0x43,
        0xaa, 0x10, 0xf1, 0x6d, 0x22, 0x7d, 0xc4, 0x1b
    };
#endif
#ifdef WOLFSSL_AES_192
    const byte tag2[] = /* Known */
    {
        0x20, 0x4b, 0xdb, 0x1b, 0xd6, 0x21, 0x54, 0xbf,
        0x08, 0x92, 0x2a, 0xaa, 0x54, 0xee, 0xd7, 0x05
    };
#endif
    const byte tag3[] = /* Known */
    {
        0x3e, 0x5d, 0x48, 0x6a, 0xa2, 0xe3, 0x0b, 0x22,
        0xe0, 0x40, 0xb8, 0x57, 0x23, 0xa0, 0x6e, 0x76
    };
#ifdef WOLFSSL_AES_128
    const byte iv[] =
    {
        0xd1, 0xb1, 0x04, 0xc8, 0x15, 0xbf, 0x1e, 0x94,
        0xe2, 0x8c, 0x8f, 0x16
    };
#endif
#ifdef WOLFSSL_AES_192
    const byte iv2[] =
    {
        0x05, 0xad, 0x13, 0xa5, 0xe2, 0xc2, 0xab, 0x66,
        0x7e, 0x1a, 0x6f, 0xbc
    };
#endif
    const byte iv3[] =
    {
        0xd7, 0x9c, 0xf2, 0x2d, 0x50, 0x4c, 0xc7, 0x93,
        0xc3, 0xfb, 0x6c, 0x8a
    };
    byte    tagOut[16];
    byte    tagOut2[24];
    byte    tagOut3[32];

    /* Init stack varaibles. */
    XMEMSET(tagOut, 0, sizeof(tagOut));
    XMEMSET(tagOut2, 0, sizeof(tagOut2));
    XMEMSET(tagOut3, 0, sizeof(tagOut3));

    printf(testingFmt, "wc_GmacUpdate()");

    ret = wc_AesInit(&gmac.aes, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

#ifdef WOLFSSL_AES_128
    ret = wc_GmacSetKey(&gmac, key16, sizeof(key16));
    if (ret == 0) {
        ret = wc_GmacUpdate(&gmac, iv, sizeof(iv), authIn, sizeof(authIn),
                                                    tagOut, sizeof(tag1));
        if (ret == 0) {
            ret = XMEMCMP(tag1, tagOut, sizeof(tag1));
        }
    }
#endif

#ifdef WOLFSSL_AES_192
    if (ret == 0) {
        XMEMSET(&gmac, 0, sizeof(Gmac));
        ret = wc_GmacSetKey(&gmac, key24, sizeof(key24)/sizeof(byte));
    }
    if (ret == 0) {
        ret = wc_GmacUpdate(&gmac, iv2, sizeof(iv2), authIn2,
                            sizeof(authIn2), tagOut2, sizeof(tag2));
    }
    if (ret == 0) {
        ret = XMEMCMP(tagOut2, tag2, sizeof(tag2));
    }
#endif

#ifdef WOLFSSL_AES_256
    if (ret == 0) {
        XMEMSET(&gmac, 0, sizeof(Gmac));
        ret = wc_GmacSetKey(&gmac, key32, sizeof(key32)/sizeof(byte));
    }
    if (ret == 0) {
        ret = wc_GmacUpdate(&gmac, iv3, sizeof(iv3), authIn3,
                            sizeof(authIn3), tagOut3, sizeof(tag3));
    }
    if (ret == 0) {
        ret = XMEMCMP(tag3, tagOut3, sizeof(tag3));
    }
#endif

    /*Pass bad args. */
    if (ret == 0) {
        ret = wc_GmacUpdate(NULL, iv3, sizeof(iv3), authIn3,
                                sizeof(authIn3), tagOut3, sizeof(tag3));
        if (ret == BAD_FUNC_ARG) {
            ret = wc_GmacUpdate(&gmac, iv3, sizeof(iv3), authIn3,
                                sizeof(authIn3), tagOut3, sizeof(tag3) - 5);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_GmacUpdate(&gmac, iv3, sizeof(iv3), authIn3,
                                sizeof(authIn3), tagOut3, sizeof(tag3) + 1);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    wc_AesFree(&gmac.aes);

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_GmacUpdate */


/*
 * testing wc_CamelliaSetKey
 */
static int test_wc_CamelliaSetKey (void)
{
    int ret = 0;
#ifdef HAVE_CAMELLIA
    Camellia camellia;
    /*128-bit key*/
    static const byte key16[] =
    {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
    };
    /* 192-bit key */
    static const byte key24[] =
    {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
    };
    /* 256-bit key */
    static const byte key32[] =
    {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff
    };
    static const byte iv[] =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };

    printf(testingFmt, "wc_CamelliaSetKey()");

    ret = wc_CamelliaSetKey(&camellia, key16, (word32)sizeof(key16), iv);
    if (ret == 0) {
        ret = wc_CamelliaSetKey(&camellia, key16,
                                        (word32)sizeof(key16), NULL);
        if (ret == 0) {
            ret = wc_CamelliaSetKey(&camellia, key24,
                                        (word32)sizeof(key24), iv);
        }
        if (ret == 0) {
            ret = wc_CamelliaSetKey(&camellia, key24,
                                        (word32)sizeof(key24), NULL);
        }
        if (ret == 0) {
            ret = wc_CamelliaSetKey(&camellia, key32,
                                        (word32)sizeof(key32), iv);
        }
        if (ret == 0) {
            ret = wc_CamelliaSetKey(&camellia, key32,
                                        (word32)sizeof(key32), NULL);
        }
    }
    /* Bad args. */
    if (ret == 0) {
        ret = wc_CamelliaSetKey(NULL, key32, (word32)sizeof(key32), iv);
        if (ret != BAD_FUNC_ARG) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    } /* END bad args. */


#endif
    return ret;

} /* END test_wc_CammeliaSetKey */

/*
 * Testing wc_CamelliaSetIV()
 */
static int test_wc_CamelliaSetIV (void)
{
    int ret = 0;
#ifdef HAVE_CAMELLIA
    Camellia    camellia;
    static const byte iv[] =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };

    printf(testingFmt, "wc_CamelliaSetIV()");

    ret = wc_CamelliaSetIV(&camellia, iv);
    if (ret == 0) {
        ret = wc_CamelliaSetIV(&camellia, NULL);
    }
    /* Bad args. */
    if (ret == 0) {
        ret = wc_CamelliaSetIV(NULL, NULL);
        if (ret != BAD_FUNC_ARG) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
} /*END test_wc_CamelliaSetIV*/

/*
 * Test wc_CamelliaEncryptDirect and wc_CamelliaDecryptDirect
 */
static int test_wc_CamelliaEncryptDecryptDirect (void)
{
    int     ret = 0;
#ifdef HAVE_CAMELLIA
    Camellia camellia;
    static const byte key24[] =
    {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
    };
    static const byte iv[] =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    static const byte plainT[] =
    {
        0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96,
        0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17, 0x2A
    };
    byte    enc[sizeof(plainT)];
    byte    dec[sizeof(enc)];
    int     camE = WOLFSSL_FATAL_ERROR;
    int     camD = WOLFSSL_FATAL_ERROR;

    /*Init stack variables.*/
    XMEMSET(enc, 0, 16);
    XMEMSET(enc, 0, 16);

    ret = wc_CamelliaSetKey(&camellia, key24, (word32)sizeof(key24), iv);
    if (ret == 0) {
        ret = wc_CamelliaEncryptDirect(&camellia, enc, plainT);
        if (ret == 0) {
            ret = wc_CamelliaDecryptDirect(&camellia, dec, enc);
            if (XMEMCMP(plainT, dec, CAMELLIA_BLOCK_SIZE)) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    }
    printf(testingFmt, "wc_CamelliaEncryptDirect()");
    /* Pass bad args. */
    if (ret == 0) {
        camE = wc_CamelliaEncryptDirect(NULL, enc, plainT);
        if (camE == BAD_FUNC_ARG) {
            camE = wc_CamelliaEncryptDirect(&camellia, NULL, plainT);
        }
        if (camE == BAD_FUNC_ARG) {
            camE = wc_CamelliaEncryptDirect(&camellia, enc, NULL);
        }
        if (camE == BAD_FUNC_ARG) {
            camE = 0;
        } else {
            camE = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, camE == 0 ? passed : failed);
    if (camE != 0) {
        return camE;
    }

    printf(testingFmt, "wc_CamelliaDecryptDirect()");

    if (ret == 0) {
        camD = wc_CamelliaDecryptDirect(NULL, dec, enc);
        if (camD == BAD_FUNC_ARG) {
            camD = wc_CamelliaDecryptDirect(&camellia, NULL, enc);
        }
        if (camD == BAD_FUNC_ARG) {
            camD = wc_CamelliaDecryptDirect(&camellia, dec, NULL);
        }
        if (camD == BAD_FUNC_ARG) {
            camD = 0;
        } else {
            camD = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, camD == 0 ? passed : failed);
    if (camD != 0) {
        return camD;
    }

#endif
    return ret;

} /* END test-wc_CamelliaEncryptDecryptDirect */

/*
 * Testing wc_CamelliaCbcEncrypt and wc_CamelliaCbcDecrypt
 */
static int test_wc_CamelliaCbcEncryptDecrypt (void)
{
    int     ret = 0;
#ifdef HAVE_CAMELLIA
    Camellia camellia;
    static const byte key24[] =
    {
        0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
        0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10,
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
    };
    static const byte plainT[] =
    {
        0x6B, 0xC1, 0xBE, 0xE2, 0x2E, 0x40, 0x9F, 0x96,
        0xE9, 0x3D, 0x7E, 0x11, 0x73, 0x93, 0x17, 0x2A
    };
    byte    enc[CAMELLIA_BLOCK_SIZE];
    byte    dec[CAMELLIA_BLOCK_SIZE];
    int     camCbcE = WOLFSSL_FATAL_ERROR;
    int     camCbcD = WOLFSSL_FATAL_ERROR;

    /* Init stack variables. */
    XMEMSET(enc, 0, CAMELLIA_BLOCK_SIZE);
    XMEMSET(enc, 0, CAMELLIA_BLOCK_SIZE);

    ret = wc_CamelliaSetKey(&camellia, key24, (word32)sizeof(key24), NULL);
    if (ret == 0) {
        ret = wc_CamelliaCbcEncrypt(&camellia, enc, plainT, CAMELLIA_BLOCK_SIZE);
        if (ret != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    if (ret == 0) {
        ret = wc_CamelliaSetKey(&camellia, key24, (word32)sizeof(key24), NULL);
        if (ret == 0) {
            ret = wc_CamelliaCbcDecrypt(&camellia, dec, enc, CAMELLIA_BLOCK_SIZE);
            if (XMEMCMP(plainT, dec, CAMELLIA_BLOCK_SIZE)) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    printf(testingFmt, "wc_CamelliaCbcEncrypt");
    /* Pass in bad args. */
    if (ret == 0) {
        camCbcE = wc_CamelliaCbcEncrypt(NULL, enc, plainT, CAMELLIA_BLOCK_SIZE);
        if (camCbcE == BAD_FUNC_ARG) {
            camCbcE = wc_CamelliaCbcEncrypt(&camellia, NULL, plainT,
                                                    CAMELLIA_BLOCK_SIZE);
        }
        if (camCbcE == BAD_FUNC_ARG) {
            camCbcE = wc_CamelliaCbcEncrypt(&camellia, enc, NULL,
                                                    CAMELLIA_BLOCK_SIZE);
        }
        if (camCbcE == BAD_FUNC_ARG) {
            camCbcE = 0;
        } else {
            camCbcE = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, camCbcE == 0 ? passed : failed);
    if (camCbcE != 0) {
        return camCbcE;
    }

    printf(testingFmt, "wc_CamelliaCbcDecrypt()");

    if (ret == 0) {
        camCbcD = wc_CamelliaCbcDecrypt(NULL, dec, enc, CAMELLIA_BLOCK_SIZE);
        if (camCbcD == BAD_FUNC_ARG) {
            camCbcD = wc_CamelliaCbcDecrypt(&camellia, NULL, enc,
                                                    CAMELLIA_BLOCK_SIZE);
        }
        if (camCbcD == BAD_FUNC_ARG) {
            camCbcD = wc_CamelliaCbcDecrypt(&camellia, dec, NULL,
                                                    CAMELLIA_BLOCK_SIZE);
        }
        if (camCbcD == BAD_FUNC_ARG) {
            camCbcD = 0;
        } else {
            camCbcD = WOLFSSL_FATAL_ERROR;
        }
    } /* END bad args. */

    printf(resultFmt, camCbcD == 0 ? passed : failed);
    if (camCbcD != 0) {
        return camCbcD;
    }

#endif
    return ret;

} /* END test_wc_CamelliaCbcEncryptDecrypt */

/*
 * Testing wc_RabbitSetKey()
 */
static int test_wc_RabbitSetKey (void)
{
    int     ret = 0;
#ifndef NO_RABBIT
    Rabbit  rabbit;
    const char* key =  "\xAC\xC3\x51\xDC\xF1\x62\xFC\x3B"
                        "\xFE\x36\x3D\x2E\x29\x13\x28\x91";
    const char* iv =   "\x59\x7E\x26\xC1\x75\xF5\x73\xC3";

    printf(testingFmt, "wc_RabbitSetKey()");

    ret = wc_RabbitSetKey(&rabbit, (byte*)key, (byte*)iv);

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_RabbitSetKey(NULL, (byte*)key, (byte*)iv);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_RabbitSetKey(&rabbit, NULL, (byte*)iv);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_RabbitSetKey(&rabbit, (byte*)key, NULL);
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_RabbitSetKey */

/*
 * Test wc_RabbitProcess()
 */
static int test_wc_RabbitProcess (void)
{
    int     ret = 0;
#ifndef NO_RABBIT
    Rabbit  enc, dec;
    byte    cipher[25];
    byte    plain[25];
    const char* key     =  "\xAC\xC3\x51\xDC\xF1\x62\xFC\x3B"
                            "\xFE\x36\x3D\x2E\x29\x13\x28\x91";
    const char* iv      =   "\x59\x7E\x26\xC1\x75\xF5\x73\xC3";
    const char* input   =   "Everyone gets Friday off.";
    unsigned long int inlen = XSTRLEN(input);

    /* Initialize stack variables. */
    XMEMSET(cipher, 0, sizeof(cipher));
    XMEMSET(plain, 0, sizeof(plain));

    printf(testingFmt, "wc_RabbitProcess()");

    ret = wc_RabbitSetKey(&enc, (byte*)key, (byte*)iv);
    if (ret == 0) {
        ret = wc_RabbitSetKey(&dec, (byte*)key, (byte*)iv);
    }
    if (ret == 0) {
       ret = wc_RabbitProcess(&enc, cipher, (byte*)input, (word32)inlen);
    }
    if (ret == 0) {
        ret = wc_RabbitProcess(&dec, plain, cipher, (word32)inlen);
        if (ret != 0 || XMEMCMP(input, plain, inlen)) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_RabbitProcess(NULL, plain, cipher, (word32)inlen);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_RabbitProcess(&dec, NULL, cipher, (word32)inlen);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_RabbitProcess(&dec, plain, NULL, (word32)inlen);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_RabbitProcess */





/*
 * Testing wc_Arc4SetKey()
 */
static int test_wc_Arc4SetKey (void)
{
    int ret = 0;
#ifndef NO_RC4
    Arc4 arc;
    const char* key[] =
    {
        "\x01\x23\x45\x67\x89\xab\xcd\xef"
    };
    int keyLen = 8;

    printf(testingFmt, "wc_Arch4SetKey()");

    ret = wc_Arc4SetKey(&arc, (byte*)key, keyLen);
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_Arc4SetKey(NULL, (byte*)key, keyLen);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Arc4SetKey(&arc, NULL, keyLen);
        }
        if (ret == BAD_FUNC_ARG) {
            /* Exits normally if keyLen is incorrect. */
            ret = wc_Arc4SetKey(&arc, (byte*)key, 0);
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    } /* END test bad args. */

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_Arc4SetKey */

/*
 * Testing wc_Arc4Process for ENC/DEC.
 */
static int test_wc_Arc4Process (void)
{
    int ret = 0;
#ifndef NO_RC4
    Arc4 enc, dec;
    const char* key[] = {"\x01\x23\x45\x67\x89\xab\xcd\xef"};
    int keyLen = 8;
    const char* input[] = {"\x01\x23\x45\x67\x89\xab\xcd\xef"};
    byte cipher[8];
    byte plain[8];

    /* Init stack variables */
    XMEMSET(cipher, 0, sizeof(cipher));
    XMEMSET(plain, 0, sizeof(plain));

    /* Use for async. */
    ret = wc_Arc4Init(&enc, NULL, INVALID_DEVID);
    if (ret == 0) {
        ret = wc_Arc4Init(&dec, NULL, INVALID_DEVID);
    }

    printf(testingFmt, "wc_Arc4Process()");

    if (ret == 0) {
        ret = wc_Arc4SetKey(&enc, (byte*)key, keyLen);
    }
    if (ret == 0) {
        ret = wc_Arc4SetKey(&dec, (byte*)key, keyLen);
    }
    if (ret == 0) {
        ret = wc_Arc4Process(&enc, cipher, (byte*)input, keyLen);
    }
    if (ret == 0) {
        ret = wc_Arc4Process(&dec, plain, cipher, keyLen);
        if (ret != 0 || XMEMCMP(plain, input, keyLen)) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    /* Bad args. */
    if (ret == 0) {
        ret = wc_Arc4Process(NULL, plain, cipher, keyLen);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Arc4Process(&dec, NULL, cipher, keyLen);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Arc4Process(&dec, plain, NULL, keyLen);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_Arc4Free(&enc);
    wc_Arc4Free(&dec);

#endif
    return ret;

}/* END test_wc_Arc4Process */


/*
 * Testing wc_Init RsaKey()
 */
static int test_wc_InitRsaKey (void)
{
    int     ret = 0;
#ifndef NO_RSA
    RsaKey  key;

    printf(testingFmt, "wc_InitRsaKey()");

    ret = wc_InitRsaKey(&key, NULL);

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_InitRsaKey(NULL, NULL);
        #ifndef HAVE_USER_RSA
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else {
        #else
            if (ret == USER_CRYPTO_ERROR) {
                ret = 0;
            } else {
        #endif
            ret = WOLFSSL_FATAL_ERROR;
        }
    } /* end if */

    if (wc_FreeRsaKey(&key) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
} /* END test_wc_InitRsaKey */


/*
 * Testing wc_RsaPrivateKeyDecode()
 */
static int test_wc_RsaPrivateKeyDecode (void)
{
    int     ret = 0;
#if !defined(NO_RSA) && (defined(USE_CERT_BUFFERS_1024)\
        || defined(USE_CERT_BUFFERS_2048)) && !defined(HAVE_FIPS)
    RsaKey  key;
    byte*   tmp;
    word32  idx = 0;
    int     bytes = 0;

    printf(testingFmt, "wc_RsaPrivateKeyDecode()");

    tmp = (byte*)XMALLOC(FOURK_BUF, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    if (tmp == NULL) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    if (ret == 0) {
        ret = wc_InitRsaKey(&key, NULL);
    }
    if (ret == 0) {
        #ifdef USE_CERT_BUFFERS_1024
            XMEMCPY(tmp, client_key_der_1024, sizeof_client_key_der_1024);
            bytes = sizeof_client_key_der_1024;
        #else
            XMEMCPY(tmp, client_key_der_2048, sizeof_client_key_der_2048);
            bytes = sizeof_client_key_der_2048;
        #endif /* Use cert buffers. */

        ret = wc_RsaPrivateKeyDecode(tmp, &idx, &key, (word32)bytes);
    }
    #ifndef HAVE_USER_RSA
        /* Test bad args. */
        if (ret == 0) {
            ret = wc_RsaPrivateKeyDecode(NULL, &idx, &key, (word32)bytes);
            if (ret == ASN_PARSE_E) {
                ret = wc_RsaPrivateKeyDecode(tmp, NULL, &key, (word32)bytes);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaPrivateKeyDecode(tmp, &idx, NULL, (word32)bytes);
            }
            if (ret == ASN_PARSE_E) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #else
        /* Test bad args. User RSA. */
        if (ret == 0) {
            ret = wc_RsaPrivateKeyDecode(NULL, &idx, &key, (word32)bytes);
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaPrivateKeyDecode(tmp, NULL, &key, (word32)bytes);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaPrivateKeyDecode(tmp, &idx, NULL, (word32)bytes);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #endif

    if (tmp != NULL) {
        XFREE(tmp, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    }
    if (wc_FreeRsaKey(&key) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_RsaPrivateKeyDecode */

/*
 * Testing wc_RsaPublicKeyDecode()
 */
static int test_wc_RsaPublicKeyDecode (void)
{
    int     ret = 0;
#if !defined(NO_RSA) && (defined(USE_CERT_BUFFERS_1024)\
        || defined(USE_CERT_BUFFERS_2048)) && !defined(HAVE_FIPS)
    RsaKey  keyPub;
    byte*   tmp;
    word32  idx = 0;
    int     bytes = 0;

    tmp = (byte*)XMALLOC(GEN_BUF, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    if (tmp == NULL) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    if (ret == 0) {
        ret = wc_InitRsaKey(&keyPub, NULL);
    }
    if (ret == 0) {
        #ifdef USE_CERT_BUFFERS_1024
            XMEMCPY(tmp, client_keypub_der_1024, sizeof_client_keypub_der_1024);
            bytes = sizeof_client_keypub_der_1024;
        #else
            XMEMCPY(tmp, client_keypub_der_2048, sizeof_client_keypub_der_2048);
            bytes = sizeof_client_keypub_der_2048;
        #endif

        printf(testingFmt, "wc_RsaPublicKeyDecode()");

        ret = wc_RsaPublicKeyDecode(tmp, &idx, &keyPub, (word32)bytes);
    }
    #ifndef HAVE_USER_RSA
        /* Pass in bad args. */
        if (ret == 0) {
            ret = wc_RsaPublicKeyDecode(NULL, &idx, &keyPub, (word32)bytes);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaPublicKeyDecode(tmp, NULL, &keyPub, (word32)bytes);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaPublicKeyDecode(tmp, &idx, NULL, (word32)bytes);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #else
        /* Pass in bad args. */
        if (ret == 0) {
            ret = wc_RsaPublicKeyDecode(NULL, &idx, &keyPub, (word32)bytes);
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaPublicKeyDecode(tmp, NULL, &keyPub, (word32)bytes);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaPublicKeyDecode(tmp, &idx, NULL, (word32)bytes);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #endif

    if (tmp != NULL) {
        XFREE(tmp, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    }
    if (wc_FreeRsaKey(&keyPub) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);


#endif
    return ret;

}  /* END test_wc_RsaPublicKeyDecode */

/*
 * Testing wc_RsaPublicKeyDecodeRaw()
 */
static int test_wc_RsaPublicKeyDecodeRaw (void)
{
    int         ret = 0;
#if !defined(NO_RSA)
    RsaKey      key;
    const byte  n = 0x23;
    const byte  e = 0x03;
    int         nSz = sizeof(n);
    int         eSz = sizeof(e);

    printf(testingFmt, "wc_RsaPublicKeyDecodeRaw()");

    ret = wc_InitRsaKey(&key, NULL);
    if (ret == 0) {
        ret = wc_RsaPublicKeyDecodeRaw(&n, nSz, &e, eSz, &key);
    }
#ifndef HAVE_USER_RSA
    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_RsaPublicKeyDecodeRaw(NULL, nSz, &e, eSz, &key);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_RsaPublicKeyDecodeRaw(&n, nSz, NULL, eSz, &key);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_RsaPublicKeyDecodeRaw(&n, nSz, &e, eSz, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
#else
    /* Pass in bad args. User RSA. */
    if (ret == 0) {
        ret = wc_RsaPublicKeyDecodeRaw(NULL, nSz, &e, eSz, &key);
        if (ret == USER_CRYPTO_ERROR) {
            ret = wc_RsaPublicKeyDecodeRaw(&n, nSz, NULL, eSz, &key);
        }
        if (ret == USER_CRYPTO_ERROR) {
            ret = wc_RsaPublicKeyDecodeRaw(&n, nSz, &e, eSz, NULL);
        }
        if (ret == USER_CRYPTO_ERROR) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
#endif

    if (wc_FreeRsaKey(&key) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_RsaPublicKeyDecodeRaw */


#if (!defined(NO_RSA) || !defined(HAVE_FAST_RSA)) && (defined(WOLFSSL_KEY_GEN) || \
    defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL))
    /* In FIPS builds, wc_MakeRsaKey() will return an error if it cannot find
     * a probable prime in 5*(modLen/2) attempts. In non-FIPS builds, it keeps
     * trying until it gets a probable prime. */
    #ifdef WOLFSSL_FIPS
        static int MakeRsaKeyRetry(RsaKey* key, int size, long e, WC_RNG* rng)
        {
            int ret;

            for (;;) {
                ret = wc_MakeRsaKey(key, size, e, rng);
                if (ret != PRIME_GEN_E) break;
                printf("MakeRsaKey couldn't find prime; trying again.\n");
            }

            return ret;
        }
        #define MAKE_RSA_KEY(a, b, c, d) MakeRsaKeyRetry(a, b, c, d)
    #else
        #define MAKE_RSA_KEY(a, b, c, d) wc_MakeRsaKey(a, b, c, d)
    #endif
#endif


/*
 * Testing wc_MakeRsaKey()
 */
static int test_wc_MakeRsaKey (void)
{
    int     ret = 0;
#if !defined(NO_RSA) && defined(WOLFSSL_KEY_GEN)

    RsaKey  genKey;
    WC_RNG  rng;

    printf(testingFmt, "wc_MakeRsaKey()");

    ret = wc_InitRsaKey(&genKey, NULL);
    if (ret == 0) {
        ret = wc_InitRng(&rng);
        if (ret == 0) {
            ret = MAKE_RSA_KEY(&genKey, 1024, WC_RSA_EXPONENT, &rng);
            if (ret == 0 && wc_FreeRsaKey(&genKey) != 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    }
    #ifndef HAVE_USER_RSA
        /* Test bad args. */
        if (ret == 0) {
            ret = MAKE_RSA_KEY(NULL, 1024, WC_RSA_EXPONENT, &rng);
            if (ret == BAD_FUNC_ARG) {
                ret = MAKE_RSA_KEY(&genKey, 1024, WC_RSA_EXPONENT, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                /* e < 3 */
                ret = MAKE_RSA_KEY(&genKey, 1024, 2, &rng);
            }
            if (ret == BAD_FUNC_ARG) {
                /* e & 1 == 0 */
                ret = MAKE_RSA_KEY(&genKey, 1024, 6, &rng);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #else
        /* Test bad args. */
        if (ret == 0) {
            ret = MAKE_RSA_KEY(NULL, 1024, WC_RSA_EXPONENT, &rng);
            if (ret == USER_CRYPTO_ERROR) {
                ret = MAKE_RSA_KEY(&genKey, 1024, WC_RSA_EXPONENT, NULL);
            }
            if (ret == USER_CRYPTO_ERROR) {
                /* e < 3 */
                ret = MAKE_RSA_KEY(&genKey, 1024, 2, &rng);
            }
            if (ret == USER_CRYPTO_ERROR) {
                /* e & 1 == 0 */
                ret = MAKE_RSA_KEY(&genKey, 1024, 6, &rng);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #endif

    if (wc_FreeRng(&rng) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_MakeRsaKey */

/*
 * Test the bounds checking on the cipher text versus the key modulus.
 * 1. Make a new RSA key.
 * 2. Set c to 1.
 * 3. Decrypt c into k. (error)
 * 4. Copy the key modulus to c and sub 1 from the copy.
 * 5. Decrypt c into k. (error)
 * Valid bounds test cases are covered by all the other RSA tests.
 */
static int test_RsaDecryptBoundsCheck(void)
{
    int ret = 0;
#if !defined(NO_RSA) && defined(WC_RSA_NO_PADDING) && \
    (defined(USE_CERT_BUFFERS_1024) || defined(USE_CERT_BUFFERS_2048)) && \
    defined(WOLFSSL_PUBLIC_MP) && !defined(NO_RSA_BOUNDS_CHECK)
    RsaKey key;
    byte flatC[256];
    word32 flatCSz;
    byte out[256];
    word32 outSz = sizeof(out);
    WC_RNG rng;

    printf(testingFmt, "RSA decrypt bounds check");

    ret = wc_InitRng(&rng);

    if (ret == 0)
        ret = wc_InitRsaKey(&key, NULL);

    if (ret == 0) {
        const byte* derKey;
        word32 derKeySz;
        word32 idx = 0;

        #ifdef USE_CERT_BUFFERS_1024
            derKey = server_key_der_1024;
            derKeySz = (word32)sizeof_server_key_der_1024;
            flatCSz = 128;
        #else
            derKey = server_key_der_2048;
            derKeySz = (word32)sizeof_server_key_der_2048;
            flatCSz = 256;
        #endif

        ret = wc_RsaPrivateKeyDecode(derKey, &idx, &key, derKeySz);
    }

    if (ret == 0) {
        XMEMSET(flatC, 0, flatCSz);
        flatC[flatCSz-1] = 1;

        ret = wc_RsaDirect(flatC, flatCSz, out, &outSz, &key,
                           RSA_PRIVATE_DECRYPT, &rng);
    }
    if (ret == RSA_OUT_OF_RANGE_E) {
        mp_int c;
        mp_init_copy(&c, &key.n);
        mp_sub_d(&c, 1, &c);
        mp_to_unsigned_bin(&c, flatC);
        ret = wc_RsaDirect(flatC, sizeof(flatC), out, &outSz, &key,
                           RSA_PRIVATE_DECRYPT, NULL);
        mp_clear(&c);
    }
    if (ret == RSA_OUT_OF_RANGE_E)
        ret = 0;

    if (wc_FreeRsaKey(&key) || wc_FreeRng(&rng) || ret != 0)
        ret = WOLFSSL_FATAL_ERROR;

    printf(resultFmt, ret == 0 ? passed : failed);


#endif
    return ret;

} /* END test_wc_RsaDecryptBoundsCheck */

/*
 * Testing wc_SetKeyUsage()
 */
static int test_wc_SetKeyUsage (void)
{
    int     ret = 0;
#if !defined(NO_RSA) && defined(WOLFSSL_CERT_EXT) && !defined(HAVE_FIPS)
    Cert    myCert;

    ret = wc_InitCert(&myCert);

    printf(testingFmt, "wc_SetKeyUsage()");
    if (ret == 0) {
        ret = wc_SetKeyUsage(&myCert, "keyEncipherment,keyAgreement");
        if (ret == 0) {
            ret = wc_SetKeyUsage(&myCert, "digitalSignature,nonRepudiation");
        }
        if (ret == 0) {
            ret = wc_SetKeyUsage(&myCert, "contentCommitment,encipherOnly");
        }
        if (ret == 0) {
            ret = wc_SetKeyUsage(&myCert, "decipherOnly");
        }
        if (ret == 0) {
            ret = wc_SetKeyUsage(&myCert, "cRLSign,keyCertSign");
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_SetKeyUsage(NULL, "decipherOnly");
        if (ret == BAD_FUNC_ARG) {
            ret = wc_SetKeyUsage(&myCert, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_SetKeyUsage(&myCert, "");
        }
        if (ret == KEYUSAGE_E) {
            ret = wc_SetKeyUsage(&myCert, ",");
        }
        if (ret == KEYUSAGE_E) {
            ret = wc_SetKeyUsage(&myCert, "digitalSignature, cRLSign");
        }
        if (ret == KEYUSAGE_E) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END  test_wc_SetKeyUsage */

/*
 * Testing wc_RsaKeyToDer()
 */
static int test_wc_RsaKeyToDer (void)
{
    int     ret = 0;
#if !defined(NO_RSA) && defined(WOLFSSL_KEY_GEN)
    RsaKey  genKey;
    WC_RNG  rng;
    byte*   der;
    word32  derSz = 611;

    /* (2 x 128) + 2 (possible leading 00) + (5 x 64) + 5 (possible leading 00)
       + 3 (e) + 8 (ASN tag) + 10 (ASN length) + 4 seqSz + 3 version */
    der = (byte*)XMALLOC(derSz, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    if (der == NULL) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    /* Init structures. */
    if (ret == 0) {
        ret = wc_InitRsaKey(&genKey, NULL);
    }
        if (ret == 0) {
        ret = wc_InitRng(&rng);
    }
    /* Make key. */
    if (ret == 0) {
        ret = MAKE_RSA_KEY(&genKey, 1024, WC_RSA_EXPONENT, &rng);
        if (ret != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(testingFmt, "wc_RsaKeyToDer()");

    if (ret == 0) {
        ret = wc_RsaKeyToDer(&genKey, der, derSz);
        if (ret > 0) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    #ifndef HAVE_USER_RSA
        /* Pass bad args. */
        if (ret == 0) {
            ret = wc_RsaKeyToDer(NULL, der, FOURK_BUF);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaKeyToDer(&genKey, NULL, FOURK_BUF);
            }
            if (ret == BAD_FUNC_ARG) {
                /* Try Public Key. */
                genKey.type = 0;
                ret = wc_RsaKeyToDer(&genKey, der, FOURK_BUF);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #else
        /* Pass bad args. */
        if (ret == 0) {
            ret = wc_RsaKeyToDer(NULL, der, FOURK_BUF);
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaKeyToDer(&genKey, NULL, FOURK_BUF);
            }
            if (ret == USER_CRYPTO_ERROR) {
                /* Try Public Key. */
                genKey.type = 0;
                ret = wc_RsaKeyToDer(&genKey, der, FOURK_BUF);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #endif

    if (der != NULL) {
        XFREE(der, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    }
    if (wc_FreeRsaKey(&genKey) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    if (wc_FreeRng(&rng) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;
} /* END test_wc_RsaKeyToDer */

/*
 *  Testing wc_RsaKeyToPublicDer()
 */
static int test_wc_RsaKeyToPublicDer (void)
{ 
    int         ret = 0;
#if !defined(NO_RSA) && !defined(HAVE_FAST_RSA) && defined(WOLFSSL_KEY_GEN) &&\
     (defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL))
    RsaKey      key;
    WC_RNG      rng;
    byte*       der;
    word32      derLen = 162;

    der = (byte*)XMALLOC(derLen, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    if (der == NULL) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    if (ret == 0) {
        ret = wc_InitRsaKey(&key, NULL);
    }
    if (ret == 0) {
        ret = wc_InitRng(&rng);
    }
    if (ret == 0) {
        ret = MAKE_RSA_KEY(&key, 1024, WC_RSA_EXPONENT, &rng);
    }

    printf(testingFmt, "wc_RsaKeyToPublicDer()");

    if (ret == 0) {
        ret = wc_RsaKeyToPublicDer(&key, der, derLen);
        if (ret >= 0) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    #ifndef HAVE_USER_RSA
        /* Pass in bad args. */
        if (ret == 0) {
            ret = wc_RsaKeyToPublicDer(NULL, der, derLen);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaKeyToPublicDer(&key, NULL, derLen);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaKeyToPublicDer(&key, der, -1);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #else
        /* Pass in bad args. */
        if (ret == 0) {
            ret = wc_RsaKeyToPublicDer(NULL, der, derLen);
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaKeyToPublicDer(&key, NULL, derLen);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaKeyToPublicDer(&key, der, -1);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #endif

    if (der != NULL) {
        XFREE(der, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    }
    if (wc_FreeRsaKey(&key) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    if (wc_FreeRng(&rng) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_RsaKeyToPublicDer */

/*
 *  Testing wc_RsaPublicEncrypt() and wc_RsaPrivateDecrypt()
 */
static int test_wc_RsaPublicEncryptDecrypt (void)
{
    int     ret = 0;
#if !defined(NO_RSA) && defined(WOLFSSL_KEY_GEN)
    RsaKey  key;
    WC_RNG  rng;
    const char* inStr = "Everyone gets Friday off.";
    word32  cipherLen = 128;
    word32  plainLen = 25;
    word32  inLen = (word32)XSTRLEN(inStr);

    DECLARE_VAR_INIT(in, byte, inLen, inStr, NULL);
    DECLARE_VAR(plain, byte, plainLen, NULL);
    DECLARE_VAR(cipher, byte, cipherLen, NULL);

    ret = wc_InitRsaKey(&key, NULL);
    if (ret == 0) {
        ret = wc_InitRng(&rng);
    }
    if (ret == 0) {
        ret = MAKE_RSA_KEY(&key, 1024, WC_RSA_EXPONENT, &rng);
    }
    /* Encrypt. */
    printf(testingFmt, "wc_RsaPublicEncrypt()");

    if (ret == 0) {
        ret = wc_RsaPublicEncrypt(in, inLen, cipher, cipherLen, &key, &rng);
        if (ret >= 0) {
            cipherLen = ret;
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Pass bad args. */
   /* Tests PsaPublicEncryptEx() which, is tested by another fn. No need dup.*/
    printf(resultFmt, ret == 0 ? passed : failed);
    if (ret != 0) {
        return ret;
    }

    /* Decrypt */
    printf(testingFmt, "wc_RsaPrivateDecrypt()");
    #if defined(WC_RSA_BLINDING)
        /* Bind rng */
        if (ret == 0) {
            ret = wc_RsaSetRNG(&key, &rng);
        }
    #endif
    if (ret == 0) {
        ret = wc_RsaPrivateDecrypt(cipher, cipherLen, plain, plainLen, &key);
    }
    if (ret >= 0) {
        ret = XMEMCMP(plain, inStr, plainLen);
    }

    /* Pass in bad args. */
   /* Tests RsaPrivateDecryptEx() which, is tested by another fn. No need dup.*/

    FREE_VAR(in, NULL);
    FREE_VAR(plain, NULL);
    FREE_VAR(cipher, NULL);
    if (wc_FreeRsaKey(&key) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    if (wc_FreeRng(&rng) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_RsaPublicEncryptDecrypt */

/*
 * Testing wc_RsaPrivateDecrypt_ex() and wc_RsaPrivateDecryptInline_ex()
 */
static int test_wc_RsaPublicEncryptDecrypt_ex (void)
{
    int     ret = 0;
#if !defined(NO_RSA) && defined(WOLFSSL_KEY_GEN) && !defined(HAVE_FIPS)\
        && !defined(WC_NO_RSA_OAEP) && !defined(HAVE_USER_RSA)\
        && !defined(NO_SHA)
    RsaKey  key;
    WC_RNG  rng;
    const char* inStr = "Everyone gets Friday off.";
    word32  inLen = (word32)XSTRLEN(inStr);
    const word32 cipherSz = 128;
    const word32 plainSz = 25;
    byte*   res = NULL;
    int     idx = 0;

    DECLARE_VAR_INIT(in, byte, inLen, inStr, NULL);
    DECLARE_VAR(plain, byte, plainSz, NULL);
    DECLARE_VAR(cipher, byte, cipherSz, NULL);

    /* Initialize stack structures. */
    XMEMSET(&rng, 0, sizeof(rng));
    XMEMSET(&key, 0, sizeof(key));

    ret = wc_InitRsaKey_ex(&key, NULL, INVALID_DEVID);
    if (ret == 0) {
        ret = wc_InitRng(&rng);
    }
    if (ret == 0) {
        ret = MAKE_RSA_KEY(&key, 1024, WC_RSA_EXPONENT, &rng);
    }
    /* Encrypt */
    printf(testingFmt, "wc_RsaPublicEncrypt_ex()");
    if (ret == 0) {
        ret = wc_RsaPublicEncrypt_ex(in, inLen, cipher, cipherSz, &key, &rng,
                WC_RSA_OAEP_PAD, WC_HASH_TYPE_SHA, WC_MGF1SHA1, NULL, 0);
        if (ret >= 0) {
            idx = ret;
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    /*Pass bad args.*/
   /* Tests RsaPublicEncryptEx again. No need duplicate. */
    printf(resultFmt, ret == 0 ? passed : failed);
    if (ret != 0) {
        return ret;
    }

    /* Decrypt */
    printf(testingFmt, "wc_RsaPrivateDecrypt_ex()");
    #if defined(WC_RSA_BLINDING)
        if (ret == 0) {
            ret = wc_RsaSetRNG(&key, &rng);
        }
    #endif
    if (ret == 0) {
        ret = wc_RsaPrivateDecrypt_ex(cipher, (word32)idx,
                plain, plainSz, &key, WC_RSA_OAEP_PAD, WC_HASH_TYPE_SHA,
                WC_MGF1SHA1, NULL, 0);
    }
   if (ret >= 0) {
        if (!XMEMCMP(plain, inStr, plainSz)) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    /*Pass bad args.*/
   /* Tests RsaPrivateDecryptEx() again. No need duplicate. */
    printf(resultFmt, ret == 0 ? passed : failed);
    if (ret != 0) {
        return ret;
    }

    printf(testingFmt, "wc_RsaPrivateDecryptInline_ex()");
    if (ret == 0) {
        ret = wc_RsaPrivateDecryptInline_ex(cipher, (word32)idx,
                &res, &key, WC_RSA_OAEP_PAD, WC_HASH_TYPE_SHA,
                WC_MGF1SHA1, NULL, 0);

        if (ret >= 0) {
            if (!XMEMCMP(inStr, res, plainSz)) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    FREE_VAR(in, NULL);
    FREE_VAR(plain, NULL);
    FREE_VAR(cipher, NULL);
    if (wc_FreeRsaKey(&key) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    if (wc_FreeRng(&rng) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_RsaPublicEncryptDecrypt_ex */

/*
 * Tesing wc_RsaSSL_Sign() and wc_RsaSSL_Verify()
 */
static int test_wc_RsaSSL_SignVerify (void)
{
    int     ret = 0;
#if !defined(NO_RSA) && defined(WOLFSSL_KEY_GEN)
    RsaKey  key;
    WC_RNG  rng;
    const char* inStr = "Everyone gets Friday off.";
    const word32 outSz = 128;
    const word32 plainSz = 25;
    word32  inLen = (word32)XSTRLEN(inStr);
    word32  idx = 0;

    DECLARE_VAR_INIT(in, byte, inLen, inStr, NULL);
    DECLARE_VAR(out, byte, outSz, NULL);
    DECLARE_VAR(plain, byte, plainSz, NULL);

    ret = wc_InitRsaKey(&key, NULL);

    if (ret == 0) {
        ret = wc_InitRng(&rng);
    }

    if (ret == 0) {
        ret = MAKE_RSA_KEY(&key, 1024, WC_RSA_EXPONENT, &rng);
    }
    /* Sign. */
    printf(testingFmt, "wc_RsaSSL_Sign()");

    if (ret == 0) {
        ret = wc_RsaSSL_Sign(in, inLen, out, outSz, &key, &rng);
        if (ret == (int)outSz) {
            idx = ret;
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
#ifndef HAVE_USER_RSA
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_RsaSSL_Sign(NULL, inLen, out, outSz, &key, &rng);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_RsaSSL_Sign(in, 0, out, outSz, &key, &rng);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_RsaSSL_Sign(in, inLen, NULL, outSz, &key, &rng);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_RsaSSL_Sign(in, inLen, out, outSz, NULL, &rng);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
#else
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_RsaSSL_Sign(NULL, inLen, out, outSz, &key, &rng);
        if (ret == USER_CRYPTO_ERROR) {
            ret = wc_RsaSSL_Sign(in, 0, out, outSz, &key, &rng);
        }
        if (ret == USER_CRYPTO_ERROR) {
            ret = wc_RsaSSL_Sign(in, inLen, NULL, outSz, &key, &rng);
        }
        if (ret == USER_CRYPTO_ERROR) {
           ret = wc_RsaSSL_Sign(in, inLen, out, outSz, NULL, &rng);
        }
        if (ret == USER_CRYPTO_ERROR) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
#endif
    printf(resultFmt, ret == 0 ? passed : failed);
    if (ret != 0) {
        return ret;
    }

    /* Verify. */
    printf(testingFmt, "wc_RsaSSL_Verify()");

    if (ret == 0) {
        ret = wc_RsaSSL_Verify(out, idx, plain, plainSz, &key);
        if (ret == (int)inLen) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    #ifndef HAVE_USER_RSA
        /* Pass bad args. */
         if (ret == 0) {
                ret = wc_RsaSSL_Verify(NULL, idx, plain, plainSz, &key);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaSSL_Verify(out, 0, plain, plainSz, &key);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaSSL_Verify(out, idx, NULL, plainSz, &key);
            }
            if (ret == BAD_FUNC_ARG) {
               ret = wc_RsaSSL_Verify(out, idx, plain, plainSz, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #else
        /* Pass bad args. */
         if (ret == 0) {
                ret = wc_RsaSSL_Verify(NULL, idx, plain, plainSz, &key);
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaSSL_Verify(out, 0, plain, plainSz, &key);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaSSL_Verify(out, idx, NULL, plainSz, &key);
            }
            if (ret == USER_CRYPTO_ERROR) {
               ret = wc_RsaSSL_Verify(out, idx, plain, plainSz, NULL);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #endif

    FREE_VAR(in, NULL);
    FREE_VAR(out, NULL);
    FREE_VAR(plain, NULL);
    if (wc_FreeRsaKey(&key) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    if (wc_FreeRng(&rng) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_RsaSSL_SignVerify */

/*
 * Testing wc_RsaEncryptSize()
 */
static int test_wc_RsaEncryptSize (void)
{
    int     ret = 0;
#if !defined(NO_RSA) && defined(WOLFSSL_KEY_GEN)
    RsaKey  key;
    WC_RNG  rng;
    int     enc128 = 128;
    int     enc512 = 512;

    ret = wc_InitRsaKey(&key, NULL);

    if (ret == 0) {
        ret = wc_InitRng(&rng);
    }

    printf(testingFmt, "wc_RsaEncryptSize()");
    if (ret == 0) {
        ret = MAKE_RSA_KEY(&key, 1024, WC_RSA_EXPONENT, &rng);
        if (ret == 0) {
            ret = wc_RsaEncryptSize(&key);
        }
        if (ret == enc128) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    if (wc_FreeRsaKey(&key) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    } else {
        ret = 0;
    }

    if (ret == 0) {
        ret = MAKE_RSA_KEY(&key, FOURK_BUF, WC_RSA_EXPONENT, &rng);
        if (ret == 0) {
            ret = wc_RsaEncryptSize(&key);
        }
        if (ret == enc512) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Pass in bad arg. */
    if (ret == 0) {
        ret = wc_RsaEncryptSize(NULL);
        #ifndef HAVE_USER_RSA
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        #endif
    }

    if (wc_FreeRsaKey(&key) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    if (wc_FreeRng(&rng) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_RsaEncryptSize*/

/*
 * Testing wc_RsaFlattenPublicKey()
 */
static int test_wc_RsaFlattenPublicKey (void)
{
    int     ret = 0;
#if !defined(NO_RSA) && defined(WOLFSSL_KEY_GEN)
    RsaKey  key;
    WC_RNG  rng;
    byte    e[256];
    byte    n[256];
    word32  eSz = sizeof(e);
    word32  nSz = sizeof(n);

    ret = wc_InitRsaKey(&key, NULL);
    if (ret == 0) {
        ret = wc_InitRng(&rng);
    }

    if (ret == 0) {
        ret = MAKE_RSA_KEY(&key, 1024, WC_RSA_EXPONENT, &rng);
        if (ret >= 0) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(testingFmt, "wc_RsaFlattenPublicKey()");

    if (ret == 0) {
        ret = wc_RsaFlattenPublicKey(&key, e, &eSz, n, &nSz);
    }
    #ifndef HAVE_USER_RSA
        /* Pass bad args. */
        if (ret == 0) {
            ret = wc_RsaFlattenPublicKey(NULL, e, &eSz, n, &nSz);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaFlattenPublicKey(&key, NULL, &eSz, n, &nSz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaFlattenPublicKey(&key, e, NULL, n, &nSz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaFlattenPublicKey(&key, e, &eSz, NULL, &nSz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_RsaFlattenPublicKey(&key, e, &eSz, n, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #else
        /* Pass bad args. */
        if (ret == 0) {
            ret = wc_RsaFlattenPublicKey(NULL, e, &eSz, n, &nSz);
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaFlattenPublicKey(&key, NULL, &eSz, n, &nSz);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaFlattenPublicKey(&key, e, NULL, n, &nSz);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaFlattenPublicKey(&key, e, &eSz, NULL, &nSz);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_RsaFlattenPublicKey(&key, e, &eSz, n, NULL);
            }
            if (ret == USER_CRYPTO_ERROR) {
                ret = 0;
            } else {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #endif
    if (wc_FreeRsaKey(&key) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    if (wc_FreeRng(&rng) || ret != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_RsaFlattenPublicKey */



/*
 * unit test for wc_AesCcmSetKey
 */
static int test_wc_AesCcmSetKey (void)
{
    int ret = 0;
#ifdef HAVE_AESCCM
    Aes aes;
    const byte  key16[] =
    {
        0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
        0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf
    };
    const byte  key24[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
    };
    const byte  key32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };

    printf(testingFmt, "wc_AesCcmSetKey()");

    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

#ifdef WOLFSSL_AES_128
    ret = wc_AesCcmSetKey(&aes, key16, sizeof(key16));
#endif
#ifdef WOLFSSL_AES_192
    if (ret == 0) {
        ret = wc_AesCcmSetKey(&aes, key24, sizeof(key24));
    }
#endif
#ifdef WOLFSSL_AES_256
    if (ret == 0) {
        ret = wc_AesCcmSetKey(&aes, key32, sizeof(key32));
    }
#endif

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_AesCcmSetKey(&aes, key16, sizeof(key16) - 1);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_AesCcmSetKey(&aes, key24, sizeof(key24) - 1);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_AesCcmSetKey(&aes, key32, sizeof(key32) - 1);
        }
        if (ret != BAD_FUNC_ARG) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    wc_AesFree(&aes);

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_AesCcmSetKey */

/*
 * Unit test function for wc_AesCcmEncrypt and wc_AesCcmDecrypt
 */
static int test_wc_AesCcmEncryptDecrypt (void)
{
    int ret = 0;
#if defined(HAVE_AESCCM) && defined(WOLFSSL_AES_128)
    Aes aes;
    const byte  key16[] =
    {
        0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
        0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf
    };
    /* plaintext */
    const byte plainT[] =
    {
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
        0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e
    };
    /* nonce */
    const byte iv[] =
    {
        0x00, 0x00, 0x00, 0x03, 0x02, 0x01, 0x00, 0xa0,
        0xa1, 0xa2, 0xa3, 0xa4, 0xa5
    };
    const byte c[] =  /* cipher text. */
    {
        0x58, 0x8c, 0x97, 0x9a, 0x61, 0xc6, 0x63, 0xd2,
        0xf0, 0x66, 0xd0, 0xc2, 0xc0, 0xf9, 0x89, 0x80,
        0x6d, 0x5f, 0x6b, 0x61, 0xda, 0xc3, 0x84
    };
    const byte t[] =  /* Auth tag */
    {
        0x17, 0xe8, 0xd1, 0x2c, 0xfd, 0xf9, 0x26, 0xe0
    };
    const byte authIn[] =
    {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
    };
    byte cipherOut[sizeof(plainT)];
    byte authTag[sizeof(t)];
    int ccmE = WOLFSSL_FATAL_ERROR;
    #ifdef HAVE_AES_DECRYPT
        int ccmD = WOLFSSL_FATAL_ERROR;
        byte plainOut[sizeof(cipherOut)];
    #endif

    ret = wc_AesInit(&aes, NULL, INVALID_DEVID);
    if (ret != 0)
        return ret;

    ret = wc_AesCcmSetKey(&aes, key16, sizeof(key16));
    if (ret == 0) {
        ccmE = wc_AesCcmEncrypt(&aes, cipherOut, plainT, sizeof(cipherOut),
                                    iv, sizeof(iv), authTag, sizeof(authTag),
                                    authIn , sizeof(authIn));
        if ((XMEMCMP(cipherOut, c, sizeof(c)) && ccmE == 0) ||
                XMEMCMP(t, authTag, sizeof(t))) {
            ccmE = WOLFSSL_FATAL_ERROR;
            ret = WOLFSSL_FATAL_ERROR;
        }
        #ifdef HAVE_AES_DECRYPT
            if (ret == 0) {
                ccmD = wc_AesCcmDecrypt(&aes, plainOut, cipherOut,
                                        sizeof(plainOut), iv, sizeof(iv),
                                        authTag, sizeof(authTag),
                                        authIn, sizeof(authIn));
             }
            if (XMEMCMP(plainOut, plainT, sizeof(plainT)) && ccmD == 0) {
                ccmD = WOLFSSL_FATAL_ERROR;
            }
        #endif
    }

    printf(testingFmt, "wc_AesCcmEncrypt()");

    /* Pass in bad args. Encrypt*/
    if (ret == 0 && ccmE == 0) {
        ccmE = wc_AesCcmEncrypt(NULL, cipherOut, plainT, sizeof(cipherOut),
                                    iv, sizeof(iv), authTag, sizeof(authTag),
                                    authIn , sizeof(authIn));
        if (ccmE == BAD_FUNC_ARG) {
            ccmE = wc_AesCcmEncrypt(&aes, NULL, plainT, sizeof(cipherOut),
                                    iv, sizeof(iv), authTag, sizeof(authTag),
                                    authIn , sizeof(authIn));
        }
        if (ccmE == BAD_FUNC_ARG) {
            ccmE = wc_AesCcmEncrypt(&aes, cipherOut, NULL, sizeof(cipherOut),
                                    iv, sizeof(iv), authTag, sizeof(authTag),
                                    authIn , sizeof(authIn));
        }
        if (ccmE == BAD_FUNC_ARG) {
            ccmE = wc_AesCcmEncrypt(&aes, cipherOut, plainT, sizeof(cipherOut),
                                    NULL, sizeof(iv), authTag, sizeof(authTag),
                                    authIn , sizeof(authIn));
        }
        if (ccmE == BAD_FUNC_ARG) {
            ccmE = wc_AesCcmEncrypt(&aes, cipherOut, plainT, sizeof(cipherOut),
                                    iv, sizeof(iv), NULL, sizeof(authTag),
                                    authIn , sizeof(authIn));
        }
        if (ccmE == BAD_FUNC_ARG) {
            ccmE = wc_AesCcmEncrypt(&aes, cipherOut, plainT, sizeof(cipherOut),
                                    iv, sizeof(iv) + 1, authTag, sizeof(authTag),
                                    authIn , sizeof(authIn));
        }
        if (ccmE == BAD_FUNC_ARG) {
            ccmE = wc_AesCcmEncrypt(&aes, cipherOut, plainT, sizeof(cipherOut),
                                    iv, sizeof(iv) - 7, authTag, sizeof(authTag),
                                    authIn , sizeof(authIn));
        }

        if (ccmE != BAD_FUNC_ARG) {
            ccmE = WOLFSSL_FATAL_ERROR;
        } else {
            ccmE = 0;
        }
    } /* End Encrypt */

    printf(resultFmt, ccmE == 0 ? passed : failed);
    if (ccmE != 0) {
        wc_AesFree(&aes);
        return ccmE;
    }
    #ifdef HAVE_AES_DECRYPT
        printf(testingFmt, "wc_AesCcmDecrypt()");

        /* Pass in bad args. Decrypt*/
        if (ret == 0 && ccmD == 0) {
            ccmD = wc_AesCcmDecrypt(NULL, plainOut, cipherOut, sizeof(plainOut),
                                        iv, sizeof(iv), authTag, sizeof(authTag),
                                        authIn, sizeof(authIn));
            if (ccmD == BAD_FUNC_ARG) {
                ccmD = wc_AesCcmDecrypt(&aes, NULL, cipherOut, sizeof(plainOut),
                                        iv, sizeof(iv), authTag, sizeof(authTag),
                                        authIn, sizeof(authIn));
            }
            if (ccmD == BAD_FUNC_ARG) {
                ccmD = wc_AesCcmDecrypt(&aes, plainOut, NULL, sizeof(plainOut),
                                        iv, sizeof(iv), authTag, sizeof(authTag),
                                        authIn, sizeof(authIn));
            }
            if (ccmD == BAD_FUNC_ARG) {
                ccmD = wc_AesCcmDecrypt(&aes, plainOut, cipherOut,
                                        sizeof(plainOut), NULL, sizeof(iv),
                                        authTag, sizeof(authTag),
                                        authIn, sizeof(authIn));
            }
            if (ccmD == BAD_FUNC_ARG) {
                ccmD = wc_AesCcmDecrypt(&aes, plainOut, cipherOut,
                                        sizeof(plainOut), iv, sizeof(iv), NULL,
                                        sizeof(authTag), authIn, sizeof(authIn));
            }
            if (ccmD == BAD_FUNC_ARG) {
                ccmD = wc_AesCcmDecrypt(&aes, plainOut, cipherOut,
                                        sizeof(plainOut), iv, sizeof(iv) + 1,
                                        authTag, sizeof(authTag),
                                        authIn, sizeof(authIn));
            }
            if (ccmD == BAD_FUNC_ARG) {
                ccmD = wc_AesCcmDecrypt(&aes, plainOut, cipherOut,
                                        sizeof(plainOut), iv, sizeof(iv) - 7,
                                        authTag, sizeof(authTag),
                                        authIn, sizeof(authIn));
            }
            if (ccmD != BAD_FUNC_ARG) {
                ccmD = WOLFSSL_FATAL_ERROR;
            } else {
                ccmD = 0;
            }
        } /* END Decrypt */

        printf(resultFmt, ccmD == 0 ? passed : failed);
        if (ccmD != 0) {
            return ccmD;
        }
    #endif

    wc_AesFree(&aes);

#endif  /* HAVE_AESCCM */

    return ret;

} /* END test_wc_AesCcmEncryptDecrypt */



/*
 * Test wc_Hc128_SetKey()
 */
static int test_wc_Hc128_SetKey (void)
{
    int ret = 0;
#ifdef HAVE_HC128
    HC128 ctx;
    const char* key = "\x80\x00\x00\x00\x00\x00\x00\x00"
                      "\x00\x00\x00\x00\x00\x00\x00\x00";
    const char* iv =  "\x0D\x74\xDB\x42\xA9\x10\x77\xDE"
                      "\x45\xAC\x13\x7A\xE1\x48\xAF\x16";

    printf(testingFmt, "wc_Hc128_SetKey()");
        ret = wc_Hc128_SetKey(&ctx, (byte*)key, (byte*)iv);
        /* Test bad args. */
        if (ret == 0) {
            ret = wc_Hc128_SetKey(NULL, (byte*)key, (byte*)iv);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_Hc128_SetKey(&ctx, NULL, (byte*)iv);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_Hc128_SetKey(&ctx, (byte*)key, NULL);
            }
        }

    printf(resultFmt, ret == 0 ? passed : failed);


#endif
    return ret;

} /* END test_wc_Hc128_SetKey */

/*
 * Testing wc_Hc128_Process()
 */
static int test_wc_Hc128_Process (void)
{
    int ret = 0;
#ifdef HAVE_HC128
    HC128 enc;
    HC128 dec;
    const char* key =  "\x0F\x62\xB5\x08\x5B\xAE\x01\x54"
                       "\xA7\xFA\x4D\xA0\xF3\x46\x99\xEC";
    const char* input = "Encrypt Hc128, and then Decrypt.";
    size_t inlen = XSTRLEN(input) + 1; /* Add null terminator */
    byte cipher[inlen];
    byte plain[inlen];

    printf(testingFmt, "wc_Hc128_Process()");
    ret = wc_Hc128_SetKey(&enc, (byte*)key, NULL);
    if (ret == 0) {
        ret = wc_Hc128_SetKey(&dec, (byte*)key, NULL);
    }
    if (ret == 0) {
        ret = wc_Hc128_Process(&enc, cipher, (byte*)input, (word32)inlen);
        if (ret == 0) {
            ret = wc_Hc128_Process(&dec, plain, cipher, (word32)inlen);
        }
    }

    /* Bad args. */
    if (ret == 0) {
        ret = wc_Hc128_Process(NULL, plain, cipher, (word32)inlen);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Hc128_Process(&dec, NULL, cipher, (word32)inlen);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Hc128_Process(&dec, plain, NULL, (word32)inlen);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

   #endif
    return ret;

} /* END test_wc_Hc128_Process */


/*
 * Testing wc_InitDsaKey()
 */
static int test_wc_InitDsaKey (void)
{
    int     ret = 0;

#ifndef NO_DSA
    DsaKey  key;

    printf(testingFmt, "wc_InitDsaKey()");

    ret = wc_InitDsaKey(&key);

    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_InitDsaKey(NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_FreeDsaKey(&key);

#endif
    return ret;

} /* END test_wc_InitDsaKey */

/*
 * Testing wc_DsaSign() and wc_DsaVerify()
 */
static int test_wc_DsaSignVerify (void)
{
    int     ret = 0;
#if !defined(NO_DSA)
    DsaKey  key;
    WC_RNG  rng;
    wc_Sha  sha;
    byte    signature[DSA_SIG_SIZE];
    byte    hash[WC_SHA_DIGEST_SIZE];
    word32  idx = 0;
    word32  bytes;
    int      answer;
#ifdef USE_CERT_BUFFERS_1024
    byte    tmp[ONEK_BUF];
    XMEMSET(tmp, 0, sizeof(tmp));
    XMEMCPY(tmp, dsa_key_der_1024, sizeof_dsa_key_der_1024);
    bytes = sizeof_dsa_key_der_1024;
#elif defined(USE_CERT_BUFFERS_2048)
    byte    tmp[TWOK_BUF];
    XMEMSET(tmp, 0, sizeof(tmp));
    XMEMCPY(tmp, dsa_key_der_2048, sizeof_dsa_key_der_2048);
    bytes = sizeof_dsa_key_der_2048;
#else
    byte    tmp[TWOK_BUF];
    XMEMSET(tmp, 0, sizeof(tmp));
    FILE* fp = fopen("./certs/dsa2048.der", "rb");
    if (!fp) {
        return WOLFSSL_BAD_FILE;
    }
    bytes = (word32) fread(tmp, 1, sizeof(tmp), fp);
    fclose(fp);
#endif /* END USE_CERT_BUFFERS_1024 */

    ret = wc_InitSha(&sha);
    if (ret == 0) {
        ret = wc_ShaUpdate(&sha, tmp, bytes);
        if (ret == 0) {
            ret = wc_ShaFinal(&sha, hash);
        }
        if (ret == 0) {
            ret = wc_InitDsaKey(&key);
        }
        if (ret == 0) {
            ret = wc_DsaPrivateKeyDecode(tmp, &idx, &key, bytes);
        }
        if (ret == 0) {
            ret = wc_InitRng(&rng);
        }
    }

    printf(testingFmt, "wc_DsaSign()");
    /* Sign. */
    if (ret == 0) {
        ret = wc_DsaSign(hash, signature, &key, &rng);
    }

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_DsaSign(NULL, signature, &key, &rng);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_DsaSign(hash, NULL, &key, &rng);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_DsaSign(hash, signature, NULL, &rng);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_DsaSign(hash, signature, &key, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (ret != 0) {
        return ret;
    }

    /* Verify. */
    printf(testingFmt, "wc_DsaVerify()");

    ret = wc_DsaVerify(hash, signature, &key, &answer);
    if (ret != 0 || answer != 1) {
        ret = WOLFSSL_FATAL_ERROR;
    } else {
        ret = 0;
    }

    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_DsaVerify(NULL, signature, &key, &answer);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_DsaVerify(hash, NULL, &key, &answer);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_DsaVerify(hash, signature, NULL, &answer);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_DsaVerify(hash, signature, &key, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_FreeDsaKey(&key);
    wc_ShaFree(&sha);

#endif
    return ret;

} /* END test_wc_DsaSign */

/*
 * Testing wc_DsaPrivateKeyDecode() and wc_DsaPublicKeyDecode()
 */
static int test_wc_DsaPublicPrivateKeyDecode (void)
{
    int     ret = 0;

#if !defined(NO_DSA)
    DsaKey  key;
    word32  bytes;
    word32  idx  = 0;
    int     priv = WOLFSSL_FATAL_ERROR;
    int     pub  = WOLFSSL_FATAL_ERROR;

#ifdef USE_CERT_BUFFERS_1024
    byte    tmp[ONEK_BUF];
    XMEMCPY(tmp, dsa_key_der_1024, sizeof_dsa_key_der_1024);
    bytes = sizeof_dsa_key_der_1024;
#elif defined(USE_CERT_BUFFERS_2048)
    byte    tmp[TWOK_BUF];
    XMEMCPY(tmp, dsa_key_der_2048, sizeof_dsa_key_der_2048);
    bytes = sizeof_dsa_key_der_2048;
#else
    byte    tmp[TWOK_BUF];
    XMEMSET(tmp, 0, sizeof(tmp));
    FILE* fp = fopen("./certs/dsa2048.der", "rb");
    if (!fp) {
        return WOLFSSL_BAD_FILE;
    }
    bytes = (word32) fread(tmp, 1, sizeof(tmp), fp);
    fclose(fp);
#endif /* END USE_CERT_BUFFERS_1024 */

    ret = wc_InitDsaKey(&key);

    printf(testingFmt, "wc_DsaPrivateKeyDecode()");
    if (ret == 0) {
        priv = wc_DsaPrivateKeyDecode(tmp, &idx, &key, bytes);

        /* Test bad args. */
        if (priv == 0) {
            priv = wc_DsaPrivateKeyDecode(NULL, &idx, &key, bytes);
            if (priv == BAD_FUNC_ARG) {
                priv = wc_DsaPrivateKeyDecode(tmp, NULL, &key, bytes);
            }
            if (priv == BAD_FUNC_ARG) {
                priv = wc_DsaPrivateKeyDecode(tmp, &idx, NULL, bytes);
            }
            if (priv == BAD_FUNC_ARG) {
                priv = wc_DsaPrivateKeyDecode(tmp, &idx, &key, bytes);
            }
            if (priv == ASN_PARSE_E) {
                priv = 0;
            } else {
                priv = WOLFSSL_FATAL_ERROR;
            }
        }
    } /* END Private Key  */
    if (ret == 0) {
        wc_FreeDsaKey(&key);
        ret = wc_InitDsaKey(&key);
    }

    printf(resultFmt, priv == 0 ? passed : failed);

    printf(testingFmt, "wc_DsaPublicKeyDecode()");
    if (ret == 0) {
        idx = 0; /* Reset */
        pub = wc_DsaPublicKeyDecode(tmp, &idx, &key, bytes);
        /* Test bad args. */
        if (pub == 0) {
            pub = wc_DsaPublicKeyDecode(NULL, &idx, &key, bytes);
            if (pub == BAD_FUNC_ARG) {
                pub = wc_DsaPublicKeyDecode(tmp, NULL, &key, bytes);
            }
            if (pub == BAD_FUNC_ARG) {
                pub = wc_DsaPublicKeyDecode(tmp, &idx, NULL, bytes);
            }
            if (pub == BAD_FUNC_ARG) {
                pub = wc_DsaPublicKeyDecode(tmp, &idx, &key, bytes);
            }
            if (pub == ASN_PARSE_E) {
                pub = 0;
            } else {
                pub = WOLFSSL_FATAL_ERROR;
            }
        }

    } /* END Public Key */

    printf(resultFmt, pub == 0 ? passed : failed);

    wc_FreeDsaKey(&key);

#endif
    return ret;

} /* END test_wc_DsaPublicPrivateKeyDecode */


/*
 * Testing wc_MakeDsaKey() and wc_MakeDsaParameters()
 */
static int test_wc_MakeDsaKey (void)
{
    int     ret = 0;

#if !defined(NO_DSA) && defined(WOLFSSL_KEY_GEN)
    DsaKey  genKey;
    WC_RNG  rng;

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_InitDsaKey(&genKey);
    }

    printf(testingFmt, "wc_MakeDsaParameters()");
    if (ret == 0) {
        ret = wc_MakeDsaParameters(&rng, ONEK_BUF, &genKey);
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_MakeDsaParameters(NULL, ONEK_BUF, &genKey);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_MakeDsaParameters(&rng, ONEK_BUF, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_MakeDsaParameters(&rng, ONEK_BUF + 1, &genKey);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    printf(testingFmt, "wc_MakeDsaKey()");

    if (ret == 0) {
        ret = wc_MakeDsaKey(&rng, &genKey);
    }

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_MakeDsaKey(NULL, &genKey);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_MakeDsaKey(&rng, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FAILURE;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_FreeDsaKey(&genKey);
#endif
    return ret;
} /* END test_wc_MakeDsaKey */

/*
 * Testing wc_DsaKeyToDer()
 */
static int test_wc_DsaKeyToDer (void)
{
    int     ret = 0;

#if !defined(NO_DSA) && defined(WOLFSSL_KEY_GEN)
    DsaKey  genKey;
    WC_RNG  rng;
    word32  bytes;
    word32  idx = 0;

#ifdef USE_CERT_BUFFERS_1024
    byte    tmp[ONEK_BUF];
    byte    der[ONEK_BUF];
    XMEMSET(tmp, 0, sizeof(tmp));
    XMEMSET(der, 0, sizeof(der));
    XMEMCPY(tmp, dsa_key_der_1024, sizeof_dsa_key_der_1024);
    bytes = sizeof_dsa_key_der_1024;
#elif defined(USE_CERT_BUFFERS_2048)
    byte    tmp[TWOK_BUF];
    byte    der[TWOK_BUF];
    XMEMSET(tmp, 0, sizeof(tmp));
    XMEMSET(der, 0, sizeof(der));
    XMEMCPY(tmp, dsa_key_der_2048, sizeof_dsa_key_der_2048);
    bytes = sizeof_dsa_key_der_2048;
#else
    byte    tmp[TWOK_BUF];
    byte    der[TWOK_BUF];
    XMEMSET(tmp, 0, sizeof(tmp));
    XMEMSET(der, 0, sizeof(der));
    FILE* fp = fopen("./certs/dsa2048.der", "rb");
    if (!fp) {
        return WOLFSSL_BAD_FILE;
    }
    bytes = (word32) fread(tmp, 1, sizeof(tmp), fp);
    fclose(fp);
#endif /* END USE_CERT_BUFFERS_1024 */

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_InitDsaKey(&genKey);
    }
    if (ret == 0) {
        ret = wc_MakeDsaParameters(&rng, sizeof(tmp), &genKey);
        if (ret == 0) {
            wc_FreeDsaKey(&genKey);
            ret = wc_InitDsaKey(&genKey);
        }
    }
    if (ret == 0) {
        ret = wc_DsaPrivateKeyDecode(tmp, &idx, &genKey, bytes);
    }

    printf(testingFmt, "wc_DsaKeyToDer()");

    if (ret == 0) {
        ret = wc_DsaKeyToDer(&genKey, der, bytes);
        if ( ret >= 0 && ( ret = XMEMCMP(der, tmp, bytes) ) == 0 ) {
            ret = 0;
        }
    }

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_DsaKeyToDer(NULL, der, FOURK_BUF);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_DsaKeyToDer(&genKey, NULL, FOURK_BUF);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_FreeDsaKey(&genKey);

#endif

    return ret;

} /* END test_wc_DsaKeyToDer */

/*
 * Testing wc_DsaImportParamsRaw()
 */
static int test_wc_DsaImportParamsRaw (void)
{
    int     ret = 0;

#if !defined(NO_DSA)
    DsaKey  key;

    /* [mod = L=1024, N=160], from CAVP KeyPair */
    const char* p = "d38311e2cd388c3ed698e82fdf88eb92b5a9a483dc88005d"
                    "4b725ef341eabb47cf8a7a8a41e792a156b7ce97206c4f9c"
                    "5ce6fc5ae7912102b6b502e59050b5b21ce263dddb2044b6"
                    "52236f4d42ab4b5d6aa73189cef1ace778d7845a5c1c1c71"
                    "47123188f8dc551054ee162b634d60f097f719076640e209"
                    "80a0093113a8bd73";
    const char* q = "96c5390a8b612c0e422bb2b0ea194a3ec935a281";
    const char* g = "06b7861abbd35cc89e79c52f68d20875389b127361ca66822"
                    "138ce4991d2b862259d6b4548a6495b195aa0e0b6137ca37e"
                    "b23b94074d3c3d300042bdf15762812b6333ef7b07ceba786"
                    "07610fcc9ee68491dbc1e34cd12615474e52b18bc934fb00c"
                    "61d39e7da8902291c4434a4e2224c3f4fd9f93cd6f4f17fc0"
                    "76341a7e7d9";

    /* invalid p and q parameters */
    const char* invalidP = "d38311e2cd388c3ed698e82fdf88eb92b5a9a483dc88005d";
    const char* invalidQ = "96c5390a";

    printf(testingFmt, "wc_DsaImportParamsRaw()");

    ret = wc_InitDsaKey(&key);
    if (ret == 0) {
        ret = wc_DsaImportParamsRaw(&key, p, q, g);
    }

    /* test bad args */
    if (ret == 0) {
        /* null key struct */
        ret = wc_DsaImportParamsRaw(NULL, p, q, g);
        if (ret == BAD_FUNC_ARG) {
            /* null param pointers */
            ret = wc_DsaImportParamsRaw(&key, NULL, NULL, NULL);
        }

        if (ret == BAD_FUNC_ARG) {
            /* illegal p length */
            ret = wc_DsaImportParamsRaw(&key, invalidP, q, g);
        }

        if (ret == BAD_FUNC_ARG) {
            /* illegal q length */
            ret = wc_DsaImportParamsRaw(&key, p, invalidQ, g);
            if (ret == BAD_FUNC_ARG)
                ret = 0;
        }

    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_FreeDsaKey(&key);

#endif

    return ret;

} /* END test_wc_DsaImportParamsRaw */

/*
 * Testing wc_DsaExportParamsRaw()
 */
static int test_wc_DsaExportParamsRaw (void)
{
    int     ret = 0;

#if !defined(NO_DSA)
    DsaKey  key;

    /* [mod = L=1024, N=160], from CAVP KeyPair */
    const char* p = "d38311e2cd388c3ed698e82fdf88eb92b5a9a483dc88005d"
                    "4b725ef341eabb47cf8a7a8a41e792a156b7ce97206c4f9c"
                    "5ce6fc5ae7912102b6b502e59050b5b21ce263dddb2044b6"
                    "52236f4d42ab4b5d6aa73189cef1ace778d7845a5c1c1c71"
                    "47123188f8dc551054ee162b634d60f097f719076640e209"
                    "80a0093113a8bd73";
    const char* q = "96c5390a8b612c0e422bb2b0ea194a3ec935a281";
    const char* g = "06b7861abbd35cc89e79c52f68d20875389b127361ca66822"
                    "138ce4991d2b862259d6b4548a6495b195aa0e0b6137ca37e"
                    "b23b94074d3c3d300042bdf15762812b6333ef7b07ceba786"
                    "07610fcc9ee68491dbc1e34cd12615474e52b18bc934fb00c"
                    "61d39e7da8902291c4434a4e2224c3f4fd9f93cd6f4f17fc0"
                    "76341a7e7d9";

    const char* pCompare = "\xd3\x83\x11\xe2\xcd\x38\x8c\x3e\xd6\x98\xe8\x2f"
                           "\xdf\x88\xeb\x92\xb5\xa9\xa4\x83\xdc\x88\x00\x5d"
                           "\x4b\x72\x5e\xf3\x41\xea\xbb\x47\xcf\x8a\x7a\x8a"
                           "\x41\xe7\x92\xa1\x56\xb7\xce\x97\x20\x6c\x4f\x9c"
                           "\x5c\xe6\xfc\x5a\xe7\x91\x21\x02\xb6\xb5\x02\xe5"
                           "\x90\x50\xb5\xb2\x1c\xe2\x63\xdd\xdb\x20\x44\xb6"
                           "\x52\x23\x6f\x4d\x42\xab\x4b\x5d\x6a\xa7\x31\x89"
                           "\xce\xf1\xac\xe7\x78\xd7\x84\x5a\x5c\x1c\x1c\x71"
                           "\x47\x12\x31\x88\xf8\xdc\x55\x10\x54\xee\x16\x2b"
                           "\x63\x4d\x60\xf0\x97\xf7\x19\x07\x66\x40\xe2\x09"
                           "\x80\xa0\x09\x31\x13\xa8\xbd\x73";
    const char* qCompare = "\x96\xc5\x39\x0a\x8b\x61\x2c\x0e\x42\x2b\xb2\xb0"
                           "\xea\x19\x4a\x3e\xc9\x35\xa2\x81";
    const char* gCompare = "\x06\xb7\x86\x1a\xbb\xd3\x5c\xc8\x9e\x79\xc5\x2f"
                           "\x68\xd2\x08\x75\x38\x9b\x12\x73\x61\xca\x66\x82"
                           "\x21\x38\xce\x49\x91\xd2\xb8\x62\x25\x9d\x6b\x45"
                           "\x48\xa6\x49\x5b\x19\x5a\xa0\xe0\xb6\x13\x7c\xa3"
                           "\x7e\xb2\x3b\x94\x07\x4d\x3c\x3d\x30\x00\x42\xbd"
                           "\xf1\x57\x62\x81\x2b\x63\x33\xef\x7b\x07\xce\xba"
                           "\x78\x60\x76\x10\xfc\xc9\xee\x68\x49\x1d\xbc\x1e"
                           "\x34\xcd\x12\x61\x54\x74\xe5\x2b\x18\xbc\x93\x4f"
                           "\xb0\x0c\x61\xd3\x9e\x7d\xa8\x90\x22\x91\xc4\x43"
                           "\x4a\x4e\x22\x24\xc3\xf4\xfd\x9f\x93\xcd\x6f\x4f"
                           "\x17\xfc\x07\x63\x41\xa7\xe7\xd9";

    byte pOut[MAX_DSA_PARAM_SIZE];
    byte qOut[MAX_DSA_PARAM_SIZE];
    byte gOut[MAX_DSA_PARAM_SIZE];
    word32 pOutSz, qOutSz, gOutSz;

    printf(testingFmt, "wc_DsaExportParamsRaw()");

    ret = wc_InitDsaKey(&key);
    if (ret == 0) {
        /* first test using imported raw parameters, for expected */
        ret = wc_DsaImportParamsRaw(&key, p, q, g);
    }

    if (ret == 0) {
        pOutSz = sizeof(pOut);
        qOutSz = sizeof(qOut);
        gOutSz = sizeof(gOut);
        ret = wc_DsaExportParamsRaw(&key, pOut, &pOutSz, qOut, &qOutSz,
                                    gOut, &gOutSz);
    }

    if (ret == 0) {
        /* validate exported parameters are correct */
        if ((XMEMCMP(pOut, pCompare, pOutSz) != 0) ||
            (XMEMCMP(qOut, qCompare, qOutSz) != 0) ||
            (XMEMCMP(gOut, gCompare, gOutSz) != 0) ) {
            ret = -1;
        }
    }

    /* test bad args */
    if (ret == 0) {
        /* null key struct */
        ret = wc_DsaExportParamsRaw(NULL, pOut, &pOutSz, qOut, &qOutSz,
                                    gOut, &gOutSz);

        if (ret == BAD_FUNC_ARG) {
            /* null output pointers */
            ret = wc_DsaExportParamsRaw(&key, NULL, &pOutSz, NULL, &qOutSz,
                                        NULL, &gOutSz);
        }

        if (ret == LENGTH_ONLY_E) {
            /* null output size pointers */
            ret = wc_DsaExportParamsRaw(&key, pOut, NULL, qOut, NULL,
                                        gOut, NULL);
        }

        if (ret == BAD_FUNC_ARG) {
            /* p output buffer size too small */
            pOutSz = 1;
            ret = wc_DsaExportParamsRaw(&key, pOut, &pOutSz, qOut, &qOutSz,
                                        gOut, &gOutSz);
            pOutSz = sizeof(pOut);
        }

        if (ret == BUFFER_E) {
            /* q output buffer size too small */
            qOutSz = 1;
            ret = wc_DsaExportParamsRaw(&key, pOut, &pOutSz, qOut, &qOutSz,
                                        gOut, &gOutSz);
            qOutSz = sizeof(qOut);
        }

        if (ret == BUFFER_E) {
            /* g output buffer size too small */
            gOutSz = 1;
            ret = wc_DsaExportParamsRaw(&key, pOut, &pOutSz, qOut, &qOutSz,
                                        gOut, &gOutSz);
            if (ret == BUFFER_E)
                ret = 0;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_FreeDsaKey(&key);

#endif

    return ret;

} /* END test_wc_DsaExportParamsRaw */

/*
 * Testing wc_DsaExportKeyRaw()
 */
static int test_wc_DsaExportKeyRaw (void)
{
    int     ret = 0;

#if !defined(NO_DSA) && defined(WOLFSSL_KEY_GEN)
    DsaKey  key;
    WC_RNG  rng;

    byte xOut[MAX_DSA_PARAM_SIZE];
    byte yOut[MAX_DSA_PARAM_SIZE];
    word32 xOutSz, yOutSz;

    printf(testingFmt, "wc_DsaExportKeyRaw()");

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_InitDsaKey(&key);
    }

    if (ret == 0) {
        ret = wc_MakeDsaParameters(&rng, 1024, &key);

        if (ret == 0)  {
            ret = wc_MakeDsaKey(&rng, &key);
        }
    }

    /* try successful export */
    if (ret == 0) {
        xOutSz = sizeof(xOut);
        yOutSz = sizeof(yOut);
        ret = wc_DsaExportKeyRaw(&key, xOut, &xOutSz, yOut, &yOutSz);
    }

    /* test bad args */
    if (ret == 0) {
        /* null key struct */
        ret = wc_DsaExportKeyRaw(NULL, xOut, &xOutSz, yOut, &yOutSz);

        if (ret == BAD_FUNC_ARG) {
            /* null output pointers */
            ret = wc_DsaExportKeyRaw(&key, NULL, &xOutSz, NULL, &yOutSz);
        }

        if (ret == LENGTH_ONLY_E) {
            /* null output size pointers */
            ret = wc_DsaExportKeyRaw(&key, xOut, NULL, yOut, NULL);
        }

        if (ret == BAD_FUNC_ARG) {
            /* x output buffer size too small */
            xOutSz = 1;
            ret = wc_DsaExportKeyRaw(&key, xOut, &xOutSz, yOut, &yOutSz);
            xOutSz = sizeof(xOut);
        }

        if (ret == BUFFER_E) {
            /* y output buffer size too small */
            yOutSz = 1;
            ret = wc_DsaExportKeyRaw(&key, xOut, &xOutSz, yOut, &yOutSz);

            if (ret == BUFFER_E)
                ret = 0;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_FreeDsaKey(&key);
    wc_FreeRng(&rng);

#endif

    return ret;

} /* END test_wc_DsaExportParamsRaw */


/*
 * Testing wc_ed25519_make_key().
 */
static int test_wc_ed25519_make_key (void)
{
    int ret = 0;

#if defined(HAVE_ED25519)
    ed25519_key     key;
    WC_RNG          rng;

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ed25519_init(&key);
    }
    printf(testingFmt, "wc_ed25519_make_key()");
    if (ret == 0) {
        ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE, &key);
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ed25519_make_key(NULL, ED25519_KEY_SIZE, &key);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE - 1, &key);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE + 1, &key);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = SSL_FATAL_ERROR;
        }
    }


    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = SSL_FATAL_ERROR;
    }
    wc_ed25519_free(&key);

#endif
    return ret;

} /* END test_wc_ed25519_make_key */


/*
 * Testing wc_ed25519_init()
 */
static int test_wc_ed25519_init (void)
{
    int             ret = 0;

#if defined(HAVE_ED25519)

    ed25519_key    key;

    printf(testingFmt, "wc_ed25519_init()");

    ret = wc_ed25519_init(&key);

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ed25519_init(NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = SSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_ed25519_free(&key);

#endif
    return ret;

} /* END test_wc_ed25519_init */

/*
 * Test wc_ed25519_sign_msg() and wc_ed25519_verify_msg()
 */
static int test_wc_ed25519_sign_msg (void)
{
    int             ret = 0;

#if defined(HAVE_ED25519) && defined(HAVE_ED25519_SIGN)
    WC_RNG          rng;
    ed25519_key     key;
    byte            msg[] = "Everybody gets Friday off.\n";
    byte            sig[ED25519_SIG_SIZE];
    word32          msglen = sizeof(msg);
    word32          siglen = sizeof(sig);
    word32          badSigLen = sizeof(sig) - 1;
    int             stat = 0; /*1 = Verify success.*/

    /* Initialize stack variables. */
    XMEMSET(sig, 0, siglen);

    /* Initialize key. */
    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ed25519_init(&key);
        if (ret == 0) {
            ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE, &key);
        }
    }

    printf(testingFmt, "wc_ed25519_sign_msg()");

    if (ret == 0) {
        ret = wc_ed25519_sign_msg(msg, msglen, sig, &siglen, &key);
    }
    /* Test bad args. */
    if (ret == 0 && siglen == ED25519_SIG_SIZE) {
        ret = wc_ed25519_sign_msg(NULL, msglen, sig, &siglen, &key);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_sign_msg(msg, msglen, NULL, &siglen, &key);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_sign_msg(msg, msglen, sig, NULL, &key);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_sign_msg(msg, msglen, sig, &siglen, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_sign_msg(msg, msglen, sig, &badSigLen, &key);
        }
        if (ret == BUFFER_E && badSigLen == ED25519_SIG_SIZE) {
            badSigLen -= 1;
            ret = 0;
        } else if (ret == 0) {
            ret = SSL_FATAL_ERROR;
        }
    } /* END sign */

    printf(resultFmt, ret == 0 ? passed : failed);
    #ifdef HAVE_ED25519_VERIFY
        printf(testingFmt, "wc_ed25519_verify_msg()");

        if (ret == 0) {

            ret = wc_ed25519_verify_msg(sig, siglen, msg, msglen, &stat, &key);
            if (ret == 0  && stat == 1) {
                ret = 0;
            } else if (ret == 0) {
                ret = SSL_FATAL_ERROR;
            }

            /* Test bad args. */
            if (ret == 0) {
                ret = wc_ed25519_verify_msg(NULL, siglen, msg, msglen, &stat,
                                                                        &key);
                if (ret == BAD_FUNC_ARG) {
                    ret = wc_ed25519_verify_msg(sig, siglen, NULL, msglen,
                                                                &stat, &key);
                }
                if (ret == BAD_FUNC_ARG) {
                    ret = wc_ed25519_verify_msg(sig, siglen, msg, msglen,
                                                                  NULL, &key);
                }
                if (ret == BAD_FUNC_ARG) {
                    ret = wc_ed25519_verify_msg(sig, siglen, msg, msglen,
                                                                &stat, NULL);
                }
                if (ret == BAD_FUNC_ARG) {
                    ret = wc_ed25519_verify_msg(sig, badSigLen, msg, msglen,
                                                                &stat, &key);
                }
                if (ret == BAD_FUNC_ARG) {
                    ret = 0;
                } else if (ret == 0) {
                    ret = SSL_FATAL_ERROR;
                }
            }

        } /* END verify. */

        printf(resultFmt, ret == 0 ? passed : failed);
    #endif /* Verify. */

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = SSL_FATAL_ERROR;
    }
    wc_ed25519_free(&key);

#endif
    return ret;

} /* END test_wc_ed25519_sign_msg */

/*
 * Testing wc_ed25519_import_public()
 */
static int test_wc_ed25519_import_public (void)
{
    int             ret = 0;

#if defined(HAVE_ED25519) && defined(HAVE_ED25519_KEY_IMPORT)
    WC_RNG          rng;
    ed25519_key     pubKey;
    const byte      in[] = "Ed25519PublicKeyUnitTest......\n";
    word32          inlen = sizeof(in);


    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ed25519_init(&pubKey);
        if (ret == 0) {
            ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE, &pubKey);
        }
    }
    printf(testingFmt, "wc_ed25519_import_public()");

    if (ret == 0) {
        ret = wc_ed25519_import_public(in, inlen, &pubKey);

        if (ret == 0 && XMEMCMP(in, pubKey.p, inlen) == 0) {
            ret = 0;
        } else {
            ret = SSL_FATAL_ERROR;
        }

        /* Test bad args. */
        if (ret == 0) {
            ret = wc_ed25519_import_public(NULL, inlen, &pubKey);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ed25519_import_public(in, inlen, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ed25519_import_public(in, inlen - 1, &pubKey);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = SSL_FATAL_ERROR;
            }
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = SSL_FATAL_ERROR;
    }
    wc_ed25519_free(&pubKey);

#endif
    return ret;

} /* END wc_ed25519_import_public */

/*
 * Testing wc_ed25519_import_private_key()
 */
static int test_wc_ed25519_import_private_key (void)
{
    int         ret = 0;

#if defined(HAVE_ED25519) && defined(HAVE_ED25519_KEY_IMPORT)
    WC_RNG      rng;
    ed25519_key key;
    const byte  privKey[] = "Ed25519PrivateKeyUnitTest.....\n";
    const byte  pubKey[] = "Ed25519PublicKeyUnitTest......\n";
    word32      privKeySz = sizeof(privKey);
    word32      pubKeySz = sizeof(pubKey);

    ret = wc_InitRng(&rng);
    if (ret != 0) {
        return ret;
    }
    ret = wc_ed25519_init(&key);
    if (ret != 0) {
        wc_FreeRng(&rng);
        return ret;
    }
    ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE, &key);

    printf(testingFmt, "wc_ed25519_import_private_key()");

    if (ret == 0) {
        ret = wc_ed25519_import_private_key(privKey, privKeySz, pubKey,
                                                            pubKeySz, &key);
        if (ret == 0 && (XMEMCMP(pubKey, key.p, privKeySz) != 0
                                || XMEMCMP(privKey, key.k, pubKeySz) != 0)) {
            ret = SSL_FATAL_ERROR;
        }
    }

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ed25519_import_private_key(NULL, privKeySz, pubKey, pubKeySz,
                                                                        &key);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_import_private_key(privKey, privKeySz, NULL,
                                                                pubKeySz, &key);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_import_private_key(privKey, privKeySz, pubKey,
                                                                pubKeySz, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_import_private_key(privKey, privKeySz - 1, pubKey,
                                                                pubKeySz, &key);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_import_private_key(privKey, privKeySz, pubKey,
                                                            pubKeySz - 1, &key);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = SSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = SSL_FATAL_ERROR;
    }
    wc_ed25519_free(&key);

#endif
    return ret;

} /* END test_wc_ed25519_import_private_key */

/*
 * Testing wc_ed25519_export_public() and wc_ed25519_export_private_only()
 */
static int test_wc_ed25519_export (void)
{
    int             ret = 0;

#if defined(HAVE_ED25519) && defined(HAVE_ED25519_KEY_EXPORT)
    WC_RNG          rng;
    ed25519_key     key;
    byte            priv[ED25519_PRV_KEY_SIZE];
    byte            pub[ED25519_PUB_KEY_SIZE];
    word32          privSz = sizeof(priv);
    word32          pubSz = sizeof(pub);

    ret = wc_InitRng(&rng);
    if (ret != 0) {
        return ret;
    }

    ret = wc_ed25519_init(&key);
    if (ret != 0) {
        wc_FreeRng(&rng);
        return ret;
    }

    if (ret == 0) {
        ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE, &key);
    }

    printf(testingFmt, "wc_ed25519_export_public()");

    if (ret == 0) {
        ret = wc_ed25519_export_public(&key, pub, &pubSz);
        if (ret == 0 && (pubSz != ED25519_KEY_SIZE
                                        || XMEMCMP(key.p, pub, pubSz) != 0)) {
            ret = SSL_FATAL_ERROR;
        }
        if (ret == 0) {
            ret = wc_ed25519_export_public(NULL, pub, &pubSz);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ed25519_export_public(&key, NULL, &pubSz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ed25519_export_public(&key, pub, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = SSL_FATAL_ERROR;
            }
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);
    printf(testingFmt, "wc_ed25519_export_private_only()");

    if (ret == 0) {
        ret = wc_ed25519_export_private_only(&key, priv, &privSz);
        if (ret == 0 && (privSz != ED25519_KEY_SIZE
                                        || XMEMCMP(key.k, priv, privSz) != 0)) {
            ret = SSL_FATAL_ERROR;
        }
        if (ret == 0) {
            ret = wc_ed25519_export_private_only(NULL, priv, &privSz);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ed25519_export_private_only(&key, NULL, &privSz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ed25519_export_private_only(&key, priv, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = SSL_FATAL_ERROR;
            }
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = SSL_FATAL_ERROR;
    }
    wc_ed25519_free(&key);

#endif
    return ret;

} /* END test_wc_ed25519_export */

/*
 *  Testing wc_ed25519_size()
 */
static int test_wc_ed25519_size (void)
{
    int             ret = 0;
#if defined(HAVE_ED25519)
    WC_RNG          rng;
    ed25519_key     key;

    ret = wc_InitRng(&rng);
    if (ret != 0) {
        return ret;
    }
    ret = wc_ed25519_init(&key);
    if (ret != 0) {
        wc_FreeRng(&rng);
        return ret;
    }

    ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE, &key);
    if (ret != 0) {
        wc_FreeRng(&rng);
        wc_ed25519_free(&key);
        return ret;
    }

    printf(testingFmt, "wc_ed25519_size()");
    ret = wc_ed25519_size(&key);
    /* Test bad args. */
    if (ret == ED25519_KEY_SIZE) {
        ret = wc_ed25519_size(NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);

    if (ret == 0) {
        printf(testingFmt, "wc_ed25519_sig_size()");

        ret = wc_ed25519_sig_size(&key);
        if (ret == ED25519_SIG_SIZE) {
            ret = 0;
        }
        /* Test bad args. */
        if (ret == 0) {
            ret = wc_ed25519_sig_size(NULL);
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            }
        }

        printf(resultFmt, ret == 0 ? passed : failed);
    } /* END wc_ed25519_sig_size() */

    if (ret == 0) {
        printf(testingFmt, "wc_ed25519_pub_size");
        ret = wc_ed25519_pub_size(&key);
        if (ret == ED25519_PUB_KEY_SIZE) {
            ret = 0;
        }
        if (ret == 0) {
            ret = wc_ed25519_pub_size(NULL);
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            }
        }
        printf(resultFmt, ret == 0 ? passed : failed);
    } /* END wc_ed25519_pub_size */

    if (ret == 0) {
        printf(testingFmt, "wc_ed25519_priv_size");
        ret = wc_ed25519_priv_size(&key);
        if (ret == ED25519_PRV_KEY_SIZE) {
            ret = 0;
        }
        if (ret == 0) {
            ret = wc_ed25519_priv_size(NULL);
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            }
        }
        printf(resultFmt, ret == 0 ? passed : failed);
    } /* END wc_ed25519_pub_size */

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = SSL_FATAL_ERROR;
    }
    wc_ed25519_free(&key);

#endif
    return ret;

} /* END test_wc_ed25519_size */

/*
 * Testing wc_ed25519_export_private() and wc_ed25519_export_key()
 */
static int test_wc_ed25519_exportKey (void)
{
    int             ret = 0;
#if defined(HAVE_ED25519) && defined(HAVE_ED25519_KEY_EXPORT)
    WC_RNG          rng;
    ed25519_key     key;
    byte            priv[ED25519_PRV_KEY_SIZE];
    byte            pub[ED25519_PUB_KEY_SIZE];
    byte            privOnly[ED25519_PRV_KEY_SIZE];
    word32          privSz      = sizeof(priv);
    word32          pubSz       = sizeof(pub);
    word32          privOnlySz  = sizeof(privOnly);

    ret = wc_InitRng(&rng);
    if (ret != 0) {
        return ret;
    }
    ret = wc_ed25519_init(&key);
    if (ret != 0) {
        wc_FreeRng(&rng);
        return ret;
    }

    ret = wc_ed25519_make_key(&rng, ED25519_KEY_SIZE, &key);
    if (ret != 0) {
        wc_FreeRng(&rng);
        wc_ed25519_free(&key);
        return ret;
    }

    printf(testingFmt, "wc_ed25519_export_private()");

    ret = wc_ed25519_export_private(&key, privOnly, &privOnlySz);
    if (ret == 0) {
        ret = wc_ed25519_export_private(NULL, privOnly, &privOnlySz);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_export_private(&key, NULL, &privOnlySz);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ed25519_export_private(&key, privOnly, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = SSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (ret == 0) {
        printf(testingFmt, "wc_ed25519_export_key()");

        ret = wc_ed25519_export_key(&key, priv, &privSz, pub, &pubSz);
        if (ret == 0) {
            ret = wc_ed25519_export_key(NULL, priv, &privSz, pub, &pubSz);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ed25519_export_key(&key, NULL, &privSz, pub, &pubSz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ed25519_export_key(&key, priv, NULL, pub, &pubSz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ed25519_export_key(&key, priv, &privSz, NULL, &pubSz);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ed25519_export_key(&key, priv, &privSz, pub, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = SSL_FATAL_ERROR;
            }
        }
        printf(resultFmt, ret == 0 ? passed : failed);
    } /* END wc_ed25519_export_key() */

    /* Cross check output. */
    if (ret == 0 && XMEMCMP(priv, privOnly, privSz) != 0) {
        ret = SSL_FATAL_ERROR;
    }

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = SSL_FATAL_ERROR;
    }
    wc_ed25519_free(&key);

#endif
    return ret;

} /* END test_wc_ed25519_exportKey */

/*
 * Testing wc_curve25519_init and wc_curve25519_free.
 */
static int test_wc_curve25519_init (void)
{
    int             ret = 0;

#if defined(HAVE_CURVE25519)

    curve25519_key  key;

    printf(testingFmt, "wc_curve25519_init()");

    ret = wc_curve25519_init(&key);

    /* Test bad args for wc_curve25519_init */
    if (ret == 0) {
        ret = wc_curve25519_init(NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = SSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    /*  Test good args for wc_curve_25519_free */
    wc_curve25519_free(&key);

    wc_curve25519_free(NULL);

#endif
    return ret;

} /* END test_wc_curve25519_init and wc_curve_25519_free*/

/*
 * Testing wc_ecc_make_key.
 */
static int test_wc_ecc_make_key (void)
{
    int     ret = 0;

#if defined(HAVE_ECC)
    WC_RNG rng;
    ecc_key key;

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
    }

    printf(testingFmt, "wc_ecc_make_key()");

    if (ret == 0) {
        ret = wc_ecc_make_key(&rng, KEY14, &key);
    }

    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_ecc_make_key(NULL, KEY14, &key);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_make_key(&rng, KEY14, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_ecc_free(&key);

#endif
    return ret;

} /* END test_wc_ecc_make_key */


/*
 * Testing wc_ecc_init()
 */
static int test_wc_ecc_init (void)
{
    int         ret = 0;

#ifdef HAVE_ECC
    ecc_key     key;

    printf(testingFmt, "wc_ecc_init()");

    ret = wc_ecc_init(&key);
    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_ecc_init(NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_ecc_free(&key);

#endif
    return ret;

} /* END test_wc_ecc_init */

/*
 * Testing wc_ecc_check_key()
 */
static int test_wc_ecc_check_key (void)
{
    int         ret = 0;

#if defined(HAVE_ECC)
    WC_RNG      rng;
    ecc_key     key;

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, KEY14, &key);
        }
    }

    printf(testingFmt, "wc_ecc_check_key()");

    if (ret == 0) {
        ret = wc_ecc_check_key(&key);
    }

    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_ecc_check_key(NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);

#endif
    return ret;

} /* END test_wc_ecc_check_key */

/*
 * Testing wc_ecc_size()
 */
static int test_wc_ecc_size (void)
{
    int         ret = 0;

#if defined(HAVE_ECC)
    WC_RNG      rng;
    ecc_key     key;

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, KEY14, &key);
        }
    }

    printf(testingFmt, "wc_ecc_size()");

    if (ret == 0) {
        ret = wc_ecc_size(&key);
        if (ret == KEY14) {
            ret = 0;
        } else if (ret == 0){
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        /* Returns Zero for bad arg. */
        ret = wc_ecc_size(NULL);
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);

#endif
    return ret;
} /* END test_wc_ecc_size */

/*
 * Testing wc_ecc_sign_hash() and wc_ecc_verify_hash()
 */
static int test_wc_ecc_signVerify_hash (void)
{
    int         ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_SIGN) && !defined(NO_ASN)
    WC_RNG      rng;
    ecc_key     key;
    int         signH = WOLFSSL_FATAL_ERROR;
    #ifdef HAVE_ECC_VERIFY
        int     verifyH = WOLFSSL_FATAL_ERROR;
        int     verify  = 0;
    #endif
    word32      siglen = ECC_BUFSIZE;
    byte        sig[ECC_BUFSIZE];
    byte        digest[] = "Everyone gets Friday off.";
    word32      digestlen = (word32)XSTRLEN((char*)digest);

    /* Init stack var */
    XMEMSET(sig, 0, siglen);

    /* Init structs. */
    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, KEY14, &key);
        }
    }

    printf(testingFmt, "wc_ecc_sign_hash()");

    if (ret == 0) {
        ret = wc_ecc_sign_hash(digest, digestlen, sig, &siglen, &rng, &key);
    }

    /* Checkk bad args. */
    if (ret == 0) {
        signH = wc_ecc_sign_hash(NULL, digestlen, sig, &siglen, &rng, &key);
        if (signH == ECC_BAD_ARG_E) {
            signH = wc_ecc_sign_hash(digest, digestlen, NULL, &siglen,
                                                                &rng, &key);
        }
        if (signH == ECC_BAD_ARG_E) {
            signH = wc_ecc_sign_hash(digest, digestlen, sig, NULL,
                                                                &rng, &key);
        }
        if (signH == ECC_BAD_ARG_E) {
            signH = wc_ecc_sign_hash(digest, digestlen, sig, &siglen,
                                                                NULL, &key);
        }
        if (signH == ECC_BAD_ARG_E) {
            signH = wc_ecc_sign_hash(digest, digestlen, sig, &siglen,
                                                                &rng, NULL);
        }
        if (signH == ECC_BAD_ARG_E) {
            signH = 0;
        } else if (ret == 0) {
            signH = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, signH == 0 ? passed : failed);

    #ifdef HAVE_ECC_VERIFY
        printf(testingFmt, "wc_ecc_verify_hash()");

        ret = wc_ecc_verify_hash(sig, siglen, digest, digestlen, &verify, &key);
        if (verify != 1 && ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
        /* Test bad args. */
        if (ret == 0) {
            verifyH = wc_ecc_verify_hash(NULL, siglen, digest, digestlen,
                                                            &verify, &key);
            if (verifyH == ECC_BAD_ARG_E) {
                verifyH = wc_ecc_verify_hash(sig, siglen, NULL, digestlen,
                                                            &verify, &key);
            }
            if (verifyH == ECC_BAD_ARG_E) {
                verifyH = wc_ecc_verify_hash(sig, siglen, digest, digestlen,
                                                                NULL, &key);
            }
            if (verifyH == ECC_BAD_ARG_E) {
                verifyH = wc_ecc_verify_hash(sig, siglen, digest, digestlen,
                                                            &verify, NULL);
            }
            if (verifyH == ECC_BAD_ARG_E) {
                verifyH = 0;
            } else if (ret == 0) {
                verifyH = WOLFSSL_FATAL_ERROR;
            }
        }

        printf(resultFmt, verifyH == 0 ? passed : failed);

    #endif /* HAVE_ECC_VERIFY */

        if (wc_FreeRng(&rng) && ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
        wc_ecc_free(&key);
#endif
    return ret;

} /*  END test_wc_ecc_sign_hash */


/*
 * Testing wc_ecc_shared_secret()
 */
static int test_wc_ecc_shared_secret (void)
{
    int         ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_DHE)
    ecc_key     key, pubKey;
    WC_RNG      rng;
    int         keySz = KEY16;
    byte        out[keySz];
    word32      outlen = (word32)sizeof(out);

    /* Initialize variables. */
    XMEMSET(out, 0, keySz);

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_init(&pubKey);
        }
    }
    if (ret == 0) {
        ret = wc_ecc_make_key(&rng, keySz, &key);
    }
    if (ret == 0) {
        ret = wc_ecc_make_key(&rng, keySz, &pubKey);
    }

    printf(testingFmt, "wc_ecc_shared_secret()");
    if (ret == 0) {
        ret = wc_ecc_shared_secret(&key, &pubKey, out, &outlen);
        /* Test bad args. */
        if (ret == 0) {
            ret = wc_ecc_shared_secret(NULL, &pubKey, out, &outlen);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ecc_shared_secret(&key, NULL, out, &outlen);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ecc_shared_secret(&key, &pubKey, NULL, &outlen);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ecc_shared_secret(&key, &pubKey, out, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = 0;
            } else if (ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);
    wc_ecc_free(&pubKey);

#endif
    return ret;

} /* END tests_wc_ecc_shared_secret */

/*
 * testint wc_ecc_export_x963()
 */
static int test_wc_ecc_export_x963 (void)
{
    int     ret = 0;

#ifdef HAVE_ECC
    ecc_key key;
    WC_RNG  rng;
    byte    out[ECC_ASN963_MAX_BUF_SZ];
    word32  outlen = sizeof(out);

    /* Initialize variables. */
    XMEMSET(out, 0, outlen);

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, KEY20, &key);
        }
    }
    printf(testingFmt, "wc_ecc_export_x963()");

    if (ret == 0) {
        ret = wc_ecc_export_x963(&key, out, &outlen);
    }

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_export_x963(NULL, out, &outlen);
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_export_x963(&key, NULL, &outlen);
        }
        if (ret == LENGTH_ONLY_E) {
            ret = wc_ecc_export_x963(&key, out, NULL);
        }
        if (ret == ECC_BAD_ARG_E) {
            key.idx = -4;
            ret = wc_ecc_export_x963(&key, out, &outlen);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);

#endif
    return ret;

} /* END test_wc_ecc_export_x963 */

/*
 * Testing wc_ecc_export_x963_ex()
 * compile with --enable-compkey will use compression.
 */
static int test_wc_ecc_export_x963_ex (void)
{
    int     ret = 0;

#if defined(HAVE_ECC)
    ecc_key key;
    WC_RNG  rng;
    byte    out[ECC_ASN963_MAX_BUF_SZ];
    word32  outlen = sizeof(out);
    #ifdef HAVE_COMP_KEY
        word32  badOutLen = 5;
    #endif

    /* Init stack variables. */
    XMEMSET(out, 0, outlen);

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, KEY64, &key);
        }
    }

    printf(testingFmt, "wc_ecc_export_x963_ex()");

    #ifdef HAVE_COMP_KEY
        if (ret == 0) {
            ret = wc_ecc_export_x963_ex(&key, out, &outlen, COMP);
        }
    #else
        if (ret == 0) {
            ret = wc_ecc_export_x963_ex(&key, out, &outlen, NOCOMP);
        }
    #endif

    /* Test bad args. */
    #ifdef HAVE_COMP_KEY
    if (ret == 0) {
        ret = wc_ecc_export_x963_ex(NULL, out, &outlen, COMP);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_export_x963_ex(&key, NULL, &outlen, COMP);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_export_x963_ex(&key, out, NULL, COMP);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_export_x963_ex(&key, out, &badOutLen, COMP);
        }
        if (ret == BUFFER_E) {
            key.idx = -4;
            ret = wc_ecc_export_x963_ex(&key, out, &outlen, COMP);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    #else
        if (ret == 0) {
            ret = wc_ecc_export_x963_ex(NULL, out, &outlen, NOCOMP);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ecc_export_x963_ex(&key, NULL, &outlen, NOCOMP);
            }
            if (ret == BAD_FUNC_ARG) {
                ret = wc_ecc_export_x963_ex(&key, out, &outlen, 1);
            }
            if (ret == NOT_COMPILED_IN) {
                ret = wc_ecc_export_x963_ex(&key, out, NULL, NOCOMP);
            }
            if (ret == BAD_FUNC_ARG) {
                key.idx = -4;
                ret = wc_ecc_export_x963_ex(&key, out, &outlen, NOCOMP);
            }
            if (ret == ECC_BAD_ARG_E) {
                ret = 0;
            } else if (ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    #endif

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);

#endif
    return ret;

} /* END test_wc_ecc_export_x963_ex */

/*
 * testing wc_ecc_import_x963()
 */
static int test_wc_ecc_import_x963 (void)
{
    int     ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_KEY_IMPORT)
    ecc_key pubKey, key;
    WC_RNG  rng;
    byte    x963[ECC_ASN963_MAX_BUF_SZ];
    word32  x963Len = (word32)sizeof(x963);

    /* Init stack variables. */
    XMEMSET(x963, 0, x963Len);
    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&pubKey);
        if (ret == 0) {
            ret = wc_ecc_init(&key);
        }
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, KEY24, &key);
        }
        if (ret == 0) {
            ret = wc_ecc_export_x963(&key, x963, &x963Len);
        }
    }

    printf(testingFmt, "wc_ecc_import_x963()");
    if (ret == 0) {
        ret = wc_ecc_import_x963(x963, x963Len, &pubKey);
    }

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_import_x963(NULL, x963Len, &pubKey);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_import_x963(x963, x963Len, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_import_x963(x963, x963Len + 1, &pubKey);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);
    wc_ecc_free(&pubKey);

#endif
    return ret;

} /* END wc_ecc_import_x963 */

/*
 * testing wc_ecc_import_private_key()
 */
static int ecc_import_private_key (void)
{
    int     ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_KEY_IMPORT)
    ecc_key key, keyImp;
    WC_RNG  rng;
    byte    privKey[ECC_PRIV_KEY_BUF]; /* Raw private key.*/
    byte    x963Key[ECC_ASN963_MAX_BUF_SZ];
    word32  privKeySz = (word32)sizeof(privKey);
    word32  x963KeySz = (word32)sizeof(x963Key);

    /* Init stack variables. */
    XMEMSET(privKey, 0, privKeySz);
    XMEMSET(x963Key, 0, x963KeySz);

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_init(&keyImp);
        }
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, KEY48, &key);
        }
        if (ret == 0) {
            ret = wc_ecc_export_x963(&key, x963Key, &x963KeySz);
        }
        if (ret == 0) {
            ret = wc_ecc_export_private_only(&key, privKey, &privKeySz);
        }
    }

    printf(testingFmt, "wc_ecc_import_private_key()");

    if (ret == 0) {
        ret = wc_ecc_import_private_key(privKey, privKeySz, x963Key,
                                                x963KeySz, &keyImp);
    }
    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_ecc_import_private_key(privKey, privKeySz, x963Key,
                                                x963KeySz, NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_import_private_key(NULL, privKeySz, x963Key,
                                                x963KeySz, &keyImp);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);
    wc_ecc_free(&keyImp);

#endif
    return ret;

} /* END wc_ecc_import_private_key */


/*
 * Testing wc_ecc_export_private_only()
 */
static int test_wc_ecc_export_private_only (void)
{
    int     ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_KEY_EXPORT)
    ecc_key key;
    WC_RNG  rng;
    byte    out[ECC_PRIV_KEY_BUF];
    word32  outlen = sizeof(out);

    /* Init stack variables. */
    XMEMSET(out, 0, outlen);

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, KEY32, &key);
        }
    }
    printf(testingFmt, "wc_ecc_export_private_only()");

    if (ret == 0) {
        ret = wc_ecc_export_private_only(&key, out, &outlen);
    }
    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_ecc_export_private_only(NULL, out, &outlen);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_export_private_only(&key, NULL, &outlen);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_export_private_only(&key, out, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);

#endif
    return ret;

} /* END test_wc_ecc_export_private_only */


/*
 * Testing wc_ecc_rs_to_sig()
 */
static int test_wc_ecc_rs_to_sig (void)
{

    int           ret = 0;

#if defined(HAVE_ECC) && !defined(NO_ASN)
    /* first [P-192,SHA-1] vector from FIPS 186-3 NIST vectors */
    const char*   R = "6994d962bdd0d793ffddf855ec5bf2f91a9698b46258a63e";
    const char*   S = "02ba6465a234903744ab02bc8521405b73cf5fc00e1a9f41";
    byte          sig[ECC_MAX_SIG_SIZE];
    word32        siglen = (word32)sizeof(sig);
    /*R and S max size is the order of curve. 2^192.*/
    int           keySz = KEY24;
    byte          r[keySz];
    byte          s[keySz];
    word32        rlen = (word32)sizeof(r);
    word32        slen = (word32)sizeof(s);

    /* Init stack variables. */
    XMEMSET(sig, 0, ECC_MAX_SIG_SIZE);
    XMEMSET(r, 0, keySz);
    XMEMSET(s, 0, keySz);

    printf(testingFmt, "wc_ecc_rs_to_sig()");

    ret = wc_ecc_rs_to_sig(R, S, sig, &siglen);
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_rs_to_sig(NULL, S, sig, &siglen);
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_rs_to_sig(R, NULL, sig, &siglen);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_rs_to_sig(R, S, sig, NULL);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_rs_to_sig(R, S, NULL, &siglen);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);
    printf(testingFmt, "wc_ecc_sig_to_rs()");
    if (ret == 0) {
        ret = wc_ecc_sig_to_rs(sig, siglen, r, &rlen, s, &slen);
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_sig_to_rs(NULL, siglen, r, &rlen, s, &slen);
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_sig_to_rs(sig, siglen, NULL, &rlen, s, &slen);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_sig_to_rs(sig, siglen, r, NULL, s, &slen);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_sig_to_rs(sig, siglen, r, &rlen, NULL, &slen);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_sig_to_rs(sig, siglen, r, &rlen, s, NULL);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

#endif
    return ret;

} /* END test_wc_ecc_rs_to_sig */

static int test_wc_ecc_import_raw (void)
{
    int         ret = 0;

#ifdef HAVE_ECC
    ecc_key     key;
#ifdef HAVE_ALL_CURVES
    const char* qx = "07008ea40b08dbe76432096e80a2494c94982d2d5bcf98e6";
    const char* qy = "76fab681d00b414ea636ba215de26d98c41bd7f2e4d65477";
    const char* d  = "e14f37b3d1374ff8b03f41b9b3fdd2f0ebccf275d660d7f3";
    const char* curveName = "SECP192R1";
#else
    const char* qx =
              "6c450448386596485678dcf46ccf75e80ff292443cddab1ff216d0c72cd9341";
    const char* qy =
              "9cac72ff8a90e4939e37714bfa07ae4612588535c3fdeab63ceb29b1d80f0d1";
    const char* d  =
             "1e1dd938e15bdd036b0b0e2a6dc62fe7b46dbe042ac42310c6d5db0cda63e807";
    const char* curveName = "SECP256R1";
#endif

    ret = wc_ecc_init(&key);

    printf(testingFmt, "wc_ecc_import_raw()");

    if (ret == 0) {
        ret = wc_ecc_import_raw(&key, qx, qy, d, curveName);
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_import_raw(NULL, qx, qy, d, curveName);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_import_raw(&key, NULL, qy, d, curveName);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_import_raw(&key, qx, NULL, d, curveName);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_import_raw(&key, qx, qy, d, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_ecc_free(&key);

#endif

    return ret;

} /* END test_wc_ecc_import_raw */


/*
 * Testing wc_ecc_sig_size()
 */
static int test_wc_ecc_sig_size (void)
{
   int         ret = 0;

#ifdef HAVE_ECC
    ecc_key     key;
    WC_RNG      rng;
    int         keySz = KEY16;

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, keySz, &key);
        }
    }

    printf(testingFmt, "wc_ecc_sig_size()");

    if (ret == 0) {
        ret = wc_ecc_sig_size(&key);
        if (ret == (2 * keySz + SIG_HEADER_SZ + ECC_MAX_PAD_SZ)) {
            ret = 0;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);

#endif
    return ret;

} /* END test_wc_ecc_sig_size */

/*
 * Testing wc_ecc_ctx_new()
 */
static int test_wc_ecc_ctx_new (void)
{
    int         ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_ENCRYPT)
    WC_RNG      rng;
    ecEncCtx*   cli = NULL;
    ecEncCtx*   srv = NULL;

    ret = wc_InitRng(&rng);

    printf(testingFmt, "wc_ecc_ctx_new()");
    if (ret == 0) {
        cli = wc_ecc_ctx_new(REQ_RESP_CLIENT, &rng);
        srv = wc_ecc_ctx_new(REQ_RESP_SERVER, &rng);
    }
    if (ret == 0 && (cli == NULL || srv == NULL)) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    wc_ecc_ctx_free(cli);
    wc_ecc_ctx_free(srv);

    /* Test bad args. */
    if (ret == 0) {
        /* wc_ecc_ctx_new_ex() will free if returned NULL. */
        cli = wc_ecc_ctx_new(0, &rng);
        if (cli != NULL) {
            ret = WOLFSSL_FATAL_ERROR;
        }
        cli = wc_ecc_ctx_new(REQ_RESP_CLIENT, NULL);
        if (cli != NULL) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_ctx_free(cli);

#endif
    return ret;

} /* END test_wc_ecc_ctx_new */

/*
 * Tesing wc_ecc_reset()
 */
static int test_wc_ecc_ctx_reset (void)
{
    int         ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_ENCRYPT)
    ecEncCtx*   ctx = NULL;
    WC_RNG      rng;

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        if ( (ctx = wc_ecc_ctx_new(REQ_RESP_CLIENT, &rng)) == NULL ) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(testingFmt, "wc_ecc_ctx_reset()");

    if (ret == 0) {
        ret = wc_ecc_ctx_reset(ctx, &rng);
    }

    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_ecc_ctx_reset(NULL, &rng);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_ctx_reset(ctx, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_ctx_free(ctx);

#endif
    return ret;

} /* END test_wc_ecc_ctx_reset */

/*
 * Testing wc_ecc_ctx_set_peer_salt() and wc_ecc_ctx_get_own_salt()
 */
static int test_wc_ecc_ctx_set_peer_salt (void)
{
    int         ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_ENCRYPT)
    WC_RNG          rng;
    ecEncCtx*       cliCtx      = NULL;
    ecEncCtx*       servCtx     = NULL;
    const byte*     cliSalt     = NULL;
    const byte*     servSalt    = NULL;

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        if ( ( (cliCtx = wc_ecc_ctx_new(REQ_RESP_CLIENT, &rng)) == NULL ) ||
           ( (servCtx = wc_ecc_ctx_new(REQ_RESP_SERVER, &rng)) == NULL) ) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(testingFmt, "wc_ecc_ctx_get_own_salt()");

    /* Test bad args. */
    if (ret == 0) {
        cliSalt  = wc_ecc_ctx_get_own_salt(NULL);
        if (cliSalt != NULL) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    if (ret == 0) {
        cliSalt  = wc_ecc_ctx_get_own_salt(cliCtx);
        servSalt = wc_ecc_ctx_get_own_salt(servCtx);
        if (cliSalt == NULL || servSalt == NULL) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);
    printf(testingFmt, "wc_ecc_ctx_set_peer_salt()");

    if (ret == 0) {
        ret = wc_ecc_ctx_set_peer_salt(cliCtx, servSalt);
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_ctx_set_peer_salt(NULL, servSalt);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_ctx_set_peer_salt(cliCtx, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_ctx_free(cliCtx);
    wc_ecc_ctx_free(servCtx);

#endif
    return ret;

} /* END test_wc_ecc_ctx_set_peer_salt */

/*
 * Testing wc_ecc_ctx_set_info()
 */
static int test_wc_ecc_ctx_set_info (void)
{
    int         ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_ENCRYPT)
    ecEncCtx*   ctx = NULL;
    WC_RNG      rng;
    const char* optInfo = "Optional Test Info.";
    int         optInfoSz = (int)XSTRLEN(optInfo);
    const char* badOptInfo = NULL;

    ret = wc_InitRng(&rng);
    if ( (ctx = wc_ecc_ctx_new(REQ_RESP_CLIENT, &rng)) == NULL || ret != 0 ) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(testingFmt, "wc_ecc_ctx_set_info()");

    if (ret == 0) {
        ret = wc_ecc_ctx_set_info(ctx, (byte*)optInfo, optInfoSz);
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_ctx_set_info(NULL, (byte*)optInfo, optInfoSz);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_ctx_set_info(ctx, (byte*)badOptInfo, optInfoSz);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_ctx_set_info(ctx, (byte*)optInfo, -1);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_ctx_free(ctx);

#endif
    return ret;

} /* END test_wc_ecc_ctx_set_info */

/*
 * Testing wc_ecc_encrypt() and wc_ecc_decrypt()
 */
static int test_wc_ecc_encryptDecrypt (void)
{
    int         ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_ENCRYPT) && defined(WOLFSSL_AES_128)
    ecc_key     srvKey, cliKey;
    WC_RNG      rng;
    const char* msg   = "EccBlock Size 16";
    word32      msgSz = (word32)XSTRLEN(msg);
    byte        out[XSTRLEN(msg) + WC_SHA256_DIGEST_SIZE];
    word32      outSz = (word32)sizeof(out);
    byte        plain[XSTRLEN(msg) + 1];
    word32      plainSz = (word32)sizeof(plain);
    int         keySz = KEY20;

    /* Init stack variables. */
    XMEMSET(out, 0, outSz);
    XMEMSET(plain, 0, plainSz);

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&cliKey);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, keySz, &cliKey);
        }
        if (ret == 0) {
            ret = wc_ecc_init(&srvKey);
        }
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, keySz, &srvKey);
        }
    }

    printf(testingFmt, "wc_ecc_encrypt()");

    if (ret == 0) {
        ret = wc_ecc_encrypt(&cliKey, &srvKey, (byte*)msg, msgSz, out,
                                                            &outSz, NULL);
    }
    if (ret == 0) {
        ret = wc_ecc_encrypt(NULL, &srvKey, (byte*)msg, msgSz, out,
                                                            &outSz, NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_encrypt(&cliKey, NULL, (byte*)msg, msgSz, out,
                                                            &outSz, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_encrypt(&cliKey, &srvKey, NULL, msgSz, out,
                                                            &outSz, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_encrypt(&cliKey, &srvKey, (byte*)msg, msgSz, NULL,
                                                            &outSz, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_encrypt(&cliKey, &srvKey, (byte*)msg, msgSz, out,
                                                            NULL, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);
    printf(testingFmt, "wc_ecc_decrypt()");

    if (ret == 0) {
        ret = wc_ecc_decrypt(&srvKey, &cliKey, out, outSz, plain,
                                                        &plainSz, NULL);
    }
    if (ret == 0) {
        ret = wc_ecc_decrypt(NULL, &cliKey, out, outSz, plain,
                                                        &plainSz, NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_decrypt(&srvKey, NULL, out, outSz, plain,
                                                        &plainSz, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_decrypt(&srvKey, &cliKey, NULL, outSz, plain,
                                                        &plainSz, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_decrypt(&srvKey, &cliKey, out, outSz, NULL,
                                                        &plainSz, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_decrypt(&srvKey, &cliKey, out, outSz,
                                                        plain, NULL, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    if (XMEMCMP(msg, plain, msgSz) != 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&cliKey);
    wc_ecc_free(&srvKey);

#endif
    return ret;

} /* END test_wc_ecc_encryptDecrypt */

/*
 * Testing wc_ecc_del_point() and wc_ecc_new_point()
 */
static int test_wc_ecc_del_point (void)
{
    int         ret = 0;

#if defined(HAVE_ECC)
    ecc_point*   pt;

    printf(testingFmt, "wc_ecc_new_point()");

    pt = wc_ecc_new_point();
    if (!pt) {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    wc_ecc_del_point(pt);

#endif
    return ret;

} /* END test_wc_ecc_del_point */

/*
 * Testing wc_ecc_point_is_at_infinity(), wc_ecc_export_point_der(),
 * wc_ecc_import_point_der(), wc_ecc_copy_point(), and wc_ecc_cmp_point()
 */
static int test_wc_ecc_pointFns (void)
{
    int         ret = 0;

#if defined(HAVE_ECC)
    ecc_key     key;
    WC_RNG      rng;
    ecc_point*  point = NULL;
    ecc_point*  cpypt = NULL;
    int         idx = 0;
    int         keySz = KEY32;
    byte        der[DER_SZ];
    word32      derlenChk = 0;
    word32      derSz = (int)sizeof(der);

    /* Init stack variables. */
    XMEMSET(der, 0, derSz);

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, keySz, &key);
        }
    }

    if (ret == 0) {
        point = wc_ecc_new_point();
        if (!point) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    if (ret == 0) {
        cpypt = wc_ecc_new_point();
        if (!cpypt) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    /* Export */
    printf(testingFmt, "wc_ecc_export_point_der()");
    if (ret == 0) {
        ret = wc_ecc_export_point_der((idx = key.idx), &key.pubkey,
                                                       NULL, &derlenChk);
        /* Check length value. */
        if (derSz == derlenChk && ret == LENGTH_ONLY_E) {
            ret = wc_ecc_export_point_der((idx = key.idx), &key.pubkey,
                                                           der, &derSz);
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_export_point_der(-2, &key.pubkey, der, &derSz);
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_export_point_der((idx = key.idx), NULL, der, &derSz);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_export_point_der((idx = key.idx), &key.pubkey,
                                                                der, NULL);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    /* Import */
    printf(testingFmt, "wc_ecc_import_point_der()");

    if (ret == 0) {
        ret = wc_ecc_import_point_der(der, derSz, idx, point);
        /* Condition double checks wc_ecc_cmp_point().  */
        if (ret == 0 && XMEMCMP(&key.pubkey, point, sizeof(key.pubkey))) {
            ret = wc_ecc_cmp_point(&key.pubkey, point);
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_import_point_der(NULL, derSz, idx, point);
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_import_point_der(der, derSz, idx, NULL);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_import_point_der(der, derSz, -1, point);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_import_point_der(der, derSz + 1, idx, point);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);

    /* Copy */
    printf(testingFmt, "wc_ecc_copy_point()");

    if (ret == 0) {
        ret = wc_ecc_copy_point(point, cpypt);
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_copy_point(NULL, cpypt);
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_copy_point(point, NULL);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    printf(testingFmt, "wc_ecc_cmp_point()");
    /* Compare point */
    if (ret == 0) {
        ret = wc_ecc_cmp_point(point, cpypt);
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_cmp_point(NULL, cpypt);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_cmp_point(point, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, ret == 0 ? passed : failed);

    printf(testingFmt, "wc_ecc_point_is_at_infinity()");
    /* At infinity if return == 1, otherwise return == 0. */
    if (ret == 0) {
        ret = wc_ecc_point_is_at_infinity(point);
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_point_is_at_infinity(NULL);
        if (ret == BAD_FUNC_ARG) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    /* Free */
    wc_ecc_del_point(point);
    wc_ecc_del_point(cpypt);
    wc_ecc_free(&key);
    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }

#endif
    return ret;

} /* END test_wc_ecc_pointFns */


/*
 * Testing wc_ecc_sahred_secret_ssh()
 */
static int test_wc_ecc_shared_secret_ssh (void)
{
    int         ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_DHE)
    ecc_key     key, key2;
    WC_RNG      rng;
    int         keySz = KEY32;
    int         key2Sz = KEY24;
    byte        secret[keySz];
    word32      secretLen = keySz;

    /* Init stack variables. */
    XMEMSET(secret, 0, secretLen);

    /* Make keys */
    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, keySz, &key);
        }
        if (wc_FreeRng(&rng) && ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    if (ret == 0) {
        ret = wc_InitRng(&rng);
        if (ret == 0) {
            ret = wc_ecc_init(&key2);
        }
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, key2Sz, &key2);
        }
    }

    printf(testingFmt, "ecc_shared_secret_ssh()");

    if (ret == 0) {
        ret = wc_ecc_shared_secret_ssh(&key, &key2.pubkey, secret, &secretLen);
    }
    /* Pass in bad args. */
    if (ret == 0) {
        ret = wc_ecc_shared_secret_ssh(NULL, &key2.pubkey, secret, &secretLen);
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_shared_secret_ssh(&key, NULL, secret, &secretLen);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_shared_secret_ssh(&key, &key2.pubkey, NULL, &secretLen);
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_ecc_shared_secret_ssh(&key, &key2.pubkey, secret, NULL);
        }
        if (ret == BAD_FUNC_ARG) {
            key.type = ECC_PUBLICKEY;
            ret = wc_ecc_shared_secret_ssh(&key, &key2.pubkey, secret, &secretLen);
            if (ret == ECC_BAD_ARG_E) {
                ret = 0;
            } else if (ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);
    wc_ecc_free(&key2);

#endif
    return ret;

} /* END test_wc_ecc_shared_secret_ssh */

/*
 * Testing wc_ecc_verify_hash_ex() and wc_ecc_verify_hash_ex()
 */
static int test_wc_ecc_verify_hash_ex (void)
{
    int             ret = 0;

#if defined(HAVE_ECC) && defined(HAVE_ECC_SIGN) && defined(WOLFSSL_PUBLIC_MP)
    ecc_key         key;
    WC_RNG          rng;
    mp_int          r;
    mp_int          s;
    unsigned char   hash[] = "Everyone gets Friday off.EccSig";
    unsigned char   iHash[] = "Everyone gets Friday off.......";
    unsigned char   shortHash[] = "Everyone gets Friday off.";
    word32          hashlen = sizeof(hash);
    word32          iHashLen = sizeof(iHash);
    word32          shortHashLen = sizeof(shortHash);
    int             keySz = KEY32;
    int             sig = WOLFSSL_FATAL_ERROR;
    int             ver = WOLFSSL_FATAL_ERROR;
    int             stat = 0;

    /* Initialize r and s. */
    ret = mp_init_multi(&r, &s, NULL, NULL, NULL, NULL);
    if (ret != MP_OKAY) {
        return MP_INIT_E;
    }

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, keySz, &key);
        }
    }
    if (ret == 0) {
        ret = wc_ecc_sign_hash_ex(hash, hashlen, &rng, &key, &r, &s);
        if (ret == 0) {
            /* stat should be 1. */
            ret = wc_ecc_verify_hash_ex(&r, &s, hash, hashlen, &stat, &key);
            if (stat != 1 && ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
        if (ret == 0) {
            /* stat should be 0 */
            ret = wc_ecc_verify_hash_ex(&r, &s, iHash, iHashLen,
                                                    &stat, &key);
            if (stat != 0 && ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
        if (ret == 0) {
            /* stat should be 0. */
            ret = wc_ecc_verify_hash_ex(&r, &s, shortHash, shortHashLen,
                                                            &stat, &key);
            if (stat != 0 && ret == 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    }

    printf(testingFmt, "wc_ecc_sign_hash_ex()");
    /* Test bad args. */
    if (ret == 0) {
        if (wc_ecc_sign_hash_ex(NULL, hashlen, &rng, &key, &r, &s)
                                                == ECC_BAD_ARG_E) {
            sig = 0;
        }
        if (sig == 0 && wc_ecc_sign_hash_ex(hash, hashlen, NULL, &key, &r, &s)
                                                            != ECC_BAD_ARG_E) {
            sig = WOLFSSL_FATAL_ERROR;
        }
        if (sig == 0 && wc_ecc_sign_hash_ex(hash, hashlen, &rng, NULL, &r, &s)
                                                            != ECC_BAD_ARG_E) {
            sig = WOLFSSL_FATAL_ERROR;
        }
        if (sig == 0 && wc_ecc_sign_hash_ex(hash, hashlen, &rng, &key, NULL, &s)
                                                            != ECC_BAD_ARG_E) {
            sig = WOLFSSL_FATAL_ERROR;
        }
        if (sig == 0 && wc_ecc_sign_hash_ex(hash, hashlen, &rng, &key, &r, NULL)
                                                            != ECC_BAD_ARG_E) {
            sig = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, sig == 0 ? passed : failed);
    printf(testingFmt, "wc_ecc_verify_hash_ex()");
    /* Test bad args. */
    if (ret == 0) {
        if (wc_ecc_verify_hash_ex(NULL, &s, shortHash, shortHashLen, &stat, &key)
                                                            == ECC_BAD_ARG_E) {
            ver = 0;
        }
        if (ver == 0 && wc_ecc_verify_hash_ex(&r, NULL, shortHash, shortHashLen,
                                                &stat, &key) != ECC_BAD_ARG_E) {
            ver = WOLFSSL_FATAL_ERROR;
        }
        if (ver == 0 && wc_ecc_verify_hash_ex(&r, &s, NULL, shortHashLen, &stat,
                                                       &key) != ECC_BAD_ARG_E) {
            ver = WOLFSSL_FATAL_ERROR;
        }
        if (ver == 0 && wc_ecc_verify_hash_ex(&r, &s, shortHash, shortHashLen,
                                                NULL, &key) != ECC_BAD_ARG_E) {
            ver = WOLFSSL_FATAL_ERROR;
        }
        if (ver == 0 && wc_ecc_verify_hash_ex(&r, &s, shortHash, shortHashLen,
                                                &stat, NULL) != ECC_BAD_ARG_E) {
            ver = WOLFSSL_FATAL_ERROR;
        }
    }
    printf(resultFmt, ver == 0 ? passed : failed);

    wc_ecc_free(&key);
    mp_free(&r);
    mp_free(&s);
    if (wc_FreeRng(&rng)) {
        return WOLFSSL_FATAL_ERROR;
    }
    if (ret == 0 && (sig != 0 || ver != 0)) {
        ret = WOLFSSL_FATAL_ERROR;
    }
#endif
    return ret;

} /* END test_wc_ecc_verify_hash_ex */

/*
 * Testing wc_ecc_mulmod()
 */

static int test_wc_ecc_mulmod (void)
{
    int         ret = 0;

#if defined(HAVE_ECC) && !defined(WOLFSSL_ATECC508A)
    ecc_key     key1, key2, key3;
    WC_RNG      rng;

    ret = wc_InitRng(&rng);
    if (ret == 0) {
        if (ret == 0) {
            ret = wc_ecc_init(&key1);
        }
        if (ret == 0) {
            ret = wc_ecc_init(&key2);
        }
        if (ret == 0) {
            ret = wc_ecc_init(&key3);
        }
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, KEY32, &key1);
        }
    }
    if (ret == 0) {
        ret = wc_ecc_import_raw_ex(&key2, key1.dp->Gx, key1.dp->Gy, key1.dp->Af,
                                                                 ECC_SECP256R1);
        if (ret == 0) {
            ret = wc_ecc_import_raw_ex(&key3, key1.dp->Gx, key1.dp->Gy,
                                        key1.dp->prime, ECC_SECP256R1);
        }
    }

    printf(testingFmt, "wc_ecc_mulmod()");
    if (ret == 0) {
        ret = wc_ecc_mulmod(&key1.k, &key2.pubkey, &key3.pubkey, &key2.k,
                                                            &key3.k, 1);
    }

    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_mulmod(NULL, &key2.pubkey, &key3.pubkey, &key2.k,
                                                            &key3.k, 1);
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_mulmod(&key1.k, NULL, &key3.pubkey, &key2.k,
                                                            &key3.k, 1);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_mulmod(&key1.k, &key2.pubkey, NULL, &key2.k,
                                                            &key3.k, 1);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = wc_ecc_mulmod(&key1.k, &key2.pubkey, &key3.pubkey,
                                                            &key2.k, NULL, 1);
        }
        if (ret == ECC_BAD_ARG_E) {
            ret = 0;
        } else if (ret == 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key1);
    wc_ecc_free(&key2);
    wc_ecc_free(&key3);


#endif /* HAVE_ECC && !WOLFSSL_ATECC508A */
    return ret;


} /* END test_wc_ecc_mulmod */

/*
 * Testing wc_ecc_is_valid_idx()
 */
static int test_wc_ecc_is_valid_idx (void)
{
    int         ret = 0;

#if defined(HAVE_ECC)
    ecc_key     key;
    WC_RNG      rng;
    int         iVal = -2;
    int         iVal2 = 3000;


    ret = wc_InitRng(&rng);
    if (ret == 0) {
        ret = wc_ecc_init(&key);
        if (ret == 0) {
            ret = wc_ecc_make_key(&rng, 32, &key);
        }
    }

    printf(testingFmt, "wc_ecc_is_valid_idx()");
    if (ret == 0) {
        ret = wc_ecc_is_valid_idx(key.idx);
        if (ret == 1) {
            ret = 0;
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }
    /* Test bad args. */
    if (ret == 0) {
        ret = wc_ecc_is_valid_idx(iVal); /* should return 0 */
        if (ret == 0) {
            ret = wc_ecc_is_valid_idx(iVal2);
        }
        if (ret != 0) {
            ret = WOLFSSL_FATAL_ERROR;
        }
    }

    printf(resultFmt, ret == 0 ? passed : failed);

    if (wc_FreeRng(&rng) && ret == 0) {
        ret = WOLFSSL_FATAL_ERROR;
    }
    wc_ecc_free(&key);

#endif
    return ret;


} /* END test_wc_ecc_is_valid_idx */


/*
 * Testing wc_PKCS7_Init()
 */
static void test_wc_PKCS7_Init (void)
{
#if defined(HAVE_PKCS7)
    PKCS7       pkcs7;
    void*       heap = NULL;

    printf(testingFmt, "wc_PKCS7_Init()");

    AssertIntEQ(wc_PKCS7_Init(&pkcs7, heap, devId), 0);

    /* Pass in bad args. */
    AssertIntEQ(wc_PKCS7_Init(NULL, heap, devId), BAD_FUNC_ARG);

    printf(resultFmt, passed);
    wc_PKCS7_Free(&pkcs7);
#endif
} /* END test-wc_PKCS7_Init */


/*
 * Testing wc_PKCS7_InitWithCert()
 */
static void test_wc_PKCS7_InitWithCert (void)
{
#if defined(HAVE_PKCS7)
    PKCS7       pkcs7;

#ifndef NO_RSA
    #if defined(USE_CERT_BUFFERS_2048)
        unsigned char    cert[sizeof_client_cert_der_2048];
        int              certSz = (int)sizeof(cert);
        XMEMSET(cert, 0, certSz);
        XMEMCPY(cert, client_cert_der_2048, sizeof_client_cert_der_2048);
    #elif defined(USE_CERT_BUFFERS_1024)
        unsigned char    cert[sizeof_client_cert_der_1024];
        int              certSz = (int)sizeof(cert);
        XMEMSET(cert, 0, certSz);
        XMEMCPY(cert, client_cert_der_1024, sizeof_client_cert_der_1024);
    #else
        unsigned char   cert[ONEK_BUF];
        FILE*           fp;
        int             certSz;
        fp = fopen("./certs/1024/client-cert.der", "rb");

        AssertNotNull(fp);

        certSz = fread(cert, 1, sizeof_client_cert_der_1024, fp);
        fclose(fp);
    #endif
#elif defined(HAVE_ECC)
    #if defined(USE_CERT_BUFFERS_256)
        unsigned char    cert[sizeof_cliecc_cert_der_256];
        int              certSz = (int)sizeof(cert);
        XMEMSET(cert, 0, certSz);
        XMEMCPY(cert, cliecc_cert_der_256, sizeof_cliecc_cert_der_256);
    #else
        unsigned char   cert[ONEK_BUF];
        FILE*           fp;
        int             certSz;
        fp = fopen("./certs/client-ecc-cert.der", "rb");

        AssertNotNull(fp);

        certSz = fread(cert, 1, sizeof_cliecc_cert_der_256, fp);
        fclose(fp);
    #endif
#else
        #error PKCS7 requires ECC or RSA
#endif
    printf(testingFmt, "wc_PKCS7_InitWithCert()");
    /* If initialization is not successful, it's free'd in init func. */
    AssertIntEQ(wc_PKCS7_InitWithCert(&pkcs7, (byte*)cert, (word32)certSz), 0);

    wc_PKCS7_Free(&pkcs7);

    /* Valid initialization usage. */
    AssertIntEQ(wc_PKCS7_InitWithCert(&pkcs7, NULL, 0), 0);

    /* Pass in bad args. No need free for null checks, free at end.*/
    AssertIntEQ(wc_PKCS7_InitWithCert(NULL, (byte*)cert, (word32)certSz),
                                                           BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_InitWithCert(&pkcs7, NULL, (word32)certSz),
                                                      BAD_FUNC_ARG);

    printf(resultFmt, passed);

    wc_PKCS7_Free(&pkcs7);
#endif
} /* END test_wc_PKCS7_InitWithCert */


/*
 * Testing wc_PKCS7_EncodeData()
 */
static void test_wc_PKCS7_EncodeData (void)
{
#if defined(HAVE_PKCS7)
    PKCS7       pkcs7;
    byte        output[FOURK_BUF];
    byte        data[] = "My encoded DER cert.";

#ifndef NO_RSA
    #if defined(USE_CERT_BUFFERS_2048)
        unsigned char cert[sizeof_client_cert_der_2048];
        unsigned char key[sizeof_client_key_der_2048];
        int certSz = (int)sizeof(cert);
        int keySz = (int)sizeof(key);
        XMEMSET(cert, 0, certSz);
        XMEMSET(key, 0, keySz);
        XMEMCPY(cert, client_cert_der_2048, certSz);
        XMEMCPY(key, client_key_der_2048, keySz);

    #elif defined(USE_CERT_BUFFERS_1024)
        unsigned char cert[sizeof_client_cert_der_1024];
        unsigned char key[sizeof_client_key_der_1024];
        int certSz = (int)sizeof(cert);
        int keySz = (int)sizeof(key);
        XMEMSET(cert, 0, certSz);
        XMEMSET(key, 0, keySz);
        XMEMCPY(cert, client_cert_der_1024, certSz);
        XMEMCPY(key, client_key_der_1024, keySz);
    #else
        unsigned char   cert[ONEK_BUF];
        unsigned char   key[ONEK_BUF];
        FILE*           fp;
        int             certSz;
        int             keySz;

        fp = fopen("./certs/1024/client-cert.der", "rb");
        AssertNotNull(fp);
        certSz = fread(cert, 1, sizeof_client_cert_der_1024, fp);
        fclose(fp);

        fp = fopen("./certs/1024/client-key.der", "rb");
        AssertNotNull(fp);
        keySz = fread(key, 1, sizeof_client_key_der_1024, fp);
        fclose(fp);
    #endif
#elif defined(HAVE_ECC)
    #if defined(USE_CERT_BUFFERS_256)
        unsigned char    cert[sizeof_cliecc_cert_der_256];
        unsigned char    key[sizeof_ecc_clikey_der_256];
        int              certSz = (int)sizeof(cert);
        int              keySz = (int)sizeof(key);
        XMEMSET(cert, 0, certSz);
        XMEMSET(key, 0, keySz);
        XMEMCPY(cert, cliecc_cert_der_256, sizeof_cliecc_cert_der_256);
        XMEMCPY(key, ecc_clikey_der_256, sizeof_ecc_clikey_der_256);
    #else
        unsigned char   cert[ONEK_BUF];
        unsigned char   key[ONEK_BUF];
        FILE*           fp;
        int             certSz, keySz;

        fp = fopen("./certs/client-ecc-cert.der", "rb");
        AssertNotNull(fp);
        certSz = fread(cert, 1, sizeof_cliecc_cert_der_256, fp);
        fclose(fp);

        fp = fopen("./certs/client-ecc-key.der", "rb");
        AssertNotNull(fp);
        keySz = fread(key, 1, sizeof_ecc_clikey_der_256, fp);
        fclose(fp);
    #endif
#endif

    XMEMSET(output, 0, sizeof(output));

    AssertIntEQ(wc_PKCS7_InitWithCert(&pkcs7, (byte*)cert, certSz), 0);

    printf(testingFmt, "wc_PKCS7_EncodeData()");

    pkcs7.content = data;
    pkcs7.contentSz = sizeof(data);
    pkcs7.privateKey = key;
    pkcs7.privateKeySz = keySz;
    AssertIntGT(wc_PKCS7_EncodeData(&pkcs7, output, (word32)sizeof(output)), 0);

    /* Test bad args. */
    AssertIntEQ(wc_PKCS7_EncodeData(NULL, output, (word32)sizeof(output)),
                                                            BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_EncodeData(&pkcs7, NULL, (word32)sizeof(output)),
                                                            BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_EncodeData(&pkcs7, output, 5), BUFFER_E);

    printf(resultFmt, passed);

    wc_PKCS7_Free(&pkcs7);
#endif
}  /* END test_wc_PKCS7_EncodeData */


/*
 * Testing wc_PKCS7_EncodeSignedData()
 */
static void test_wc_PKCS7_EncodeSignedData (void)
{
#if defined(HAVE_PKCS7)
    PKCS7       pkcs7;
    WC_RNG      rng;
    byte        output[FOURK_BUF];
    byte        badOut[0];
    word32      outputSz = (word32)sizeof(output);
    word32      badOutSz = (word32)sizeof(badOut);
    byte        data[] = "Test data to encode.";

#ifndef NO_RSA
    #if defined(USE_CERT_BUFFERS_2048)
        byte        key[sizeof_client_key_der_2048];
        byte        cert[sizeof_client_cert_der_2048];
        word32      keySz = (word32)sizeof(key);
        word32      certSz = (word32)sizeof(cert);
        XMEMSET(key, 0, keySz);
        XMEMSET(cert, 0, certSz);
        XMEMCPY(key, client_key_der_2048, keySz);
        XMEMCPY(cert, client_cert_der_2048, certSz);
    #elif defined(USE_CERT_BUFFERS_1024)
        byte        key[sizeof_client_key_der_1024];
        byte        cert[sizeof_client_cert_der_1024];
        word32      keySz = (word32)sizeof(key);
        word32      certSz = (word32)sizeof(cert);
        XMEMSET(key, 0, keySz);
        XMEMSET(cert, 0, certSz);
        XMEMCPY(key, client_key_der_1024, keySz);
        XMEMCPY(cert, client_cert_der_1024, certSz);
    #else
        unsigned char   cert[ONEK_BUF];
        unsigned char   key[ONEK_BUF];
        FILE*           fp;
        int             certSz;
        int             keySz;

        fp = fopen("./certs/1024/client-cert.der", "rb");
        AssertNotNull(fp);
        certSz = fread(cert, 1, sizeof_client_cert_der_1024, fp);
        fclose(fp);

        fp = fopen("./certs/1024/client-key.der", "rb");
        AssertNotNull(fp);
        keySz = fread(key, 1, sizeof_client_key_der_1024, fp);
        fclose(fp);
    #endif
#elif defined(HAVE_ECC)
    #if defined(USE_CERT_BUFFERS_256)
        unsigned char    cert[sizeof_cliecc_cert_der_256];
        unsigned char    key[sizeof_ecc_clikey_der_256];
        int              certSz = (int)sizeof(cert);
        int              keySz = (int)sizeof(key);
        XMEMSET(cert, 0, certSz);
        XMEMSET(key, 0, keySz);
        XMEMCPY(cert, cliecc_cert_der_256, sizeof_cliecc_cert_der_256);
        XMEMCPY(key, ecc_clikey_der_256, sizeof_ecc_clikey_der_256);
    #else
        unsigned char   cert[ONEK_BUF];
        unsigned char   key[ONEK_BUF];
        FILE*           fp;
        int             certSz, keySz;

        fp = fopen("./certs/client-ecc-cert.der", "rb");
        AssertNotNull(fp);
        certSz = fread(cert, 1, sizeof_cliecc_cert_der_256, fp);
        fclose(fp);

        fp = fopen("./certs/client-ecc-key.der", "rb");
        AssertNotNull(fp);
        keySz = fread(key, 1, sizeof_ecc_clikey_der_256, fp);
        fclose(fp);
    #endif
#endif

    XMEMSET(output, 0, outputSz);
    AssertIntEQ(wc_InitRng(&rng), 0);

    AssertIntEQ(wc_PKCS7_InitWithCert(&pkcs7, cert, certSz), 0);

    printf(testingFmt, "wc_PKCS7_EncodeSignedData()");

    pkcs7.content = data;
    pkcs7.contentSz = (word32)sizeof(data);
    pkcs7.privateKey = key;
    pkcs7.privateKeySz = (word32)sizeof(key);
    pkcs7.encryptOID = RSAk;
    pkcs7.hashOID = SHAh;
    pkcs7.rng = &rng;

    AssertIntGT(wc_PKCS7_EncodeSignedData(&pkcs7, output, outputSz), 0);

    wc_PKCS7_Free(&pkcs7);
    AssertIntEQ(wc_PKCS7_InitWithCert(&pkcs7, NULL, 0), 0);
    AssertIntEQ(wc_PKCS7_VerifySignedData(&pkcs7, output, outputSz), 0);

    /* Pass in bad args. */
    AssertIntEQ(wc_PKCS7_EncodeSignedData(NULL, output, outputSz), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_EncodeSignedData(&pkcs7, NULL, outputSz), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_EncodeSignedData(&pkcs7, badOut,
                                badOutSz), BAD_FUNC_ARG);

    printf(resultFmt, passed);

    wc_PKCS7_Free(&pkcs7);
    wc_FreeRng(&rng);

#endif
} /* END test_wc_PKCS7_EncodeSignedData */


/*
 * Testing wc_PKCS_VerifySignedData()
 */
static void test_wc_PKCS7_VerifySignedData(void)
{
#if defined(HAVE_PKCS7)
    PKCS7       pkcs7;
    WC_RNG      rng;
    byte        output[FOURK_BUF];
    byte        badOut[0];
    word32      outputSz = (word32)sizeof(output);
    word32      badOutSz = (word32)sizeof(badOut);
    byte        data[] = "Test data to encode.";

#ifndef NO_RSA
    #if defined(USE_CERT_BUFFERS_2048)
        byte        key[sizeof_client_key_der_2048];
        byte        cert[sizeof_client_cert_der_2048];
        word32      keySz = (word32)sizeof(key);
        word32      certSz = (word32)sizeof(cert);
        XMEMSET(key, 0, keySz);
        XMEMSET(cert, 0, certSz);
        XMEMCPY(key, client_key_der_2048, keySz);
        XMEMCPY(cert, client_cert_der_2048, certSz);
    #elif defined(USE_CERT_BUFFERS_1024)
        byte        key[sizeof_client_key_der_1024];
        byte        cert[sizeof_client_cert_der_1024];
        word32      keySz = (word32)sizeof(key);
        word32      certSz = (word32)sizeof(cert);
        XMEMSET(key, 0, keySz);
        XMEMSET(cert, 0, certSz);
        XMEMCPY(key, client_key_der_1024, keySz);
        XMEMCPY(cert, client_cert_der_1024, certSz);
    #else
        unsigned char   cert[ONEK_BUF];
        unsigned char   key[ONEK_BUF];
        FILE*           fp;
        int             certSz;
        int             keySz;

        fp = fopen("./certs/1024/client-cert.der", "rb");
        AssertNotNull(fp);
        certSz = fread(cert, 1, sizeof_client_cert_der_1024, fp);
        fclose(fp);

        fp = fopen("./certs/1024/client-key.der", "rb");
        AssertNotNull(fp);
        keySz = fread(key, 1, sizeof_client_key_der_1024, fp);
        fclose(fp);
    #endif
#elif defined(HAVE_ECC)
    #if defined(USE_CERT_BUFFERS_256)
        unsigned char    cert[sizeof_cliecc_cert_der_256];
        unsigned char    key[sizeof_ecc_clikey_der_256];
        int              certSz = (int)sizeof(cert);
        int              keySz = (int)sizeof(key);
        XMEMSET(cert, 0, certSz);
        XMEMSET(key, 0, keySz);
        XMEMCPY(cert, cliecc_cert_der_256, sizeof_cliecc_cert_der_256);
        XMEMCPY(key, ecc_clikey_der_256, sizeof_ecc_clikey_der_256);
    #else
        unsigned char   cert[ONEK_BUF];
        unsigned char   key[ONEK_BUF];
        FILE*           fp;
        int             certSz, keySz;

        fp = fopen("./certs/client-ecc-cert.der", "rb");
        AssertNotNull(fp);
        certSz = fread(cert, 1, sizeof_cliecc_cert_der_256, fp);
        fclose(fp);

        fp = fopen("./certs/client-ecc-key.der", "rb");
        AssertNotNull(fp);
        keySz = fread(key, 1, sizeof_ecc_clikey_der_256, fp);
        fclose(fp);
    #endif
#endif

    XMEMSET(output, 0, outputSz);
    AssertIntEQ(wc_InitRng(&rng), 0);

    AssertIntEQ(wc_PKCS7_InitWithCert(&pkcs7, cert, certSz), 0);

    printf(testingFmt, "wc_PKCS7_VerifySignedData()");

    pkcs7.content = data;
    pkcs7.contentSz = (word32)sizeof(data);
    pkcs7.privateKey = key;
    pkcs7.privateKeySz = (word32)sizeof(key);
    pkcs7.encryptOID = RSAk;
    pkcs7.hashOID = SHAh;
    pkcs7.rng = &rng;

    AssertIntGT(wc_PKCS7_EncodeSignedData(&pkcs7, output, outputSz), 0);
    wc_PKCS7_Free(&pkcs7);
    AssertIntEQ(wc_PKCS7_InitWithCert(&pkcs7, NULL, 0), 0);
    AssertIntEQ(wc_PKCS7_VerifySignedData(&pkcs7, output, outputSz), 0);

    /* Test bad args. */
    AssertIntEQ(wc_PKCS7_VerifySignedData(NULL, output, outputSz), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_VerifySignedData(&pkcs7, NULL, outputSz), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_VerifySignedData(&pkcs7, badOut,
                                badOutSz), BAD_FUNC_ARG);

    printf(resultFmt, passed);

    wc_PKCS7_Free(&pkcs7);
    wc_FreeRng(&rng);
#endif
} /* END test_wc_PKCS7_VerifySignedData() */


/*
 * Testing wc_PKCS7_EncodeEnvelopedData()
 */
static void test_wc_PKCS7_EncodeDecodeEnvelopedData (void)
{
#if defined(HAVE_PKCS7)
    PKCS7       pkcs7;
    word32      tempWrd32   = 0;
    byte*       tmpBytePtr = NULL;
    const char  input[] = "Test data to encode.";
    int         i;
    int         testSz = 0;
    #if !defined(NO_RSA) && (!defined(NO_AES) || (!defined(NO_SHA) ||\
        !defined(NO_SHA256) || !defined(NO_SHA512)))

        byte*   rsaCert     = NULL;
        byte*   rsaPrivKey  = NULL;
        word32  rsaCertSz;
        word32  rsaPrivKeySz;
        #if !defined(NO_FILESYSTEM) && (!defined(USE_CERT_BUFFERS_1024) && \
                                           !defined(USE_CERT_BUFFERS_2048) )
            static const char* rsaClientCert = "./certs/client-cert.der";
            static const char* rsaClientKey = "./certs/client-key.der";
            rsaCertSz = (word32)sizeof(rsaClientCert);
            rsaPrivKeySz = (word32)sizeof(rsaClientKey);
        #endif
    #endif
    #if defined(HAVE_ECC) && (!defined(NO_AES) || (!defined(NO_SHA) ||\
        !defined(NO_SHA256) || !defined(NO_SHA512)))

        byte*   eccCert     = NULL;
        byte*   eccPrivKey  = NULL;
        word32  eccCertSz;
        word32  eccPrivKeySz;
        #if !defined(NO_FILESYSTEM) && !defined(USE_CERT_BUFFERS_256)
            static const char* eccClientCert = "./certs/client-ecc-cert.der";
            static const char* eccClientKey = "./certs/ecc-client-key.der";
        #endif
    #endif
    /* Generic buffer size. */
    byte    output[ONEK_BUF];
    byte    decoded[sizeof(input)/sizeof(char)];
    int     decodedSz = 0;
#ifndef NO_FILESYSTEM
    FILE* certFile;
    FILE* keyFile;
#endif

#if !defined(NO_RSA) && (!defined(NO_AES) || (!defined(NO_SHA) ||\
    !defined(NO_SHA256) || !defined(NO_SHA512)))
    /* RSA certs and keys. */
    #if defined(USE_CERT_BUFFERS_1024)
        /* Allocate buffer space. */
        AssertNotNull(rsaCert =
                (byte*)XMALLOC(ONEK_BUF, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER));
        /* Init buffer. */
        rsaCertSz = (word32)sizeof_client_cert_der_1024;
        XMEMCPY(rsaCert, client_cert_der_1024, rsaCertSz);
        AssertNotNull(rsaPrivKey = (byte*)XMALLOC(ONEK_BUF, HEAP_HINT,
                                DYNAMIC_TYPE_TMP_BUFFER));
        rsaPrivKeySz = (word32)sizeof_client_key_der_1024;
        XMEMCPY(rsaPrivKey, client_key_der_1024, rsaPrivKeySz);

    #elif defined(USE_CERT_BUFFERS_2048)
        /* Allocate buffer */
        AssertNotNull(rsaCert =
                (byte*)XMALLOC(TWOK_BUF, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER));
        /* Init buffer. */
        rsaCertSz = (word32)sizeof_client_cert_der_2048;
        XMEMCPY(rsaCert, client_cert_der_2048, rsaCertSz);
        AssertNotNull(rsaPrivKey = (byte*)XMALLOC(TWOK_BUF, HEAP_HINT,
                                DYNAMIC_TYPE_TMP_BUFFER));
        rsaPrivKeySz = (word32)sizeof_client_key_der_2048;
        XMEMCPY(rsaPrivKey, client_key_der_2048, rsaPrivKeySz);

    #else
        /* File system. */
        certFile = fopen(rsaClientCert, "rb");
        AssertNotNull(certFile);
        rsaCertSz = (word32)FOURK_BUF;
        AssertNotNull(rsaCert =
                (byte*)XMALLOC(FOURK_BUF, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER));
        rsaCertSz = (word32)fread(rsaCert, 1, rsaCertSz, certFile);
        fclose(certFile);
        keyFile = fopen(rsaClientKey, "rb");
        AssertNotNull(keyFile);
        AssertNotNull(rsaPrivKey = (byte*)XMALLOC(FOURK_BUF, HEAP_HINT,
                                DYNAMIC_TYPE_TMP_BUFFER));
        rsaPrivKeySz = (word32)FOURK_BUF;
        rsaPrivKeySz = (word32)fread(rsaPrivKey, 1, rsaPrivKeySz, keyFile);
        fclose(keyFile);
    #endif /* USE_CERT_BUFFERS */
#endif /* NO_RSA */

/* ECC */
#if defined(HAVE_ECC) && (!defined(NO_AES) || (!defined(NO_SHA) ||\
    !defined(NO_SHA256) || !defined(NO_SHA512)))

    #ifdef USE_CERT_BUFFERS_256
        AssertNotNull(eccCert =
                (byte*)XMALLOC(TWOK_BUF, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER));
        /* Init buffer. */
        eccCertSz = (word32)sizeof_cliecc_cert_der_256;
        XMEMCPY(eccCert, cliecc_cert_der_256, eccCertSz);
        AssertNotNull(eccPrivKey = (byte*)XMALLOC(TWOK_BUF, HEAP_HINT,
                                DYNAMIC_TYPE_TMP_BUFFER));
        eccPrivKeySz = (word32)sizeof_ecc_clikey_der_256;
        XMEMCPY(eccPrivKey, ecc_clikey_der_256, eccPrivKeySz);
    #else /* File system. */
        certFile = fopen(eccClientCert, "rb");
        AssertNotNull(certFile);
        eccCertSz = (word32)FOURK_BUF;
        AssertNotNull(eccCert =
                (byte*)XMALLOC(FOURK_BUF, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER));
        eccCertSz = (word32)fread(eccCert, 1, eccCertSz, certFile);
        fclose(certFile);
        keyFile = fopen(eccClientKey, "rb");
        AssertNotNull(keyFile);
        eccPrivKeySz = (word32)FOURK_BUF;
        AssertNotNull(eccPrivKey = (byte*)XMALLOC(FOURK_BUF, HEAP_HINT,
                                DYNAMIC_TYPE_TMP_BUFFER));
        eccPrivKeySz = (word32)fread(eccPrivKey, 1, eccPrivKeySz, keyFile);
        fclose(keyFile);
    #endif /* USE_CERT_BUFFERS_256 */
#endif /* END HAVE_ECC */

    /* Silence. */
    (void)keyFile;
    (void)certFile;

    const pkcs7EnvelopedVector testVectors[] = {
    /* DATA is a global variable defined in the makefile. */
#if !defined(NO_RSA)
    #ifndef NO_DES3
        {(byte*)input, (word32)(sizeof(input)/sizeof(char)), DATA, DES3b, 0, 0,
            rsaCert, rsaCertSz, rsaPrivKey, rsaPrivKeySz},
    #endif /* NO_DES3 */
    #ifndef NO_AES
        {(byte*)input, (word32)(sizeof(input)/sizeof(char)), DATA, AES128CBCb,
            0, 0, rsaCert, rsaCertSz, rsaPrivKey, rsaPrivKeySz},
        {(byte*)input, (word32)(sizeof(input)/sizeof(char)), DATA, AES192CBCb,
            0, 0, rsaCert, rsaCertSz, rsaPrivKey, rsaPrivKeySz},
        {(byte*)input, (word32)(sizeof(input)/sizeof(char)), DATA, AES256CBCb,
            0, 0, rsaCert, rsaCertSz, rsaPrivKey, rsaPrivKeySz},
    #endif /* NO_AES */

#endif /* NO_RSA */
#if defined(HAVE_ECC)
    #ifndef NO_AES
        #ifndef NO_SHA
            {(byte*)input, (word32)(sizeof(input)/sizeof(char)), DATA, AES128CBCb,
                AES128_WRAP, dhSinglePass_stdDH_sha1kdf_scheme, eccCert,
                eccCertSz, eccPrivKey, eccPrivKeySz},
        #endif
        #ifndef NO_SHA256
            {(byte*)input, (word32)(sizeof(input)/sizeof(char)), DATA, AES256CBCb,
                AES256_WRAP, dhSinglePass_stdDH_sha256kdf_scheme, eccCert,
                eccCertSz, eccPrivKey, eccPrivKeySz},
        #endif
        #ifdef WOLFSSL_SHA512
            {(byte*)input, (word32)(sizeof(input)/sizeof(char)), DATA, AES256CBCb,
                AES256_WRAP, dhSinglePass_stdDH_sha512kdf_scheme, eccCert,
                eccCertSz, eccPrivKey, eccPrivKeySz},
        #endif
    #endif /* NO_AES */
#endif /* END HAVE_ECC */
    }; /* END pkcs7EnvelopedVector */

    printf(testingFmt, "wc_PKCS7_EncodeEnvelopedData()");

    AssertIntEQ(wc_PKCS7_Init(&pkcs7, HEAP_HINT, devId), 0);

    testSz = (int)sizeof(testVectors)/(int)sizeof(pkcs7EnvelopedVector);
    for (i = 0; i < testSz; i++) {
        AssertIntEQ(wc_PKCS7_InitWithCert(&pkcs7, (testVectors + i)->cert,
                                    (word32)(testVectors + i)->certSz), 0);

        pkcs7.content       = (byte*)(testVectors + i)->content;
        pkcs7.contentSz     = (testVectors + i)->contentSz;
        pkcs7.contentOID    = (testVectors + i)->contentOID;
        pkcs7.encryptOID    = (testVectors + i)->encryptOID;
        pkcs7.keyWrapOID    = (testVectors + i)->keyWrapOID;
        pkcs7.keyAgreeOID   = (testVectors + i)->keyAgreeOID;
        pkcs7.privateKey    = (testVectors + i)->privateKey;
        pkcs7.privateKeySz  = (testVectors + i)->privateKeySz;

        AssertIntGE(wc_PKCS7_EncodeEnvelopedData(&pkcs7, output,
                            (word32)sizeof(output)), 0);

        decodedSz = wc_PKCS7_DecodeEnvelopedData(&pkcs7, output,
                (word32)sizeof(output), decoded, (word32)sizeof(decoded));
        AssertIntGE(decodedSz, 0);
        /* Verify the size of each buffer. */
        AssertIntEQ((word32)sizeof(input)/sizeof(char), decodedSz);
        /* Don't free the last time through the loop. */
        if (i < testSz - 1 ){
            wc_PKCS7_Free(&pkcs7);
        }
    }  /* END test loop. */

    /* Test bad args. */
    AssertIntEQ(wc_PKCS7_EncodeEnvelopedData(NULL, output,
                    (word32)sizeof(output)), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_EncodeEnvelopedData(&pkcs7, NULL,
                    (word32)sizeof(output)), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_EncodeEnvelopedData(&pkcs7, output, 0), BAD_FUNC_ARG);
    printf(resultFmt, passed);

    /* Decode.  */
    printf(testingFmt, "wc_PKCS7_DecodeEnvelopedData()");

    AssertIntEQ(wc_PKCS7_DecodeEnvelopedData(NULL, output,
        (word32)sizeof(output), decoded, (word32)sizeof(decoded)), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_DecodeEnvelopedData(&pkcs7, output,
        (word32)sizeof(output), NULL, (word32)sizeof(decoded)), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_DecodeEnvelopedData(&pkcs7, output,
        (word32)sizeof(output), decoded, 0), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_DecodeEnvelopedData(&pkcs7, NULL,
        (word32)sizeof(output), decoded, (word32)sizeof(decoded)), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_DecodeEnvelopedData(&pkcs7, output, 0, decoded,
        (word32)sizeof(decoded)), BAD_FUNC_ARG);
    /* Should get a return of BAD_FUNC_ARG with structure data. Order matters.*/
    tempWrd32 = pkcs7.singleCertSz;
    pkcs7.singleCertSz = 0;
    AssertIntEQ(wc_PKCS7_DecodeEnvelopedData(&pkcs7, output,
        (word32)sizeof(output), decoded, (word32)sizeof(decoded)), BAD_FUNC_ARG);
    pkcs7.singleCertSz = tempWrd32;
    tempWrd32 = pkcs7.privateKeySz;
    pkcs7.privateKeySz = 0;
    AssertIntEQ(wc_PKCS7_DecodeEnvelopedData(&pkcs7, output,
        (word32)sizeof(output), decoded, (word32)sizeof(decoded)), BAD_FUNC_ARG);
    pkcs7.privateKeySz = tempWrd32;
    tmpBytePtr = pkcs7.singleCert;
    pkcs7.singleCert = NULL;
    AssertIntEQ(wc_PKCS7_DecodeEnvelopedData(&pkcs7, output,
        (word32)sizeof(output), decoded, (word32)sizeof(decoded)), BAD_FUNC_ARG);
    pkcs7.singleCert = tmpBytePtr;
    tmpBytePtr = pkcs7.privateKey;
    pkcs7.privateKey = NULL;
    AssertIntEQ(wc_PKCS7_DecodeEnvelopedData(&pkcs7, output,
        (word32)sizeof(output), decoded, (word32)sizeof(decoded)), BAD_FUNC_ARG);
    pkcs7.privateKey = tmpBytePtr;

    printf(resultFmt, passed);

    wc_PKCS7_Free(&pkcs7);
#ifndef NO_RSA
    if (rsaCert) {
        XFREE(rsaCert, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }
    if (rsaPrivKey) {
        XFREE(rsaPrivKey, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }
#endif /*NO_RSA */
#ifdef HAVE_ECC
    if (eccCert) {
        XFREE(eccCert, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }
    if (eccPrivKey) {
        XFREE(eccPrivKey, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    }
#endif /* HAVE_ECC */

#endif /* HAVE_PKCS7 */
} /* END test_wc_PKCS7_EncodeEnvelopedData() */


/*
 * Testing wc_PKCS7_EncodeEncryptedData()
 */
static void test_wc_PKCS7_EncodeEncryptedData (void)
{
#if defined(HAVE_PKCS7) && !defined(NO_PKCS7_ENCRYPTED_DATA)
    PKCS7       pkcs7;
    byte*       tmpBytePtr = NULL;
    byte        encrypted[TWOK_BUF];
    byte        decoded[TWOK_BUF];
    word32      tmpWrd32 = 0;
    int         tmpInt = 0;
    int         decodedSz;
    int         encryptedSz;
    int         testSz;
    int         i;

    const byte data[] = { /* Hello World */
        0x48,0x65,0x6c,0x6c,0x6f,0x20,0x57,0x6f,
        0x72,0x6c,0x64
    };

    #ifndef NO_DES3
        byte desKey[] = {
            0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef
        };
        byte des3Key[] = {
            0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef,
            0xfe,0xde,0xba,0x98,0x76,0x54,0x32,0x10,
            0x89,0xab,0xcd,0xef,0x01,0x23,0x45,0x67
        };
    #endif

    #ifndef NO_AES
        byte aes128Key[] = {
            0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
            0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08
        };
        byte aes192Key[] = {
            0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
            0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
            0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08
        };
        byte aes256Key[] = {
            0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
            0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
            0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
            0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08
        };
    #endif
    const pkcs7EncryptedVector testVectors[] =
    {
    #ifndef NO_DES3
        {data, (word32)sizeof(data), DATA, DES3b, des3Key, sizeof(des3Key)},

        {data, (word32)sizeof(data), DATA, DESb, desKey, sizeof(desKey)},
    #endif /* NO_DES3 */
    #ifndef NO_AES
        {data, (word32)sizeof(data), DATA, AES128CBCb, aes128Key,
         sizeof(aes128Key)},

        {data, (word32)sizeof(data), DATA, AES192CBCb, aes192Key,
         sizeof(aes192Key)},

        {data, (word32)sizeof(data), DATA, AES256CBCb, aes256Key,
         sizeof(aes256Key)},

    #endif /* NO_AES */
    };

    testSz = sizeof(testVectors) / sizeof(pkcs7EncryptedVector);

    for (i = 0; i < testSz; i++) {
        AssertIntEQ(wc_PKCS7_Init(&pkcs7, HEAP_HINT, devId), 0);
        pkcs7.content              = (byte*)testVectors[i].content;
        pkcs7.contentSz            = testVectors[i].contentSz;
        pkcs7.contentOID           = testVectors[i].contentOID;
        pkcs7.encryptOID           = testVectors[i].encryptOID;
        pkcs7.encryptionKey        = testVectors[i].encryptionKey;
        pkcs7.encryptionKeySz      = testVectors[i].encryptionKeySz;
        pkcs7.heap                 = HEAP_HINT;

        /* encode encryptedData */
        encryptedSz = wc_PKCS7_EncodeEncryptedData(&pkcs7, encrypted,
                                                   sizeof(encrypted));
        AssertIntGT(encryptedSz, 0);

       /* Decode encryptedData */
        decodedSz = wc_PKCS7_DecodeEncryptedData(&pkcs7, encrypted, encryptedSz,
                                                    decoded, sizeof(decoded));

        AssertIntEQ(XMEMCMP(decoded, data, decodedSz), 0);
        /* Keep values for last itr. */
        if (i < testSz - 1) {
            wc_PKCS7_Free(&pkcs7);
        }
    }

    printf(testingFmt, "wc_PKCS7_EncodeEncryptedData()");
    AssertIntEQ(wc_PKCS7_EncodeEncryptedData(NULL, encrypted,
                     sizeof(encrypted)),BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_EncodeEncryptedData(&pkcs7, NULL,
                     sizeof(encrypted)), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_EncodeEncryptedData(&pkcs7, encrypted,
                     0), BAD_FUNC_ARG);
    /* Testing the struct. */
    tmpBytePtr = pkcs7.content;
    pkcs7.content = NULL;
    AssertIntEQ(wc_PKCS7_EncodeEncryptedData(&pkcs7, encrypted,
                             sizeof(encrypted)), BAD_FUNC_ARG);
    pkcs7.content = tmpBytePtr;
    tmpWrd32 = pkcs7.contentSz;
    pkcs7.contentSz = 0;
    AssertIntEQ(wc_PKCS7_EncodeEncryptedData(&pkcs7, encrypted,
                             sizeof(encrypted)), BAD_FUNC_ARG);
    pkcs7.contentSz = tmpWrd32;
    tmpInt = pkcs7.encryptOID;
    pkcs7.encryptOID = 0;
    AssertIntEQ(wc_PKCS7_EncodeEncryptedData(&pkcs7, encrypted,
                             sizeof(encrypted)), BAD_FUNC_ARG);
    pkcs7.encryptOID = tmpInt;
    tmpBytePtr = pkcs7.encryptionKey;
    pkcs7.encryptionKey = NULL;
    AssertIntEQ(wc_PKCS7_EncodeEncryptedData(&pkcs7, encrypted,
                             sizeof(encrypted)), BAD_FUNC_ARG);
    pkcs7.encryptionKey = tmpBytePtr;
    tmpWrd32 = pkcs7.encryptionKeySz;
    pkcs7.encryptionKeySz = 0;
    AssertIntEQ(wc_PKCS7_EncodeEncryptedData(&pkcs7, encrypted,
                             sizeof(encrypted)), BAD_FUNC_ARG);
    pkcs7.encryptionKeySz = tmpWrd32;

    printf(resultFmt, passed);

    printf(testingFmt, "wc_PKCS7_EncodeEncryptedData()");

    AssertIntEQ(wc_PKCS7_DecodeEncryptedData(NULL, encrypted, encryptedSz,
                decoded, sizeof(decoded)), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_DecodeEncryptedData(&pkcs7, NULL, encryptedSz,
                decoded, sizeof(decoded)), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_DecodeEncryptedData(&pkcs7, encrypted, 0,
                decoded, sizeof(decoded)), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_DecodeEncryptedData(&pkcs7, encrypted, encryptedSz,
                NULL, sizeof(decoded)), BAD_FUNC_ARG);
    AssertIntEQ(wc_PKCS7_DecodeEncryptedData(&pkcs7, encrypted, encryptedSz,
                decoded, 0), BAD_FUNC_ARG);
    /* Test struct fields */

    tmpBytePtr = pkcs7.encryptionKey;
    pkcs7.encryptionKey = NULL;
    AssertIntEQ(wc_PKCS7_DecodeEncryptedData(&pkcs7, encrypted, encryptedSz,
                                   decoded, sizeof(decoded)), BAD_FUNC_ARG);
    pkcs7.encryptionKey = tmpBytePtr;
    pkcs7.encryptionKeySz = 0;
    AssertIntEQ(wc_PKCS7_DecodeEncryptedData(&pkcs7, encrypted, encryptedSz,
                                   decoded, sizeof(decoded)), BAD_FUNC_ARG);

    printf(resultFmt, passed);
    wc_PKCS7_Free(&pkcs7);
#endif
} /* END test_wc_PKCS7_EncodeEncryptedData() */

/* Testing wc_SignatureGetSize() for signature type ECC */
static int test_wc_SignatureGetSize_ecc(void)
{    
    int ret = 0; 
    #if defined(HAVE_ECC) && !defined(NO_ECC256)
        enum wc_SignatureType sig_type;
        word32 key_len;

        /* Initialize ECC Key */
        ecc_key ecc; 
        const char* qx =
            "fa2737fb93488d19caef11ae7faf6b7f4bcd67b286e3fc54e8a65c2b74aeccb0";
        const char* qy = 
            "d4ccd6dae698208aa8c3a6f39e45510d03be09b2f124bfc067856c324f9b4d09";
        const char* d = 
            "be34baa8d040a3b991f9075b56ba292f755b90e4b6dc10dad36715c33cfdac25";
    
        ret = wc_ecc_init(&ecc);
        if (ret == 0) {
            ret = wc_ecc_import_raw(&ecc, qx, qy, d, "SECP256R1");
        }
        printf(testingFmt, "wc_SigntureGetSize_ecc()");
        if (ret == 0) { 
            /* Input for signature type ECC */
            sig_type = WC_SIGNATURE_TYPE_ECC;
            key_len = sizeof(ecc_key);
            ret = wc_SignatureGetSize(sig_type, &ecc, key_len);
            
            /* Test bad args */ 
            if (ret > 0) {
                sig_type = (enum wc_SignatureType) 100;
                ret = wc_SignatureGetSize(sig_type, &ecc, key_len);
                if (ret == BAD_FUNC_ARG) {
                    sig_type = WC_SIGNATURE_TYPE_ECC;
                    ret = wc_SignatureGetSize(sig_type, NULL, key_len);
                }  
                if (ret >= 0) {
                    key_len = (word32) 0;
                    ret = wc_SignatureGetSize(sig_type, &ecc, key_len); 
                }
                if (ret == BAD_FUNC_ARG) {
                    ret = SIG_TYPE_E;
                }
            }
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
        wc_ecc_free(&ecc);
    #else
        ret = SIG_TYPE_E;
    #endif

    if (ret == SIG_TYPE_E) {
        ret = 0;
    }
    else {
        ret = WOLFSSL_FATAL_ERROR;
    }

    printf(resultFmt, ret == 0 ? passed : failed);
    return ret;
}/* END test_wc_SignatureGetSize_ecc() */

/* Testing wc_SignatureGetSize() for signature type rsa */
static int test_wc_SignatureGetSize_rsa(void)
{
    int ret = 0; 
    #ifndef NO_RSA
        enum wc_SignatureType sig_type;
        word32 key_len;
        word32 idx = 0;

        /* Initialize RSA Key */
        RsaKey rsa_key;
        byte* tmp = NULL;
        size_t bytes;
     
        #ifdef USE_CERT_BUFFERS_1024
            bytes = (size_t)sizeof_client_key_der_1024;
            if (bytes < (size_t)sizeof_client_key_der_1024)
                bytes = (size_t)sizeof_client_cert_der_1024;
        #elif defined(USE_CERT_BUFFERS_2048)
            bytes = (size_t)sizeof_client_key_der_2048;
            if (bytes < (size_t)sizeof_client_cert_der_2048)
                bytes = (size_t)sizeof_client_cert_der_2048;
        #else
            bytes = FOURK_BUF;
        #endif

        tmp = (byte*)XMALLOC(bytes, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
        if (tmp != NULL) {
            #ifdef USE_CERT_BUFFERS_1024
                XMEMCPY(tmp, client_key_der_1024, 
                    (size_t)sizeof_client_key_der_1024);
            #elif defined(USE_CERT_BUFFERS_2048)
                XMEMCPY(tmp, client_key_der_2048, 
                    (size_t)sizeof_client_key_der_2048);
            #elif !defined(NO_FILESYSTEM)
                file = fopen(clientKey, "rb");
                if (file != NULL) {
                    bytes = fread(tmp, 1, FOURK_BUF, file);
                    fclose(file);
                }
                else {
                    ret = WOLFSSL_FATAL_ERROR;
                }
            #else
                ret = WOLFSSL_FATAL_ERROR;
            #endif
            if (ret == 0) {
                ret = wc_InitRsaKey_ex(&rsa_key, HEAP_HINT, devId);
                if (ret == 0) {
                    ret = wc_RsaPrivateKeyDecode(tmp, &idx, &rsa_key, 
                        (word32)bytes);
                }
            }
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }

        printf(testingFmt, "wc_SigntureGetSize_rsa()");
        if (ret == 0) {
            /* Input for signature type RSA */
            sig_type = WC_SIGNATURE_TYPE_RSA;
            key_len = sizeof(RsaKey);
            ret = wc_SignatureGetSize(sig_type, &rsa_key, key_len);
            
            /* Test bad args */
            if (ret > 0) {
                sig_type = (enum wc_SignatureType) 100;
                ret = wc_SignatureGetSize(sig_type, &rsa_key, key_len);
                if (ret == BAD_FUNC_ARG) {
                    sig_type = WC_SIGNATURE_TYPE_RSA;
                    ret = wc_SignatureGetSize(sig_type, NULL, key_len);
                }
            #ifndef HAVE_USER_RSA
                if (ret == BAD_FUNC_ARG) {
            #else        
                if (ret == 0) {
            #endif
                    key_len = (word32)0;
                    ret = wc_SignatureGetSize(sig_type, &rsa_key, key_len);
                }
                if (ret == BAD_FUNC_ARG) {
                    ret = SIG_TYPE_E;
                }
            }
        } else {
            ret = WOLFSSL_FATAL_ERROR;
        }
        wc_FreeRsaKey(&rsa_key);
        XFREE(tmp, HEAP_HINT, DYNAMIC_TYPE_TMP_BUFFER);
    #else
        ret = SIG_TYPE_E;
    #endif
            
    if (ret == SIG_TYPE_E) {
        ret = 0;
    }else {
        ret = WOLFSSL_FATAL_ERROR;
    }
 
   printf(resultFmt, ret == 0 ? passed : failed);
   return ret;
}/* END test_wc_SignatureGetSize_rsa(void) */
  
/*----------------------------------------------------------------------------*
 | hash.h Tests
 *----------------------------------------------------------------------------*/
  
static int test_wc_HashInit(void)
{
    int ret = 0, i;  /* 0 indicates tests passed, 1 indicates failure */

    wc_HashAlg hash;

    /* enum for holding supported algorithms, #ifndef's restrict if disabled */
    enum wc_HashType enumArray[] = {
    #ifndef NO_MD5
            WC_HASH_TYPE_MD5,
    #endif
    #ifndef NO_SHA
            WC_HASH_TYPE_SHA,
    #endif
    #ifndef WOLFSSL_SHA224
            WC_HASH_TYPE_SHA224,
    #endif
    #ifndef NO_SHA256
            WC_HASH_TYPE_SHA256,
    #endif
    #ifndef WOLFSSL_SHA384
            WC_HASH_TYPE_SHA384,
    #endif
    #ifndef WOLFSSL_SHA512
            WC_HASH_TYPE_SHA512,
    #endif
    };
    /* dynamically finds the length */
    int enumlen = (sizeof(enumArray)/sizeof(enum wc_HashType));

    /* For loop to test various arguments... */
    for (i = 0; i < enumlen; i++) {
        /* check for bad args */
        if (wc_HashInit(&hash, enumArray[i]) == BAD_FUNC_ARG) {
            ret = 1;
            break;
        }
        /* check for null ptr */
        if (wc_HashInit(NULL, enumArray[i]) != BAD_FUNC_ARG) {
            ret = 1;
            break;
        }

    }  /* end of for loop */

    printf(testingFmt, "wc_HashInit()");
    if (ret==0) {  /* all tests have passed */
        printf(resultFmt, passed);
    }
    else {  /* a test has failed */
        printf(resultFmt, failed);
    }
    return ret;
}  /* end of test_wc_HashInit */

/*----------------------------------------------------------------------------*
 | Compatibility Tests
 *----------------------------------------------------------------------------*/

static void test_wolfSSL_X509_NAME(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && !defined(NO_FILESYSTEM) \
        && !defined(NO_RSA) && defined(WOLFSSL_CERT_GEN)
    X509* x509;
    const unsigned char* c;
    unsigned char buf[4096];
    int bytes;
    FILE* f;
    const X509_NAME* a;
    const X509_NAME* b;
    int sz;
    unsigned char* tmp;
    char file[] = "./certs/ca-cert.der";

    printf(testingFmt, "wolfSSL_X509_NAME()");

    /* test compile of depricated function, returns 0 */
    AssertIntEQ(CRYPTO_thread_id(), 0);

    AssertNotNull(a = X509_NAME_new());
    X509_NAME_free((X509_NAME*)a);

    f = fopen(file, "rb");
    AssertNotNull(f);
    bytes = (int)fread(buf, 1, sizeof(buf), f);
    fclose(f);

    c = buf;
    AssertNotNull(x509 = wolfSSL_X509_load_certificate_buffer(c, bytes,
                SSL_FILETYPE_ASN1));

    /* test cmp function */
    AssertNotNull(a = X509_get_issuer_name(x509));
    AssertNotNull(b = X509_get_subject_name(x509));

    AssertIntEQ(X509_NAME_cmp(a, b), 0); /* self signed should be 0 */

    tmp = buf;
    AssertIntGT((sz = i2d_X509_NAME((X509_NAME*)a, &tmp)), 0);
    if (tmp == buf) {
        printf("\nERROR - %s line %d failed with:", __FILE__, __LINE__);           \
        printf(" Expected pointer to be incremented\n");
        abort();
    }

    /* retry but with the function creating a buffer */
    tmp = NULL;
    AssertIntGT((sz = i2d_X509_NAME((X509_NAME*)b, &tmp)), 0);
    XFREE(tmp, NULL, DYNAMIC_TYPE_OPENSSL);

    X509_free(x509);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_DES3) */
}


static void test_wolfSSL_DES(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_DES3)
    const_DES_cblock myDes;
    DES_cblock iv;
    DES_key_schedule key;
    word32 i;
    DES_LONG dl;
    unsigned char msg[] = "hello wolfssl";

    printf(testingFmt, "wolfSSL_DES()");

    DES_check_key(1);
    DES_set_key(&myDes, &key);

    /* check, check of odd parity */
    XMEMSET(myDes, 4, sizeof(const_DES_cblock));  myDes[0] = 6; /*set even parity*/
    XMEMSET(key, 5, sizeof(DES_key_schedule));
    AssertIntEQ(DES_set_key_checked(&myDes, &key), -1);
    AssertIntNE(key[0], myDes[0]); /* should not have copied over key */

    /* set odd parity for success case */
    DES_set_odd_parity(&myDes);
    printf("%02x %02x %02x %02x", myDes[0], myDes[1], myDes[2], myDes[3]);
    AssertIntEQ(DES_set_key_checked(&myDes, &key), 0);
    for (i = 0; i < sizeof(DES_key_schedule); i++) {
        AssertIntEQ(key[i], myDes[i]);
    }
    AssertIntEQ(DES_is_weak_key(&myDes), 0);

    /* check weak key */
    XMEMSET(myDes, 1, sizeof(const_DES_cblock));
    XMEMSET(key, 5, sizeof(DES_key_schedule));
    AssertIntEQ(DES_set_key_checked(&myDes, &key), -2);
    AssertIntNE(key[0], myDes[0]); /* should not have copied over key */

    /* now do unchecked copy of a weak key over */
    DES_set_key_unchecked(&myDes, &key);
    /* compare arrays, should be the same */
    for (i = 0; i < sizeof(DES_key_schedule); i++) {
        AssertIntEQ(key[i], myDes[i]);
    }
    AssertIntEQ(DES_is_weak_key(&myDes), 1);

    /* check DES_key_sched API */
    XMEMSET(key, 1, sizeof(DES_key_schedule));
    AssertIntEQ(DES_key_sched(&myDes, NULL), 0);
    AssertIntEQ(DES_key_sched(NULL, &key),   0);
    AssertIntEQ(DES_key_sched(&myDes, &key), 0);
    /* compare arrays, should be the same */
    for (i = 0; i < sizeof(DES_key_schedule); i++) {
        AssertIntEQ(key[i], myDes[i]);
    }

    /* DES_cbc_cksum should return the last 4 of the last 8 bytes after
     * DES_cbc_encrypt on the input */
    XMEMSET(iv, 0, sizeof(DES_cblock));
    XMEMSET(myDes, 5, sizeof(DES_key_schedule));
    AssertIntGT((dl = DES_cbc_cksum(msg, &key, sizeof(msg), &myDes, &iv)), 0);
    AssertIntEQ(dl, 480052723);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_DES3) */
}


static void test_wolfSSL_certs(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)
    X509*  x509;
    WOLFSSL*     ssl;
    WOLFSSL_CTX* ctx;
    WOLF_STACK_OF(ASN1_OBJECT)* sk;
    int crit;

    printf(testingFmt, "wolfSSL_certs()");

    AssertNotNull(ctx = SSL_CTX_new(wolfSSLv23_server_method()));
    AssertTrue(SSL_CTX_use_certificate_file(ctx, svrCertFile, SSL_FILETYPE_PEM));
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, SSL_FILETYPE_PEM));
    #ifndef HAVE_USER_RSA
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, cliKeyFile, SSL_FILETYPE_PEM));
    AssertIntEQ(SSL_CTX_check_private_key(ctx), SSL_FAILURE);
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, SSL_FILETYPE_PEM));
    AssertIntEQ(SSL_CTX_check_private_key(ctx), SSL_SUCCESS);
    #endif
    AssertNotNull(ssl = SSL_new(ctx));

    AssertIntEQ(wolfSSL_check_private_key(ssl), WOLFSSL_SUCCESS);

    #ifdef HAVE_PK_CALLBACKS
    AssertIntEQ((int)SSL_set_tlsext_debug_arg(ssl, NULL), WOLFSSL_SUCCESS);
    #endif /* HAVE_PK_CALLBACKS */

    /* create and use x509 */
    x509 = wolfSSL_X509_load_certificate_file(cliCertFile, WOLFSSL_FILETYPE_PEM);
    AssertNotNull(x509);
    AssertIntEQ(SSL_use_certificate(ssl, x509), WOLFSSL_SUCCESS);

    #ifndef HAVE_USER_RSA
    /* with loading in a new cert the check on private key should now fail */
    AssertIntNE(wolfSSL_check_private_key(ssl), WOLFSSL_SUCCESS);
    #endif


    #if defined(USE_CERT_BUFFERS_2048)
        AssertIntEQ(SSL_use_certificate_ASN1(ssl,
                                  (unsigned char*)server_cert_der_2048,
                                  sizeof_server_cert_der_2048), WOLFSSL_SUCCESS);
    #endif

    #if !defined(NO_SHA) && !defined(NO_SHA256)
    /************* Get Digest of Certificate ******************/
    {
        byte   digest[64]; /* max digest size */
        word32 digestSz;

        XMEMSET(digest, 0, sizeof(digest));
        AssertIntEQ(X509_digest(x509, wolfSSL_EVP_sha1(), digest, &digestSz),
                    WOLFSSL_SUCCESS);
        AssertIntEQ(X509_digest(x509, wolfSSL_EVP_sha256(), digest, &digestSz),
                    WOLFSSL_SUCCESS);

        AssertIntEQ(X509_digest(NULL, wolfSSL_EVP_sha1(), digest, &digestSz),
                    WOLFSSL_FAILURE);
    }
    #endif /* !NO_SHA && !NO_SHA256*/

    /* test and checkout X509 extensions */
    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_basic_constraints,
            &crit, NULL);
    AssertNotNull(sk);
    AssertIntEQ(crit, 0);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_key_usage,
            &crit, NULL);
    /* AssertNotNull(sk); NID not yet supported */
    AssertIntEQ(crit, -1);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_ext_key_usage,
            &crit, NULL);
    /* AssertNotNull(sk); no extension set */
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509,
            NID_authority_key_identifier, &crit, NULL);
    AssertNotNull(sk);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509,
            NID_private_key_usage_period, &crit, NULL);
    /* AssertNotNull(sk); NID not yet supported */
    AssertIntEQ(crit, -1);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_subject_alt_name,
            &crit, NULL);
    /* AssertNotNull(sk); no alt names set */
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_issuer_alt_name,
            &crit, NULL);
    /* AssertNotNull(sk); NID not yet supported */
    AssertIntEQ(crit, -1);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_info_access, &crit,
            NULL);
    /* AssertNotNull(sk); no auth info set */
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_sinfo_access,
            &crit, NULL);
    /* AssertNotNull(sk); NID not yet supported */
    AssertIntEQ(crit, -1);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_name_constraints,
            &crit, NULL);
    /* AssertNotNull(sk); NID not yet supported */
    AssertIntEQ(crit, -1);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509,
            NID_certificate_policies, &crit, NULL);
    #if !defined(WOLFSSL_SEP) && !defined(WOLFSSL_CERT_EXT)
        AssertNull(sk);
    #else
        /* AssertNotNull(sk); no cert policy set */
    #endif
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_policy_mappings,
            &crit, NULL);
    /* AssertNotNull(sk); NID not yet supported */
    AssertIntEQ(crit, -1);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_policy_constraints,
            &crit, NULL);
    /* AssertNotNull(sk); NID not yet supported */
    AssertIntEQ(crit, -1);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_inhibit_any_policy,
            &crit, NULL);
    /* AssertNotNull(sk); NID not yet supported */
    AssertIntEQ(crit, -1);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, NID_tlsfeature, &crit,
            NULL);
    /* AssertNotNull(sk); NID not yet supported */
    AssertIntEQ(crit, -1);
    wolfSSL_sk_ASN1_OBJECT_free(sk);

    /* test invalid cases */
    crit = 0;
    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509, -1, &crit, NULL);
    AssertNull(sk);
    AssertIntEQ(crit, -1);
    sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(NULL, NID_tlsfeature,
            NULL, NULL);
    AssertNull(sk);

    AssertIntEQ(SSL_get_hit(ssl), 0);
    X509_free(x509);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) */
}


static void test_wolfSSL_ASN1_TIME_print()
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && !defined(NO_RSA) \
        && (defined(WOLFSSL_MYSQL_COMPATIBLE) || defined(WOLFSSL_NGINX) || \
            defined(WOLFSSL_HAPROXY)) && defined(USE_CERT_BUFFERS_2048)
    BIO*  bio;
    X509*  x509;
    const unsigned char* der = client_cert_der_2048;
    ASN1_TIME* t;
    unsigned char buf[25];

    printf(testingFmt, "wolfSSL_ASN1_TIME_print()");

    AssertNotNull(bio = BIO_new(BIO_s_mem()));
    AssertNotNull(x509 = wolfSSL_X509_load_certificate_buffer(der,
                sizeof_client_cert_der_2048, WOLFSSL_FILETYPE_ASN1));
    AssertIntEQ(ASN1_TIME_print(bio, X509_get_notBefore(x509)), 1);
    AssertIntEQ(BIO_read(bio, buf, sizeof(buf)), 24);
    AssertIntEQ(XMEMCMP(buf, "Apr 13 15:23:09 2018 GMT", sizeof(buf) - 1), 0);

    /* create a bad time and test results */
    AssertNotNull(t = X509_get_notAfter(x509));
    t->data[10] = 0;
    t->data[5]  = 0;
    AssertIntNE(ASN1_TIME_print(bio, t), 1);
    AssertIntEQ(BIO_read(bio, buf, sizeof(buf)), 14);
    AssertIntEQ(XMEMCMP(buf, "Bad time value", 14), 0);

    BIO_free(bio);
    X509_free(x509);

    printf(resultFmt, passed);
    #endif
}


static void test_wolfSSL_ASN1_GENERALIZEDTIME_free(){
    #if defined(OPENSSL_EXTRA)
    WOLFSSL_ASN1_GENERALIZEDTIME* asn1_gtime;
    unsigned char nullstr[32];

    XMEMSET(nullstr, 0, 32);
    asn1_gtime = (WOLFSSL_ASN1_GENERALIZEDTIME*)XMALLOC(
                    sizeof(WOLFSSL_ASN1_GENERALIZEDTIME), NULL, 
                    DYNAMIC_TYPE_TMP_BUFFER);
    XMEMCPY(asn1_gtime->data,"20180504123500Z",ASN_GENERALIZED_TIME_SIZE);
    wolfSSL_ASN1_GENERALIZEDTIME_free(asn1_gtime);
    AssertIntEQ(0, XMEMCMP(asn1_gtime->data, nullstr, 32));

    XFREE(asn1_gtime, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    #endif /* OPENSSL_EXTRA */
}


static void test_wolfSSL_private_keys(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM)
    WOLFSSL*     ssl;
    WOLFSSL_CTX* ctx;
    EVP_PKEY* pkey = NULL;

    printf(testingFmt, "wolfSSL_private_keys()");

    OpenSSL_add_all_digests();
    OpenSSL_add_all_algorithms();

#ifndef NO_RSA
    AssertNotNull(ctx = SSL_CTX_new(wolfSSLv23_server_method()));
    AssertTrue(SSL_CTX_use_certificate_file(ctx, svrCertFile, WOLFSSL_FILETYPE_PEM));
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, WOLFSSL_FILETYPE_PEM));
    AssertNotNull(ssl = SSL_new(ctx));

    AssertIntEQ(wolfSSL_check_private_key(ssl), WOLFSSL_SUCCESS);

#ifdef USE_CERT_BUFFERS_2048
    {
    const unsigned char* server_key = (const unsigned char*)server_key_der_2048;
    unsigned char buf[FOURK_BUF];
    word32 bufSz;

    AssertIntEQ(SSL_use_RSAPrivateKey_ASN1(ssl,
                (unsigned char*)client_key_der_2048,
                sizeof_client_key_der_2048), WOLFSSL_SUCCESS);
#ifndef HAVE_USER_RSA
    /* Should missmatch now that a different private key loaded */
    AssertIntNE(wolfSSL_check_private_key(ssl), WOLFSSL_SUCCESS);
#endif

    AssertIntEQ(SSL_use_PrivateKey_ASN1(0, ssl,
                (unsigned char*)server_key,
                sizeof_server_key_der_2048), WOLFSSL_SUCCESS);
    /* After loading back in DER format of original key, should match */
    AssertIntEQ(wolfSSL_check_private_key(ssl), WOLFSSL_SUCCESS);

    /* pkey not set yet, expecting to fail */
    AssertIntEQ(SSL_use_PrivateKey(ssl, pkey), WOLFSSL_FAILURE);

    /* set PKEY and test again */
    AssertNotNull(wolfSSL_d2i_PrivateKey(EVP_PKEY_RSA, &pkey,
                &server_key, (long)sizeof_server_key_der_2048));
    AssertIntEQ(SSL_use_PrivateKey(ssl, pkey), WOLFSSL_SUCCESS);

    /* reuse PKEY structure and test
     * this should be checked with a memory management sanity checker */
    AssertFalse(server_key == (const unsigned char*)server_key_der_2048);
    server_key = (const unsigned char*)server_key_der_2048;
    AssertNotNull(wolfSSL_d2i_PrivateKey(EVP_PKEY_RSA, &pkey,
                &server_key, (long)sizeof_server_key_der_2048));
    AssertIntEQ(SSL_use_PrivateKey(ssl, pkey), WOLFSSL_SUCCESS);

    /* check striping PKCS8 header with wolfSSL_d2i_PrivateKey */
    bufSz = FOURK_BUF;
    AssertIntGT((bufSz = wc_CreatePKCS8Key(buf, &bufSz,
                    (byte*)server_key_der_2048, sizeof_server_key_der_2048,
                    RSAk, NULL, 0)), 0);
    server_key = (const unsigned char*)buf;
    AssertNotNull(wolfSSL_d2i_PrivateKey(EVP_PKEY_RSA, &pkey, &server_key,
                (long)bufSz));
    }
#endif


    EVP_PKEY_free(pkey);
    SSL_free(ssl); /* frees x509 also since loaded into ssl */
    SSL_CTX_free(ctx);
#endif /* end of RSA private key match tests */


#ifdef HAVE_ECC
    AssertNotNull(ctx = SSL_CTX_new(wolfSSLv23_server_method()));
    AssertTrue(SSL_CTX_use_certificate_file(ctx, eccCertFile,
                                                         WOLFSSL_FILETYPE_PEM));
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, eccKeyFile,
                                                         WOLFSSL_FILETYPE_PEM));
    AssertNotNull(ssl = SSL_new(ctx));

    AssertIntEQ(wolfSSL_check_private_key(ssl), WOLFSSL_SUCCESS);
    SSL_free(ssl);


    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, cliEccKeyFile,
                                                         WOLFSSL_FILETYPE_PEM));
    AssertNotNull(ssl = SSL_new(ctx));

    AssertIntNE(wolfSSL_check_private_key(ssl), WOLFSSL_SUCCESS);

    SSL_free(ssl);
    SSL_CTX_free(ctx);
#endif /* end of ECC private key match tests */


    /* test existence of no-op macros in wolfssl/openssl/ssl.h */
    CONF_modules_free();
    ENGINE_cleanup();
    CONF_modules_unload();

    (void)ssl;
    (void)ctx;
    (void)pkey;

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) */
}


static void test_wolfSSL_PEM_PrivateKey(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
    (!defined(NO_RSA) || defined(HAVE_ECC)) && \
    defined(USE_CERT_BUFFERS_2048)

    BIO*      bio = NULL;
    EVP_PKEY* pkey  = NULL;
    const unsigned char* server_key = (const unsigned char*)server_key_der_2048;

    /* test creating new EVP_PKEY with bad arg */
    AssertNull((pkey = PEM_read_bio_PrivateKey(NULL, NULL, NULL, NULL)));

    /* test loading RSA key using BIO */
#if !defined(NO_RSA) && !defined(NO_FILESYSTEM)
    {
        XFILE file;
        const char* fname = "./certs/server-key.pem";
        size_t sz;
        byte* buf;

        file = XFOPEN(fname, "rb");
        AssertTrue((file != XBADFILE));
        XFSEEK(file, 0, XSEEK_END);
        sz = XFTELL(file);
        XREWIND(file);
        AssertNotNull(buf = (byte*)XMALLOC(sz, NULL, DYNAMIC_TYPE_FILE));
        AssertIntEQ(XFREAD(buf, 1, sz, file), sz);
        XFCLOSE(file);

        /* Test using BIO new mem and loading PEM private key */
        AssertNotNull(bio = BIO_new_mem_buf(buf, (int)sz));
        AssertNotNull((pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL)));
        XFREE(buf, NULL, DYNAMIC_TYPE_FILE);
        BIO_free(bio);
        bio = NULL;
        EVP_PKEY_free(pkey);
        pkey  = NULL;
    }
#endif

    /* test loading ECC key using BIO */
#if defined(HAVE_ECC) && !defined(NO_FILESYSTEM)
    {
        XFILE file;
        const char* fname = "./certs/ecc-key.pem";
        size_t sz;
        byte* buf;

        file = XFOPEN(fname, "rb");
        AssertTrue((file != XBADFILE));
        XFSEEK(file, 0, XSEEK_END);
        sz = XFTELL(file);
        XREWIND(file);
        AssertNotNull(buf = (byte*)XMALLOC(sz, NULL, DYNAMIC_TYPE_FILE));
        AssertIntEQ(XFREAD(buf, 1, sz, file), sz);
        XFCLOSE(file);

        /* Test using BIO new mem and loading PEM private key */
        AssertNotNull(bio = BIO_new_mem_buf(buf, (int)sz));
        AssertNotNull((pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL)));
        XFREE(buf, NULL, DYNAMIC_TYPE_FILE);
        BIO_free(bio);
        bio = NULL;
        EVP_PKEY_free(pkey);
        pkey  = NULL;
    }
#endif

#if !defined(NO_RSA) && (defined(WOLFSSL_KEY_GEN) || defined(WOLFSSL_CERT_GEN))
    {
        EVP_PKEY* pkey2 = NULL;
        unsigned char extra[10];
        int i;

        printf(testingFmt, "wolfSSL_PEM_PrivateKey()");

        XMEMSET(extra, 0, sizeof(extra));

        AssertNotNull(bio = wolfSSL_BIO_new(wolfSSL_BIO_s_mem()));
        AssertIntEQ(BIO_set_write_buf_size(bio, 4096), SSL_FAILURE);

        AssertNull(d2i_PrivateKey(EVP_PKEY_EC, &pkey,
                &server_key, (long)sizeof_server_key_der_2048));
        AssertNull(pkey);

        AssertNotNull(wolfSSL_d2i_PrivateKey(EVP_PKEY_RSA, &pkey,
                &server_key, (long)sizeof_server_key_der_2048));
        AssertIntEQ(PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL),
                WOLFSSL_SUCCESS);

        /* test creating new EVP_PKEY with good args */
        AssertNotNull((pkey2 = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL)));
        AssertIntEQ((int)XMEMCMP(pkey->pkey.ptr, pkey2->pkey.ptr, pkey->pkey_sz),0);

        /* test of reuse of EVP_PKEY */
        AssertNull(PEM_read_bio_PrivateKey(bio, &pkey, NULL, NULL));
        AssertIntEQ(BIO_pending(bio), 0);
        AssertIntEQ(PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL),
                SSL_SUCCESS);
        AssertIntEQ(BIO_write(bio, extra, 10), 10); /*add 10 extra bytes after PEM*/
        AssertNotNull(PEM_read_bio_PrivateKey(bio, &pkey, NULL, NULL));
        AssertNotNull(pkey);
        AssertIntEQ((int)XMEMCMP(pkey->pkey.ptr, pkey2->pkey.ptr, pkey->pkey_sz),0);
        AssertIntEQ(BIO_pending(bio), 10); /* check 10 extra bytes still there */
        AssertIntEQ(BIO_read(bio, extra, 10), 10);
        for (i = 0; i < 10; i++) {
            AssertIntEQ(extra[i], 0);
        }

        BIO_free(bio);
        bio = NULL;
        EVP_PKEY_free(pkey);
        pkey  = NULL;
        EVP_PKEY_free(pkey2);
    }
    #endif

    /* key is DES encrypted */
    #if !defined(NO_DES3) && defined(WOLFSSL_ENCRYPTED_KEYS) && !defined(NO_FILESYSTEM)
    {
        pem_password_cb* passwd_cb;
        void* passwd_cb_userdata;
        SSL_CTX* ctx;
        char passwd[] = "bad password";

    #ifndef WOLFSSL_NO_TLS12
        AssertNotNull(ctx = SSL_CTX_new(TLSv1_2_server_method()));
    #else
        AssertNotNull(ctx = SSL_CTX_new(TLSv1_3_server_method()));
    #endif

        AssertNotNull(bio = BIO_new_file("./certs/server-keyEnc.pem", "rb"));
        SSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
        AssertNotNull(passwd_cb = SSL_CTX_get_default_passwd_cb(ctx));
        AssertNull(passwd_cb_userdata =
            SSL_CTX_get_default_passwd_cb_userdata(ctx));

        /* fail case with password call back */
        AssertNull(pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL,
                    (void*)passwd));
        BIO_free(bio);
        AssertNotNull(bio = BIO_new_file("./certs/server-keyEnc.pem", "rb"));
        AssertNull(pkey = PEM_read_bio_PrivateKey(bio, NULL, passwd_cb,
                    (void*)passwd));
        BIO_free(bio);
        AssertNotNull(bio = BIO_new_file("./certs/server-keyEnc.pem", "rb"));

        /* use callback that works */
        AssertNotNull(pkey = PEM_read_bio_PrivateKey(bio, NULL, passwd_cb,
                (void*)"yassl123"));

        AssertIntEQ(SSL_CTX_use_PrivateKey(ctx, pkey), SSL_SUCCESS);

        EVP_PKEY_free(pkey);
        pkey  = NULL;
        BIO_free(bio);
        bio = NULL;
        SSL_CTX_free(ctx);
    }
    #endif /* !defined(NO_DES3) */

    #if defined(HAVE_ECC) && !defined(NO_FILESYSTEM)
    {
        unsigned char buf[2048];
        size_t bytes;
        XFILE f;
        SSL_CTX* ctx;

    #ifndef WOLFSSL_NO_TLS12
        AssertNotNull(ctx = SSL_CTX_new(TLSv1_2_server_method()));
    #else
        AssertNotNull(ctx = SSL_CTX_new(TLSv1_3_server_method()));
    #endif

        AssertNotNull(f = XFOPEN("./certs/ecc-key.der", "rb"));
        bytes = XFREAD(buf, 1, sizeof(buf), f);
        XFCLOSE(f);

        server_key = buf;
        pkey = NULL;
        AssertNull(d2i_PrivateKey(EVP_PKEY_RSA, &pkey, &server_key, bytes));
        AssertNull(pkey);
        AssertNotNull(d2i_PrivateKey(EVP_PKEY_EC, &pkey, &server_key, bytes));
        AssertIntEQ(SSL_CTX_use_PrivateKey(ctx, pkey), SSL_SUCCESS);

        EVP_PKEY_free(pkey);
        pkey = NULL;
        SSL_CTX_free(ctx);
    }
    #endif

    printf(resultFmt, passed);

    (void)server_key;
    (void)bio;
    (void)pkey;

#endif /* OPENSSL_EXTRA && !NO_CERTS && !NO_RSA && USE_CERT_BUFFERS_2048 */
}


static void test_wolfSSL_PEM_RSAPrivateKey(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)
    RSA* rsa = NULL;
    BIO* bio = NULL;

    printf(testingFmt, "wolfSSL_PEM_RSAPrivateKey()");

    AssertNotNull(bio = BIO_new_file(svrKeyFile, "rb"));
    AssertNotNull((rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL)));
    AssertIntEQ(RSA_size(rsa), 256);

    BIO_free(bio);
    RSA_free(rsa);

#ifdef HAVE_ECC
    AssertNotNull(bio = BIO_new_file(eccKeyFile, "rb"));
    AssertNull((rsa = PEM_read_bio_RSAPrivateKey(bio, NULL, NULL, NULL)));

    BIO_free(bio);
#endif /* HAVE_ECC */

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) */
}


static void test_wolfSSL_tmp_dh(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_DSA) && !defined(NO_RSA) && \
       !defined(NO_DH)
    byte buffer[5300];
    char file[] = "./certs/dsaparams.pem";
    FILE *f;
    int  bytes;
    DSA* dsa;
    DH*  dh;
    BIO*     bio;
    SSL*     ssl;
    SSL_CTX* ctx;

    printf(testingFmt, "wolfSSL_tmp_dh()");

    AssertNotNull(ctx = SSL_CTX_new(wolfSSLv23_server_method()));
    AssertTrue(SSL_CTX_use_certificate_file(ctx, svrCertFile, WOLFSSL_FILETYPE_PEM));
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, WOLFSSL_FILETYPE_PEM));
    AssertNotNull(ssl = SSL_new(ctx));

    f = fopen(file, "rb");
    AssertNotNull(f);
    bytes = (int)fread(buffer, 1, sizeof(buffer), f);
    fclose(f);

    bio = BIO_new_mem_buf((void*)buffer, bytes);
    AssertNotNull(bio);

    dsa = wolfSSL_PEM_read_bio_DSAparams(bio, NULL, NULL, NULL);
    AssertNotNull(dsa);

    dh = wolfSSL_DSA_dup_DH(dsa);
    AssertNotNull(dh);

    AssertIntEQ((int)SSL_CTX_set_tmp_dh(ctx, dh), WOLFSSL_SUCCESS);
    AssertIntEQ((int)SSL_set_tmp_dh(ssl, dh), WOLFSSL_SUCCESS);

    BIO_free(bio);
    DSA_free(dsa);
    DH_free(dh);
    SSL_free(ssl);
    SSL_CTX_free(ctx);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) */
}

static void test_wolfSSL_ctrl(void)
{
    #if defined(OPENSSL_EXTRA)
    byte buff[5300];
    BIO* bio;
    int  bytes;
    BUF_MEM* ptr = NULL;

    printf(testingFmt, "wolfSSL_crtl()");

    bytes = sizeof(buff);
    bio = BIO_new_mem_buf((void*)buff, bytes);
    AssertNotNull(bio);
    AssertNotNull(BIO_s_socket());

    AssertIntEQ((int)wolfSSL_BIO_get_mem_ptr(bio, &ptr), WOLFSSL_SUCCESS);

    /* needs tested after stubs filled out @TODO
        SSL_ctrl
        SSL_CTX_ctrl
    */

    BIO_free(bio);
    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) */
}


static void test_wolfSSL_EVP_PKEY_new_mac_key(void)
{
#ifdef OPENSSL_EXTRA
    static const unsigned char pw[] = "password";
    static const int pwSz = sizeof(pw) - 1;
    size_t checkPwSz = 0;
    const unsigned char* checkPw = NULL;
    WOLFSSL_EVP_PKEY* key = NULL;

    printf(testingFmt, "wolfSSL_EVP_PKEY_new_mac_key()");

    AssertNull(key = wolfSSL_EVP_PKEY_new_mac_key(0, NULL, pw, pwSz));
    AssertNull(key = wolfSSL_EVP_PKEY_new_mac_key(0, NULL, NULL, pwSz));

    AssertNotNull(key = wolfSSL_EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, pw, pwSz));
    AssertIntEQ(key->type, EVP_PKEY_HMAC);
    AssertIntEQ(key->save_type, EVP_PKEY_HMAC);
    AssertIntEQ(key->pkey_sz, pwSz);
    AssertIntEQ(XMEMCMP(key->pkey.ptr, pw, pwSz), 0);
    AssertNotNull(checkPw = wolfSSL_EVP_PKEY_get0_hmac(key, &checkPwSz));
    AssertIntEQ((int)checkPwSz, pwSz);
    AssertIntEQ(XMEMCMP(checkPw, pw, pwSz), 0);
    wolfSSL_EVP_PKEY_free(key);

    AssertNotNull(key = wolfSSL_EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, pw, 0));
    AssertIntEQ(key->pkey_sz, 0);
    checkPw = wolfSSL_EVP_PKEY_get0_hmac(key, &checkPwSz);
    (void)checkPw;
    AssertIntEQ((int)checkPwSz, 0);
    wolfSSL_EVP_PKEY_free(key);

    AssertNotNull(key = wolfSSL_EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL, NULL, 0));
    AssertIntEQ(key->pkey_sz, 0);
    checkPw = wolfSSL_EVP_PKEY_get0_hmac(key, &checkPwSz);
    (void)checkPw;
    AssertIntEQ((int)checkPwSz, 0);
    wolfSSL_EVP_PKEY_free(key);

    printf(resultFmt, passed);
#endif /* OPENSSL_EXTRA */
}


static void test_wolfSSL_EVP_MD_hmac_signing(void)
{
#ifdef OPENSSL_EXTRA
    const unsigned char testKey[] =
    {
        0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b,
        0x0b, 0x0b, 0x0b, 0x0b
    };
    const char testData[] = "Hi There";
    const unsigned char testResult[] =
    {
        0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53,
        0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
        0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7,
        0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7
    };
    unsigned char check[sizeof(testResult)];
    size_t checkSz = -1;
    WOLFSSL_EVP_PKEY* key;
    WOLFSSL_EVP_MD_CTX mdCtx;

    printf(testingFmt, "wolfSSL_EVP_MD_hmac_signing()");
    AssertNotNull(key = wolfSSL_EVP_PKEY_new_mac_key(EVP_PKEY_HMAC, NULL,
                                               testKey, (int)sizeof(testKey)));
    wolfSSL_EVP_MD_CTX_init(&mdCtx);
    AssertIntEQ(wolfSSL_EVP_DigestSignInit(&mdCtx, NULL, wolfSSL_EVP_sha256(),
                                                                NULL, key), 1);
    AssertIntEQ(wolfSSL_EVP_DigestSignUpdate(&mdCtx, testData,
                                          (unsigned int)XSTRLEN(testData)), 1);
    AssertIntEQ(wolfSSL_EVP_DigestSignFinal(&mdCtx, NULL, &checkSz), 1);
    AssertIntEQ((int)checkSz, sizeof(testResult));
    AssertIntEQ(wolfSSL_EVP_DigestSignFinal(&mdCtx, check, &checkSz), 1);
    AssertIntEQ((int)checkSz,(int)sizeof(testResult));
    AssertIntEQ(XMEMCMP(testResult, check, sizeof(testResult)), 0);
    AssertIntEQ(wolfSSL_EVP_MD_CTX_cleanup(&mdCtx), 1);

    wolfSSL_EVP_MD_CTX_init(&mdCtx);
    AssertIntEQ(wolfSSL_EVP_DigestSignInit(&mdCtx, NULL, wolfSSL_EVP_sha256(),
                                                                NULL, key), 1);
    AssertIntEQ(wolfSSL_EVP_DigestSignUpdate(&mdCtx, testData, 4), 1);
    AssertIntEQ(wolfSSL_EVP_DigestSignFinal(&mdCtx, NULL, &checkSz), 1);
    AssertIntEQ((int)checkSz, sizeof(testResult));
    AssertIntEQ(wolfSSL_EVP_DigestSignFinal(&mdCtx, check, &checkSz), 1);
    AssertIntEQ((int)checkSz,(int)sizeof(testResult));
    AssertIntEQ(wolfSSL_EVP_DigestSignUpdate(&mdCtx, testData + 4,
                                      (unsigned int)XSTRLEN(testData) - 4), 1);
    AssertIntEQ(wolfSSL_EVP_DigestSignFinal(&mdCtx, check, &checkSz), 1);
    AssertIntEQ((int)checkSz,(int)sizeof(testResult));
    AssertIntEQ(XMEMCMP(testResult, check, sizeof(testResult)), 0);
    AssertIntEQ(wolfSSL_EVP_MD_CTX_cleanup(&mdCtx), 1);

    wolfSSL_EVP_PKEY_free(key);
    printf(resultFmt, passed);
#endif /* OPENSSL_EXTRA */
}


static void test_wolfSSL_CTX_add_extra_chain_cert(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)
    char caFile[] = "./certs/client-ca.pem";
    char clientFile[] = "./certs/client-cert.pem";
    SSL_CTX* ctx;
    X509* x509 = NULL;

    printf(testingFmt, "wolfSSL_CTX_add_extra_chain_cert()");

    AssertNotNull(ctx = SSL_CTX_new(wolfSSLv23_server_method()));

    x509 = wolfSSL_X509_load_certificate_file(caFile, WOLFSSL_FILETYPE_PEM);
    AssertNotNull(x509);
    AssertIntEQ((int)SSL_CTX_add_extra_chain_cert(ctx, x509), WOLFSSL_SUCCESS);

    x509 = wolfSSL_X509_load_certificate_file(clientFile, WOLFSSL_FILETYPE_PEM);
    AssertNotNull(x509);

    #if !defined(HAVE_USER_RSA) && !defined(HAVE_FAST_RSA)
    /* additional test of getting EVP_PKEY key size from X509
     * Do not run with user RSA because wolfSSL_RSA_size is not currently
     * allowed with user RSA */
    {
        EVP_PKEY* pkey;
        #if defined(HAVE_ECC)
        X509* ecX509;
        #endif /* HAVE_ECC */

        AssertNotNull(pkey = X509_get_pubkey(x509));
        /* current RSA key is 2048 bit (256 bytes) */
        AssertIntEQ(EVP_PKEY_size(pkey), 256);

        EVP_PKEY_free(pkey);

        #if defined(HAVE_ECC)
        #if defined(USE_CERT_BUFFERS_256)
        AssertNotNull(ecX509 = wolfSSL_X509_load_certificate_buffer(
                    cliecc_cert_der_256, sizeof_cliecc_cert_der_256,
                    SSL_FILETYPE_ASN1));
        #else
        AssertNotNull(ecX509 = wolfSSL_X509_load_certificate_file(cliEccCertFile,
                    SSL_FILETYPE_PEM));
        #endif
        AssertNotNull(pkey = X509_get_pubkey(ecX509));
        /* current ECC key is 256 bit (32 bytes) */
        AssertIntEQ(EVP_PKEY_size(pkey), 32);

        X509_free(ecX509);
        EVP_PKEY_free(pkey);
        #endif /* HAVE_ECC */
    }
    #endif /* !defined(HAVE_USER_RSA) && !defined(HAVE_FAST_RSA) */

    AssertIntEQ((int)SSL_CTX_add_extra_chain_cert(ctx, x509), SSL_SUCCESS);

#ifdef WOLFSSL_ENCRYPTED_KEYS
    AssertNull(SSL_CTX_get_default_passwd_cb(ctx));
    AssertNull(SSL_CTX_get_default_passwd_cb_userdata(ctx));
#endif

    SSL_CTX_free(ctx);
    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
             !defined(NO_FILESYSTEM) && !defined(NO_RSA) */
}


#if !defined(NO_WOLFSSL_CLIENT) && !defined(NO_WOLFSSL_SERVER)
static void test_wolfSSL_ERR_peek_last_error_line(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && defined(DEBUG_WOLFSSL) && \
       !defined(NO_OLD_TLS) && !defined(WOLFSSL_NO_TLS12) && \
       defined(HAVE_IO_TESTS_DEPENDENCIES)
    tcp_ready ready;
    func_args client_args;
    func_args server_args;
#ifndef SINGLE_THREADED
    THREAD_TYPE serverThread;
#endif
    callback_functions client_cb;
    callback_functions server_cb;
    int         line = 0;
    int         flag = ERR_TXT_STRING;
    const char* file = NULL;
    const char* data = NULL;

    printf(testingFmt, "wolfSSL_ERR_peek_last_error_line()");

    /* create a failed connection and inspect the error */
#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif
    XMEMSET(&client_args, 0, sizeof(func_args));
    XMEMSET(&server_args, 0, sizeof(func_args));

    StartTCP();
    InitTcpReady(&ready);

    XMEMSET(&client_cb, 0, sizeof(callback_functions));
    XMEMSET(&server_cb, 0, sizeof(callback_functions));
    client_cb.method  = wolfTLSv1_1_client_method;
    server_cb.method  = wolfTLSv1_2_server_method;

    server_args.signal    = &ready;
    server_args.callbacks = &server_cb;
    client_args.signal    = &ready;
    client_args.callbacks = &client_cb;

#ifndef SINGLE_THREADED
    start_thread(test_server_nofail, &server_args, &serverThread);
    wait_tcp_ready(&server_args);
    test_client_nofail(&client_args, NULL);
    join_thread(serverThread);
#endif

    FreeTcpReady(&ready);

    AssertIntGT(ERR_get_error_line_data(NULL, NULL, &data, &flag), 0);
    AssertNotNull(data);

    /* check clearing error state */
    ERR_remove_state(0);
    AssertIntEQ((int)ERR_peek_last_error_line(NULL, NULL), 0);
    ERR_peek_last_error_line(NULL, &line);
    AssertIntEQ(line, 0);
    ERR_peek_last_error_line(&file, NULL);
    AssertNull(file);

    /* retry connection to fill error queue */
    XMEMSET(&client_args, 0, sizeof(func_args));
    XMEMSET(&server_args, 0, sizeof(func_args));

    StartTCP();
    InitTcpReady(&ready);

    client_cb.method  = wolfTLSv1_1_client_method;
    server_cb.method  = wolfTLSv1_2_server_method;

    server_args.signal    = &ready;
    server_args.callbacks = &server_cb;
    client_args.signal    = &ready;
    client_args.callbacks = &client_cb;

    start_thread(test_server_nofail, &server_args, &serverThread);
    wait_tcp_ready(&server_args);
    test_client_nofail(&client_args, NULL);
    join_thread(serverThread);

    FreeTcpReady(&ready);

    /* check that error code was stored */
    AssertIntNE((int)ERR_peek_last_error_line(NULL, NULL), 0);
    ERR_peek_last_error_line(NULL, &line);
    AssertIntNE(line, 0);
    ERR_peek_last_error_line(&file, NULL);
    AssertNotNull(file);

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    printf(resultFmt, passed);

    printf("\nTesting error print out\n");
    ERR_print_errors_fp(stdout);
    printf("Done testing print out\n\n");
    fflush(stdout);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
             !defined(NO_FILESYSTEM) && !defined(DEBUG_WOLFSSL) */
}
#endif

#if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)
static int verify_cb(int ok, X509_STORE_CTX *ctx)
{
    (void) ok;
    (void) ctx;
    printf("ENTER verify_cb\n");
    return SSL_SUCCESS;
}
#endif



static void test_wolfSSL_X509_STORE_CTX(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)

    X509_STORE_CTX* ctx;
    X509_STORE* str;
    X509* x509;

    printf(testingFmt, "wolfSSL_X509_STORE_CTX()");
    AssertNotNull(ctx = X509_STORE_CTX_new());
    AssertNotNull((str = wolfSSL_X509_STORE_new()));
    AssertNotNull((x509 =
                wolfSSL_X509_load_certificate_file(svrCertFile, SSL_FILETYPE_PEM)));
    AssertIntEQ(X509_STORE_add_cert(str, x509), SSL_SUCCESS);
    AssertIntEQ(X509_STORE_CTX_init(ctx, str, x509, NULL), SSL_SUCCESS);
    AssertIntEQ(SSL_get_ex_data_X509_STORE_CTX_idx(), 0);
    X509_STORE_CTX_set_error(ctx, -5);
    X509_STORE_CTX_set_error(NULL, -5);

    X509_STORE_CTX_free(ctx);

    AssertNotNull(ctx = X509_STORE_CTX_new());
    X509_STORE_CTX_set_verify_cb(ctx, (void *)verify_cb);
    X509_STORE_CTX_free(ctx);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
             !defined(NO_FILESYSTEM) && !defined(NO_RSA) */
}

static void test_wolfSSL_X509_STORE_set_flags(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)

    X509_STORE* store;
    X509* x509;

    printf(testingFmt, "wolfSSL_X509_STORE_set_flags()");
    AssertNotNull((store = wolfSSL_X509_STORE_new()));
    AssertNotNull((x509 =
                wolfSSL_X509_load_certificate_file(svrCertFile, WOLFSSL_FILETYPE_PEM)));
    AssertIntEQ(X509_STORE_add_cert(store, x509), WOLFSSL_SUCCESS);

#ifdef HAVE_CRL
    AssertIntEQ(X509_STORE_set_flags(store, WOLFSSL_CRL_CHECKALL), WOLFSSL_SUCCESS);
#else
    AssertIntEQ(X509_STORE_set_flags(store, WOLFSSL_CRL_CHECKALL),
        NOT_COMPILED_IN);
#endif

    wolfSSL_X509_free(x509);
    wolfSSL_X509_STORE_free(store);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
             !defined(NO_FILESYSTEM) && !defined(NO_RSA) */
}

static void test_wolfSSL_X509_LOOKUP_load_file(void)
{
    #if defined(OPENSSL_EXTRA) && defined(HAVE_CRL) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)
    WOLFSSL_X509_STORE*  store;
    WOLFSSL_X509_LOOKUP* lookup;

    printf(testingFmt, "wolfSSL_X509_LOOKUP_load_file()");

    AssertNotNull(store = wolfSSL_X509_STORE_new());
    AssertNotNull(lookup = X509_STORE_add_lookup(store, X509_LOOKUP_file()));
    AssertIntEQ(wolfSSL_X509_LOOKUP_load_file(lookup, "certs/client-ca.pem",
                                              X509_FILETYPE_PEM), 1);
    AssertIntEQ(wolfSSL_X509_LOOKUP_load_file(lookup, "certs/crl/crl2.pem",
                                                         X509_FILETYPE_PEM), 1);

    AssertIntEQ(wolfSSL_CertManagerVerify(store->cm, cliCertFile,
                WOLFSSL_FILETYPE_PEM), 1);
    AssertIntEQ(wolfSSL_CertManagerVerify(store->cm, svrCertFile,
                WOLFSSL_FILETYPE_PEM), ASN_NO_SIGNER_E);
    AssertIntEQ(wolfSSL_X509_LOOKUP_load_file(lookup, "certs/ca-cert.pem",
                                              X509_FILETYPE_PEM), 1);
    AssertIntEQ(wolfSSL_CertManagerVerify(store->cm, svrCertFile,
                WOLFSSL_FILETYPE_PEM), 1);

    wolfSSL_X509_STORE_free(store);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && defined(HAVE_CRL) && \
             !defined(NO_FILESYSTEM) && !defined(NO_RSA) */
}

static void test_wolfSSL_X509_STORE_CTX_set_time(void)
{
    #if defined(OPENSSL_EXTRA)
    WOLFSSL_X509_STORE_CTX*  ctx;
    time_t c_time;

    printf(testingFmt, "wolfSSL_X509_set_time()");
    AssertNotNull(ctx = wolfSSL_X509_STORE_CTX_new());
    c_time = 365*24*60*60;
    wolfSSL_X509_STORE_CTX_set_time(ctx, 0, c_time);
    AssertTrue(
      (ctx->param->flags & WOLFSSL_USE_CHECK_TIME) == WOLFSSL_USE_CHECK_TIME);
    AssertTrue(ctx->param->check_time == c_time);
    wolfSSL_X509_STORE_CTX_free(ctx);

    printf(resultFmt, passed);
    #endif /* OPENSSL_EXTRA */
}

static void test_wolfSSL_CTX_set_client_CA_list(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_RSA) && !defined(NO_CERTS)
    WOLFSSL_CTX* ctx;
    WOLF_STACK_OF(WOLFSSL_X509_NAME)* names = NULL;
    WOLF_STACK_OF(WOLFSSL_X509_NAME)* ca_list = NULL;

    printf(testingFmt, "wolfSSL_CTX_set_client_CA_list()");
    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_client_method()));
    names = wolfSSL_load_client_CA_file(cliCertFile);
    AssertNotNull(names);
    wolfSSL_CTX_set_client_CA_list(ctx,names);
    AssertNotNull(ca_list = wolfSSL_SSL_CTX_get_client_CA_list(ctx));
    wolfSSL_CTX_free(ctx);
    printf(resultFmt, passed);
#endif /* OPENSSL_EXTRA  && !NO_RSA && !NO_CERTS */
}

static void test_wolfSSL_CTX_add_client_CA(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_RSA) && !defined(NO_CERTS)
    WOLFSSL_CTX* ctx;
    WOLFSSL_X509* x509;
    WOLFSSL_X509* x509_a;
    WOLF_STACK_OF(WOLFSSLX509_NAME)* ca_list;
    int ret = 0;

    printf(testingFmt, "wolfSSL_CTX_add_client_CA()");
    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_client_method()));
    /* Add client cert */
    AssertNotNull(x509 = wolfSSL_X509_load_certificate_file(cliCertFile,
                                                      SSL_FILETYPE_PEM));
    ret = wolfSSL_CTX_add_client_CA(ctx, x509);
    AssertIntEQ(ret ,SSL_SUCCESS);
    AssertNotNull(ca_list = wolfSSL_SSL_CTX_get_client_CA_list(ctx));
    /* Add another client cert */
    AssertNotNull(x509_a = wolfSSL_X509_load_certificate_file(cliCertFile,
                                                        SSL_FILETYPE_PEM));
    AssertIntEQ(wolfSSL_CTX_add_client_CA(ctx, x509_a),SSL_SUCCESS);

    wolfSSL_X509_free(x509);
    wolfSSL_X509_free(x509_a);
    wolfSSL_CTX_free(ctx);

    printf(resultFmt, passed);
#endif /* OPENSSL_EXTRA  && !NO_RSA && !NO_CERTS */
}

static void test_wolfSSL_X509_NID(void)
{
    #if (defined(OPENSSL_EXTRA) || defined(OPENSSL_EXTRA_X509_SMALL)) && \
    !defined(NO_RSA) && defined(USE_CERT_BUFFERS_2048) && !defined(NO_ASN)
    int   sigType;
    int   nameSz;

    X509*  cert;
    EVP_PKEY*  pubKeyTmp;
    X509_NAME* name;

    char commonName[80];
    char countryName[80];
    char localityName[80];
    char stateName[80];
    char orgName[80];
    char orgUnit[80];

    printf(testingFmt, "wolfSSL_X509_NID()");
    /* ------ PARSE ORIGINAL SELF-SIGNED CERTIFICATE ------ */

    /* convert cert from DER to internal WOLFSSL_X509 struct */
    AssertNotNull(cert = wolfSSL_X509_d2i(&cert, client_cert_der_2048,
            sizeof_client_cert_der_2048));

    /* ------ EXTRACT CERTIFICATE ELEMENTS ------ */

    /* extract PUBLIC KEY from cert */
    AssertNotNull(pubKeyTmp = X509_get_pubkey(cert));

    /* extract signatureType */
    AssertIntNE((sigType = wolfSSL_X509_get_signature_type(cert)), 0);

    /* extract subjectName info */
    AssertNotNull(name = X509_get_subject_name(cert));
    AssertIntEQ(X509_NAME_get_text_by_NID(name, -1, NULL, 0), -1);
    AssertIntGT((nameSz = X509_NAME_get_text_by_NID(name, ASN_COMMON_NAME,
                                           NULL, 0)), 0);
    AssertIntEQ(nameSz, 15);
    AssertIntGT((nameSz = X509_NAME_get_text_by_NID(name, ASN_COMMON_NAME,
                                           commonName, sizeof(commonName))), 0);
    AssertIntEQ(nameSz, 15);
    AssertIntEQ(XMEMCMP(commonName, "www.wolfssl.com", nameSz), 0);
    AssertIntGT((nameSz = X509_NAME_get_text_by_NID(name, ASN_COMMON_NAME,
                                            commonName, 9)), 0);
    AssertIntEQ(nameSz, 8);
    AssertIntEQ(XMEMCMP(commonName, "www.wolf", nameSz), 0);

    AssertIntGT((nameSz = X509_NAME_get_text_by_NID(name, ASN_COUNTRY_NAME,
                                         countryName, sizeof(countryName))), 0);
    AssertIntEQ(XMEMCMP(countryName, "US", nameSz), 0);

    AssertIntGT((nameSz = X509_NAME_get_text_by_NID(name, ASN_LOCALITY_NAME,
                                       localityName, sizeof(localityName))), 0);
    AssertIntEQ(XMEMCMP(localityName, "Bozeman", nameSz), 0);

    AssertIntGT((nameSz = X509_NAME_get_text_by_NID(name, ASN_STATE_NAME,
                                            stateName, sizeof(stateName))), 0);
    AssertIntEQ(XMEMCMP(stateName, "Montana", nameSz), 0);

    AssertIntGT((nameSz = X509_NAME_get_text_by_NID(name, ASN_ORG_NAME,
                                            orgName, sizeof(orgName))), 0);
    AssertIntEQ(XMEMCMP(orgName, "wolfSSL_2048", nameSz), 0);

    AssertIntGT((nameSz = X509_NAME_get_text_by_NID(name, ASN_ORGUNIT_NAME,
                                            orgUnit, sizeof(orgUnit))), 0);
    AssertIntEQ(XMEMCMP(orgUnit, "Programming-2048", nameSz), 0);

    EVP_PKEY_free(pubKeyTmp);
    X509_free(cert);

    printf(resultFmt, passed);
    #endif
}

static void test_wolfSSL_CTX_set_srp_username(void)
{
#if defined(OPENSSL_EXTRA) && defined(WOLFCRYPT_HAVE_SRP) \
    && !defined(NO_SHA256) && !defined(WC_NO_RNG)
    WOLFSSL_CTX* ctx;
    const char *username = "TESTUSER";
    const char *password = "TESTPASSWORD";
    int r;

    printf(testingFmt, "wolfSSL_CTX_set_srp_username()");

    ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    AssertNotNull(ctx);
    r = wolfSSL_CTX_set_srp_username(ctx, (char *)username);
    AssertIntEQ(r,SSL_SUCCESS);
    wolfSSL_CTX_free(ctx);

    ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    AssertNotNull(ctx);
    r = wolfSSL_CTX_set_srp_password(ctx, (char *)password);
    AssertIntEQ(r,SSL_SUCCESS);
    r = wolfSSL_CTX_set_srp_username(ctx, (char *)username);
    AssertIntEQ(r,SSL_SUCCESS);
    wolfSSL_CTX_free(ctx);

    printf(resultFmt, passed);
#endif /* OPENSSL_EXTRA && WOLFCRYPT_HAVE_SRP */
       /* && !NO_SHA256 && !WC_NO_RNG */
}

static void test_wolfSSL_CTX_set_srp_password(void)
{
#if defined(OPENSSL_EXTRA) && defined(WOLFCRYPT_HAVE_SRP) \
    && !defined(NO_SHA256) && !defined(WC_NO_RNG)
    WOLFSSL_CTX* ctx;
    const char *username = "TESTUSER";
    const char *password = "TESTPASSWORD";
    int r;

    printf(testingFmt, "wolfSSL_CTX_set_srp_password()");
    ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    AssertNotNull(ctx);
    r = wolfSSL_CTX_set_srp_password(ctx, (char *)password);
    AssertIntEQ(r,SSL_SUCCESS);
    wolfSSL_CTX_free(ctx);

    ctx = wolfSSL_CTX_new(wolfSSLv23_client_method());
    AssertNotNull(ctx);
    r = wolfSSL_CTX_set_srp_username(ctx, (char *)username);
    AssertIntEQ(r,SSL_SUCCESS);
    r = wolfSSL_CTX_set_srp_password(ctx, (char *)password);
    AssertIntEQ(r,SSL_SUCCESS);
    wolfSSL_CTX_free(ctx);

    printf(resultFmt, passed);
#endif /* OPENSSL_EXTRA && WOLFCRYPT_HAVE_SRP */
       /* && !NO_SHA256 && !WC_NO_RNG */
}

static void test_wolfSSL_X509_STORE(void)
{
#if defined(OPENSSL_EXTRA) && defined(HAVE_CRL)
    X509_STORE *store;
    X509_CRL *crl;
    X509 *x509;
    const char crl_pem[] = "./certs/crl/crl.pem";
    const char svrCert[] = "./certs/server-cert.pem";
    XFILE fp;

    printf(testingFmt, "test_wolfSSL_X509_STORE");
    AssertNotNull(store = (X509_STORE *)X509_STORE_new());
    AssertNotNull((x509 =
                       wolfSSL_X509_load_certificate_file(svrCert, SSL_FILETYPE_PEM)));
    AssertIntEQ(X509_STORE_add_cert(store, x509), SSL_SUCCESS);
    X509_free(x509);
    AssertNotNull(fp = XFOPEN(crl_pem, "rb"));
    AssertNotNull(crl = (X509_CRL *)PEM_read_X509_CRL(fp, (X509_CRL **)NULL, NULL, NULL));
    XFCLOSE(fp);
    AssertIntEQ(X509_STORE_add_crl(store, crl), SSL_SUCCESS);
    X509_CRL_free(crl);
    X509_STORE_free(store);
    printf(resultFmt, passed);
#endif
    return;
}

static void test_wolfSSL_BN(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_ASN)
    BIGNUM* a;
    BIGNUM* b;
    BIGNUM* c;
    BIGNUM* d;
    ASN1_INTEGER* ai;
    unsigned char value[1];

    printf(testingFmt, "wolfSSL_BN()");

    AssertNotNull(b = BN_new());
    AssertNotNull(c = BN_new());
    AssertNotNull(d = BN_new());

    value[0] = 0x03;

    AssertNotNull(ai = ASN1_INTEGER_new());
    /* at the moment hard setting since no set function */
    ai->data[0] = 0x02; /* tag for ASN_INTEGER */
    ai->data[1] = 0x01; /* length of integer */
    ai->data[2] = value[0];

    AssertNotNull(a = ASN1_INTEGER_to_BN(ai, NULL));
    ASN1_INTEGER_free(ai);

    value[0] = 0x02;
    AssertNotNull(BN_bin2bn(value, sizeof(value), b));

    value[0] = 0x05;
    AssertNotNull(BN_bin2bn(value, sizeof(value), c));

    /* a^b mod c = */
    AssertIntEQ(BN_mod_exp(d, NULL, b, c, NULL), WOLFSSL_FAILURE);
    AssertIntEQ(BN_mod_exp(d, a, b, c, NULL), WOLFSSL_SUCCESS);

    /* check result  3^2 mod 5 */
    value[0] = 0;
    AssertIntEQ(BN_bn2bin(d, value), WOLFSSL_SUCCESS);
    AssertIntEQ(BN_bn2bin(d, value), SSL_SUCCESS);
    AssertIntEQ((int)(value[0]), 4);

    /* a*b mod c = */
    AssertIntEQ(BN_mod_mul(d, NULL, b, c, NULL), SSL_FAILURE);
    AssertIntEQ(BN_mod_mul(d, a, b, c, NULL), SSL_SUCCESS);

    /* check result  3*2 mod 5 */
    value[0] = 0;
    AssertIntEQ(BN_bn2bin(d, value), SSL_SUCCESS);
    AssertIntEQ((int)(value[0]), 1);

    /* BN_mod_inverse test */
    value[0] = 0;
    BIGNUM *r = BN_new();
    BIGNUM *val = BN_mod_inverse(r,b,c,NULL);
    AssertIntEQ(BN_bn2bin(r, value), 1);
    AssertIntEQ((int)(value[0] & 0x03), 3);
    BN_free(val);

    AssertIntEQ(BN_set_word(a, 1), SSL_SUCCESS);
    AssertIntEQ(BN_set_word(b, 5), SSL_SUCCESS);
    AssertIntEQ(BN_sub(c, a, b), SSL_SUCCESS);
#if defined(WOLFSSL_KEY_GEN) || defined(HAVE_COMP_KEY)
    {
    char* ret;
    AssertNotNull(ret = BN_bn2dec(c));
    AssertIntEQ(XMEMCMP(ret, "-4", sizeof("-4")), 0);
    XFREE(ret, NULL, DYNAMIC_TYPE_OPENSSL);
    }
#endif
    AssertIntEQ(BN_get_word(c), 4);

    BN_free(a);
    BN_free(b);
    BN_free(c);
    BN_clear_free(d);

    /* check that converting NULL and the null string returns an error */
    a = NULL;
    AssertIntLE(BN_hex2bn(&a, NULL), 0);
    AssertIntLE(BN_hex2bn(&a, ""), 0);
    AssertNull(a);

    /* check that getting a string and a bin of the same number are equal,
     * and that the comparison works EQ, LT and GT */
    AssertIntGT(BN_hex2bn(&a, "03"), 0);
    value[0] = 0x03;
    AssertNotNull(b = BN_new());
    AssertNotNull(BN_bin2bn(value, sizeof(value), b));
    value[0] = 0x04;
    AssertNotNull(c = BN_new());
    AssertNotNull(BN_bin2bn(value, sizeof(value), c));
    AssertIntEQ(BN_cmp(a, b), 0);
    AssertIntLT(BN_cmp(a, c), 0);
    AssertIntGT(BN_cmp(c, b), 0);

    BN_free(a);
    BN_free(b);
    BN_free(c);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_ASN) */
}

#if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
   !defined(NO_FILESYSTEM) && !defined(NO_RSA)
#define TEST_ARG 0x1234
static void msg_cb(int write_p, int version, int content_type,
                   const void *buf, size_t len, SSL *ssl, void *arg)
{
    (void)write_p;
    (void)version;
    (void)content_type;
    (void)buf;
    (void)len;
    (void)ssl;

    AssertTrue(arg == (void*)TEST_ARG);
}
#endif

#if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
   !defined(NO_FILESYSTEM) && defined(DEBUG_WOLFSSL) && \
   defined(HAVE_IO_TESTS_DEPENDENCIES)
#ifndef SINGLE_THREADED
static int msgCb(SSL_CTX *ctx, SSL *ssl)
{
    (void) ctx;
    (void) ssl;
    printf("\n===== msgcb called ====\n");
    #if defined(SESSION_CERTS) && defined(TEST_PEER_CERT_CHAIN)
    AssertTrue(SSL_get_peer_cert_chain(ssl) != NULL);
    AssertIntEQ(((WOLFSSL_X509_CHAIN *)SSL_get_peer_cert_chain(ssl))->count, 1);
    #endif
    return SSL_SUCCESS;
}
#endif
#endif

static void test_wolfSSL_msgCb(void)
{
  #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
     !defined(NO_FILESYSTEM) && defined(DEBUG_WOLFSSL) && \
     defined(HAVE_IO_TESTS_DEPENDENCIES) && !defined(NO_WOLFSSL_CLIENT) && \
     !defined(NO_WOLFSSL_SERVER)

    tcp_ready ready;
    func_args client_args;
    func_args server_args;
    #ifndef SINGLE_THREADED
    THREAD_TYPE serverThread;
    #endif
    callback_functions client_cb;
    callback_functions server_cb;

    printf(testingFmt, "test_wolfSSL_msgCb");

/* create a failed connection and inspect the error */
#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif
    XMEMSET(&client_args, 0, sizeof(func_args));
    XMEMSET(&server_args, 0, sizeof(func_args));

    StartTCP();
    InitTcpReady(&ready);

    XMEMSET(&client_cb, 0, sizeof(callback_functions));
    XMEMSET(&server_cb, 0, sizeof(callback_functions));
#ifndef WOLFSSL_NO_TLS12
    client_cb.method  = wolfTLSv1_2_client_method;
    server_cb.method  = wolfTLSv1_2_server_method;
#else
    client_cb.method  = wolfTLSv1_3_client_method;
    server_cb.method  = wolfTLSv1_3_server_method;
#endif

    server_args.signal    = &ready;
    server_args.callbacks = &server_cb;
    client_args.signal    = &ready;
    client_args.callbacks = &client_cb;
    client_args.return_code = TEST_FAIL;

    #ifndef SINGLE_THREADED
    start_thread(test_server_nofail, &server_args, &serverThread);
    wait_tcp_ready(&server_args);
    test_client_nofail(&client_args, (void *)msgCb);
    join_thread(serverThread);
    AssertTrue(client_args.return_code);
    AssertTrue(server_args.return_code);
    #endif

    FreeTcpReady(&ready);

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    printf(resultFmt, passed);

#endif
}

static void test_wolfSSL_set_options(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)
    SSL*     ssl;
    SSL_CTX* ctx;
    char appData[] = "extra msg";

    unsigned char protos[] = {
        7, 't', 'l', 's', '/', '1', '.', '2',
        8, 'h', 't', 't', 'p', '/', '1', '.', '1'
    };
    unsigned int len = sizeof(protos);

    void *arg = (void *)TEST_ARG;

    printf(testingFmt, "wolfSSL_set_options()");

    AssertNotNull(ctx = SSL_CTX_new(wolfSSLv23_server_method()));
    AssertTrue(SSL_CTX_use_certificate_file(ctx, svrCertFile, SSL_FILETYPE_PEM));
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, SSL_FILETYPE_PEM));

    AssertTrue(SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1) == SSL_OP_NO_TLSv1);
    AssertTrue(SSL_CTX_get_options(ctx) == SSL_OP_NO_TLSv1);

    AssertIntGT((int)SSL_CTX_set_options(ctx, (SSL_OP_COOKIE_EXCHANGE |
                                                              SSL_OP_NO_SSLv2)), 0);
    AssertTrue((SSL_CTX_set_options(ctx, SSL_OP_COOKIE_EXCHANGE) &
                                 SSL_OP_COOKIE_EXCHANGE) == SSL_OP_COOKIE_EXCHANGE);
    AssertTrue((SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_2) &
                                           SSL_OP_NO_TLSv1_2) == SSL_OP_NO_TLSv1_2);
    AssertTrue((SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION) &
                                   SSL_OP_NO_COMPRESSION) == SSL_OP_NO_COMPRESSION);
    AssertNull((SSL_CTX_clear_options(ctx, SSL_OP_NO_COMPRESSION) &
                                               SSL_OP_NO_COMPRESSION));

    SSL_CTX_free(ctx);

    AssertNotNull(ctx = SSL_CTX_new(wolfSSLv23_server_method()));
    AssertTrue(SSL_CTX_use_certificate_file(ctx, svrCertFile, SSL_FILETYPE_PEM));
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, SSL_FILETYPE_PEM));

    AssertNotNull(ssl = SSL_new(ctx));
#if defined(HAVE_EX_DATA) || defined(FORTRESS)
    AssertIntEQ(SSL_set_app_data(ssl, (void*)appData), SSL_SUCCESS);
    AssertNotNull(SSL_get_app_data((const WOLFSSL*)ssl));
    AssertIntEQ(XMEMCMP(SSL_get_app_data((const WOLFSSL*)ssl),
                appData, sizeof(appData)), 0);
#else
    AssertIntEQ(SSL_set_app_data(ssl, (void*)appData), SSL_FAILURE);
    AssertNull(SSL_get_app_data((const WOLFSSL*)ssl));
#endif

    AssertTrue(SSL_set_options(ssl, SSL_OP_NO_TLSv1) == SSL_OP_NO_TLSv1);
    AssertTrue(SSL_get_options(ssl) == SSL_OP_NO_TLSv1);

    AssertIntGT((int)SSL_set_options(ssl, (SSL_OP_COOKIE_EXCHANGE |
                                                          WOLFSSL_OP_NO_SSLv2)), 0);
    AssertTrue((SSL_set_options(ssl, SSL_OP_COOKIE_EXCHANGE) &
                             SSL_OP_COOKIE_EXCHANGE) == SSL_OP_COOKIE_EXCHANGE);
    AssertTrue((SSL_set_options(ssl, SSL_OP_NO_TLSv1_2) &
                                       SSL_OP_NO_TLSv1_2) == SSL_OP_NO_TLSv1_2);
    AssertTrue((SSL_set_options(ssl, SSL_OP_NO_COMPRESSION) &
                               SSL_OP_NO_COMPRESSION) == SSL_OP_NO_COMPRESSION);
    AssertNull((SSL_clear_options(ssl, SSL_OP_NO_COMPRESSION) &
                                       SSL_OP_NO_COMPRESSION));

    AssertTrue(SSL_set_msg_callback(ssl, msg_cb) == SSL_SUCCESS);
    SSL_set_msg_callback_arg(ssl, arg);

    AssertTrue(SSL_CTX_set_alpn_protos(ctx, protos, len) == SSL_SUCCESS);

    SSL_free(ssl);
    SSL_CTX_free(ctx);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
             !defined(NO_FILESYSTEM) && !defined(NO_RSA) */
}

/* Testing  wolfSSL_set_tlsext_status_type funciton.
 * PRE: OPENSSL and HAVE_CERTIFICATE_STATUS_REQUEST defined.
 */
static void test_wolfSSL_set_tlsext_status_type(void){
    #if defined(OPENSSL_EXTRA) && defined(HAVE_CERTIFICATE_STATUS_REQUEST)
    SSL*     ssl;
    SSL_CTX* ctx;

    printf(testingFmt, "wolfSSL_set_tlsext_status_type()");

    AssertNotNull(ctx = SSL_CTX_new(wolfSSLv23_server_method()));
    AssertTrue(SSL_CTX_use_certificate_file(ctx, svrCertFile, SSL_FILETYPE_PEM));
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, SSL_FILETYPE_PEM));
    AssertNotNull(ssl = SSL_new(ctx));
    AssertTrue(SSL_set_tlsext_status_type(ssl,TLSEXT_STATUSTYPE_ocsp)
               == SSL_SUCCESS);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    #endif  /* OPENSSL_EXTRA && HAVE_CERTIFICATE_STATUS_REQUEST */
}

static void test_wolfSSL_PEM_read_bio(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)
    byte buff[5300];
    FILE *f;
    int  bytes;
    X509* x509;
    BIO*  bio = NULL;
    BUF_MEM* buf;

    printf(testingFmt, "wolfSSL_PEM_read_bio()");

    AssertNotNull(f = fopen(cliCertFile, "rb"));
    bytes = (int)fread(buff, 1, sizeof(buff), f);
    fclose(f);

    AssertNull(x509 = PEM_read_bio_X509_AUX(bio, NULL, NULL, NULL));
    AssertNotNull(bio = BIO_new_mem_buf((void*)buff, bytes));
    AssertNotNull(x509 = PEM_read_bio_X509_AUX(bio, NULL, NULL, NULL));
    AssertIntEQ((int)BIO_set_fd(bio, 0, BIO_NOCLOSE), 1);
    AssertIntEQ(SSL_SUCCESS, BIO_get_mem_ptr(bio, &buf));

    BIO_free(bio);
    BUF_MEM_free(buf);
    X509_free(x509);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
             !defined(NO_FILESYSTEM) && !defined(NO_RSA) */
}


static void test_wolfSSL_BIO(void)
{
    #if defined(OPENSSL_EXTRA)
    byte buff[20];
    BIO* bio1;
    BIO* bio2;
    BIO* bio3;
    char* bufPt;
    int i;

    printf(testingFmt, "wolfSSL_BIO()");

    for (i = 0; i < 20; i++) {
        buff[i] = i;
    }

    /* Creating and testing type BIO_s_bio */
    AssertNotNull(bio1 = BIO_new(BIO_s_bio()));
    AssertNotNull(bio2 = BIO_new(BIO_s_bio()));
    AssertNotNull(bio3 = BIO_new(BIO_s_bio()));

    /* read/write before set up */
    AssertIntEQ(BIO_read(bio1, buff, 2),  WOLFSSL_BIO_UNSET);
    AssertIntEQ(BIO_write(bio1, buff, 2), WOLFSSL_BIO_UNSET);

    AssertIntEQ(BIO_set_write_buf_size(bio1, 20), WOLFSSL_SUCCESS);
    AssertIntEQ(BIO_set_write_buf_size(bio2, 8),  WOLFSSL_SUCCESS);
    AssertIntEQ(BIO_make_bio_pair(bio1, bio2),    WOLFSSL_SUCCESS);

    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 10), 10);
    XMEMCPY(bufPt, buff, 10);
    AssertIntEQ(BIO_write(bio1, buff + 10, 10), 10);
    /* write buffer full */
    AssertIntEQ(BIO_write(bio1, buff, 10), WOLFSSL_BIO_ERROR);
    AssertIntEQ(BIO_flush(bio1), WOLFSSL_SUCCESS);
    AssertIntEQ((int)BIO_ctrl_pending(bio1), 0);

    /* write the other direction with pair */
    AssertIntEQ((int)BIO_nwrite(bio2, &bufPt, 10), 8);
    XMEMCPY(bufPt, buff, 8);
    AssertIntEQ(BIO_write(bio2, buff, 10), WOLFSSL_BIO_ERROR);

    /* try read */
    AssertIntEQ((int)BIO_ctrl_pending(bio1), 8);
    AssertIntEQ((int)BIO_ctrl_pending(bio2), 20);

    AssertIntEQ(BIO_nread(bio2, &bufPt, (int)BIO_ctrl_pending(bio2)), 20);
    for (i = 0; i < 20; i++) {
        AssertIntEQ((int)bufPt[i], i);
    }
    AssertIntEQ(BIO_nread(bio2, &bufPt, 1), WOLFSSL_BIO_ERROR);
    AssertIntEQ(BIO_nread(bio1, &bufPt, (int)BIO_ctrl_pending(bio1)), 8);
    for (i = 0; i < 8; i++) {
        AssertIntEQ((int)bufPt[i], i);
    }
    AssertIntEQ(BIO_nread(bio1, &bufPt, 1), WOLFSSL_BIO_ERROR);
    AssertIntEQ(BIO_ctrl_reset_read_request(bio1), 1);

    /* new pair */
    AssertIntEQ(BIO_make_bio_pair(bio1, bio3), WOLFSSL_FAILURE);
    BIO_free(bio2); /* free bio2 and automaticly remove from pair */
    AssertIntEQ(BIO_make_bio_pair(bio1, bio3), WOLFSSL_SUCCESS);
    AssertIntEQ((int)BIO_ctrl_pending(bio3), 0);
    AssertIntEQ(BIO_nread(bio3, &bufPt, 10), WOLFSSL_BIO_ERROR);

    /* test wrap around... */
    AssertIntEQ(BIO_reset(bio1), 0);
    AssertIntEQ(BIO_reset(bio3), 0);

    /* fill write buffer, read only small amount then write again */
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 20), 20);
    XMEMCPY(bufPt, buff, 20);
    AssertIntEQ(BIO_nread(bio3, &bufPt, 4), 4);
    for (i = 0; i < 4; i++) {
        AssertIntEQ(bufPt[i], i);
    }

    /* try writing over read index */
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 5), 4);
    XMEMSET(bufPt, 0, 4);
    AssertIntEQ((int)BIO_ctrl_pending(bio3), 20);

    /* read and write 0 bytes */
    AssertIntEQ(BIO_nread(bio3, &bufPt, 0), 0);
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 0), 0);

    /* should read only to end of write buffer then need to read again */
    AssertIntEQ(BIO_nread(bio3, &bufPt, 20), 16);
    for (i = 0; i < 16; i++) {
        AssertIntEQ(bufPt[i], buff[4 + i]);
    }

    AssertIntEQ(BIO_nread(bio3, NULL, 0), WOLFSSL_FAILURE);
    AssertIntEQ(BIO_nread0(bio3, &bufPt), 4);
    for (i = 0; i < 4; i++) {
        AssertIntEQ(bufPt[i], 0);
    }

    /* read index should not have advanced with nread0 */
    AssertIntEQ(BIO_nread(bio3, &bufPt, 5), 4);
    for (i = 0; i < 4; i++) {
        AssertIntEQ(bufPt[i], 0);
    }

    /* write and fill up buffer checking reset of index state */
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 20), 20);
    XMEMCPY(bufPt, buff, 20);

    /* test reset on data in bio1 write buffer */
    AssertIntEQ(BIO_reset(bio1), 0);
    AssertIntEQ((int)BIO_ctrl_pending(bio3), 0);
    AssertIntEQ(BIO_nread(bio3, &bufPt, 3), WOLFSSL_BIO_ERROR);
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 20), 20);
    XMEMCPY(bufPt, buff, 20);
    AssertIntEQ(BIO_nread(bio3, &bufPt, 6), 6);
    for (i = 0; i < 6; i++) {
        AssertIntEQ(bufPt[i], i);
    }

    /* test case of writing twice with offset read index */
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 3), 3);
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 4), 3); /* try overwriting */
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 4), WOLFSSL_BIO_ERROR);
    AssertIntEQ(BIO_nread(bio3, &bufPt, 0), 0);
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 4), WOLFSSL_BIO_ERROR);
    AssertIntEQ(BIO_nread(bio3, &bufPt, 1), 1);
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 4), 1);
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 4), WOLFSSL_BIO_ERROR);

    BIO_free(bio1);
    BIO_free(bio3);

    /* BIOs with file pointers */
    #if !defined(NO_FILESYSTEM)
    {
        XFILE f1;
        XFILE f2;
        BIO*  f_bio1;
        BIO*  f_bio2;
        unsigned char cert[300];
        char testFile[] = "tests/bio_write_test.txt";
        char msg[]      = "bio_write_test.txt contains the first 300 bytes of certs/server-cert.pem\ncreated by tests/unit.test\n\n";

        AssertNotNull(f_bio1 = BIO_new(BIO_s_file()));
        AssertNotNull(f_bio2 = BIO_new(BIO_s_file()));

        AssertIntEQ((int)BIO_set_mem_eof_return(f_bio1, -1), 0);
        AssertIntEQ((int)BIO_set_mem_eof_return(NULL, -1),   0);

        f1 = XFOPEN(svrCertFile, "rwb");
        AssertIntEQ((int)BIO_set_fp(f_bio1, f1, BIO_CLOSE), WOLFSSL_SUCCESS);
        AssertIntEQ(BIO_write_filename(f_bio2, testFile),
                WOLFSSL_SUCCESS);

        AssertIntEQ(BIO_read(f_bio1, cert, sizeof(cert)), sizeof(cert));
        AssertIntEQ(BIO_write(f_bio2, msg, sizeof(msg)), sizeof(msg));
        AssertIntEQ(BIO_write(f_bio2, cert, sizeof(cert)), sizeof(cert));

        AssertIntEQ((int)BIO_get_fp(f_bio2, &f2), WOLFSSL_SUCCESS);
        AssertIntEQ(BIO_reset(f_bio2), 0);
        AssertIntEQ(BIO_seek(f_bio2, 4), 0);

        BIO_free(f_bio1);
        BIO_free(f_bio2);

        AssertNotNull(f_bio1 = BIO_new_file(svrCertFile, "rwb"));
        AssertIntEQ((int)BIO_set_mem_eof_return(f_bio1, -1), 0);
        AssertIntEQ(BIO_read(f_bio1, cert, sizeof(cert)), sizeof(cert));
        BIO_free(f_bio1);

    }
    #endif /* !defined(NO_FILESYSTEM) */

    printf(resultFmt, passed);
    #endif
}


static void test_wolfSSL_ASN1_STRING(void)
{
    #if defined(OPENSSL_EXTRA)
    ASN1_STRING* str = NULL;
    const char data[] = "hello wolfSSL";

    printf(testingFmt, "wolfSSL_ASN1_STRING()");

    AssertNotNull(str = ASN1_STRING_type_new(V_ASN1_OCTET_STRING));
    AssertIntEQ(ASN1_STRING_set(str, (const void*)data, sizeof(data)), 1);
    AssertIntEQ(ASN1_STRING_set(str, (const void*)data, -1), 1);
    AssertIntEQ(ASN1_STRING_set(str, NULL, -1), 0);

    ASN1_STRING_free(str);

    printf(resultFmt, passed);
    #endif
}


static void test_wolfSSL_DES_ecb_encrypt(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_DES3) && defined(WOLFSSL_DES_ECB)
    WOLFSSL_DES_cblock input1,input2,output1,output2,back1,back2;
    WOLFSSL_DES_key_schedule key;

    printf(testingFmt, "wolfSSL_DES_ecb_encrypt()");

    XMEMCPY(key,"12345678",sizeof(WOLFSSL_DES_key_schedule));
    XMEMCPY(input1, "Iamhuman",sizeof(WOLFSSL_DES_cblock));
    XMEMCPY(input2, "Whoisit?",sizeof(WOLFSSL_DES_cblock));
    XMEMSET(output1, 0, sizeof(WOLFSSL_DES_cblock));
    XMEMSET(output2, 0, sizeof(WOLFSSL_DES_cblock));
    XMEMSET(back1, 0, sizeof(WOLFSSL_DES_cblock));
    XMEMSET(back2, 0, sizeof(WOLFSSL_DES_cblock));

    /* Encrypt messages */
    wolfSSL_DES_ecb_encrypt(&input1,&output1,&key,DES_ENCRYPT);
    wolfSSL_DES_ecb_encrypt(&input2,&output2,&key,DES_ENCRYPT);

    /* Decrypt messages */
    int ret1 = 0;
    int ret2 = 0;
    wolfSSL_DES_ecb_encrypt(&output1,&back1,&key,DES_DECRYPT);
    ret1 = XMEMCMP((unsigned char *) back1,(unsigned char *) input1,sizeof(WOLFSSL_DES_cblock));
    AssertIntEQ(ret1,0);
    wolfSSL_DES_ecb_encrypt(&output2,&back2,&key,DES_DECRYPT);
    ret2 = XMEMCMP((unsigned char *) back2,(unsigned char *) input2,sizeof(WOLFSSL_DES_cblock));
    AssertIntEQ(ret2,0);

    printf(resultFmt, passed);
    #endif
}

static void test_wolfSSL_ASN1_TIME_adj(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_ASN1_TIME) \
&& !defined(USER_TIME) && !defined(TIME_OVERRIDES)

    const int year = 365*24*60*60;
    const int day  = 24*60*60;
    const int hour = 60*60;
    const int mini = 60;
    const byte asn_utc_time = ASN_UTC_TIME;
#if !defined(TIME_T_NOT_LONG) && !defined(NO_64BIT)
    const byte asn_gen_time = ASN_GENERALIZED_TIME;
#endif
    WOLFSSL_ASN1_TIME *asn_time, *s;
    int offset_day;
    long offset_sec;
    char date_str[20];
    time_t t;

    printf(testingFmt, "wolfSSL_ASN1_TIME_adj()");

    s = (WOLFSSL_ASN1_TIME*)XMALLOC(sizeof(WOLFSSL_ASN1_TIME), NULL,
                                    DYNAMIC_TYPE_OPENSSL);
    /* UTC notation test */
    /* 2000/2/15 20:30:00 */
    t = (time_t)30 * year + 45 * day + 20 * hour + 30 * mini + 7 * day;
    offset_day = 7;
    offset_sec = 45 * mini;
    /* offset_sec = -45 * min;*/
    asn_time = wolfSSL_ASN1_TIME_adj(s, t, offset_day, offset_sec);
    AssertTrue(asn_time->data[0] == asn_utc_time);
    XSTRNCPY(date_str,(const char*) &asn_time->data+2,13);
    AssertIntEQ(0, XMEMCMP(date_str, "000222211500Z", 13));

    /* negative offset */
    offset_sec = -45 * mini;
    asn_time = wolfSSL_ASN1_TIME_adj(s, t, offset_day, offset_sec);
    AssertTrue(asn_time->data[0] == asn_utc_time);
    XSTRNCPY(date_str,(const char*) &asn_time->data+2,13);
    AssertIntEQ(0, XMEMCMP(date_str, "000222194500Z", 13));

    XFREE(s,NULL,DYNAMIC_TYPE_OPENSSL);
    XMEMSET(date_str, 0, sizeof(date_str));

    /* Generalized time will overflow time_t if not long */
#if !defined(TIME_T_NOT_LONG) && !defined(NO_64BIT)
    s = (WOLFSSL_ASN1_TIME*)XMALLOC(sizeof(WOLFSSL_ASN1_TIME), NULL,
                                    DYNAMIC_TYPE_OPENSSL);
    /* GeneralizedTime notation test */
    /* 2055/03/01 09:00:00 */
    t = (time_t)85 * year + 59 * day + 9 * hour + 21 * day;
        offset_day = 12;
        offset_sec = 10 * mini;
    asn_time = wolfSSL_ASN1_TIME_adj(s, t, offset_day, offset_sec);
    AssertTrue(asn_time->data[0] == asn_gen_time);
    XSTRNCPY(date_str,(const char*) &asn_time->data+2, 15);
    AssertIntEQ(0, XMEMCMP(date_str, "20550313091000Z", 15));

    XFREE(s,NULL,DYNAMIC_TYPE_OPENSSL);
    XMEMSET(date_str, 0, sizeof(date_str));
#endif /* !TIME_T_NOT_LONG && !NO_64BIT */

    /* if WOLFSSL_ASN1_TIME struct is not allocated */
    s = NULL;

    t = (time_t)30 * year + 45 * day + 20 * hour + 30 * mini + 15 + 7 * day;
    offset_day = 7;
    offset_sec = 45 * mini;
    asn_time = wolfSSL_ASN1_TIME_adj(s, t, offset_day, offset_sec);
    AssertTrue(asn_time->data[0] == asn_utc_time);
    XSTRNCPY(date_str,(const char*) &asn_time->data+2,13);
    AssertIntEQ(0, XMEMCMP(date_str, "000222211515Z", 13));
    XFREE(asn_time,NULL,DYNAMIC_TYPE_OPENSSL);

    asn_time = wolfSSL_ASN1_TIME_adj(NULL, t, offset_day, offset_sec);
    AssertTrue(asn_time->data[0] == asn_utc_time);
    XSTRNCPY(date_str,(const char*) &asn_time->data+2,13);
    AssertIntEQ(0, XMEMCMP(date_str, "000222211515Z", 13));
    XFREE(asn_time,NULL,DYNAMIC_TYPE_OPENSSL);

    printf(resultFmt, passed);
#endif
}


static void test_wolfSSL_X509(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && !defined(NO_FILESYSTEM)\
    && !defined(NO_RSA)
    X509* x509;
    BIO*  bio;
    X509_STORE_CTX* ctx;
    X509_STORE* store;

    char der[] = "certs/ca-cert.der";
    XFILE fp;

    printf(testingFmt, "wolfSSL_X509()");

    AssertNotNull(x509 = X509_new());
    X509_free(x509);

    x509 = wolfSSL_X509_load_certificate_file(cliCertFile, SSL_FILETYPE_PEM);

    AssertNotNull(bio = BIO_new(BIO_s_mem()));

    AssertIntEQ(i2d_X509_bio(bio, x509), SSL_SUCCESS);

    AssertNotNull(ctx = X509_STORE_CTX_new());

    AssertIntEQ(X509_verify_cert(ctx), SSL_FATAL_ERROR);

    AssertNotNull(store = X509_STORE_new());
    AssertIntEQ(X509_STORE_add_cert(store, x509), SSL_SUCCESS);
    AssertIntEQ(X509_STORE_CTX_init(ctx, store, x509, NULL), SSL_SUCCESS);
    AssertIntEQ(X509_verify_cert(ctx), SSL_SUCCESS);


    X509_STORE_CTX_free(ctx);
    BIO_free(bio);

    /** d2i_X509_fp test **/
    AssertNotNull(fp = XFOPEN(der, "rb"));
    AssertNotNull(x509 = (X509 *)d2i_X509_fp(fp, (X509 **)NULL));
    AssertNotNull(x509);
    X509_free(x509);
    XFCLOSE(fp);
    AssertNotNull(fp = XFOPEN(der, "rb"));
    AssertNotNull((X509 *)d2i_X509_fp(fp, (X509 **)&x509));
    AssertNotNull(x509);
    X509_free(x509);
    XFCLOSE(fp);

    printf(resultFmt, passed);
    #endif
}


static void test_wolfSSL_RAND(void)
{
    #if defined(OPENSSL_EXTRA)
    byte seed[16];

    printf(testingFmt, "wolfSSL_RAND()");

    RAND_seed(seed, sizeof(seed));
    AssertIntEQ(RAND_poll(), 1);
    RAND_cleanup();

    AssertIntEQ(RAND_egd(NULL), -1);
#ifndef NO_FILESYSTEM
    {
        char fname[100];

        AssertNotNull(RAND_file_name(fname, (sizeof(fname) - 1)));
        AssertIntEQ(RAND_write_file(NULL), 0);
    }
#endif

    printf(resultFmt, passed);
    #endif
}


static void test_wolfSSL_BUF(void)
{
    #if defined(OPENSSL_EXTRA)
    BUF_MEM* buf;
    AssertNotNull(buf = BUF_MEM_new());
    AssertIntEQ(BUF_MEM_grow(buf, 10), 10);
    AssertIntEQ(BUF_MEM_grow(buf, -1), 0);
    BUF_MEM_free(buf);
    #endif /* OPENSSL_EXTRA */
}


static void test_wolfSSL_pseudo_rand(void)
{
    #if defined(OPENSSL_EXTRA)
    BIGNUM* bn;
    unsigned char bin[8];
    int i;

    printf(testingFmt, "wolfSSL_pseudo_rand()");

    /* BN_pseudo_rand returns 1 on success 0 on failure
     * int BN_pseudo_rand(BIGNUM* bn, int bits, int top, int bottom) */
    for (i = 0; i < 10; i++) {
        AssertNotNull(bn = BN_new());
        AssertIntEQ(BN_pseudo_rand(bn, 8, 0, 0), SSL_SUCCESS);
        AssertIntGT(BN_bn2bin(bn, bin),0);
        AssertIntEQ((bin[0] & 0x80), 0x80); /* top bit should be set */
        BN_free(bn);
    }

    for (i = 0; i < 10; i++) {
        AssertNotNull(bn = BN_new());
        AssertIntEQ(BN_pseudo_rand(bn, 8, 1, 1), SSL_SUCCESS);
        AssertIntGT(BN_bn2bin(bn, bin),0);
        AssertIntEQ((bin[0] & 0xc1), 0xc1); /* top bit should be set */
        BN_free(bn);
    }

    printf(resultFmt, passed);
    #endif
}

static void test_wolfSSL_PKCS8_Compat(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_FILESYSTEM) && defined(HAVE_ECC)
    PKCS8_PRIV_KEY_INFO* pt;
    BIO* bio;
    FILE* f;
    int bytes;
    char buffer[512];

    printf(testingFmt, "wolfSSL_pkcs8()");

    /* file from wolfssl/certs/ directory */
    AssertNotNull(f = fopen("./certs/ecc-keyPkcs8.pem", "rb"));
    AssertIntGT((bytes = (int)fread(buffer, 1, sizeof(buffer), f)), 0);
    fclose(f);
    AssertNotNull(bio = BIO_new_mem_buf((void*)buffer, bytes));
    AssertNotNull(pt = d2i_PKCS8_PRIV_KEY_INFO_bio(bio, NULL));
    BIO_free(bio);
    PKCS8_PRIV_KEY_INFO_free(pt);

    printf(resultFmt, passed);
    #endif
}


static void test_wolfSSL_ERR_put_error(void)
{
    #if defined(OPENSSL_EXTRA) && defined(DEBUG_WOLFSSL)
    const char* file;
    int line;

    printf(testingFmt, "wolfSSL_ERR_put_error()");


    ERR_clear_error(); /* clear out any error nodes */
    ERR_put_error(0,SYS_F_ACCEPT, 0, "this file", 0);
    AssertIntEQ(ERR_get_error_line(&file, &line), 0);
    ERR_put_error(0,SYS_F_BIND, 1, "this file", 1);
    AssertIntEQ(ERR_get_error_line(&file, &line), 1);
    ERR_put_error(0,SYS_F_CONNECT, 2, "this file", 2);
    AssertIntEQ(ERR_get_error_line(&file, &line), 2);
    ERR_put_error(0,SYS_F_FOPEN, 3, "this file", 3);
    AssertIntEQ(ERR_get_error_line(&file, &line), 3);
    ERR_put_error(0,SYS_F_FREAD, 4, "this file", 4);
    AssertIntEQ(ERR_get_error_line(&file, &line), 4);
    ERR_put_error(0,SYS_F_GETADDRINFO, 5, "this file", 5);
    AssertIntEQ(ERR_get_error_line(&file, &line), 5);
    ERR_put_error(0,SYS_F_GETSOCKOPT, 6, "this file", 6);
    AssertIntEQ(ERR_get_error_line(&file, &line), 6);
    ERR_put_error(0,SYS_F_GETSOCKNAME, 7, "this file", 7);
    AssertIntEQ(ERR_get_error_line(&file, &line), 7);
    ERR_put_error(0,SYS_F_GETHOSTBYNAME, 8, "this file", 8);
    AssertIntEQ(ERR_get_error_line(&file, &line), 8);
    ERR_put_error(0,SYS_F_GETNAMEINFO, 9, "this file", 9);
    AssertIntEQ(ERR_get_error_line(&file, &line), 9);
    ERR_put_error(0,SYS_F_GETSERVBYNAME, 10, "this file", 10);
    AssertIntEQ(ERR_get_error_line(&file, &line), 10);
    ERR_put_error(0,SYS_F_IOCTLSOCKET, 11, "this file", 11);
    AssertIntEQ(ERR_get_error_line(&file, &line), 11);
    ERR_put_error(0,SYS_F_LISTEN, 12, "this file", 12);
    AssertIntEQ(ERR_get_error_line(&file, &line), 12);
    ERR_put_error(0,SYS_F_OPENDIR, 13, "this file", 13);
    AssertIntEQ(ERR_get_error_line(&file, &line), 13);
    ERR_put_error(0,SYS_F_SETSOCKOPT, 14, "this file", 14);
    AssertIntEQ(ERR_get_error_line(&file, &line), 14);
    ERR_put_error(0,SYS_F_SOCKET, 15, "this file", 15);
    AssertIntEQ(ERR_get_error_line(&file, &line), 15);

    /* try reading past end of error queue */
    file = NULL;
    AssertIntEQ(ERR_get_error_line(&file, &line), 0);
    AssertNull(file);
    AssertIntEQ(ERR_get_error_line_data(&file, &line, NULL, NULL), 0);

    /* Empty and free up all error nodes */
    ERR_clear_error();

    printf(resultFmt, passed);
    #endif
}


static void test_wolfSSL_HMAC(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_SHA256)
    HMAC_CTX hmac;
    ENGINE* e = NULL;
    const unsigned char key[] = "simple test key";
    unsigned char hash[WC_MAX_DIGEST_SIZE];
    unsigned int len;


    printf(testingFmt, "wolfSSL_HMAC()");

    HMAC_CTX_init(&hmac);
    AssertIntEQ(HMAC_Init_ex(&hmac, (void*)key, (int)sizeof(key),
                EVP_sha256(), e), SSL_SUCCESS);

    /* re-using test key as data to hash */
    AssertIntEQ(HMAC_Update(&hmac, key, (int)sizeof(key)), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&hmac, NULL, 0), SSL_SUCCESS);
    AssertIntEQ(HMAC_Final(&hmac, hash, &len), SSL_SUCCESS);
    AssertIntEQ(len, (int)WC_SHA256_DIGEST_SIZE);

    HMAC_cleanup(&hmac);
#endif

#if defined(OPENSSL_EXTRA) && !defined(NO_SHA256)
    len = 0;
    AssertNotNull(HMAC(EVP_sha256(), key, (int)sizeof(key), NULL, 0, hash, &len));
    AssertIntEQ(len, (int)WC_SHA256_DIGEST_SIZE);
#endif
#if defined(OPENSSL_EXTRA) && defined(WOLFSSL_SHA224)
    len = 0;
    AssertNotNull(HMAC(EVP_sha224(), key, (int)sizeof(key), NULL, 0, hash, &len));
    AssertIntEQ(len, (int)WC_SHA224_DIGEST_SIZE);
#endif
#if defined(OPENSSL_EXTRA) && (defined(WOLFSSL_SHA384) && defined(WOLFSSL_SHA512))
    len = 0;
    AssertNotNull(HMAC(EVP_sha384(), key, (int)sizeof(key), NULL, 0, hash, &len));
    AssertIntEQ(len, (int)WC_SHA384_DIGEST_SIZE);
#endif
#if defined(OPENSSL_EXTRA) && defined(WOLFSSL_SHA512)
    len = 0;
    AssertNotNull(HMAC(EVP_sha512(), key, (int)sizeof(key), NULL, 0, hash, &len));
    AssertIntEQ(len, (int)WC_SHA512_DIGEST_SIZE);
#endif

    printf(resultFmt, passed);

}


static void test_wolfSSL_OBJ(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_SHA256)
    ASN1_OBJECT* obj = NULL;
    char buf[50];

    printf(testingFmt, "wolfSSL_OBJ()");

    AssertIntEQ(OBJ_obj2txt(buf, (int)sizeof(buf), obj, 1), SSL_FAILURE);
    AssertNotNull(obj = OBJ_nid2obj(NID_any_policy));
    AssertIntEQ(OBJ_obj2nid(obj), NID_any_policy);
    AssertIntEQ(OBJ_obj2txt(buf, (int)sizeof(buf), obj, 1), 11);
    AssertIntGT(OBJ_obj2txt(buf, (int)sizeof(buf), obj, 0), 0);
    ASN1_OBJECT_free(obj);

    AssertNotNull(obj = OBJ_nid2obj(NID_sha256));
    AssertIntEQ(OBJ_obj2nid(obj), NID_sha256);
    AssertIntEQ(OBJ_obj2txt(buf, (int)sizeof(buf), obj, 1), 22);
    AssertIntGT(OBJ_obj2txt(buf, (int)sizeof(buf), obj, 0), 0);
    ASN1_OBJECT_free(obj);

    printf(resultFmt, passed);
    #endif
}


static void test_wolfSSL_X509_NAME_ENTRY(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) \
    && !defined(NO_FILESYSTEM) && !defined(NO_RSA) && defined(WOLFSSL_CERT_GEN)
    X509*      x509;
    BIO*       bio;
    X509_NAME* nm;
    X509_NAME_ENTRY* entry;
    unsigned char cn[] = "another name to add";


    printf(testingFmt, "wolfSSL_X509_NAME_ENTRY()");

    AssertNotNull(x509 =
            wolfSSL_X509_load_certificate_file(cliCertFile, SSL_FILETYPE_PEM));
    AssertNotNull(bio = BIO_new(BIO_s_mem()));
    AssertIntEQ(PEM_write_bio_X509_AUX(bio, x509), SSL_SUCCESS);

#ifdef WOLFSSL_CERT_REQ
    {
        X509_REQ* req;
        BIO*      bReq;

        AssertNotNull(req =
            wolfSSL_X509_load_certificate_file(cliCertFile, SSL_FILETYPE_PEM));
        AssertNotNull(bReq = BIO_new(BIO_s_mem()));
        AssertIntEQ(PEM_write_bio_X509_REQ(bReq, req), SSL_SUCCESS);

        BIO_free(bReq);
        X509_free(req);
    }
#endif

    AssertNotNull(nm = X509_get_subject_name(x509));
    AssertNotNull(entry = X509_NAME_ENTRY_create_by_NID(NULL, NID_commonName,
                0x0c, cn, (int)sizeof(cn)));
    AssertIntEQ(X509_NAME_add_entry(nm, entry, -1, 0), SSL_SUCCESS);


    X509_NAME_ENTRY_free(entry);
    BIO_free(bio);
    X509_free(x509);

    printf(resultFmt, passed);
    #endif
}


static void test_wolfSSL_BIO_gets(void)
{
    #if defined(OPENSSL_EXTRA)
    BIO* bio;
    BIO* bio2;
    char msg[] = "\nhello wolfSSL\n security plus\t---...**adf\na...b.c";
    char emp[] = "";
    char buffer[20];
    int bufferSz = 20;

    printf(testingFmt, "wolfSSL_X509_BIO_gets()");

    /* try with bad args */
    AssertNull(bio = BIO_new_mem_buf(NULL, sizeof(msg)));
    AssertNull(bio = BIO_new_mem_buf((void*)msg, -1));

    /* try with real msg */
    AssertNotNull(bio = BIO_new_mem_buf((void*)msg, sizeof(msg)));
    XMEMSET(buffer, 0, bufferSz);
    AssertNotNull(BIO_push(bio, BIO_new(BIO_s_bio())));
    AssertNull(bio2 = BIO_find_type(bio, BIO_TYPE_FILE));
    AssertNotNull(bio2 = BIO_find_type(bio, BIO_TYPE_BIO));
    AssertFalse(bio2 != BIO_next(bio));

    /* make buffer filled with no terminating characters */
    XMEMSET(buffer, 1, bufferSz);

    /* BIO_gets reads a line of data */
    AssertIntEQ(BIO_gets(bio, buffer, -3), 0);
    AssertIntEQ(BIO_gets(bio, buffer, bufferSz), 1);
    AssertIntEQ(BIO_gets(bio, buffer, bufferSz), 14);
    AssertStrEQ(buffer, "hello wolfSSL\n");
    AssertIntEQ(BIO_gets(bio, buffer, bufferSz), 19);
    AssertIntEQ(BIO_gets(bio, buffer, bufferSz), 8);
    AssertIntEQ(BIO_gets(bio, buffer, -1), 0);

    /* check not null terminated string */
    BIO_free(bio);
    msg[0] = 0x33;
    msg[1] = 0x33;
    msg[2] = 0x33;
    AssertNotNull(bio = BIO_new_mem_buf((void*)msg, 3));
    AssertIntEQ(BIO_gets(bio, buffer, 3), 2);
    AssertIntEQ(buffer[0], msg[0]);
    AssertIntEQ(buffer[1], msg[1]);
    AssertIntNE(buffer[2], msg[2]);

    BIO_free(bio);
    msg[3]    = 0x33;
    buffer[3] = 0x33;
    AssertNotNull(bio = BIO_new_mem_buf((void*)msg, 3));
    AssertIntEQ(BIO_gets(bio, buffer, bufferSz), 3);
    AssertIntEQ(buffer[0], msg[0]);
    AssertIntEQ(buffer[1], msg[1]);
    AssertIntEQ(buffer[2], msg[2]);
    AssertIntNE(buffer[3], 0x33); /* make sure null terminator was set */

    /* check reading an empty string */
    BIO_free(bio);
    AssertNotNull(bio = BIO_new_mem_buf((void*)emp, sizeof(emp)));
    AssertIntEQ(BIO_gets(bio, buffer, bufferSz), 1); /* just terminator */
    AssertStrEQ(emp, buffer);

    /* check error cases */
    BIO_free(bio);
    AssertIntEQ(BIO_gets(NULL, NULL, 0), SSL_FAILURE);
    AssertNotNull(bio = BIO_new(BIO_s_mem()));
    AssertIntEQ(BIO_gets(bio, buffer, 2), -1); /* nothing to read */

#if !defined(NO_FILESYSTEM)
    {
        BIO*  f_bio;
        XFILE f;
        AssertNotNull(f_bio = BIO_new(BIO_s_file()));
        AssertIntLE(BIO_gets(f_bio, buffer, bufferSz), 0);

        f = XFOPEN(svrCertFile, "rb");
        AssertIntEQ((int)BIO_set_fp(f_bio, f, BIO_CLOSE), SSL_SUCCESS);
        AssertIntGT(BIO_gets(f_bio, buffer, bufferSz), 0);

        BIO_free(f_bio);
    }
#endif /* NO_FILESYSTEM */

    BIO_free(bio);
    BIO_free(bio2);

    /* try with type BIO */
    XMEMCPY(msg, "\nhello wolfSSL\n security plus\t---...**adf\na...b.c",
            sizeof(msg));
    AssertNotNull(bio = BIO_new(BIO_s_bio()));
    AssertNotNull(bio2 = BIO_new(BIO_s_bio()));

    AssertIntEQ(BIO_set_write_buf_size(bio, 10),           SSL_SUCCESS);
    AssertIntEQ(BIO_set_write_buf_size(bio2, sizeof(msg)), SSL_SUCCESS);
    AssertIntEQ(BIO_make_bio_pair(bio, bio2),              SSL_SUCCESS);

    AssertIntEQ(BIO_write(bio2, msg, sizeof(msg)), sizeof(msg));
    AssertIntEQ(BIO_gets(bio, buffer, -3), 0);
    AssertIntEQ(BIO_gets(bio, buffer, bufferSz), 1);
    AssertIntEQ(BIO_gets(bio, buffer, bufferSz), 14);
    AssertStrEQ(buffer, "hello wolfSSL\n");
    AssertIntEQ(BIO_gets(bio, buffer, bufferSz), 19);
    AssertIntEQ(BIO_gets(bio, buffer, bufferSz), 8);
    AssertIntEQ(BIO_gets(bio, buffer, -1), 0);

    BIO_free(bio);
    BIO_free(bio2);

    printf(resultFmt, passed);
    #endif
}


static void test_wolfSSL_BIO_write(void)
{
    #if defined(OPENSSL_EXTRA) && defined(WOLFSSL_BASE64_ENCODE)
    BIO* bio;
    BIO* bio64;
    BIO* ptr;
    int  sz;
    char msg[] = "conversion test";
    char out[40];
    char expected[] = "Y29udmVyc2lvbiB0ZXN0AA==\n";

    printf(testingFmt, "wolfSSL_BIO_write()");

    AssertNotNull(bio64 = BIO_new(BIO_f_base64()));
    AssertNotNull(bio   = BIO_push(bio64, BIO_new(BIO_s_mem())));

    /* now should convert to base64 then write to memory */
    AssertIntEQ(BIO_write(bio, msg, sizeof(msg)), 25);
    BIO_flush(bio);
    AssertNotNull(ptr = BIO_find_type(bio, BIO_TYPE_MEM));
    sz = sizeof(out);
    XMEMSET(out, 0, sz);
    AssertIntEQ((sz = BIO_read(ptr, out, sz)), 25);
    AssertIntEQ(XMEMCMP(out, expected, sz), 0);

    /* write then read should return the same message */
    AssertIntEQ(BIO_write(bio, msg, sizeof(msg)), 25);
    sz = sizeof(out);
    XMEMSET(out, 0, sz);
    AssertIntEQ(BIO_read(bio, out, sz), 16);
    AssertIntEQ(XMEMCMP(out, msg, sizeof(msg)), 0);

    /* now try encoding with no line ending */
    BIO_set_flags(bio64, BIO_FLAG_BASE64_NO_NL);
    AssertIntEQ(BIO_write(bio, msg, sizeof(msg)), 24);
    BIO_flush(bio);
    sz = sizeof(out);
    XMEMSET(out, 0, sz);
    AssertIntEQ((sz = BIO_read(ptr, out, sz)), 24);
    AssertIntEQ(XMEMCMP(out, expected, sz), 0);

    BIO_free_all(bio); /* frees bio64 also */

    /* test with more than one bio64 in list */
    AssertNotNull(bio64 = BIO_new(BIO_f_base64()));
    AssertNotNull(bio   = BIO_push(BIO_new(BIO_f_base64()), bio64));
    AssertNotNull(BIO_push(bio64, BIO_new(BIO_s_mem())));

    /* now should convert to base64(x2) when stored and then decode with read */
    AssertIntEQ(BIO_write(bio, msg, sizeof(msg)), 37);
    BIO_flush(bio);
    sz = sizeof(out);
    XMEMSET(out, 0, sz);
    AssertIntEQ((sz = BIO_read(bio, out, sz)), 16);
    AssertIntEQ(XMEMCMP(out, msg, sz), 0);
    BIO_free_all(bio); /* frees bio64s also */

    printf(resultFmt, passed);
    #endif
}

static void test_wolfSSL_SESSION(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && \
    !defined(NO_RSA) && defined(HAVE_EXT_CACHE) && \
    defined(HAVE_IO_TESTS_DEPENDENCIES)

    WOLFSSL*     ssl;
    WOLFSSL_CTX* ctx;
    WOLFSSL_SESSION* sess;
    const unsigned char context[] = "user app context";
    unsigned char* sessDer = NULL;
    unsigned char* ptr     = NULL;
    unsigned int contextSz = (unsigned int)sizeof(context);
    int ret, err, sockfd, sz;
    tcp_ready ready;
    func_args server_args;
    THREAD_TYPE serverThread;

    printf(testingFmt, "wolfSSL_SESSION()");
    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_client_method()));

    AssertTrue(wolfSSL_CTX_use_certificate_file(ctx, cliCertFile, SSL_FILETYPE_PEM));
    AssertTrue(wolfSSL_CTX_use_PrivateKey_file(ctx, cliKeyFile, SSL_FILETYPE_PEM));
    AssertIntEQ(wolfSSL_CTX_load_verify_locations(ctx, caCertFile, 0), SSL_SUCCESS);
#ifdef WOLFSSL_ENCRYPTED_KEYS
    wolfSSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif

    XMEMSET(&server_args, 0, sizeof(func_args));
#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    StartTCP();
    InitTcpReady(&ready);

#if defined(USE_WINDOWS_API)
    /* use RNG to get random port if using windows */
    ready.port = GetRandomPort();
#endif

    server_args.signal = &ready;
    start_thread(test_server_nofail, &server_args, &serverThread);
    wait_tcp_ready(&server_args);

    /* client connection */
    ssl = wolfSSL_new(ctx);
    tcp_connect(&sockfd, wolfSSLIP, ready.port, 0, 0, ssl);
    AssertIntEQ(wolfSSL_set_fd(ssl, sockfd), SSL_SUCCESS);

    err = 0; /* Reset error */
    do {
#ifdef WOLFSSL_ASYNC_CRYPT
        if (err == WC_PENDING_E) {
            ret = wolfSSL_AsyncPoll(ssl, WOLF_POLL_FLAG_CHECK_HW);
            if (ret < 0) { break; } else if (ret == 0) { continue; }
        }
#endif

        ret = wolfSSL_connect(ssl);
        if (ret != SSL_SUCCESS) {
            err = wolfSSL_get_error(ssl, 0);
        }
    } while (ret != SSL_SUCCESS && err == WC_PENDING_E);
    AssertIntEQ(ret, SSL_SUCCESS);
    sess = wolfSSL_get_session(ssl);
    wolfSSL_shutdown(ssl);
    wolfSSL_free(ssl);

    join_thread(serverThread);

    FreeTcpReady(&ready);

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    /* get session from DER and update the timeout */
    AssertIntEQ(wolfSSL_i2d_SSL_SESSION(NULL, &sessDer), BAD_FUNC_ARG);
    AssertIntGT((sz = wolfSSL_i2d_SSL_SESSION(sess, &sessDer)), 0);
    wolfSSL_SESSION_free(sess);
    ptr = sessDer;
    AssertNull(sess = wolfSSL_d2i_SSL_SESSION(NULL, NULL, sz));
    AssertNotNull(sess = wolfSSL_d2i_SSL_SESSION(NULL,
                (const unsigned char**)&ptr, sz));
    XFREE(sessDer, NULL, DYNAMIC_TYPE_OPENSSL);
    AssertIntGT(wolfSSL_SESSION_get_time(sess), 0);
    AssertIntEQ(wolfSSL_SSL_SESSION_set_timeout(sess, 500), SSL_SUCCESS);

    /* successful set session test */
    AssertNotNull(ssl = wolfSSL_new(ctx));
    AssertIntEQ(wolfSSL_set_session(ssl, sess), SSL_SUCCESS);

    /* fail case with miss match session context IDs (use compatibility API) */
    AssertIntEQ(SSL_set_session_id_context(ssl, context, contextSz),
            SSL_SUCCESS);
    AssertIntEQ(wolfSSL_set_session(ssl, sess), SSL_FAILURE);
    wolfSSL_free(ssl);
    AssertIntEQ(SSL_CTX_set_session_id_context(NULL, context, contextSz),
            SSL_FAILURE);
    AssertIntEQ(SSL_CTX_set_session_id_context(ctx, context, contextSz),
            SSL_SUCCESS);
    AssertNotNull(ssl = wolfSSL_new(ctx));
    AssertIntEQ(wolfSSL_set_session(ssl, sess), SSL_FAILURE);
    wolfSSL_free(ssl);

    SSL_SESSION_free(sess);
    wolfSSL_CTX_free(ctx);
    printf(resultFmt, passed);
#endif
}


static void test_wolfSSL_d2i_PUBKEY(void)
{
    #if defined(OPENSSL_EXTRA)
    BIO*  bio;
    EVP_PKEY* pkey;

    printf(testingFmt, "wolfSSL_d2i_PUBKEY()");

    AssertNotNull(bio = BIO_new(BIO_s_mem()));
    AssertNull(d2i_PUBKEY_bio(NULL, NULL));

#if defined(USE_CERT_BUFFERS_2048) && !defined(NO_RSA)
    /* RSA PUBKEY test */
    AssertIntGT(BIO_write(bio, client_keypub_der_2048,
                sizeof_client_keypub_der_2048), 0);
    AssertNotNull(pkey = d2i_PUBKEY_bio(bio, NULL));
    EVP_PKEY_free(pkey);
#endif

#if defined(USE_CERT_BUFFERS_256) && defined(HAVE_ECC)
    /* ECC PUBKEY test */
    AssertIntGT(BIO_write(bio, ecc_clikeypub_der_256,
                sizeof_ecc_clikeypub_der_256), 0);
    AssertNotNull(pkey = d2i_PUBKEY_bio(bio, NULL));
    EVP_PKEY_free(pkey);
#endif

    BIO_free(bio);

    (void)pkey;
    printf(resultFmt, passed);
    #endif
}

static void test_wolfSSL_sk_GENERAL_NAME(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && \
    !defined(NO_RSA)
    X509* x509;
    unsigned char buf[4096];
    const unsigned char* bufPt;
    int bytes;
    XFILE f;
    STACK_OF(GENERAL_NAME)* sk;

    printf(testingFmt, "wolfSSL_sk_GENERAL_NAME()");

    AssertNotNull(f = XFOPEN(cliCertDerFile, "rb"));
    AssertIntGT((bytes = (int)XFREAD(buf, 1, sizeof(buf), f)), 0);
    XFCLOSE(f);

    bufPt = buf;
    AssertNotNull(x509 = d2i_X509(NULL, &bufPt, bytes));

    /* current cert has no alt names */
    AssertNull(sk = (WOLF_STACK_OF(ASN1_OBJECT)*)X509_get_ext_d2i(x509,
                NID_subject_alt_name, NULL, NULL));

    AssertIntEQ(sk_GENERAL_NAME_num(sk), -1);
#if 0
    for (i = 0; i < sk_GENERAL_NAME_num(sk); i++) {
        GENERAL_NAME* gn = sk_GENERAL_NAME_value(sk, i);
        if (gn == NULL) {
            printf("massive falure\n");
            return -1;
        }

        if (gn->type == GEN_DNS) {
            printf("found type GEN_DNS\n");
            printf("length = %d\n", gn->d.ia5->length);
            printf("data = %s\n", (char*)gn->d.ia5->data);
        }

        if (gn->type == GEN_EMAIL) {
            printf("found type GEN_EMAIL\n");
            printf("length = %d\n", gn->d.ia5->length);
            printf("data = %s\n", (char*)gn->d.ia5->data);
        }

        if (gn->type == GEN_URI) {
            printf("found type GEN_URI\n");
            printf("length = %d\n", gn->d.ia5->length);
            printf("data = %s\n", (char*)gn->d.ia5->data);
        }
    }
#endif
    X509_free(x509);
    sk_GENERAL_NAME_pop_free(sk, GENERAL_NAME_free);

    printf(resultFmt, passed);
#endif
}

static void test_wolfSSL_MD4(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_MD4)
    MD4_CTX md4;
    unsigned char out[16]; /* MD4_DIGEST_SIZE */
    const char* msg  = "12345678901234567890123456789012345678901234567890123456"
                       "789012345678901234567890";
    const char* test = "\xe3\x3b\x4d\xdc\x9c\x38\xf2\x19\x9c\x3e\x7b\x16\x4f"
                       "\xcc\x05\x36";
    int msgSz        = (int)XSTRLEN(msg);

    printf(testingFmt, "wolfSSL_MD4()");

    XMEMSET(out, 0, sizeof(out));
    MD4_Init(&md4);
    MD4_Update(&md4, (const void*)msg, (unsigned long)msgSz);
    MD4_Final(out, &md4);
    AssertIntEQ(XMEMCMP(out, test, sizeof(out)), 0);

    printf(resultFmt, passed);
#endif
}


static void test_wolfSSL_RSA(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_RSA) && defined(WOLFSSL_KEY_GEN)
    RSA* rsa;

    printf(testingFmt, "wolfSSL_RSA()");

    AssertNotNull(rsa = RSA_generate_key(2048, 3, NULL, NULL));
    AssertIntEQ(RSA_size(rsa), 256);
    RSA_free(rsa);

    AssertNotNull(rsa = RSA_generate_key(3072, 17, NULL, NULL));
    AssertIntEQ(RSA_size(rsa), 384);
    RSA_free(rsa);

    /* remove for now with odd key size until adjusting rsa key size check with
       wc_MakeRsaKey()
    AssertNotNull(rsa = RSA_generate_key(2999, 65537, NULL, NULL));
    RSA_free(rsa);
    */

    AssertNull(RSA_generate_key(-1, 3, NULL, NULL));
    AssertNull(RSA_generate_key(511, 3, NULL, NULL)); /* RSA_MIN_SIZE - 1 */
    AssertNull(RSA_generate_key(4097, 3, NULL, NULL)); /* RSA_MAX_SIZE + 1 */
    AssertNull(RSA_generate_key(2048, 0, NULL, NULL));

    printf(resultFmt, passed);
#endif
}

static void test_wolfSSL_RSA_DER(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_RSA) && !defined(HAVE_FAST_RSA)

    RSA *rsa;
    int i;

    struct
    {
        const unsigned char *der;
        int sz;
    } tbl[] = {
#ifdef USE_CERT_BUFFERS_1024
        {client_key_der_1024, sizeof_client_key_der_1024},
        {server_key_der_1024, sizeof_server_key_der_1024},
#endif
#ifdef USE_CERT_BUFFERS_2048
        {client_key_der_2048, sizeof_client_key_der_2048},
        {server_key_der_2048, sizeof_server_key_der_2048},
#endif
        {NULL, 0}
    };

    printf(testingFmt, "test_wolfSSL_RSA_DER()");

    for (i = 0; tbl[i].der != NULL; i++)
    {
        AssertNotNull(d2i_RSAPublicKey(&rsa, &tbl[i].der, tbl[i].sz));
        AssertNotNull(rsa);
        RSA_free(rsa);
    }
    printf(resultFmt, passed);

#endif
}

static void test_wolfSSL_verify_depth(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_RSA)
    WOLFSSL*     ssl;
    WOLFSSL_CTX* ctx;
    long         depth;

    printf(testingFmt, "test_wolfSSL_verify_depth()");
    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_client_method()));

    AssertTrue(wolfSSL_CTX_use_certificate_file(ctx, cliCertFile, SSL_FILETYPE_PEM));
    AssertTrue(wolfSSL_CTX_use_PrivateKey_file(ctx, cliKeyFile, SSL_FILETYPE_PEM));
    AssertIntEQ(wolfSSL_CTX_load_verify_locations(ctx, caCertFile, 0), SSL_SUCCESS);

    AssertIntGT((depth = SSL_CTX_get_verify_depth(ctx)), 0);
    AssertNotNull(ssl = SSL_new(ctx));
    AssertIntEQ(SSL_get_verify_depth(ssl), SSL_CTX_get_verify_depth(ctx));
    SSL_free(ssl);

    SSL_CTX_set_verify_depth(ctx, -1);
    AssertIntEQ(depth, SSL_CTX_get_verify_depth(ctx));

    SSL_CTX_set_verify_depth(ctx, 2);
    AssertIntEQ(2, SSL_CTX_get_verify_depth(ctx));
    AssertNotNull(ssl = SSL_new(ctx));
    AssertIntEQ(2, SSL_get_verify_depth(ssl));

    SSL_free(ssl);
    SSL_CTX_free(ctx);
    printf(resultFmt, passed);
#endif
}

#if defined(OPENSSL_EXTRA) && !defined(NO_HMAC)
/* helper function for test_wolfSSL_HMAC_CTX, digest size is expected to be a
 * buffer of 64 bytes.
 *
 * returns the size of the digest buffer on success and a negative value on
 * failure.
 */
static int test_HMAC_CTX_helper(const EVP_MD* type, unsigned char* digest)
{
    HMAC_CTX ctx1;
    HMAC_CTX ctx2;

    unsigned char key[] = "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"
                          "\x0b\x0b\x0b\x0b\x0b\x0b\x0b";
    unsigned char long_key[] =
        "0123456789012345678901234567890123456789"
        "0123456789012345678901234567890123456789"
        "0123456789012345678901234567890123456789"
        "0123456789012345678901234567890123456789";

    unsigned char msg[] = "message to hash";
    unsigned int  digestSz = 64;
    int keySz = sizeof(key);
    int long_keySz = sizeof(long_key);
    int msgSz = sizeof(msg);

    unsigned char digest2[64];
    unsigned int digestSz2 = 64;

    HMAC_CTX_init(&ctx1);

    AssertIntEQ(HMAC_Init(&ctx1, (const void*)key, keySz, type), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_CTX_copy(&ctx2, &ctx1), SSL_SUCCESS);

    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Final(&ctx1, digest, &digestSz), SSL_SUCCESS);
    HMAC_CTX_cleanup(&ctx1);

    AssertIntEQ(HMAC_Update(&ctx2, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Final(&ctx2, digest2, &digestSz2), SSL_SUCCESS);
    HMAC_CTX_cleanup(&ctx2);

    AssertIntEQ(digestSz, digestSz2);
    AssertIntEQ(XMEMCMP(digest, digest2, digestSz), 0);

    /* test HMAC_Init with NULL key */

    /* init after copy */
    printf("test HMAC_Init with NULL key (0)\n");
    HMAC_CTX_init(&ctx1);
    AssertIntEQ(HMAC_Init(&ctx1, (const void*)key, keySz, type), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_CTX_copy(&ctx2, &ctx1), SSL_SUCCESS);

    AssertIntEQ(HMAC_Init(&ctx1, NULL, 0, NULL), SSL_SUCCESS);

    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Final(&ctx1, digest, &digestSz), SSL_SUCCESS);

    AssertIntEQ(HMAC_Init(&ctx2, NULL, 0, NULL), SSL_SUCCESS);

    AssertIntEQ(HMAC_Update(&ctx2, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&ctx2, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Final(&ctx2, digest2, &digestSz), SSL_SUCCESS);

    HMAC_CTX_cleanup(&ctx2);
    AssertIntEQ(digestSz, digestSz2);
    AssertIntEQ(XMEMCMP(digest, digest2, digestSz), 0);

    /* long key */
    printf("test HMAC_Init with NULL key (1)\n");
    HMAC_CTX_init(&ctx1);
    AssertIntEQ(HMAC_Init(&ctx1, (const void*)long_key, long_keySz, type), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_CTX_copy(&ctx2, &ctx1), SSL_SUCCESS);

    AssertIntEQ(HMAC_Init(&ctx1, NULL, 0, NULL), SSL_SUCCESS);

    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Final(&ctx1, digest, &digestSz), SSL_SUCCESS);

    AssertIntEQ(HMAC_Init(&ctx2, NULL, 0, NULL), SSL_SUCCESS);

    AssertIntEQ(HMAC_Update(&ctx2, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&ctx2, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Final(&ctx2, digest2, &digestSz), SSL_SUCCESS);

    HMAC_CTX_cleanup(&ctx2);
    AssertIntEQ(digestSz, digestSz2);
    AssertIntEQ(XMEMCMP(digest, digest2, digestSz), 0);

    /* init before copy */
    printf("test HMAC_Init with NULL key (2)\n");
    HMAC_CTX_init(&ctx1);
    AssertIntEQ(HMAC_Init(&ctx1, (const void*)key, keySz, type), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Init(&ctx1, NULL, 0, NULL), SSL_SUCCESS);
    AssertIntEQ(HMAC_CTX_copy(&ctx2, &ctx1), SSL_SUCCESS);

    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&ctx1, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Final(&ctx1, digest, &digestSz), SSL_SUCCESS);

    AssertIntEQ(HMAC_Update(&ctx2, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Update(&ctx2, msg, msgSz), SSL_SUCCESS);
    AssertIntEQ(HMAC_Final(&ctx2, digest2, &digestSz), SSL_SUCCESS);

    HMAC_CTX_cleanup(&ctx2);
    AssertIntEQ(digestSz, digestSz2);
    AssertIntEQ(XMEMCMP(digest, digest2, digestSz), 0);

    return digestSz;
}
#endif /* defined(OPENSSL_EXTRA) && !defined(NO_HMAC) */

static void test_wolfSSL_HMAC_CTX(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_HMAC)
    unsigned char digest[64];
    int digestSz;

    printf(testingFmt, "wolfSSL_HMAC_CTX()");

    #ifndef NO_SHA
    AssertIntEQ((digestSz = test_HMAC_CTX_helper(EVP_sha1(), digest)), 20);
    AssertIntEQ(XMEMCMP("\xD9\x68\x77\x23\x70\xFB\x53\x70\x53\xBA\x0E\xDC\xDA"
                          "\xBF\x03\x98\x31\x19\xB2\xCC", digest, digestSz), 0);
    #endif /* !NO_SHA */
    #ifdef WOLFSSL_SHA224
    AssertIntEQ((digestSz = test_HMAC_CTX_helper(EVP_sha224(), digest)), 28);
    AssertIntEQ(XMEMCMP("\x57\xFD\xF4\xE1\x2D\xB0\x79\xD7\x4B\x25\x7E\xB1\x95"
                          "\x9C\x11\xAC\x2D\x1E\x78\x94\x4F\x3A\x0F\xED\xF8\xAD"
                          "\x02\x0E", digest, digestSz), 0);

    #endif /* WOLFSSL_SHA224 */
    #ifndef NO_SHA256
    AssertIntEQ((digestSz = test_HMAC_CTX_helper(EVP_sha256(), digest)), 32);
    AssertIntEQ(XMEMCMP("\x13\xAB\x76\x91\x0C\x37\x86\x8D\xB3\x7E\x30\x0C\xFC"
                          "\xB0\x2E\x8E\x4A\xD7\xD4\x25\xCC\x3A\xA9\x0F\xA2\xF2"
                          "\x47\x1E\x62\x6F\x5D\xF2", digest, digestSz), 0);

    #endif /* !NO_SHA256 */

    #ifdef WOLFSSL_SHA512
    #ifdef WOLFSSL_SHA384
    AssertIntEQ((digestSz = test_HMAC_CTX_helper(EVP_sha384(), digest)), 48);
    AssertIntEQ(XMEMCMP("\x9E\xCB\x07\x0C\x11\x76\x3F\x23\xC3\x25\x0E\xC4\xB7"
                          "\x28\x77\x95\x99\xD5\x9D\x7A\xBB\x1A\x9F\xB7\xFD\x25"
                          "\xC9\x72\x47\x9F\x8F\x86\x76\xD6\x20\x57\x87\xB7\xE7"
                          "\xCD\xFB\xC2\xCC\x9F\x2B\xC5\x41\xAB",
                          digest, digestSz), 0);
    #endif /* WOLFSSL_SHA384 */
    AssertIntEQ((digestSz = test_HMAC_CTX_helper(EVP_sha512(), digest)), 64);
    AssertIntEQ(XMEMCMP("\xD4\x21\x0C\x8B\x60\x6F\xF4\xBF\x07\x2F\x26\xCC\xAD"
                          "\xBC\x06\x0B\x34\x78\x8B\x4F\xD6\xC0\x42\xF1\x33\x10"
                          "\x6C\x4F\x1E\x55\x59\xDD\x2A\x9F\x15\x88\x62\xF8\x60"
                          "\xA3\x99\x91\xE2\x08\x7B\xF7\x95\x3A\xB0\x92\x48\x60"
                          "\x88\x8B\x5B\xB8\x5F\xE9\xB6\xB1\x96\xE3\xB5\xF0",
                          digest, digestSz), 0);
    #endif /* WOLFSSL_SHA512 */

    #ifndef NO_MD5
    AssertIntEQ((digestSz = test_HMAC_CTX_helper(EVP_md5(), digest)), 16);
    AssertIntEQ(XMEMCMP("\xB7\x27\xC4\x41\xE5\x2E\x62\xBA\x54\xED\x72\x70\x9F"
                          "\xE4\x98\xDD", digest, digestSz), 0);
    #endif /* !NO_MD5 */

    printf(resultFmt, passed);
#endif
}

#if defined(OPENSSL_EXTRA) && !defined(NO_RSA)
static void sslMsgCb(int w, int version, int type, const void* buf,
        size_t sz, SSL* ssl, void* arg)
{
    int i;
    unsigned char* pt = (unsigned char*)buf;

    printf("%s %d bytes of version %d , type %d : ", (w)?"Writing":"Reading",
            (int)sz, version, type);
    for (i = 0; i < (int)sz; i++) printf("%02X", pt[i]);
    printf("\n");
    (void)ssl;
    (void)arg;
}
#endif /* OPENSSL_EXTRA */

static void test_wolfSSL_msg_callback(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_RSA)
    WOLFSSL*     ssl;
    WOLFSSL_CTX* ctx;

    printf(testingFmt, "wolfSSL_msg_callback()");
    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_client_method()));

    AssertTrue(wolfSSL_CTX_use_certificate_file(ctx, cliCertFile,
                SSL_FILETYPE_PEM));
    AssertTrue(wolfSSL_CTX_use_PrivateKey_file(ctx, cliKeyFile,
                SSL_FILETYPE_PEM));
    AssertIntEQ(wolfSSL_CTX_load_verify_locations(ctx, caCertFile, 0),
                SSL_SUCCESS);

    AssertNotNull(ssl = SSL_new(ctx));
    AssertIntEQ(SSL_set_msg_callback(ssl, NULL), SSL_SUCCESS);
    AssertIntEQ(SSL_set_msg_callback(ssl, &sslMsgCb), SSL_SUCCESS);
    AssertIntEQ(SSL_set_msg_callback(NULL, &sslMsgCb), SSL_FAILURE);

    SSL_CTX_free(ctx);
    SSL_free(ssl);

    printf(resultFmt, passed);
#endif
}

static void test_wolfSSL_SHA(void)
{
#if defined(OPENSSL_EXTRA)
    printf(testingFmt, "wolfSSL_SHA()");

    #if !defined(NO_SHA)
    {
        const unsigned char in[] = "abc";
        unsigned char expected[] = "\xA9\x99\x3E\x36\x47\x06\x81\x6A\xBA\x3E"
                                    "\x25\x71\x78\x50\xC2\x6C\x9C\xD0\xD8\x9D";
        unsigned char out[WC_SHA_DIGEST_SIZE];

        XMEMSET(out, 0, WC_SHA_DIGEST_SIZE);
        AssertNotNull(SHA1(in, XSTRLEN((char*)in), out));
        AssertIntEQ(XMEMCMP(out, expected, WC_SHA_DIGEST_SIZE), 0);
    }
    #endif

    #if !defined(NO_SHA256)
    {
        const unsigned char in[] = "abc";
        unsigned char expected[] = "\xBA\x78\x16\xBF\x8F\x01\xCF\xEA\x41\x41\x40\xDE\x5D\xAE\x22"
            "\x23\xB0\x03\x61\xA3\x96\x17\x7A\x9C\xB4\x10\xFF\x61\xF2\x00"
            "\x15\xAD";
        unsigned char out[WC_SHA256_DIGEST_SIZE];
     
        XMEMSET(out, 0, WC_SHA256_DIGEST_SIZE);
        AssertNotNull(SHA256(in, XSTRLEN((char*)in), out));
        AssertIntEQ(XMEMCMP(out, expected, WC_SHA256_DIGEST_SIZE), 0);
    }
    #endif

    #if defined(WOLFSSL_SHA384) && defined(WOLFSSL_SHA512) 
    {
        const unsigned char in[] = "abc";
        unsigned char expected[] = "\xcb\x00\x75\x3f\x45\xa3\x5e\x8b\xb5\xa0\x3d\x69\x9a\xc6\x50"
            "\x07\x27\x2c\x32\xab\x0e\xde\xd1\x63\x1a\x8b\x60\x5a\x43\xff"
            "\x5b\xed\x80\x86\x07\x2b\xa1\xe7\xcc\x23\x58\xba\xec\xa1\x34"
            "\xc8\x25\xa7";
        unsigned char out[WC_SHA384_DIGEST_SIZE];

        XMEMSET(out, 0, WC_SHA384_DIGEST_SIZE);
        AssertNotNull(SHA384(in, XSTRLEN((char*)in), out));
        AssertIntEQ(XMEMCMP(out, expected, WC_SHA384_DIGEST_SIZE), 0);
    }
    #endif

    #if defined(WOLFSSL_SHA512)
    {
        const unsigned char in[] = "abc";
        unsigned char expected[] = "\xdd\xaf\x35\xa1\x93\x61\x7a\xba\xcc\x41\x73\x49\xae\x20\x41"
           "\x31\x12\xe6\xfa\x4e\x89\xa9\x7e\xa2\x0a\x9e\xee\xe6\x4b\x55"
            "\xd3\x9a\x21\x92\x99\x2a\x27\x4f\xc1\xa8\x36\xba\x3c\x23\xa3"
            "\xfe\xeb\xbd\x45\x4d\x44\x23\x64\x3c\xe8\x0e\x2a\x9a\xc9\x4f"
            "\xa5\x4c\xa4\x9f";
        unsigned char out[WC_SHA512_DIGEST_SIZE];

        XMEMSET(out, 0, WC_SHA512_DIGEST_SIZE);
        AssertNotNull(SHA512(in, XSTRLEN((char*)in), out));
        AssertIntEQ(XMEMCMP(out, expected, WC_SHA512_DIGEST_SIZE), 0);
    }
    #endif

    printf(resultFmt, passed);
#endif
}

static void test_wolfSSL_DH_1536_prime(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_DH)
    BIGNUM* bn;
    unsigned char bits[200];
    int sz = 192; /* known binary size */
    const byte expected[] = {
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xC9,0x0F,0xDA,0xA2,0x21,0x68,0xC2,0x34,
        0xC4,0xC6,0x62,0x8B,0x80,0xDC,0x1C,0xD1,
        0x29,0x02,0x4E,0x08,0x8A,0x67,0xCC,0x74,
        0x02,0x0B,0xBE,0xA6,0x3B,0x13,0x9B,0x22,
        0x51,0x4A,0x08,0x79,0x8E,0x34,0x04,0xDD,
        0xEF,0x95,0x19,0xB3,0xCD,0x3A,0x43,0x1B,
        0x30,0x2B,0x0A,0x6D,0xF2,0x5F,0x14,0x37,
        0x4F,0xE1,0x35,0x6D,0x6D,0x51,0xC2,0x45,
        0xE4,0x85,0xB5,0x76,0x62,0x5E,0x7E,0xC6,
        0xF4,0x4C,0x42,0xE9,0xA6,0x37,0xED,0x6B,
        0x0B,0xFF,0x5C,0xB6,0xF4,0x06,0xB7,0xED,
        0xEE,0x38,0x6B,0xFB,0x5A,0x89,0x9F,0xA5,
        0xAE,0x9F,0x24,0x11,0x7C,0x4B,0x1F,0xE6,
        0x49,0x28,0x66,0x51,0xEC,0xE4,0x5B,0x3D,
        0xC2,0x00,0x7C,0xB8,0xA1,0x63,0xBF,0x05,
        0x98,0xDA,0x48,0x36,0x1C,0x55,0xD3,0x9A,
        0x69,0x16,0x3F,0xA8,0xFD,0x24,0xCF,0x5F,
        0x83,0x65,0x5D,0x23,0xDC,0xA3,0xAD,0x96,
        0x1C,0x62,0xF3,0x56,0x20,0x85,0x52,0xBB,
        0x9E,0xD5,0x29,0x07,0x70,0x96,0x96,0x6D,
        0x67,0x0C,0x35,0x4E,0x4A,0xBC,0x98,0x04,
        0xF1,0x74,0x6C,0x08,0xCA,0x23,0x73,0x27,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    };

    printf(testingFmt, "wolfSSL_DH_1536_prime()");

    AssertNotNull(bn = get_rfc3526_prime_1536(NULL));
    AssertIntEQ(sz, BN_bn2bin((const BIGNUM*)bn, bits));
    AssertIntEQ(0, XMEMCMP(expected, bits, sz));

    BN_free(bn);
    printf(resultFmt, passed);
#endif
}

static void test_wolfSSL_AES_ecb_encrypt(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_AES) && defined(HAVE_AES_ECB)
    AES_KEY aes;
    const byte msg[] =
    {
      0x6b,0xc1,0xbe,0xe2,0x2e,0x40,0x9f,0x96,
      0xe9,0x3d,0x7e,0x11,0x73,0x93,0x17,0x2a
    };

    const byte verify[] =
    {
        0xf3,0xee,0xd1,0xbd,0xb5,0xd2,0xa0,0x3c,
        0x06,0x4b,0x5a,0x7e,0x3d,0xb1,0x81,0xf8
    };

    const byte key[] =
    {
      0x60,0x3d,0xeb,0x10,0x15,0xca,0x71,0xbe,
      0x2b,0x73,0xae,0xf0,0x85,0x7d,0x77,0x81,
      0x1f,0x35,0x2c,0x07,0x3b,0x61,0x08,0xd7,
      0x2d,0x98,0x10,0xa3,0x09,0x14,0xdf,0xf4
    };


    byte out[AES_BLOCK_SIZE];

    printf(testingFmt, "wolfSSL_AES_ecb_encrypt()");

    AssertIntEQ(AES_set_encrypt_key(key, sizeof(key)*8, &aes), 0);
    XMEMSET(out, 0, AES_BLOCK_SIZE);
    AES_ecb_encrypt(msg, out, &aes, AES_ENCRYPT);
    AssertIntEQ(XMEMCMP(out, verify, AES_BLOCK_SIZE), 0);

#ifdef HAVE_AES_DECRYPT
    AssertIntEQ(AES_set_decrypt_key(key, sizeof(key)*8, &aes), 0);
    XMEMSET(out, 0, AES_BLOCK_SIZE);
    AES_ecb_encrypt(verify, out, &aes, AES_DECRYPT);
    AssertIntEQ(XMEMCMP(out, msg, AES_BLOCK_SIZE), 0);
#endif

    /* test bad arguments */
    AES_ecb_encrypt(NULL, out, &aes, AES_DECRYPT);
    AES_ecb_encrypt(verify, NULL, &aes, AES_DECRYPT);
    AES_ecb_encrypt(verify, out, NULL, AES_DECRYPT);

    printf(resultFmt, passed);
#endif
}

static void test_wolfSSL_SHA256(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_SHA256) && \
    defined(NO_OLD_SHA_NAMES) && !defined(HAVE_FIPS)
    unsigned char input[] =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    unsigned char output[] =
        "\x24\x8D\x6A\x61\xD2\x06\x38\xB8\xE5\xC0\x26\x93\x0C\x3E\x60"
        "\x39\xA3\x3C\xE4\x59\x64\xFF\x21\x67\xF6\xEC\xED\xD4\x19\xDB"
        "\x06\xC1";
    size_t inLen;
    byte hash[WC_SHA256_DIGEST_SIZE];

    printf(testingFmt, "wolfSSL_SHA256()");
    inLen  = XSTRLEN((char*)input);

    XMEMSET(hash, 0, WC_SHA256_DIGEST_SIZE);
    AssertNotNull(SHA256(input, inLen, hash));
    AssertIntEQ(XMEMCMP(hash, output, WC_SHA256_DIGEST_SIZE), 0);

    printf(resultFmt, passed);
#endif
}

static void test_wolfSSL_X509_get_serialNumber(void)
{
#if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
    !defined(NO_RSA)
    ASN1_INTEGER* a;
    BIGNUM* bn;
    X509*   x509;


    printf(testingFmt, "wolfSSL_X509_get_serialNumber()");

    AssertNotNull(x509 = wolfSSL_X509_load_certificate_file(svrCertFile,
                                                      SSL_FILETYPE_PEM));
    AssertNotNull(a = X509_get_serialNumber(x509));
    X509_free(x509);

    /* check on value of ASN1 Integer */
    AssertNotNull(bn = ASN1_INTEGER_to_BN(a, NULL));
    AssertIntEQ(BN_get_word(bn), 1);

    BN_free(bn);
    ASN1_INTEGER_free(a);

    /* hard test free'ing with dynamic buffer to make sure there is no leaks */
    a = ASN1_INTEGER_new();
    AssertNotNull(a->data = (unsigned char*)XMALLOC(100, NULL,
                DYNAMIC_TYPE_OPENSSL));
    a->isDynamic = 1;
    ASN1_INTEGER_free(a);

    printf(resultFmt, passed);
#endif
}


static void test_wolfSSL_OPENSSL_add_all_algorithms(void){
#if defined(OPENSSL_EXTRA)
    printf(testingFmt, "wolfSSL_OPENSSL_add_all_algorithms()");

    AssertIntEQ(wolfSSL_OPENSSL_add_all_algorithms_noconf(),WOLFSSL_SUCCESS);
    wolfSSL_Cleanup();

    printf(resultFmt, passed);
#endif
}

static void test_wolfSSL_ASN1_STRING_print_ex(void){
#if defined(OPENSSL_EXTRA) && !defined(NO_ASN)
    ASN1_STRING* asn_str = NULL;
    const char data[] = "Hello wolfSSL!";
    ASN1_STRING* esc_str = NULL;
    const char esc_data[] = "a+;<>";
    BIO *bio;
    unsigned long flags;
    int p_len;
    unsigned char rbuf[255];
    
    printf(testingFmt, "wolfSSL_ASN1_STRING_print_ex()");
    
    /* setup */
    XMEMSET(rbuf, 0, 255);
    bio = BIO_new(BIO_s_mem());
    BIO_set_write_buf_size(bio,255);

    asn_str = ASN1_STRING_type_new(V_ASN1_OCTET_STRING);
    ASN1_STRING_set(asn_str, (const void*)data, sizeof(data));
    esc_str = ASN1_STRING_type_new(V_ASN1_OCTET_STRING);
    ASN1_STRING_set(esc_str, (const void*)esc_data, sizeof(esc_data));

    /* no flags */
    XMEMSET(rbuf, 0, 255);
    flags = 0;
    p_len = wolfSSL_ASN1_STRING_print_ex(bio, asn_str, flags);
    AssertIntEQ(p_len, 15);
    BIO_read(bio, (void*)rbuf, 15);
    AssertStrEQ((char*)rbuf, "Hello wolfSSL!");

    /* RFC2253 Escape */
    XMEMSET(rbuf, 0, 255);
    flags = ASN1_STRFLGS_ESC_2253;
    p_len = wolfSSL_ASN1_STRING_print_ex(bio, esc_str, flags);
    AssertIntEQ(p_len, 9);
    BIO_read(bio, (void*)rbuf, 9);
    AssertStrEQ((char*)rbuf, "a\\+\\;\\<\\>");

    /* Show type */
    XMEMSET(rbuf, 0, 255);
    flags = ASN1_STRFLGS_SHOW_TYPE;
    p_len = wolfSSL_ASN1_STRING_print_ex(bio, asn_str, flags);
    AssertIntEQ(p_len, 28);
    BIO_read(bio, (void*)rbuf, 28);
    AssertStrEQ((char*)rbuf, "OCTET STRING:Hello wolfSSL!");

    /* Dump All */
    XMEMSET(rbuf, 0, 255);
    flags = ASN1_STRFLGS_DUMP_ALL;
    p_len = wolfSSL_ASN1_STRING_print_ex(bio, asn_str, flags);
    AssertIntEQ(p_len, 31);
    BIO_read(bio, (void*)rbuf, 31);
    AssertStrEQ((char*)rbuf, "#48656C6C6F20776F6C6653534C2100");

    /* Dump Der */
    XMEMSET(rbuf, 0, 255);
    flags = ASN1_STRFLGS_DUMP_ALL | ASN1_STRFLGS_DUMP_DER;
    p_len = wolfSSL_ASN1_STRING_print_ex(bio, asn_str, flags);
    AssertIntEQ(p_len, 35);
    BIO_read(bio, (void*)rbuf, 35);
    AssertStrEQ((char*)rbuf, "#040F48656C6C6F20776F6C6653534C2100");

    /* Dump All + Show type */
    XMEMSET(rbuf, 0, 255);
    flags = ASN1_STRFLGS_DUMP_ALL | ASN1_STRFLGS_SHOW_TYPE;
    p_len = wolfSSL_ASN1_STRING_print_ex(bio, asn_str, flags);
    AssertIntEQ(p_len, 44);
    BIO_read(bio, (void*)rbuf, 44);
    AssertStrEQ((char*)rbuf, "OCTET STRING:#48656C6C6F20776F6C6653534C2100");

    BIO_free(bio);
    ASN1_STRING_free(asn_str);
    ASN1_STRING_free(esc_str);

    printf(resultFmt, passed);
#endif
}

static void test_wolfSSL_ASN1_TIME_to_generalizedtime(void){
#if defined(OPENSSL_EXTRA) && !defined(NO_ASN1_TIME)
    WOLFSSL_ASN1_TIME *t;
    WOLFSSL_ASN1_TIME *out;
    WOLFSSL_ASN1_TIME *gtime;

    printf(testingFmt, "wolfSSL_ASN1_TIME_to_generalizedtime()");

    /* UTC Time test */
    AssertNotNull(t = (WOLFSSL_ASN1_TIME*)XMALLOC(sizeof(WOLFSSL_ASN1_TIME),
                NULL, DYNAMIC_TYPE_TMP_BUFFER));
    XMEMSET(t->data, 0, ASN_GENERALIZED_TIME_SIZE);
    AssertNotNull(out = (WOLFSSL_ASN1_TIME*)XMALLOC(sizeof(WOLFSSL_ASN1_TIME),
                NULL, DYNAMIC_TYPE_TMP_BUFFER));
    t->data[0] = ASN_UTC_TIME;
    t->data[1] = ASN_UTC_TIME_SIZE;
    XMEMCPY(t->data + 2,"050727123456Z",ASN_UTC_TIME_SIZE);

    AssertNotNull(gtime = wolfSSL_ASN1_TIME_to_generalizedtime(t, &out));
    AssertIntEQ(gtime->data[0], ASN_GENERALIZED_TIME);
    AssertIntEQ(gtime->data[1], ASN_GENERALIZED_TIME_SIZE);
    AssertStrEQ((char*)gtime->data + 2, "20050727123456Z");

    /* Generalized Time test */
    XMEMSET(t, 0, ASN_GENERALIZED_TIME_SIZE);
    XMEMSET(out, 0, ASN_GENERALIZED_TIME_SIZE);
    gtime = NULL;
    t->data[0] = ASN_GENERALIZED_TIME;
    t->data[1] = ASN_GENERALIZED_TIME_SIZE;
    XMEMCPY(t->data + 2,"20050727123456Z",ASN_GENERALIZED_TIME_SIZE);
    AssertNotNull(gtime = wolfSSL_ASN1_TIME_to_generalizedtime(t, &out));
    AssertIntEQ(gtime->data[0], ASN_GENERALIZED_TIME);
    AssertIntEQ(gtime->data[1], ASN_GENERALIZED_TIME_SIZE);
    AssertStrEQ((char*)gtime->data + 2, "20050727123456Z");
    XFREE(out, NULL, DYNAMIC_TYPE_TMP_BUFFER);

    /* Null parameter test */
    XMEMSET(t, 0, ASN_GENERALIZED_TIME_SIZE);
    gtime = NULL;
    out = NULL;
    t->data[0] = ASN_UTC_TIME;
    t->data[1] = ASN_UTC_TIME_SIZE;
    XMEMCPY(t->data + 2,"050727123456Z",ASN_UTC_TIME_SIZE);
    AssertNotNull(gtime = wolfSSL_ASN1_TIME_to_generalizedtime(t, NULL));
    AssertIntEQ(gtime->data[0], ASN_GENERALIZED_TIME);
    AssertIntEQ(gtime->data[1], ASN_GENERALIZED_TIME_SIZE);
    AssertStrEQ((char*)gtime->data + 2, "20050727123456Z");

    XFREE(gtime, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    XFREE(t, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    printf(resultFmt, passed);
#endif
}

static void test_wolfSSL_X509_check_ca(void){
#if defined(OPENSSL_EXTRA) && !defined(NO_RSA)
    WOLFSSL_X509 *x509;

    x509 = wolfSSL_X509_load_certificate_file(svrCertFile, WOLFSSL_FILETYPE_PEM);
    AssertIntEQ(wolfSSL_X509_check_ca(x509), 1);
    wolfSSL_X509_free(x509);

    x509 = wolfSSL_X509_load_certificate_file(ntruCertFile, WOLFSSL_FILETYPE_PEM);
    AssertIntEQ(wolfSSL_X509_check_ca(x509), 0);
    wolfSSL_X509_free(x509);
#endif
}

static void test_no_op_functions(void)
{
    #if defined(OPENSSL_EXTRA)
    printf(testingFmt, "no_op_functions()");

    /* this makes sure wolfSSL can compile and run these no-op functions */
    SSL_load_error_strings();
    ENGINE_load_builtin_engines();
    OpenSSL_add_all_ciphers();
    CRYPTO_malloc_init();

    printf(resultFmt, passed);
    #endif
}


/*----------------------------------------------------------------------------*
 | wolfCrypt ASN
 *----------------------------------------------------------------------------*/

static void test_wc_GetPkcs8TraditionalOffset(void)
{
#if !defined(NO_ASN) && !defined(NO_FILESYSTEM)
    int length, derSz;
    word32 inOutIdx;
    const char* path = "./certs/server-keyPkcs8.der";
    FILE* file;
    byte der[2048];

    printf(testingFmt, "wc_GetPkcs8TraditionalOffset");

    file = fopen(path, "rb");
    AssertNotNull(file);
    derSz = (int)fread(der, 1, sizeof(der), file);
    fclose(file);

    /* valid case */
    inOutIdx = 0;
    length = wc_GetPkcs8TraditionalOffset(der, &inOutIdx, derSz);
    AssertIntGT(length, 0);

    /* inOutIdx > sz */
    inOutIdx = 4000;
    length = wc_GetPkcs8TraditionalOffset(der, &inOutIdx, derSz);
    AssertIntEQ(length, BAD_FUNC_ARG);

    /* null input */
    inOutIdx = 0;
    length = wc_GetPkcs8TraditionalOffset(NULL, &inOutIdx, 0);
    AssertIntEQ(length, BAD_FUNC_ARG);

    /* invalid input, fill buffer with 1's */
    XMEMSET(der, 1, sizeof(der));
    inOutIdx = 0;
    length = wc_GetPkcs8TraditionalOffset(der, &inOutIdx, derSz);
    AssertIntEQ(length, ASN_PARSE_E);

    printf(resultFmt, passed);
#endif /* NO_ASN */
}


/*----------------------------------------------------------------------------*
 | wolfCrypt ECC
 *----------------------------------------------------------------------------*/

static void test_wc_ecc_get_curve_size_from_name(void)
{
#ifdef HAVE_ECC
    int ret;

    printf(testingFmt, "wc_ecc_get_curve_size_from_name");

    #if !defined(NO_ECC256) && !defined(NO_ECC_SECP)
        ret = wc_ecc_get_curve_size_from_name("SECP256R1");
        AssertIntEQ(ret, 32);
    #endif

    /* invalid case */
    ret = wc_ecc_get_curve_size_from_name("BADCURVE");
    AssertIntEQ(ret, -1);

    /* NULL input */
    ret = wc_ecc_get_curve_size_from_name(NULL);
    AssertIntEQ(ret, BAD_FUNC_ARG);

    printf(resultFmt, passed);
#endif /* HAVE_ECC */
}

static void test_wc_ecc_get_curve_id_from_name(void)
{
#ifdef HAVE_ECC
    int id;

    printf(testingFmt, "wc_ecc_get_curve_id_from_name");

    #if !defined(NO_ECC256) && !defined(NO_ECC_SECP)
        id = wc_ecc_get_curve_id_from_name("SECP256R1");
        AssertIntEQ(id, ECC_SECP256R1);
    #endif

    /* invalid case */
    id = wc_ecc_get_curve_id_from_name("BADCURVE");
    AssertIntEQ(id, -1);

    /* NULL input */
    id = wc_ecc_get_curve_id_from_name(NULL);
    AssertIntEQ(id, BAD_FUNC_ARG);

    printf(resultFmt, passed);
#endif /* HAVE_ECC */
}

static void test_wc_ecc_get_curve_id_from_params(void)
{
#ifdef HAVE_ECC
    int id;

    const byte prime[] =
    {
        0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
    };

    const byte primeInvalid[] =
    {
        0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x01,0x01
    };

    const byte Af[] =
    {
        0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFC
    };

    const byte Bf[] =
    {
        0x5A,0xC6,0x35,0xD8,0xAA,0x3A,0x93,0xE7,
        0xB3,0xEB,0xBD,0x55,0x76,0x98,0x86,0xBC,
        0x65,0x1D,0x06,0xB0,0xCC,0x53,0xB0,0xF6,
        0x3B,0xCE,0x3C,0x3E,0x27,0xD2,0x60,0x4B
    };

    const byte order[] =
    {
        0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
        0xBC,0xE6,0xFA,0xAD,0xA7,0x17,0x9E,0x84,
        0xF3,0xB9,0xCA,0xC2,0xFC,0x63,0x25,0x51
    };

    const byte Gx[] =
    {
        0x6B,0x17,0xD1,0xF2,0xE1,0x2C,0x42,0x47,
        0xF8,0xBC,0xE6,0xE5,0x63,0xA4,0x40,0xF2,
        0x77,0x03,0x7D,0x81,0x2D,0xEB,0x33,0xA0,
        0xF4,0xA1,0x39,0x45,0xD8,0x98,0xC2,0x96
    };

    const byte Gy[] =
    {
        0x4F,0xE3,0x42,0xE2,0xFE,0x1A,0x7F,0x9B,
        0x8E,0xE7,0xEB,0x4A,0x7C,0x0F,0x9E,0x16,
        0x2B,0xCE,0x33,0x57,0x6B,0x31,0x5E,0xCE,
        0xCB,0xB6,0x40,0x68,0x37,0xBF,0x51,0xF5
    };

    int cofactor = 1;
    int fieldSize = 256;

    printf(testingFmt, "wc_ecc_get_curve_id_from_params");

    #if !defined(NO_ECC256) && !defined(NO_ECC_SECP)
        id = wc_ecc_get_curve_id_from_params(fieldSize, prime, sizeof(prime),
                Af, sizeof(Af), Bf, sizeof(Bf), order, sizeof(order),
                Gx, sizeof(Gx), Gy, sizeof(Gy), cofactor);
        AssertIntEQ(id, ECC_SECP256R1);
    #endif

    /* invalid case, fieldSize = 0 */
    id = wc_ecc_get_curve_id_from_params(0, prime, sizeof(prime),
            Af, sizeof(Af), Bf, sizeof(Bf), order, sizeof(order),
            Gx, sizeof(Gx), Gy, sizeof(Gy), cofactor);
    AssertIntEQ(id, ECC_CURVE_INVALID);

    /* invalid case, NULL prime */
    id = wc_ecc_get_curve_id_from_params(fieldSize, NULL, sizeof(prime),
            Af, sizeof(Af), Bf, sizeof(Bf), order, sizeof(order),
            Gx, sizeof(Gx), Gy, sizeof(Gy), cofactor);
    AssertIntEQ(id, BAD_FUNC_ARG);

    /* invalid case, invalid prime */
    id = wc_ecc_get_curve_id_from_params(fieldSize,
            primeInvalid, sizeof(primeInvalid),
            Af, sizeof(Af), Bf, sizeof(Bf), order, sizeof(order),
            Gx, sizeof(Gx), Gy, sizeof(Gy), cofactor);
    AssertIntEQ(id, ECC_CURVE_INVALID);

    printf(resultFmt, passed);
#endif
}


/*----------------------------------------------------------------------------*
 | Certficate Failure Checks
 *----------------------------------------------------------------------------*/
#ifndef NO_CERTS
    /* Use the Cert Manager(CM) API to generate the error ASN_SIG_CONFIRM_E */
    static int verify_sig_cm(const char* ca, byte* cert_buf, size_t cert_sz,
        int type)
    {
        int ret;
        WOLFSSL_CERT_MANAGER* cm = NULL;

        switch (type) {
            case TESTING_RSA:
            #ifdef NO_RSA
                printf("RSA disabled, skipping test\n");
                return ASN_SIG_CONFIRM_E;
            #else
                break;
            #endif
            case TESTING_ECC:
            #ifndef HAVE_ECC
                printf("ECC disabled, skipping test\n");
                return ASN_SIG_CONFIRM_E;
            #else
                break;
            #endif
            default:
                printf("Bad function argument\n");
                return BAD_FUNC_ARG;
        }
        cm = wolfSSL_CertManagerNew();
        if (cm == NULL) {
            printf("wolfSSL_CertManagerNew failed\n");
            return -1;
        }

    #ifndef NO_FILESYSTEM
        ret = wolfSSL_CertManagerLoadCA(cm, ca, 0);
        if (ret != WOLFSSL_SUCCESS) {
            printf("wolfSSL_CertManagerLoadCA failed\n");
            wolfSSL_CertManagerFree(cm);
            return ret;
        }
    #else
        (void)ca;
    #endif

        ret = wolfSSL_CertManagerVerifyBuffer(cm, cert_buf, cert_sz, WOLFSSL_FILETYPE_ASN1);
        /* Let AssertIntEQ handle return code */

        wolfSSL_CertManagerFree(cm);

        return ret;
    }

    static int test_RsaSigFailure_cm(void)
    {
        int ret = 0;
        const char* ca_cert = "./certs/ca-cert.pem";
        const char* server_cert = "./certs/server-cert.der";
        byte* cert_buf = NULL;
        size_t cert_sz = 0;

        ret = load_file(server_cert, &cert_buf, &cert_sz);
        if (ret == 0) {
            /* corrupt DER - invert last byte, which is signature */
            cert_buf[cert_sz-1] = ~cert_buf[cert_sz-1];

            /* test bad cert */
            ret = verify_sig_cm(ca_cert, cert_buf, cert_sz, TESTING_RSA);
        }

        printf("Signature failure test: RSA: Ret %d\n", ret);

        if (cert_buf)
            free(cert_buf);

        return ret;
    }

    static int test_EccSigFailure_cm(void)
    {
        int ret = 0;
        /* self-signed ECC cert, so use server cert as CA */
        const char* ca_cert = "./certs/ca-ecc-cert.pem";
        const char* server_cert = "./certs/server-ecc.der";
        byte* cert_buf = NULL;
        size_t cert_sz = 0;

        ret = load_file(server_cert, &cert_buf, &cert_sz);
        if (ret == 0) {
            /* corrupt DER - invert last byte, which is signature */
            cert_buf[cert_sz-1] = ~cert_buf[cert_sz-1];

            /* test bad cert */
            ret = verify_sig_cm(ca_cert, cert_buf, cert_sz, TESTING_ECC);
        }

        printf("Signature failure test: ECC: Ret %d\n", ret);

        if (cert_buf)
            free(cert_buf);

        return ret;
    }

#endif /* NO_CERTS */

#ifdef WOLFSSL_TLS13
#if defined(WOLFSSL_SEND_HRR_COOKIE) && !defined(NO_WOLFSSL_SERVER)
static byte fixedKey[WC_SHA384_DIGEST_SIZE] = { 0, };
#endif
#ifdef WOLFSSL_EARLY_DATA
static const char earlyData[] = "Early Data";
static       char earlyDataBuffer[1];
#endif

static int test_tls13_apis(void)
{
    int          ret = 0;
#ifndef WOLFSSL_NO_TLS12
#ifndef NO_WOLFSSL_CLIENT
    WOLFSSL_CTX* clientTls12Ctx;
    WOLFSSL*     clientTls12Ssl;
#endif
#ifndef NO_WOLFSSL_SERVER
    WOLFSSL_CTX* serverTls12Ctx;
    WOLFSSL*     serverTls12Ssl;
#endif
#endif
#ifndef NO_WOLFSSL_CLIENT
    WOLFSSL_CTX* clientCtx;
    WOLFSSL*     clientSsl;
#endif
#ifndef NO_WOLFSSL_SERVER
    WOLFSSL_CTX* serverCtx;
    WOLFSSL*     serverSsl;
#ifndef NO_CERTS
    const char*  ourCert = svrCertFile;
    const char*  ourKey  = svrKeyFile;
#endif
#endif
#ifdef WOLFSSL_EARLY_DATA
    int          outSz;
#endif
    int          groups[1] = { WOLFSSL_ECC_X25519 };
    int          numGroups = 1;

#ifndef WOLFSSL_NO_TLS12
#ifndef NO_WOLFSSL_CLIENT
    clientTls12Ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    clientTls12Ssl = wolfSSL_new(clientTls12Ctx);
#endif
#ifndef NO_WOLFSSL_SERVER
    serverTls12Ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method());
#ifndef NO_CERTS
    wolfSSL_CTX_use_certificate_chain_file(serverTls12Ctx, ourCert);
    wolfSSL_CTX_use_PrivateKey_file(serverTls12Ctx, ourKey, WOLFSSL_FILETYPE_PEM);
#endif
    serverTls12Ssl = wolfSSL_new(serverTls12Ctx);
#endif
#endif

#ifndef NO_WOLFSSL_CLIENT
    clientCtx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
    clientSsl = wolfSSL_new(clientCtx);
#endif
#ifndef NO_WOLFSSL_SERVER
    serverCtx = wolfSSL_CTX_new(wolfTLSv1_3_server_method());
#ifndef NO_CERTS
    wolfSSL_CTX_use_certificate_chain_file(serverCtx, ourCert);
    wolfSSL_CTX_use_PrivateKey_file(serverCtx, ourKey, WOLFSSL_FILETYPE_PEM);
#endif
    serverSsl = wolfSSL_new(serverCtx);
#endif

#ifdef WOLFSSL_SEND_HRR_COOKIE
    AssertIntEQ(wolfSSL_send_hrr_cookie(NULL, NULL, 0), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
    AssertIntEQ(wolfSSL_send_hrr_cookie(clientSsl, NULL, 0), SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_SERVER
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_send_hrr_cookie(serverTls12Ssl, NULL, 0), BAD_FUNC_ARG);
#endif

    AssertIntEQ(wolfSSL_send_hrr_cookie(serverSsl, NULL, 0), WOLFSSL_SUCCESS);
    AssertIntEQ(wolfSSL_send_hrr_cookie(serverSsl, fixedKey, sizeof(fixedKey)),
                WOLFSSL_SUCCESS);
#endif
#endif

#ifdef HAVE_ECC
    AssertIntEQ(wolfSSL_UseKeyShare(NULL, WOLFSSL_ECC_SECP256R1), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_UseKeyShare(serverSsl, WOLFSSL_ECC_SECP256R1),
                WOLFSSL_SUCCESS);
#endif
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_UseKeyShare(clientTls12Ssl, WOLFSSL_ECC_SECP256R1),
                WOLFSSL_SUCCESS);
#endif
    AssertIntEQ(wolfSSL_UseKeyShare(clientSsl, WOLFSSL_ECC_SECP256R1),
                WOLFSSL_SUCCESS);
#endif
#elif defined(HAVE_CURVE25519)
    AssertIntEQ(wolfSSL_UseKeyShare(NULL, WOLFSSL_ECC_X25519), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_UseKeyShare(serverSsl, WOLFSSL_ECC_X25519),
                WOLFSSL_SUCCESS);
#endif
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_UseKeyShare(clientTls12Ssl, WOLFSSL_ECC_X25519),
                WOLFSSL_SUCCESS);
#endif
    AssertIntEQ(wolfSSL_UseKeyShare(clientSsl, WOLFSSL_ECC_X25519),
                WOLFSSL_SUCCESS);
#endif
#else
    AssertIntEQ(wolfSSL_UseKeyShare(NULL, WOLFSSL_ECC_SECP256R1), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_UseKeyShare(clientTls12Ssl, WOLFSSL_ECC_SECP256R1),
                NOT_COMPILED_IN);
#endif
    AssertIntEQ(wolfSSL_UseKeyShare(clientSsl, WOLFSSL_ECC_SECP256R1),
                NOT_COMPILED_IN);
#endif
#endif

    AssertIntEQ(wolfSSL_NoKeyShares(NULL), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_NoKeyShares(serverSsl), SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_NoKeyShares(clientTls12Ssl), WOLFSSL_SUCCESS);
#endif
    AssertIntEQ(wolfSSL_NoKeyShares(clientSsl), WOLFSSL_SUCCESS);
#endif

    AssertIntEQ(wolfSSL_CTX_no_ticket_TLSv13(NULL), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
    AssertIntEQ(wolfSSL_CTX_no_ticket_TLSv13(clientCtx), SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_SERVER
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_CTX_no_ticket_TLSv13(serverTls12Ctx), BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_CTX_no_ticket_TLSv13(serverCtx), 0);
#endif

    AssertIntEQ(wolfSSL_no_ticket_TLSv13(NULL), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
    AssertIntEQ(wolfSSL_no_ticket_TLSv13(clientSsl), SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_SERVER
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_no_ticket_TLSv13(serverTls12Ssl), BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_no_ticket_TLSv13(serverSsl), 0);
#endif

    AssertIntEQ(wolfSSL_CTX_no_dhe_psk(NULL), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_CTX_no_dhe_psk(clientTls12Ctx), BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_CTX_no_dhe_psk(clientCtx), 0);
#endif
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_CTX_no_dhe_psk(serverCtx), 0);
#endif

    AssertIntEQ(wolfSSL_no_dhe_psk(NULL), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_no_dhe_psk(clientTls12Ssl), BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_no_dhe_psk(clientSsl), 0);
#endif
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_no_dhe_psk(serverSsl), 0);
#endif

    AssertIntEQ(wolfSSL_update_keys(NULL), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_update_keys(clientTls12Ssl), BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_update_keys(clientSsl), BUILD_MSG_ERROR);
#endif
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_update_keys(serverSsl), BUILD_MSG_ERROR);
#endif

#if !defined(NO_CERTS) && defined(WOLFSSL_POST_HANDSHAKE_AUTH)
    AssertIntEQ(wolfSSL_CTX_allow_post_handshake_auth(NULL), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_CTX_allow_post_handshake_auth(serverCtx), SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_CTX_allow_post_handshake_auth(clientTls12Ctx),
                BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_CTX_allow_post_handshake_auth(clientCtx), 0);
#endif

    AssertIntEQ(wolfSSL_allow_post_handshake_auth(NULL), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_allow_post_handshake_auth(serverSsl), SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_allow_post_handshake_auth(clientTls12Ssl),
                BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_allow_post_handshake_auth(clientSsl), 0);
#endif

    AssertIntEQ(wolfSSL_request_certificate(NULL), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
    AssertIntEQ(wolfSSL_request_certificate(clientSsl), SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_SERVER
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_request_certificate(serverTls12Ssl),
                BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_request_certificate(serverSsl), NOT_READY_ERROR);
#endif
#endif

#ifndef WOLFSSL_NO_SERVER_GROUPS_EXT
    AssertIntEQ(wolfSSL_preferred_group(NULL), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_preferred_group(serverSsl), SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_preferred_group(clientTls12Ssl), BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_preferred_group(clientSsl), NOT_READY_ERROR);
#endif
#endif

    AssertIntEQ(wolfSSL_CTX_set_groups(NULL, NULL, 0), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
    AssertIntEQ(wolfSSL_CTX_set_groups(clientCtx, NULL, 0), BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_CTX_set_groups(NULL, groups, numGroups), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_CTX_set_groups(clientTls12Ctx, groups, numGroups),
                BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_CTX_set_groups(clientCtx, groups,
                                       WOLFSSL_MAX_GROUP_COUNT + 1),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_CTX_set_groups(clientCtx, groups, numGroups),
                WOLFSSL_SUCCESS);
#endif
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_CTX_set_groups(serverCtx, groups, numGroups),
                WOLFSSL_SUCCESS);
#endif

    AssertIntEQ(wolfSSL_set_groups(NULL, NULL, 0), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
    AssertIntEQ(wolfSSL_set_groups(clientSsl, NULL, 0), BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_set_groups(NULL, groups, numGroups), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_set_groups(clientTls12Ssl, groups, numGroups),
                BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_set_groups(clientSsl, groups,
                                   WOLFSSL_MAX_GROUP_COUNT + 1), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_set_groups(clientSsl, groups, numGroups),
                WOLFSSL_SUCCESS);
#endif
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_set_groups(serverSsl, groups, numGroups),
                WOLFSSL_SUCCESS);
#endif

#ifdef WOLFSSL_EARLY_DATA
    AssertIntEQ(wolfSSL_CTX_set_max_early_data(NULL, 0), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
    AssertIntEQ(wolfSSL_CTX_set_max_early_data(clientCtx, 0), SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_SERVER
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_CTX_set_max_early_data(serverTls12Ctx, 0),
                BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_CTX_set_max_early_data(serverCtx, 0), 0);
#endif

    AssertIntEQ(wolfSSL_set_max_early_data(NULL, 0), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
    AssertIntEQ(wolfSSL_set_max_early_data(clientSsl, 0), SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_SERVER
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_set_max_early_data(serverTls12Ssl, 0), BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_set_max_early_data(serverSsl, 0), 0);
#endif

    AssertIntEQ(wolfSSL_write_early_data(NULL, earlyData, sizeof(earlyData),
                                         &outSz), BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_CLIENT
    AssertIntEQ(wolfSSL_write_early_data(clientSsl, NULL, sizeof(earlyData),
                                         &outSz), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_write_early_data(clientSsl, earlyData, -1, &outSz),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_write_early_data(clientSsl, earlyData,
                                         sizeof(earlyData), NULL),
                BAD_FUNC_ARG);
#endif
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_write_early_data(serverSsl, earlyData,
                                         sizeof(earlyData), &outSz),
                SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_CLIENT
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_write_early_data(clientTls12Ssl, earlyData,
                                         sizeof(earlyData), &outSz),
                BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_write_early_data(clientSsl, earlyData,
                                         sizeof(earlyData), &outSz),
                WOLFSSL_FATAL_ERROR);
#endif

    AssertIntEQ(wolfSSL_read_early_data(NULL, earlyDataBuffer,
                                        sizeof(earlyDataBuffer), &outSz),
                BAD_FUNC_ARG);
#ifndef NO_WOLFSSL_SERVER
    AssertIntEQ(wolfSSL_read_early_data(serverSsl, NULL,
                                        sizeof(earlyDataBuffer), &outSz),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_read_early_data(serverSsl, earlyDataBuffer, -1, &outSz),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_read_early_data(serverSsl, earlyDataBuffer,
                                        sizeof(earlyDataBuffer), NULL),
                BAD_FUNC_ARG);
#endif
#ifndef NO_WOLFSSL_CLIENT
    AssertIntEQ(wolfSSL_read_early_data(clientSsl, earlyDataBuffer,
                                        sizeof(earlyDataBuffer), &outSz),
                SIDE_ERROR);
#endif
#ifndef NO_WOLFSSL_SERVER
#ifndef WOLFSSL_NO_TLS12
    AssertIntEQ(wolfSSL_read_early_data(serverTls12Ssl, earlyDataBuffer,
                                        sizeof(earlyDataBuffer), &outSz),
                BAD_FUNC_ARG);
#endif
    AssertIntEQ(wolfSSL_read_early_data(serverSsl, earlyDataBuffer,
                                        sizeof(earlyDataBuffer), &outSz),
                WOLFSSL_FATAL_ERROR);
#endif
#endif

#ifndef NO_WOLFSSL_SERVER
    wolfSSL_free(serverSsl);
    wolfSSL_CTX_free(serverCtx);
#endif
#ifndef NO_WOLFSSL_CLIENT
    wolfSSL_free(clientSsl);
    wolfSSL_CTX_free(clientCtx);
#endif

#ifndef WOLFSSL_NO_TLS12
#ifndef NO_WOLFSSL_SERVER
    wolfSSL_free(serverTls12Ssl);
    wolfSSL_CTX_free(serverTls12Ctx);
#endif
#ifndef NO_WOLFSSL_CLIENT
    wolfSSL_free(clientTls12Ssl);
    wolfSSL_CTX_free(clientTls12Ctx);
#endif
#endif

    return ret;
}

#endif

#ifdef HAVE_PK_CALLBACKS
#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && !defined(NO_RSA) && \
        !defined(NO_WOLFSSL_CLIENT) && !defined(NO_DH) && !defined(NO_AES) && \
         defined(HAVE_IO_TESTS_DEPENDENCIES) && !defined(SINGLE_THREADED)
static int my_DhCallback(WOLFSSL* ssl, struct DhKey* key,
        const unsigned char* priv, unsigned int privSz,
        const unsigned char* pubKeyDer, unsigned int pubKeySz,
        unsigned char* out, unsigned int* outlen,
        void* ctx)
{
    /* Test fail when context associated with WOLFSSL is NULL */
    if (ctx == NULL) {
        return -1;
    }

    (void)ssl;
    /* return 0 on success */
    return wc_DhAgree(key, out, outlen, priv, privSz, pubKeyDer, pubKeySz);
};

static void test_dh_ctx_setup(WOLFSSL_CTX* ctx) {
    wolfSSL_CTX_SetDhAgreeCb(ctx, my_DhCallback);
#ifdef WOLFSSL_AES_128
    AssertIntEQ(wolfSSL_CTX_set_cipher_list(ctx, "DHE-RSA-AES128-SHA256"),
            WOLFSSL_SUCCESS);
#endif
#ifdef WOLFSSL_AES_256
    AssertIntEQ(wolfSSL_CTX_set_cipher_list(ctx, "DHE-RSA-AES256-SHA256"),
            WOLFSSL_SUCCESS);
#endif
}


static void test_dh_ssl_setup(WOLFSSL* ssl)
{
    static int dh_test_ctx = 1;
    int ret;

    wolfSSL_SetDhAgreeCtx(ssl, &dh_test_ctx);
    AssertIntEQ(*((int*)wolfSSL_GetDhAgreeCtx(ssl)), dh_test_ctx);
    ret = wolfSSL_SetTmpDH_file(ssl, dhParamFile, WOLFSSL_FILETYPE_PEM);
    if (ret != WOLFSSL_SUCCESS && ret != SIDE_ERROR) {
        AssertIntEQ(ret, WOLFSSL_SUCCESS);
    }
}

static void test_dh_ssl_setup_fail(WOLFSSL* ssl)
{
    int ret;

    wolfSSL_SetDhAgreeCtx(ssl, NULL);
    AssertNull(wolfSSL_GetDhAgreeCtx(ssl));
    ret = wolfSSL_SetTmpDH_file(ssl, dhParamFile, WOLFSSL_FILETYPE_PEM);
    if (ret != WOLFSSL_SUCCESS && ret != SIDE_ERROR) {
        AssertIntEQ(ret, WOLFSSL_SUCCESS);
    }
}
#endif

static void test_DhCallbacks(void)
{
#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS) && !defined(NO_RSA) && \
        !defined(NO_WOLFSSL_CLIENT) && !defined(NO_DH) && !defined(NO_AES) && \
         defined(HAVE_IO_TESTS_DEPENDENCIES) && !defined(SINGLE_THREADED)
    WOLFSSL_CTX *ctx;
    WOLFSSL     *ssl;
    tcp_ready   ready;
    func_args   server_args;
    func_args   client_args;
    THREAD_TYPE serverThread;
    callback_functions func_cb_client;
    callback_functions func_cb_server;
    int  test;

    printf(testingFmt, "test_DhCallbacks");

#ifndef NO_WOLFSSL_CLIENT
    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_client_method()));
#else
    AssertNotNull(ctx = wolfSSL_CTX_new(wolfSSLv23_server_method()));
#endif
    wolfSSL_CTX_SetDhAgreeCb(ctx, &my_DhCallback);

    /* load client ca cert */
    AssertIntEQ(wolfSSL_CTX_load_verify_locations(ctx, caCertFile, 0),
            WOLFSSL_SUCCESS);

    /* test with NULL arguments */
    wolfSSL_SetDhAgreeCtx(NULL, &test);
    AssertNull(wolfSSL_GetDhAgreeCtx(NULL));

    /* test success case */
    test = 1;
    AssertNotNull(ssl = wolfSSL_new(ctx));
    wolfSSL_SetDhAgreeCtx(ssl, &test);
    AssertIntEQ(*((int*)wolfSSL_GetDhAgreeCtx(ssl)), test);

    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);

    /* test a connection where callback is used */
#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif
    XMEMSET(&server_args, 0, sizeof(func_args));
    XMEMSET(&client_args, 0, sizeof(func_args));
    XMEMSET(&func_cb_client, 0, sizeof(callback_functions));
    XMEMSET(&func_cb_server, 0, sizeof(callback_functions));

    StartTCP();
    InitTcpReady(&ready);

#if defined(USE_WINDOWS_API)
    /* use RNG to get random port if using windows */
    ready.port = GetRandomPort();
#endif

    server_args.signal = &ready;
    client_args.signal = &ready;
    server_args.return_code = TEST_FAIL;
    client_args.return_code = TEST_FAIL;

    /* set callbacks to use DH functions */
    func_cb_client.ctx_ready = &test_dh_ctx_setup;
    func_cb_client.ssl_ready = &test_dh_ssl_setup;
#ifndef WOLFSSL_NO_TLS12
    func_cb_client.method = wolfTLSv1_2_client_method;
#else
    func_cb_client.method = wolfTLSv1_3_client_method;
#endif
    client_args.callbacks = &func_cb_client;

    func_cb_server.ctx_ready = &test_dh_ctx_setup;
    func_cb_server.ssl_ready = &test_dh_ssl_setup;
#ifndef WOLFSSL_NO_TLS12
    func_cb_server.method = wolfTLSv1_2_server_method;
#else
    func_cb_server.method = wolfTLSv1_3_server_method;
#endif
    server_args.callbacks = &func_cb_server;

    start_thread(test_server_nofail, &server_args, &serverThread);
    wait_tcp_ready(&server_args);
    test_client_nofail(&client_args, NULL);
    join_thread(serverThread);

    AssertTrue(client_args.return_code);
    AssertTrue(server_args.return_code);

    FreeTcpReady(&ready);

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    /* now set user ctx to not be 1 so that the callback returns fail case */
#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif
    XMEMSET(&server_args, 0, sizeof(func_args));
    XMEMSET(&client_args, 0, sizeof(func_args));
    XMEMSET(&func_cb_client, 0, sizeof(callback_functions));
    XMEMSET(&func_cb_server, 0, sizeof(callback_functions));

    StartTCP();
    InitTcpReady(&ready);

#if defined(USE_WINDOWS_API)
    /* use RNG to get random port if using windows */
    ready.port = GetRandomPort();
#endif

    server_args.signal = &ready;
    client_args.signal = &ready;
    server_args.return_code = TEST_FAIL;
    client_args.return_code = TEST_FAIL;

    /* set callbacks to use DH functions */
    func_cb_client.ctx_ready = &test_dh_ctx_setup;
    func_cb_client.ssl_ready = &test_dh_ssl_setup_fail;
#ifndef WOLFSSL_NO_TLS12
    func_cb_client.method = wolfTLSv1_2_client_method;
#else
    func_cb_client.method = wolfTLSv1_3_client_method;
#endif
    client_args.callbacks = &func_cb_client;

    func_cb_server.ctx_ready = &test_dh_ctx_setup;
    func_cb_server.ssl_ready = &test_dh_ssl_setup_fail;
#ifndef WOLFSSL_NO_TLS12
    func_cb_server.method = wolfTLSv1_2_server_method;
#else
    func_cb_server.method = wolfTLSv1_3_server_method;
#endif
    server_args.callbacks = &func_cb_server;

    start_thread(test_server_nofail, &server_args, &serverThread);
    wait_tcp_ready(&server_args);
    test_client_nofail(&client_args, NULL);
    join_thread(serverThread);

    AssertIntEQ(client_args.return_code, TEST_FAIL);
    AssertIntEQ(server_args.return_code, TEST_FAIL);

    FreeTcpReady(&ready);

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    printf(resultFmt, passed);
#endif
}
#endif /* HAVE_PK_CALLBACKS */

#ifdef HAVE_HASHDRBG

#ifdef TEST_RESEED_INTERVAL
static int test_wc_RNG_GenerateBlock_Reseed()
{
    int i, ret;
    WC_RNG rng;
    byte key[32];

    ret = wc_InitRng(&rng);

    if (ret == 0) {
        for(i = 0; i < WC_RESEED_INTERVAL + 10; i++) {
            ret = wc_RNG_GenerateBlock(&rng, key, sizeof(key));
            if (ret != 0) {
                break;
            }
        }
    }

    wc_FreeRng(&rng);

    return ret;
}
#endif /* TEST_RESEED_INTERVAL */

static int test_wc_RNG_GenerateBlock()
{
    int i, ret;
    WC_RNG rng;
    byte key[32];

    ret = wc_InitRng(&rng);

    if (ret == 0) {
        for(i = 0; i < 10; i++) {
            ret = wc_RNG_GenerateBlock(&rng, key, sizeof(key));
            if (ret != 0) {
                break;
            }
        }
    }

    wc_FreeRng(&rng);

    return ret;
}
#endif

static void test_wolfSSL_X509_CRL(void)
{
#if defined(OPENSSL_EXTRA) && defined(HAVE_CRL)

    X509_CRL *crl;
    char pem[][100] = {
        "./certs/crl/crl.pem",
        "./certs/crl/crl2.pem",
        "./certs/crl/caEccCrl.pem",
        "./certs/crl/eccCliCRL.pem",
        "./certs/crl/eccSrvCRL.pem",
        ""
    };

#ifdef HAVE_TEST_d2i_X509_CRL_fp
    char der[][100] = {
        "./certs/crl/crl.der",
        "./certs/crl/crl2.der",
        ""};
#endif

    XFILE fp;
    int i;

    printf(testingFmt, "test_wolfSSL_X509_CRL");

    for (i = 0; pem[i][0] != '\0'; i++)
    {
        AssertNotNull(fp = XFOPEN(pem[i], "rb"));
        AssertNotNull(crl = (X509_CRL *)PEM_read_X509_CRL(fp, (X509_CRL **)NULL, NULL, NULL));
        AssertNotNull(crl);
        X509_CRL_free(crl);
        XFCLOSE(fp);
        AssertNotNull(fp = XFOPEN(pem[i], "rb"));
        AssertNotNull((X509_CRL *)PEM_read_X509_CRL(fp, (X509_CRL **)&crl, NULL, NULL));
        AssertNotNull(crl);
        X509_CRL_free(crl);
        XFCLOSE(fp);
    }

#ifdef HAVE_TEST_d2i_X509_CRL_fp
    for(i = 0; der[i][0] != '\0'; i++){
        AssertNotNull(fp = XFOPEN(der[i], "rb"));
        AssertNotNull(crl = (X509_CRL *)d2i_X509_CRL_fp((fp, X509_CRL **)NULL));
        AssertNotNull(crl);
        X509_CRL_free(crl);
        XFCLOSE(fp);
        AssertNotNull(fp = XFOPEN(der[i], "rb"));
        AssertNotNull((X509_CRL *)d2i_X509_CRL_fp(fp, (X509_CRL **)&crl));
        AssertNotNull(crl);
        X509_CRL_free(crl);
        XFCLOSE(fp);
    }
#endif

    printf(resultFmt, passed);
#endif
        return;
}

static void test_wolfSSL_i2c_ASN1_INTEGER()
{
#ifdef OPENSSL_EXTRA
    ASN1_INTEGER *a;
    unsigned char *pp,*tpp;
    int ret;

    a = wolfSSL_ASN1_INTEGER_new();

    /* 40 */
    a->intData[0] = ASN_INTEGER;
    a->intData[1] = 1;
    a->intData[2] = 40;
    ret = wolfSSL_i2c_ASN1_INTEGER(a, NULL);
    AssertIntEQ(ret, 1);
    AssertNotNull(pp = (unsigned char*)XMALLOC(ret + 1, NULL,
                DYNAMIC_TYPE_TMP_BUFFER));
    tpp = pp;
    XMEMSET(pp, 0, ret + 1);
    wolfSSL_i2c_ASN1_INTEGER(a, &pp); 
    pp--;
    AssertIntEQ(*pp, 40);
    XFREE(tpp, NULL, DYNAMIC_TYPE_TMP_BUFFER);

    /* 128 */
    a->intData[0] = ASN_INTEGER;
    a->intData[1] = 1;
    a->intData[2] = 128;
    ret = wolfSSL_i2c_ASN1_INTEGER(a, NULL);
    AssertIntEQ(ret, 2);
    AssertNotNull(pp = (unsigned char*)XMALLOC(ret + 1, NULL,
                DYNAMIC_TYPE_TMP_BUFFER));
    tpp = pp;
    XMEMSET(pp, 0, ret + 1);
    wolfSSL_i2c_ASN1_INTEGER(a, &pp); 
    pp--;
    AssertIntEQ(*(pp--), 128);
    AssertIntEQ(*pp, 0);
    XFREE(tpp, NULL, DYNAMIC_TYPE_TMP_BUFFER);

    /* -40 */
    a->intData[0] = ASN_INTEGER;
    a->intData[1] = 1;
    a->intData[2] = 40;
    a->negative = 1;
    ret = wolfSSL_i2c_ASN1_INTEGER(a, NULL);
    AssertIntEQ(ret, 1);
    AssertNotNull(pp = (unsigned char*)XMALLOC(ret + 1, NULL,
                DYNAMIC_TYPE_TMP_BUFFER));
    tpp = pp;
    XMEMSET(pp, 0, ret + 1);
    wolfSSL_i2c_ASN1_INTEGER(a, &pp); 
    pp--;
    AssertIntEQ(*pp, 216);
    XFREE(tpp, NULL, DYNAMIC_TYPE_TMP_BUFFER);

    /* -128 */
    a->intData[0] = ASN_INTEGER;
    a->intData[1] = 1;
    a->intData[2] = 128;
    a->negative = 1;
    ret = wolfSSL_i2c_ASN1_INTEGER(a, NULL);
    AssertIntEQ(ret, 1);
    AssertNotNull(pp = (unsigned char*)XMALLOC(ret + 1, NULL,
                DYNAMIC_TYPE_TMP_BUFFER));
    tpp = pp;
    XMEMSET(pp, 0, ret + 1);
    wolfSSL_i2c_ASN1_INTEGER(a, &pp); 
    pp--;
    AssertIntEQ(*pp, 128);
    XFREE(tpp, NULL, DYNAMIC_TYPE_TMP_BUFFER);

    /* -200 */
    a->intData[0] = ASN_INTEGER;
    a->intData[1] = 1;
    a->intData[2] = 200;
    a->negative = 1;
    ret = wolfSSL_i2c_ASN1_INTEGER(a, NULL);
    AssertIntEQ(ret, 2);
    AssertNotNull(pp = (unsigned char*)XMALLOC(ret + 1, NULL,
            DYNAMIC_TYPE_TMP_BUFFER));
    tpp = pp;
    XMEMSET(pp, 0, ret + 1);
    wolfSSL_i2c_ASN1_INTEGER(a, &pp); 
    pp--;
    AssertIntEQ(*(pp--), 56);
    AssertIntEQ(*pp, 255);

    XFREE(tpp, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    wolfSSL_ASN1_INTEGER_free(a); 

    printf(resultFmt, passed);
#endif /* OPENSSL_EXTRA */
}
/*----------------------------------------------------------------------------*
 | Main
 *----------------------------------------------------------------------------*/

void ApiTest(void)
{
    printf(" Begin API Tests\n");
    AssertIntEQ(test_wolfSSL_Init(), WOLFSSL_SUCCESS);
    /* wolfcrypt initialization tests */
    test_wolfSSL_Method_Allocators();
#ifndef NO_WOLFSSL_SERVER
    test_wolfSSL_CTX_new(wolfSSLv23_server_method());
#endif
    test_wolfSSL_CTX_use_certificate_file();
    AssertIntEQ(test_wolfSSL_CTX_use_certificate_buffer(), WOLFSSL_SUCCESS);
    test_wolfSSL_CTX_use_PrivateKey_file();
    test_wolfSSL_CTX_load_verify_locations();
    test_wolfSSL_CTX_trust_peer_cert();
    test_wolfSSL_CTX_SetTmpDH_file();
    test_wolfSSL_CTX_SetTmpDH_buffer();
    test_server_wolfSSL_new();
    test_client_wolfSSL_new();
    test_wolfSSL_SetTmpDH_file();
    test_wolfSSL_SetTmpDH_buffer();
#if !defined(NO_WOLFSSL_CLIENT) && !defined(NO_WOLFSSL_SERVER)
    test_wolfSSL_read_write();
#endif
    test_wolfSSL_dtls_export();
    AssertIntEQ(test_wolfSSL_SetMinVersion(), WOLFSSL_SUCCESS);
    AssertIntEQ(test_wolfSSL_CTX_SetMinVersion(), WOLFSSL_SUCCESS);

    /* TLS extensions tests */
    test_wolfSSL_UseSNI();
    test_wolfSSL_UseMaxFragment();
    test_wolfSSL_UseTruncatedHMAC();
    test_wolfSSL_UseSupportedCurve();
    test_wolfSSL_UseALPN();
    test_wolfSSL_DisableExtendedMasterSecret();

    /* X509 tests */
    test_wolfSSL_X509_NAME_get_entry();
    test_wolfSSL_PKCS12();
    test_wolfSSL_PKCS8();
    test_wolfSSL_PKCS5();
    test_wolfSSL_URI();

    /*OCSP Stapling. */
    AssertIntEQ(test_wolfSSL_UseOCSPStapling(), WOLFSSL_SUCCESS);
    AssertIntEQ(test_wolfSSL_UseOCSPStaplingV2(), WOLFSSL_SUCCESS);

    /* Multicast */
    test_wolfSSL_mcast();

    /* compatibility tests */
    test_wolfSSL_X509_NAME();
    test_wolfSSL_DES();
    test_wolfSSL_certs();
    test_wolfSSL_ASN1_TIME_print();
    test_wolfSSL_ASN1_GENERALIZEDTIME_free();
    test_wolfSSL_private_keys();
    test_wolfSSL_PEM_PrivateKey();
    test_wolfSSL_PEM_RSAPrivateKey();
    test_wolfSSL_tmp_dh();
    test_wolfSSL_ctrl();
    test_wolfSSL_EVP_PKEY_new_mac_key();
    test_wolfSSL_EVP_MD_hmac_signing();
    test_wolfSSL_CTX_add_extra_chain_cert();
#if !defined(NO_WOLFSSL_CLIENT) && !defined(NO_WOLFSSL_SERVER)
    test_wolfSSL_ERR_peek_last_error_line();
#endif
    test_wolfSSL_set_options();
    test_wolfSSL_X509_STORE_CTX();
    test_wolfSSL_msgCb();
    test_wolfSSL_X509_STORE_set_flags();
    test_wolfSSL_X509_LOOKUP_load_file();
    test_wolfSSL_X509_NID();
    test_wolfSSL_X509_STORE_CTX_set_time();
    test_wolfSSL_X509_STORE();
    test_wolfSSL_BN();
    test_wolfSSL_PEM_read_bio();
    test_wolfSSL_BIO();
    test_wolfSSL_ASN1_STRING();
    test_wolfSSL_X509();
    test_wolfSSL_RAND();
    test_wolfSSL_BUF();
    test_wolfSSL_set_tlsext_status_type();
    test_wolfSSL_ASN1_TIME_adj();
    test_wolfSSL_CTX_set_client_CA_list();
    test_wolfSSL_CTX_add_client_CA();
    test_wolfSSL_CTX_set_srp_username();
    test_wolfSSL_CTX_set_srp_password();
    test_wolfSSL_pseudo_rand();
    test_wolfSSL_PKCS8_Compat();
    test_wolfSSL_ERR_put_error();
    test_wolfSSL_HMAC();
    test_wolfSSL_OBJ();
    test_wolfSSL_X509_NAME_ENTRY();
    test_wolfSSL_BIO_gets();
    test_wolfSSL_d2i_PUBKEY();
    test_wolfSSL_BIO_write();
    test_wolfSSL_SESSION();
    test_wolfSSL_DES_ecb_encrypt();
    test_wolfSSL_sk_GENERAL_NAME();
    test_wolfSSL_MD4();
    test_wolfSSL_RSA();
    test_wolfSSL_RSA_DER();
    test_wolfSSL_verify_depth();
    test_wolfSSL_HMAC_CTX();
    test_wolfSSL_msg_callback();
    test_wolfSSL_SHA();
    test_wolfSSL_DH_1536_prime();
    test_wolfSSL_AES_ecb_encrypt();
    test_wolfSSL_SHA256();
    test_wolfSSL_X509_get_serialNumber();
    test_wolfSSL_X509_CRL();
    test_wolfSSL_OPENSSL_add_all_algorithms();
    test_wolfSSL_ASN1_STRING_print_ex();
    test_wolfSSL_ASN1_TIME_to_generalizedtime();
    test_wolfSSL_i2c_ASN1_INTEGER();
    test_wolfSSL_X509_check_ca();

    /* test the no op functions for compatibility */
    test_no_op_functions();

    AssertIntEQ(test_wolfSSL_Cleanup(), WOLFSSL_SUCCESS);

    /* wolfCrypt ASN tests */
    test_wc_GetPkcs8TraditionalOffset();

    /* wolfCrypt ECC tests */
    test_wc_ecc_get_curve_size_from_name();
    test_wc_ecc_get_curve_id_from_name();
    test_wc_ecc_get_curve_id_from_params();

#ifdef WOLFSSL_TLS13
    /* TLS v1.3 API tests */
    test_tls13_apis();
#endif

#ifndef NO_CERTS
    /* Bad certificate signature tests */
    AssertIntEQ(test_EccSigFailure_cm(), ASN_SIG_CONFIRM_E);
    AssertIntEQ(test_RsaSigFailure_cm(), ASN_SIG_CONFIRM_E);
#endif /* NO_CERTS */

#ifdef HAVE_PK_CALLBACKS
    /* public key callback tests */
    test_DhCallbacks();
#endif

    /*wolfcrypt */
    printf("\n-----------------wolfcrypt unit tests------------------\n");
    AssertFalse(test_wolfCrypt_Init());
    AssertFalse(test_wc_InitMd5());
    AssertFalse(test_wc_Md5Update());
    AssertFalse(test_wc_Md5Final());
    AssertFalse(test_wc_InitSha());
    AssertFalse(test_wc_ShaUpdate());
    AssertFalse(test_wc_ShaFinal());
    AssertFalse(test_wc_InitSha256());
    AssertFalse(test_wc_Sha256Update());
    AssertFalse(test_wc_Sha256Final());
    AssertFalse(test_wc_InitSha512());
    AssertFalse(test_wc_Sha512Update());
    AssertFalse(test_wc_Sha512Final());
    AssertFalse(test_wc_InitSha384());
    AssertFalse(test_wc_Sha384Update());
    AssertFalse(test_wc_Sha384Final());
    AssertFalse(test_wc_InitSha224());
    AssertFalse(test_wc_Sha224Update());
    AssertFalse(test_wc_Sha224Final());
    AssertFalse(test_wc_InitBlake2b());
    AssertFalse(test_wc_InitRipeMd());
    AssertFalse(test_wc_RipeMdUpdate());
    AssertFalse(test_wc_RipeMdFinal());

    AssertIntEQ(test_wc_InitSha3(), 0);
    AssertIntEQ(testing_wc_Sha3_Update(), 0);
    AssertIntEQ(test_wc_Sha3_224_Final(), 0);
    AssertIntEQ(test_wc_Sha3_256_Final(), 0);
    AssertIntEQ(test_wc_Sha3_384_Final(), 0);
    AssertIntEQ(test_wc_Sha3_512_Final(), 0);
    AssertIntEQ(test_wc_Sha3_224_Copy(), 0);
    AssertIntEQ(test_wc_Sha3_256_Copy(), 0);
    AssertIntEQ(test_wc_Sha3_384_Copy(), 0);
    AssertIntEQ(test_wc_Sha3_512_Copy(), 0);

    AssertFalse(test_wc_Md5HmacSetKey());
    AssertFalse(test_wc_Md5HmacUpdate());
    AssertFalse(test_wc_Md5HmacFinal());
    AssertFalse(test_wc_ShaHmacSetKey());
    AssertFalse(test_wc_ShaHmacUpdate());
    AssertFalse(test_wc_ShaHmacFinal());
    AssertFalse(test_wc_Sha224HmacSetKey());
    AssertFalse(test_wc_Sha224HmacUpdate());
    AssertFalse(test_wc_Sha224HmacFinal());
    AssertFalse(test_wc_Sha256HmacSetKey());
    AssertFalse(test_wc_Sha256HmacUpdate());
    AssertFalse(test_wc_Sha256HmacFinal());
    AssertFalse(test_wc_Sha384HmacSetKey());
    AssertFalse(test_wc_Sha384HmacUpdate());
    AssertFalse(test_wc_Sha384HmacFinal());

    AssertIntEQ(test_wc_HashInit(), 0);

    AssertIntEQ(test_wc_InitCmac(), 0);
    AssertIntEQ(test_wc_CmacUpdate(), 0);
    AssertIntEQ(test_wc_CmacFinal(), 0);
    AssertIntEQ(test_wc_AesCmacGenerate(), 0);

    AssertIntEQ(test_wc_Des3_SetIV(), 0);
    AssertIntEQ(test_wc_Des3_SetKey(), 0);
    AssertIntEQ(test_wc_Des3_CbcEncryptDecrypt(), 0);
    AssertIntEQ(test_wc_Des3_CbcEncryptDecryptWithKey(), 0);
    AssertIntEQ(test_wc_IdeaSetKey(), 0);
    AssertIntEQ(test_wc_IdeaSetIV(), 0);
    AssertIntEQ(test_wc_IdeaCipher(), 0);
    AssertIntEQ(test_wc_IdeaCbcEncyptDecrypt(), 0);
    AssertIntEQ(test_wc_Chacha_SetKey(), 0);
    AssertIntEQ(test_wc_Chacha_Process(), 0);
    AssertIntEQ(test_wc_ChaCha20Poly1305_aead(), 0);
    AssertIntEQ(test_wc_Poly1305SetKey(), 0);

    AssertIntEQ(test_wc_CamelliaSetKey(), 0);
    AssertIntEQ(test_wc_CamelliaSetIV(), 0);
    AssertIntEQ(test_wc_CamelliaEncryptDecryptDirect(), 0);
    AssertIntEQ(test_wc_CamelliaCbcEncryptDecrypt(), 0);


    AssertIntEQ(test_wc_RabbitSetKey(), 0);
    AssertIntEQ(test_wc_RabbitProcess(), 0);

    AssertIntEQ(test_wc_Arc4SetKey(), 0);
    AssertIntEQ(test_wc_Arc4Process(), 0);

    AssertIntEQ(test_wc_AesSetKey(), 0);
    AssertIntEQ(test_wc_AesSetIV(), 0);
    AssertIntEQ(test_wc_AesCbcEncryptDecrypt(), 0);
    AssertIntEQ(test_wc_AesCtrEncryptDecrypt(), 0);
    AssertIntEQ(test_wc_AesGcmSetKey(), 0);
    AssertIntEQ(test_wc_AesGcmEncryptDecrypt(), 0);
    AssertIntEQ(test_wc_GmacSetKey(), 0);
    AssertIntEQ(test_wc_GmacUpdate(), 0);
    AssertIntEQ(test_wc_InitRsaKey(), 0);
    AssertIntEQ(test_wc_RsaPrivateKeyDecode(), 0);
    AssertIntEQ(test_wc_RsaPublicKeyDecode(), 0);
    AssertIntEQ(test_wc_RsaPublicKeyDecodeRaw(), 0);
    AssertIntEQ(test_wc_MakeRsaKey(), 0);
    AssertIntEQ(test_wc_SetKeyUsage (), 0);


    AssertIntEQ(test_wc_RsaKeyToDer(), 0);
    AssertIntEQ(test_wc_RsaKeyToPublicDer(), 0);
    AssertIntEQ(test_wc_RsaPublicEncryptDecrypt(), 0);
    AssertIntEQ(test_wc_RsaPublicEncryptDecrypt_ex(), 0);
    AssertIntEQ(test_wc_RsaEncryptSize(), 0);
    AssertIntEQ(test_wc_RsaSSL_SignVerify(), 0);
    AssertIntEQ(test_wc_RsaFlattenPublicKey(), 0);
    AssertIntEQ(test_RsaDecryptBoundsCheck(), 0);
    AssertIntEQ(test_wc_AesCcmSetKey(), 0);
    AssertIntEQ(test_wc_AesCcmEncryptDecrypt(), 0);
    AssertIntEQ(test_wc_Hc128_SetKey(), 0);
    AssertIntEQ(test_wc_Hc128_Process(), 0);
    AssertIntEQ(test_wc_InitDsaKey(), 0);
    AssertIntEQ(test_wc_DsaSignVerify(), 0);
    AssertIntEQ(test_wc_DsaPublicPrivateKeyDecode(), 0);
    AssertIntEQ(test_wc_MakeDsaKey(), 0);
    AssertIntEQ(test_wc_DsaKeyToDer(), 0);
    AssertIntEQ(test_wc_DsaImportParamsRaw(), 0);
    AssertIntEQ(test_wc_DsaExportParamsRaw(), 0);
    AssertIntEQ(test_wc_DsaExportKeyRaw(), 0);
    AssertIntEQ(test_wc_SignatureGetSize_ecc(), 0);
    AssertIntEQ(test_wc_SignatureGetSize_rsa(), 0);

#ifdef OPENSSL_EXTRA
    /*wolfSSS_EVP_get_cipherbynid test*/
    test_wolfSSL_EVP_get_cipherbynid();
    test_wolfSSL_EC();
#endif

#ifdef HAVE_HASHDRBG
    #ifdef TEST_RESEED_INTERVAL
    AssertIntEQ(test_wc_RNG_GenerateBlock_Reseed(), 0);
    #endif
    AssertIntEQ(test_wc_RNG_GenerateBlock(), 0);
#endif

    AssertIntEQ(test_wc_ed25519_make_key(), 0);
    AssertIntEQ(test_wc_ed25519_init(), 0);
    AssertIntEQ(test_wc_ed25519_sign_msg(), 0);
    AssertIntEQ(test_wc_ed25519_import_public(), 0);
    AssertIntEQ(test_wc_ed25519_import_private_key(), 0);
    AssertIntEQ(test_wc_ed25519_export(), 0);
    AssertIntEQ(test_wc_ed25519_size(), 0);
    AssertIntEQ(test_wc_ed25519_exportKey(), 0);

    AssertIntEQ(test_wc_curve25519_init(), 0);

    AssertIntEQ(test_wc_ecc_make_key(), 0);
    AssertIntEQ(test_wc_ecc_init(), 0);
    AssertIntEQ(test_wc_ecc_check_key(), 0);
    AssertIntEQ(test_wc_ecc_size(), 0);
    AssertIntEQ(test_wc_ecc_signVerify_hash(), 0);
    AssertIntEQ(test_wc_ecc_shared_secret(), 0);
    AssertIntEQ(test_wc_ecc_export_x963(), 0);
    AssertIntEQ(test_wc_ecc_export_x963_ex(), 0);
    AssertIntEQ(test_wc_ecc_import_x963(), 0);
    AssertIntEQ(ecc_import_private_key(), 0);
    AssertIntEQ(test_wc_ecc_export_private_only(), 0);
    AssertIntEQ(test_wc_ecc_rs_to_sig(), 0);
    AssertIntEQ(test_wc_ecc_import_raw(), 0);
    AssertIntEQ(test_wc_ecc_sig_size(), 0);
    AssertIntEQ(test_wc_ecc_ctx_new(), 0);
    AssertIntEQ(test_wc_ecc_ctx_reset(), 0);
    AssertIntEQ(test_wc_ecc_ctx_set_peer_salt(), 0);
    AssertIntEQ(test_wc_ecc_ctx_set_info(), 0);
    AssertIntEQ(test_wc_ecc_encryptDecrypt(), 0);
    AssertIntEQ(test_wc_ecc_del_point(), 0);
    AssertIntEQ(test_wc_ecc_pointFns(), 0);
    AssertIntEQ(test_wc_ecc_shared_secret_ssh(), 0);
    AssertIntEQ(test_wc_ecc_verify_hash_ex(), 0);
    AssertIntEQ(test_wc_ecc_mulmod(), 0);
    AssertIntEQ(test_wc_ecc_is_valid_idx(), 0);

    test_wc_PKCS7_Init();
    test_wc_PKCS7_InitWithCert();
    test_wc_PKCS7_EncodeData();
    test_wc_PKCS7_EncodeSignedData();
    test_wc_PKCS7_VerifySignedData();
    test_wc_PKCS7_EncodeDecodeEnvelopedData();
    test_wc_PKCS7_EncodeEncryptedData();
     
    printf(" End API Tests\n");

}
