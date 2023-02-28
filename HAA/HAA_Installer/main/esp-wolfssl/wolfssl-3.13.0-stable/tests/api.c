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

#if defined(WOLFSSL_STATIC_MEMORY)
    #include <wolfssl/wolfcrypt/memory.h>
#endif /* WOLFSSL_STATIC_MEMORY */
#ifdef HAVE_ECC
    #include <wolfssl/wolfcrypt/ecc.h>   /* wc_ecc_fp_free */
#endif
#ifndef NO_ASN
    #include <wolfssl/wolfcrypt/asn_public.h>
#endif
#include <wolfssl/error-ssl.h>

#include <stdlib.h>
#include <wolfssl/ssl.h>  /* compatibility layer */
#include <wolfssl/test.h>
#include <tests/unit.h>

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


#ifndef NO_RSA
    #include <wolfssl/wolfcrypt/rsa.h>
    #include <wolfssl/wolfcrypt/hash.h>

    #define FOURK_BUF 4096
    #define GEN_BUF  294

    #ifndef USER_CRYPTO_ERROR
        #define USER_CRYPTO_ERROR -101 /* error returned by IPP lib. */
    #endif
#endif

#ifdef HAVE_AESCCM
    #include <wolfssl/wolfcrypt/aes.h>
#endif

#ifdef HAVE_HC128
    #include <wolfssl/wolfcrypt/hc128.h>
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
#endif

#ifdef OPENSSL_EXTRA
    #include <wolfssl/openssl/ssl.h>
    #include <wolfssl/openssl/pkcs12.h>
    #include <wolfssl/openssl/evp.h>
    #include <wolfssl/openssl/dh.h>
    #include <wolfssl/openssl/bn.h>
    #include <wolfssl/openssl/pem.h>
    #include <wolfssl/openssl/ec.h>
#ifndef NO_DES3
    #include <wolfssl/openssl/des.h>
#endif
#ifndef NO_ASN
    /* for ASN_COMMON_NAME DN_tags enum */
    #include <wolfssl/wolfcrypt/asn.h>
#endif
#endif /* OPENSSL_EXTRA */

#if defined(OPENSSL_EXTRA) && defined(WOLFCRYPT_HAVE_SRP) \
    && !defined(NO_SHA256) && !defined(RC_NO_RNG)
        #include <wolfssl/wolfcrypt/srp.h>
#endif

/* enable testing buffer load functions */
#ifndef USE_CERT_BUFFERS_2048
    #define USE_CERT_BUFFERS_2048
#endif
#include <wolfssl/certs_test.h>


typedef struct testVector {
    const char* input;
    const char* output;
    size_t inLen;
    size_t outLen;

} testVector;


/*----------------------------------------------------------------------------*
 | Constants
 *----------------------------------------------------------------------------*/

#define TEST_SUCCESS    (1)
#define TEST_FAIL       (0)

#define testingFmt "   %s:"
#define resultFmt  " %s\n"
static const char* passed = "passed";
static const char* failed = "failed";

#if !defined(NO_FILESYSTEM) && !defined(NO_CERTS)
    static const char* bogusFile  =
    #ifdef _WIN32
        "NUL"
    #else
        "/dev/null"
    #endif
    ;
#endif

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
    TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_1_server_method);
    TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_1_client_method);
#endif
#ifndef NO_WOLFSSL_SERVER
    TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_2_server_method);
#endif
#ifndef NO_WOLFSSL_CLIENT
    TEST_VALID_METHOD_ALLOCATOR(wolfTLSv1_2_client_method);
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
    #else
        const int versions[]  =  { WOLFSSL_TLSV1_2 };
    #endif

    AssertTrue(wolfSSL_Init());
    ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
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
      AssertNotNull(strcmp("EVP_AES_128_CBC", wolfSSL_EVP_get_cipherbynid(419)));
      AssertNotNull(strcmp("EVP_AES_192_CBC", wolfSSL_EVP_get_cipherbynid(423)));
      AssertNotNull(strcmp("EVP_AES_256_CBC", wolfSSL_EVP_get_cipherbynid(427)));
      AssertNotNull(strcmp("EVP_AES_128_CTR", wolfSSL_EVP_get_cipherbynid(904)));
      AssertNotNull(strcmp("EVP_AES_192_CTR", wolfSSL_EVP_get_cipherbynid(905)));
      AssertNotNull(strcmp("EVP_AES_256_CTR", wolfSSL_EVP_get_cipherbynid(906)));
      AssertNotNull(strcmp("EVP_AES_128_ECB", wolfSSL_EVP_get_cipherbynid(418)));
      AssertNotNull(strcmp("EVP_AES_192_ECB", wolfSSL_EVP_get_cipherbynid(422)));
      AssertNotNull(strcmp("EVP_AES_256_ECB", wolfSSL_EVP_get_cipherbynid(426)));
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
    !defined(NO_WOLFSSL_SERVER) && !defined(NO_WOLFSSL_CLIENT)
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

#ifndef NO_WOLFSSL_SERVER
static THREAD_RETURN WOLFSSL_THREAD test_server_nofail(void* args)
{
    SOCKET_T sockfd = 0;
    SOCKET_T clientfd = 0;
    word16 port;

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
    if (((func_args*)args)->callbacks != NULL &&
        ((func_args*)args)->callbacks->method != NULL) {
        method = ((func_args*)args)->callbacks->method();
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

#ifdef OPENSSL_EXTRA
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

    ssl = wolfSSL_new(ctx);
    tcp_accept(&sockfd, &clientfd, (func_args*)args, port, 0, 0, 0, 0, 1);
    CloseSocket(sockfd);

    if (wolfSSL_set_fd(ssl, clientfd) != WOLFSSL_SUCCESS) {
        /*err_sys("SSL_set_fd failed");*/
        goto done;
    }

#ifdef NO_PSK
    #if !defined(NO_FILESYSTEM) && !defined(NO_DH)
        wolfSSL_SetTmpDH_file(ssl, dhParamFile, WOLFSSL_FILETYPE_PEM);
    #elif !defined(NO_DH)
        SetDH(ssl);  /* will repick suites with DHE, higher priority than PSK */
    #endif
#endif

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
        char buffer[WOLFSSL_MAX_ERROR_SZ];
        printf("error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
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

done:
    wolfSSL_shutdown(ssl);
    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);

    CloseSocket(clientfd);
    ((func_args*)args)->return_code = TEST_SUCCESS;

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
#endif /* !NO_WOLFSSL_SERVER */

static void test_client_nofail(void* args)
{
    SOCKET_T sockfd = 0;

    WOLFSSL_METHOD*  method  = 0;
    WOLFSSL_CTX*     ctx     = 0;
    WOLFSSL*         ssl     = 0;

    char msg[64] = "hello wolfssl!";
    char reply[1024];
    int  input;
    int  msgSz = (int)XSTRLEN(msg);
    int  ret, err = 0;

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

    ((func_args*)args)->return_code = TEST_FAIL;
    if (((func_args*)args)->callbacks != NULL &&
        ((func_args*)args)->callbacks->method != NULL) {
        method = ((func_args*)args)->callbacks->method();
    }
    else {
        method = wolfSSLv23_client_method();
    }
    ctx = wolfSSL_CTX_new(method);

#ifdef OPENSSL_EXTRA
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

    ssl = wolfSSL_new(ctx);
    tcp_connect(&sockfd, wolfSSLIP, ((func_args*)args)->signal->port,
                0, 0, ssl);
    if (wolfSSL_set_fd(ssl, sockfd) != WOLFSSL_SUCCESS) {
        /*err_sys("SSL_set_fd failed");*/
        goto done2;
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
        char buffer[WOLFSSL_MAX_ERROR_SZ];
        printf("error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
        /*err_sys("SSL_connect failed");*/
        goto done2;
    }

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

done2:
    wolfSSL_free(ssl);
    wolfSSL_CTX_free(ctx);

    CloseSocket(sockfd);
    ((func_args*)args)->return_code = TEST_SUCCESS;

#ifdef WOLFSSL_TIRTOS
    fdCloseSession(Task_self());
#endif

    return;
}

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

#ifdef OPENSSL_EXTRA
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
        char buffer[WOLFSSL_MAX_ERROR_SZ];
        printf("error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
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

#ifdef OPENSSL_EXTRA
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
        char buffer[WOLFSSL_MAX_ERROR_SZ];
        printf("error = %d, %s\n", err, wolfSSL_ERR_error_string(err, buffer));
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
    test_client_nofail(&client_args);
    join_thread(serverThread);

    AssertTrue(client_args.return_code);
    AssertTrue(server_args.return_code);

    FreeTcpReady(&ready);

#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif

#endif
}


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
    AssertIntEQ(1, 0 == protoSz);
    AssertIntEQ(1, NULL == proto);
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
#if defined(OPENSSL_EXTRA) && (defined(KEEP_PEER_CERT) || defined(SESSION_CERTS)) \
    && (defined(HAVE_LIGHTY) || defined(WOLFSSL_MYSQL_COMPATIBLE)) || defined(WOLFSSL_HAPROXY)
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
#endif /* OPENSSL_EXTRA */
#endif /* !NO_CERTS */
}


/* Testing functions dealing with PKCS12 parsing out X509 certs */
static void test_wolfSSL_PKCS12(void)
{
    /* .p12 file is encrypted with DES3 */
#if defined(OPENSSL_EXTRA) && !defined(NO_DES3) && !defined(NO_FILESYSTEM) && \
    !defined(NO_ASN) && !defined(NO_PWDBASED) && !defined(NO_RSA)
    byte buffer[5300];
    char file[] = "./certs/test-servercert.p12";
    FILE *f;
    int  bytes, ret;
    WOLFSSL_BIO      *bio;
    WOLFSSL_EVP_PKEY *pkey;
    WC_PKCS12        *pkcs12;
    WOLFSSL_X509     *cert;
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
    BIO_free(bio);
    PKCS12_free(pkcs12);
    sk_X509_free(ca);

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
    #else
        const int versions[]  = { WOLFSSL_TLSV1_2 };
    #endif

    failFlag = WOLFSSL_SUCCESS;

    AssertTrue(wolfSSL_Init());
    ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());

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
        ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
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
        ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
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
#ifndef NO_AES
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
    byte    key24[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
    };
    byte    key32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
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

    ret = wc_AesSetKey(&aes, key16, (word32) sizeof(key16) / sizeof(byte),
                                                        iv, AES_ENCRYPTION);
    if (ret == 0) {
        ret = wc_AesSetKey (&aes, key24, (word32) sizeof(key24) / sizeof(byte),
                                                           iv, AES_ENCRYPTION);
    }
    if (ret == 0) {
        ret = wc_AesSetKey (&aes, key32, (word32) sizeof(key32) / sizeof(byte),
                                                           iv, AES_ENCRYPTION);
    }
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
#if !defined(NO_AES) && defined(HAVE_AES_CBC) && defined(HAVE_AES_DECRYPT)
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
#if !defined(NO_AES) && defined(WOLFSSL_AES_COUNTER)
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
    byte    key16[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
    byte    key24[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
    };
    byte    key32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
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

    ret = wc_AesGcmSetKey(&aes, key16, sizeof(key16)/sizeof(byte));
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, key24, sizeof(key24)/sizeof(byte));
    }
    if (ret == 0) {
        ret = wc_AesGcmSetKey(&aes, key32, sizeof(key32)/sizeof(byte));
    }

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
#if !defined(NO_AES) && defined(HAVE_AESGCM)

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
    byte    key24[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
    };
    byte    key32[] =
    {
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
        0x38, 0x39, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66
    };
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

    ret = wc_GmacSetKey(&gmac, key16, sizeof(key16)/sizeof(byte));
    if (ret == 0) {
        ret = wc_GmacSetKey(&gmac, key24, sizeof(key24)/sizeof(byte));
    }
    if (ret == 0) {
        ret = wc_GmacSetKey(&gmac, key32, sizeof(key32)/sizeof(byte));
    }

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
    const byte key16[] =
    {
        0x89, 0xc9, 0x49, 0xe9, 0xc8, 0x04, 0xaf, 0x01,
        0x4d, 0x56, 0x04, 0xb3, 0x94, 0x59, 0xf2, 0xc8
    };
    byte    key24[] =
    {
        0x41, 0xc5, 0xda, 0x86, 0x67, 0xef, 0x72, 0x52,
        0x20, 0xff, 0xe3, 0x9a, 0xe0, 0xac, 0x59, 0x0a,
        0xc9, 0xfc, 0xa7, 0x29, 0xab, 0x60, 0xad, 0xa0
    };
   byte    key32[] =
    {
        0x78, 0xdc, 0x4e, 0x0a, 0xaf, 0x52, 0xd9, 0x35,
        0xc3, 0xc0, 0x1e, 0xea, 0x57, 0x42, 0x8f, 0x00,
        0xca, 0x1f, 0xd4, 0x75, 0xf5, 0xda, 0x86, 0xa4,
        0x9c, 0x8d, 0xd7, 0x3d, 0x68, 0xc8, 0xe2, 0x23
    };
    const byte authIn[] =
    {
        0x82, 0xad, 0xcd, 0x63, 0x8d, 0x3f, 0xa9, 0xd9,
        0xf3, 0xe8, 0x41, 0x00, 0xd6, 0x1e, 0x07, 0x77
    };
    const byte authIn2[] =
    {
       0x8b, 0x5c, 0x12, 0x4b, 0xef, 0x6e, 0x2f, 0x0f,
       0xe4, 0xd8, 0xc9, 0x5c, 0xd5, 0xfa, 0x4c, 0xf1
    };
    const byte authIn3[] =
    {
        0xb9, 0x6b, 0xaa, 0x8c, 0x1c, 0x75, 0xa6, 0x71,
        0xbf, 0xb2, 0xd0, 0x8d, 0x06, 0xbe, 0x5f, 0x36
    };
    const byte tag1[] = /* Known. */
    {
        0x88, 0xdb, 0x9d, 0x62, 0x17, 0x2e, 0xd0, 0x43,
        0xaa, 0x10, 0xf1, 0x6d, 0x22, 0x7d, 0xc4, 0x1b
    };
    const byte tag2[] = /* Known */
    {
        0x20, 0x4b, 0xdb, 0x1b, 0xd6, 0x21, 0x54, 0xbf,
        0x08, 0x92, 0x2a, 0xaa, 0x54, 0xee, 0xd7, 0x05
    };
    const byte tag3[] = /* Known */
    {
        0x3e, 0x5d, 0x48, 0x6a, 0xa2, 0xe3, 0x0b, 0x22,
        0xe0, 0x40, 0xb8, 0x57, 0x23, 0xa0, 0x6e, 0x76
    };
    const byte iv[] =
    {
        0xd1, 0xb1, 0x04, 0xc8, 0x15, 0xbf, 0x1e, 0x94,
        0xe2, 0x8c, 0x8f, 0x16
    };
    const byte iv2[] =
    {
        0x05, 0xad, 0x13, 0xa5, 0xe2, 0xc2, 0xab, 0x66,
        0x7e, 0x1a, 0x6f, 0xbc
    };
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

    ret = wc_GmacSetKey(&gmac, key16, sizeof(key16));
    if (ret == 0) {
        ret = wc_GmacUpdate(&gmac, iv, sizeof(iv), authIn, sizeof(authIn),
                                                    tagOut, sizeof(tag1));
        if (ret == 0) {
            ret = XMEMCMP(tag1, tagOut, sizeof(tag1));
        }
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
    }

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
    const char* key = "\x01\x23\x45\x67\x89\xab\xcd\xef";
    const char* input = "\x01\x23\x45\x67\x89\xab\xcd\xef";
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
        ret = wc_Arc4SetKey(&enc, (byte*)key, sizeof(key)/sizeof(char));
    }
    if (ret == 0) {
        ret = wc_Arc4SetKey(&dec, (byte*)key, sizeof(key)/sizeof(char));
    }
    if (ret == 0) {
        ret = wc_Arc4Process(&enc, cipher, (byte*)input,
                                    (word32)(sizeof(input)/sizeof(char)));
    }
    if (ret == 0) {
        ret = wc_Arc4Process(&dec, plain, cipher,
                                    (word32)(sizeof(input)/sizeof(char)));
        if (ret != 0 || XMEMCMP(plain, input,
                            (unsigned int)(sizeof(input)/sizeof(char)))) {
            ret = WOLFSSL_FATAL_ERROR;
        } else {
            ret = 0;
        }
    }

    /* Bad args. */
    if (ret == 0) {
        ret = wc_Arc4Process(NULL, plain, cipher,
                                (word32)(sizeof(input)/sizeof(char)));
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Arc4Process(&dec, NULL, cipher,
                                (word32)(sizeof(input)/sizeof(char)));
        }
        if (ret == BAD_FUNC_ARG) {
            ret = wc_Arc4Process(&dec, plain, NULL,
                                (word32)(sizeof(input)/sizeof(char)));
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
            ret = wc_MakeRsaKey(&genKey, 1024, WC_RSA_EXPONENT, &rng);
            if (ret == 0 && wc_FreeRsaKey(&genKey) != 0) {
                ret = WOLFSSL_FATAL_ERROR;
            }
        }
    }
    #ifndef HAVE_USER_RSA
        /* Test bad args. */
        if (ret == 0) {
            ret = wc_MakeRsaKey(NULL, 1024, WC_RSA_EXPONENT, &rng);
            if (ret == BAD_FUNC_ARG) {
                ret = wc_MakeRsaKey(&genKey, 1024, WC_RSA_EXPONENT, NULL);
            }
            if (ret == BAD_FUNC_ARG) {
                /* e < 3 */
                ret = wc_MakeRsaKey(&genKey, 1024, 2, &rng);
            }
            if (ret == BAD_FUNC_ARG) {
                /* e & 1 == 0 */
                ret = wc_MakeRsaKey(&genKey, 1024, 6, &rng);
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
            ret = wc_MakeRsaKey(NULL, 1024, WC_RSA_EXPONENT, &rng);
            if (ret == USER_CRYPTO_ERROR) {
                ret = wc_MakeRsaKey(&genKey, 1024, WC_RSA_EXPONENT, NULL);
            }
            if (ret == USER_CRYPTO_ERROR) {
                /* e < 3 */
                ret = wc_MakeRsaKey(&genKey, 1024, 2, &rng);
            }
            if (ret == USER_CRYPTO_ERROR) {
                /* e & 1 == 0 */
                ret = wc_MakeRsaKey(&genKey, 1024, 6, &rng);
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
        ret = wc_MakeRsaKey(&genKey, 1024, WC_RSA_EXPONENT, &rng);
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
#if !defined(NO_RSA) && defined(WOLFSSL_KEY_GEN)
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
        ret = wc_MakeRsaKey(&key, 1024, WC_RSA_EXPONENT, &rng);
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
        ret = wc_MakeRsaKey(&key, 1024, WC_RSA_EXPONENT, &rng);
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
        ret = wc_MakeRsaKey(&key, 1024, WC_RSA_EXPONENT, &rng);
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
        ret = wc_MakeRsaKey(&key, 1024, WC_RSA_EXPONENT, &rng);
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
        ret = wc_MakeRsaKey(&key, 1024, WC_RSA_EXPONENT, &rng);
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
        ret = wc_MakeRsaKey(&key, FOURK_BUF, WC_RSA_EXPONENT, &rng);
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
        ret = wc_MakeRsaKey(&key, 1024, WC_RSA_EXPONENT, &rng);
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

    ret = wc_AesCcmSetKey(&aes, key16, sizeof(key16));
    if (ret == 0) {
        ret = wc_AesCcmSetKey(&aes, key24, sizeof(key24));
        if (ret == 0) {
            ret = wc_AesCcmSetKey(&aes, key32, sizeof(key32));
        }
    }

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
#ifdef HAVE_AESCCM
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
    size_t inlen = XSTRLEN(input);
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



/*----------------------------------------------------------------------------*
 | Compatibility Tests
 *----------------------------------------------------------------------------*/


static void test_wolfSSL_DES(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_DES3)
    const_DES_cblock myDes;
    DES_key_schedule key;
    word32 i;

    printf(testingFmt, "wolfSSL_DES()");

    DES_check_key(1);
    DES_set_key(&myDes, &key);

    /* check, check of odd parity */
    XMEMSET(key, 4, sizeof(DES_key_schedule));  key[0] = 3; /*set even parity*/
    XMEMSET(myDes, 5, sizeof(const_DES_cblock));
    AssertIntEQ(DES_set_key_checked(&myDes, &key), -1);
    AssertIntNE(key[0], myDes[0]); /* should not have copied over key */

    /* set odd parity for success case */
    key[0] = 4;
    AssertIntEQ(DES_set_key_checked(&myDes, &key), 0);
    for (i = 0; i < sizeof(DES_key_schedule); i++) {
        AssertIntEQ(key[i], myDes[i]);
    }

    /* check weak key */
    XMEMSET(key, 1, sizeof(DES_key_schedule));
    XMEMSET(myDes, 5, sizeof(const_DES_cblock));
    AssertIntEQ(DES_set_key_checked(&myDes, &key), -2);
    AssertIntNE(key[0], myDes[0]); /* should not have copied over key */

    /* now do unchecked copy of a weak key over */
    DES_set_key_unchecked(&myDes, &key);
    /* compare arrays, should be the same */
    for (i = 0; i < sizeof(DES_key_schedule); i++) {
        AssertIntEQ(key[i], myDes[i]);
    }

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
    AssertTrue(SSL_CTX_use_certificate_file(ctx, svrCertFile, WOLFSSL_FILETYPE_PEM));
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, WOLFSSL_FILETYPE_PEM));
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
    AssertIntEQ(XMEMCMP(buf, "Aug 11 20:07:37 2016 GMT", sizeof(buf) - 1), 0);

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


static void test_wolfSSL_private_keys(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)
    WOLFSSL*     ssl;
    WOLFSSL_CTX* ctx;
    EVP_PKEY* pkey = NULL;

    printf(testingFmt, "wolfSSL_private_keys()");

    OpenSSL_add_all_digests();
    OpenSSL_add_all_algorithms();

    AssertNotNull(ctx = SSL_CTX_new(wolfSSLv23_server_method()));
    AssertTrue(SSL_CTX_use_certificate_file(ctx, svrCertFile, WOLFSSL_FILETYPE_PEM));
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, WOLFSSL_FILETYPE_PEM));
    AssertNotNull(ssl = SSL_new(ctx));

    AssertIntEQ(wolfSSL_check_private_key(ssl), WOLFSSL_SUCCESS);

#ifdef USE_CERT_BUFFERS_2048
    {
    const unsigned char* server_key = (const unsigned char*)server_key_der_2048;

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
    }
#endif


    EVP_PKEY_free(pkey);
    SSL_free(ssl); /* frees x509 also since loaded into ssl */
    SSL_CTX_free(ctx);

    /* test existence of no-op macros in wolfssl/openssl/ssl.h */
    CONF_modules_free();
    ENGINE_cleanup();
    CONF_modules_unload();

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) */
}


static void test_wolfSSL_PEM_PrivateKey(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)   && \
       (defined(WOLFSSL_KEY_GEN) || defined(WOLFSSL_CERT_GEN)) && \
       defined(USE_CERT_BUFFERS_2048)
    const unsigned char* server_key = (const unsigned char*)server_key_der_2048;
    EVP_PKEY* pkey = NULL;
    BIO*      bio;

    printf(testingFmt, "wolfSSL_PEM_PrivateKey()");

    bio = wolfSSL_BIO_new(wolfSSL_BIO_s_mem());
    AssertNotNull(bio);

    AssertNotNull(wolfSSL_d2i_PrivateKey(EVP_PKEY_RSA, &pkey,
            &server_key, (long)sizeof_server_key_der_2048));
    AssertIntEQ(PEM_write_bio_PrivateKey(bio, pkey, NULL, NULL, 0, NULL, NULL),
            WOLFSSL_SUCCESS);

    BIO_free(bio);
    EVP_PKEY_free(pkey);

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
    byte buffer[5300];
    BIO* bio;
    int  bytes;
    BUF_MEM* ptr = NULL;

    printf(testingFmt, "wolfSSL_crtl()");

    bytes = sizeof(buffer);
    bio = BIO_new_mem_buf((void*)buffer, bytes);
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
    AssertIntEQ((int)SSL_CTX_add_extra_chain_cert(ctx, x509), WOLFSSL_SUCCESS);

    AssertNull(SSL_CTX_get_default_passwd_cb(ctx));
    AssertNull(SSL_CTX_get_default_passwd_cb_userdata(ctx));

    SSL_CTX_free(ctx);
    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
             !defined(NO_FILESYSTEM) && !defined(NO_RSA) */
}


static void test_wolfSSL_ERR_peek_last_error_line(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && defined(DEBUG_WOLFSSL) && \
       !defined(NO_OLD_TLS) && defined(HAVE_IO_TESTS_DEPENDENCIES)
    tcp_ready ready;
    func_args client_args;
    func_args server_args;
#ifndef SINGLE_THREADED
    THREAD_TYPE serverThread;
#endif
    callback_functions client_cb;
    callback_functions server_cb;
    int         line = 0;
    const char* file = NULL;

    printf(testingFmt, "wolfSSL_ERR_peek_last_error_line()");

    /* create a failed connection and inspect the error */
#ifdef WOLFSSL_TIRTOS
    fdOpenSession(Task_self());
#endif
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

#ifndef SINGLE_THREADED
    start_thread(test_server_nofail, &server_args, &serverThread);
    wait_tcp_ready(&server_args);
    test_client_nofail(&client_args);
    join_thread(serverThread);
#endif

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


static void test_wolfSSL_X509_STORE_set_flags(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)

    X509_STORE* store;
    X509* x509;

    printf(testingFmt, "wolfSSL_ERR_peek_last_error_line()");
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
    time_t ctime;

    printf(testingFmt, "wolfSSL_X509_set_time()");
    AssertNotNull(ctx = wolfSSL_X509_STORE_CTX_new());
    ctime = 365*24*60*60;
    wolfSSL_X509_STORE_CTX_set_time(ctx, 0, ctime);
    AssertTrue(
      (ctx->param->flags & WOLFSSL_USE_CHECK_TIME) == WOLFSSL_USE_CHECK_TIME);
    AssertTrue(ctx->param->check_time == ctime);
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
    #if defined(OPENSSL_EXTRA) && !defined(NO_RSA)\
    && defined(USE_CERT_BUFFERS_2048) && !defined(NO_ASN)
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

static void test_wolfSSL_BN(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_ASN)
    BIGNUM* a;
    BIGNUM* b;
    BIGNUM* c;
    BIGNUM* d;
    ASN1_INTEGER ai;
    unsigned char value[1];

    printf(testingFmt, "wolfSSL_BN()");

    AssertNotNull(b = BN_new());
    AssertNotNull(c = BN_new());
    AssertNotNull(d = BN_new());

    value[0] = 0x03;

    /* at the moment hard setting since no set function */
    ai.data[0] = 0x02; /* tag for ASN_INTEGER */
    ai.data[1] = 0x01; /* length of integer */
    ai.data[2] = value[0];

    AssertNotNull(a = ASN1_INTEGER_to_BN(&ai, NULL));

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
    AssertIntEQ((int)(value[0] & 0x04), 4);

    /* BN_mod_inverse test */
    value[0] = 0;
    BIGNUM *r = BN_new();
    BIGNUM *val = BN_mod_inverse(r,b,c,NULL);
    AssertIntEQ(BN_bn2bin(r, value), 1);
    AssertIntEQ((int)(value[0] & 0x03), 3);
    BN_free(val);

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


static void test_wolfSSL_set_options(void)
{
    #if defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
       !defined(NO_FILESYSTEM) && !defined(NO_RSA)
    SSL*     ssl;
    SSL_CTX* ctx;

    printf(testingFmt, "wolfSSL_set_options()");

    AssertNotNull(ctx = SSL_CTX_new(wolfSSLv23_server_method()));
    AssertTrue(SSL_CTX_use_certificate_file(ctx, svrCertFile, WOLFSSL_FILETYPE_PEM));
    AssertTrue(SSL_CTX_use_PrivateKey_file(ctx, svrKeyFile, WOLFSSL_FILETYPE_PEM));
    AssertNotNull(ssl = SSL_new(ctx));

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
    byte buffer[5300];
    FILE *f;
    int  bytes;
    X509* x509;
    BIO*  bio = NULL;

    printf(testingFmt, "wolfSSL_PEM_read_bio()");

    AssertNotNull(f = fopen(cliCertFile, "rb"));
    bytes = (int)fread(buffer, 1, sizeof(buffer), f);
    fclose(f);

    AssertNull(x509 = PEM_read_bio_X509_AUX(bio, NULL, NULL, NULL));
    AssertNotNull(bio = BIO_new_mem_buf((void*)buffer, bytes));
    AssertNotNull(x509 = PEM_read_bio_X509_AUX(bio, NULL, NULL, NULL));
    AssertIntEQ((int)BIO_set_fd(bio, 0, BIO_NOCLOSE), 1);

    BIO_free(bio);
    X509_free(x509);

    printf(resultFmt, passed);
    #endif /* defined(OPENSSL_EXTRA) && !defined(NO_CERTS) && \
             !defined(NO_FILESYSTEM) && !defined(NO_RSA) */
}


static void test_wolfSSL_BIO(void)
{
    #if defined(OPENSSL_EXTRA)
    byte buffer[20];
    BIO* bio1;
    BIO* bio2;
    BIO* bio3;
    char* bufPt;
    int i;

    printf(testingFmt, "wolfSSL_BIO()");

    for (i = 0; i < 20; i++) {
        buffer[i] = i;
    }

    /* Creating and testing type BIO_s_bio */
    AssertNotNull(bio1 = BIO_new(BIO_s_bio()));
    AssertNotNull(bio2 = BIO_new(BIO_s_bio()));
    AssertNotNull(bio3 = BIO_new(BIO_s_bio()));

    /* read/write before set up */
    AssertIntEQ(BIO_read(bio1, buffer, 2),  WOLFSSL_BIO_UNSET);
    AssertIntEQ(BIO_write(bio1, buffer, 2), WOLFSSL_BIO_UNSET);

    AssertIntEQ(BIO_set_write_buf_size(bio1, 20), WOLFSSL_SUCCESS);
    AssertIntEQ(BIO_set_write_buf_size(bio2, 8),  WOLFSSL_SUCCESS);
    AssertIntEQ(BIO_make_bio_pair(bio1, bio2),    WOLFSSL_SUCCESS);

    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 10), 10);
    XMEMCPY(bufPt, buffer, 10);
    AssertIntEQ(BIO_write(bio1, buffer + 10, 10), 10);
    /* write buffer full */
    AssertIntEQ(BIO_write(bio1, buffer, 10), WOLFSSL_BIO_ERROR);
    AssertIntEQ(BIO_flush(bio1), WOLFSSL_SUCCESS);
    AssertIntEQ((int)BIO_ctrl_pending(bio1), 0);

    /* write the other direction with pair */
    AssertIntEQ((int)BIO_nwrite(bio2, &bufPt, 10), 8);
    XMEMCPY(bufPt, buffer, 8);
    AssertIntEQ(BIO_write(bio2, buffer, 10), WOLFSSL_BIO_ERROR);

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
    XMEMCPY(bufPt, buffer, 20);
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
        AssertIntEQ(bufPt[i], buffer[4 + i]);
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
    XMEMCPY(bufPt, buffer, 20);

    /* test reset on data in bio1 write buffer */
    AssertIntEQ(BIO_reset(bio1), 0);
    AssertIntEQ((int)BIO_ctrl_pending(bio3), 0);
    AssertIntEQ(BIO_nread(bio3, &bufPt, 3), WOLFSSL_BIO_ERROR);
    AssertIntEQ(BIO_nwrite(bio1, &bufPt, 20), 20);
    XMEMCPY(bufPt, buffer, 20);
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
    }
    #endif /* !defined(NO_FILESYSTEM) */

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
    const int min  = 60;
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
    t = (time_t)30 * year + 45 * day + 20 * hour + 30 * min + 7 * day;
    offset_day = 7;
    offset_sec = 45 * min;
    /* offset_sec = -45 * min;*/
    asn_time = wolfSSL_ASN1_TIME_adj(s, t, offset_day, offset_sec);
    AssertTrue(asn_time->data[0] == asn_utc_time);
    XSTRNCPY(date_str,(const char*) &asn_time->data+2,13);
    AssertIntEQ(0, XMEMCMP(date_str, "000222211500Z", 13));

    /* negative offset */
    offset_sec = -45 * min;
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
    offset_sec = 10 * min;
    asn_time = wolfSSL_ASN1_TIME_adj(s, t, offset_day, offset_sec);
    AssertTrue(asn_time->data[0] == asn_gen_time);
    XSTRNCPY(date_str,(const char*) &asn_time->data+2, 15);
    AssertIntEQ(0, XMEMCMP(date_str, "20550313091000Z", 15));
    
    XFREE(s,NULL,DYNAMIC_TYPE_OPENSSL);
    XMEMSET(date_str, 0, sizeof(date_str));
#endif /* !TIME_T_NOT_LONG && !NO_64BIT */

    /* if WOLFSSL_ASN1_TIME struct is not allocated */
    s = NULL;

    t = (time_t)30 * year + 45 * day + 20 * hour + 30 * min + 15 + 7 * day;
    offset_day = 7;
    offset_sec = 45 * min;
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
#ifdef WOLFSSL_SEND_HRR_COOKIE
static byte fixedKey[WC_SHA384_DIGEST_SIZE] = { 0, };
#endif
#ifdef WOLFSSL_EARLY_DATA
static const char earlyData[] = "Early Data";
static       char earlyDataBuffer[1];
#endif

static int test_tls13_apis(void)
{
    int          ret = 0;
    WOLFSSL_CTX* clientTls12Ctx;
    WOLFSSL*     clientTls12Ssl;
    WOLFSSL_CTX* serverTls12Ctx;
    WOLFSSL*     serverTls12Ssl;
    WOLFSSL_CTX* clientCtx;
    WOLFSSL*     clientSsl;
    WOLFSSL_CTX* serverCtx;
    WOLFSSL*     serverSsl;
#ifndef NO_CERTS
    const char*  ourCert = svrCertFile;
    const char*  ourKey  = svrKeyFile;
#endif
#ifdef WOLFSSL_EARLY_DATA
    int          outSz;
#endif

    clientTls12Ctx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
    clientTls12Ssl = wolfSSL_new(clientTls12Ctx);
    serverTls12Ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method());
#ifndef NO_CERTS
    wolfSSL_CTX_use_certificate_chain_file(serverTls12Ctx, ourCert);
    wolfSSL_CTX_use_PrivateKey_file(serverTls12Ctx, ourKey, WOLFSSL_FILETYPE_PEM);
#endif
    serverTls12Ssl = wolfSSL_new(serverTls12Ctx);

    clientCtx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
    clientSsl = wolfSSL_new(clientCtx);
    serverCtx = wolfSSL_CTX_new(wolfTLSv1_3_server_method());
#ifndef NO_CERTS
    wolfSSL_CTX_use_certificate_chain_file(serverCtx, ourCert);
    wolfSSL_CTX_use_PrivateKey_file(serverCtx, ourKey, WOLFSSL_FILETYPE_PEM);
#endif
    serverSsl = wolfSSL_new(serverCtx);

#ifdef WOLFSSL_SEND_HRR_COOKIE
    AssertIntEQ(wolfSSL_send_hrr_cookie(NULL, NULL, 0), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_send_hrr_cookie(clientSsl, NULL, 0), SIDE_ERROR);
    AssertIntEQ(wolfSSL_send_hrr_cookie(serverTls12Ssl, NULL, 0), BAD_FUNC_ARG);

    AssertIntEQ(wolfSSL_send_hrr_cookie(serverSsl, NULL, 0), WOLFSSL_SUCCESS);
    AssertIntEQ(wolfSSL_send_hrr_cookie(serverSsl, fixedKey, sizeof(fixedKey)),
                WOLFSSL_SUCCESS);
#endif

#ifdef HAVE_ECC
    AssertIntEQ(wolfSSL_UseKeyShare(NULL, WOLFSSL_ECC_SECP256R1), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_UseKeyShare(serverSsl, WOLFSSL_ECC_SECP256R1),
                SIDE_ERROR);
    AssertIntEQ(wolfSSL_UseKeyShare(clientTls12Ssl, WOLFSSL_ECC_SECP256R1),
                WOLFSSL_SUCCESS);
    AssertIntEQ(wolfSSL_UseKeyShare(clientSsl, WOLFSSL_ECC_SECP256R1),
                WOLFSSL_SUCCESS);
#else
    AssertIntEQ(wolfSSL_UseKeyShare(NULL, WOLFSSL_ECC_SECP256R1), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_UseKeyShare(serverSsl, WOLFSSL_ECC_SECP256R1),
                SIDE_ERROR);
    AssertIntEQ(wolfSSL_UseKeyShare(clientTls12Ssl, WOLFSSL_ECC_SECP256R1),
                NOT_COMPILED_IN);
    AssertIntEQ(wolfSSL_UseKeyShare(clientSsl, WOLFSSL_ECC_SECP256R1),
                NOT_COMPILED_IN);
#endif

    AssertIntEQ(wolfSSL_NoKeyShares(NULL), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_NoKeyShares(serverSsl), SIDE_ERROR);
    AssertIntEQ(wolfSSL_NoKeyShares(clientTls12Ssl), WOLFSSL_SUCCESS);
    AssertIntEQ(wolfSSL_NoKeyShares(clientSsl), WOLFSSL_SUCCESS);

    AssertIntEQ(wolfSSL_CTX_no_ticket_TLSv13(NULL), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_CTX_no_ticket_TLSv13(clientCtx), SIDE_ERROR);
    AssertIntEQ(wolfSSL_CTX_no_ticket_TLSv13(serverTls12Ctx), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_CTX_no_ticket_TLSv13(serverCtx), 0);

    AssertIntEQ(wolfSSL_no_ticket_TLSv13(NULL), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_no_ticket_TLSv13(clientSsl), SIDE_ERROR);
    AssertIntEQ(wolfSSL_no_ticket_TLSv13(serverTls12Ssl), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_no_ticket_TLSv13(serverSsl), 0);

    AssertIntEQ(wolfSSL_CTX_no_dhe_psk(NULL), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_CTX_no_dhe_psk(clientTls12Ctx), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_CTX_no_dhe_psk(serverCtx), 0);
    AssertIntEQ(wolfSSL_CTX_no_dhe_psk(clientCtx), 0);

    AssertIntEQ(wolfSSL_no_dhe_psk(NULL), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_no_dhe_psk(clientTls12Ssl), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_no_dhe_psk(serverSsl), 0);
    AssertIntEQ(wolfSSL_no_dhe_psk(clientSsl), 0);

    AssertIntEQ(wolfSSL_update_keys(NULL), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_update_keys(clientTls12Ssl), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_update_keys(serverSsl), BUILD_MSG_ERROR);
    AssertIntEQ(wolfSSL_update_keys(clientSsl), BUILD_MSG_ERROR);

#if !defined(NO_CERTS) && defined(WOLFSSL_POST_HANDSHAKE_AUTH)
    AssertIntEQ(wolfSSL_CTX_allow_post_handshake_auth(NULL), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_CTX_allow_post_handshake_auth(serverCtx), SIDE_ERROR);
    AssertIntEQ(wolfSSL_CTX_allow_post_handshake_auth(clientTls12Ctx),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_CTX_allow_post_handshake_auth(clientCtx), 0);

    AssertIntEQ(wolfSSL_allow_post_handshake_auth(NULL), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_allow_post_handshake_auth(serverSsl), SIDE_ERROR);
    AssertIntEQ(wolfSSL_allow_post_handshake_auth(clientTls12Ssl),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_allow_post_handshake_auth(clientSsl), 0);

    AssertIntEQ(wolfSSL_request_certificate(NULL), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_request_certificate(clientSsl), SIDE_ERROR);
    AssertIntEQ(wolfSSL_request_certificate(serverTls12Ssl),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_request_certificate(serverSsl), NOT_READY_ERROR);
#endif

#ifdef WOLFSSL_EARLY_DATA
    AssertIntEQ(wolfSSL_CTX_set_max_early_data(NULL, 0), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_CTX_set_max_early_data(clientCtx, 0), SIDE_ERROR);
    AssertIntEQ(wolfSSL_CTX_set_max_early_data(serverTls12Ctx, 0),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_CTX_set_max_early_data(serverCtx, 0), 0);

    AssertIntEQ(wolfSSL_set_max_early_data(NULL, 0), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_set_max_early_data(clientSsl, 0), SIDE_ERROR);
    AssertIntEQ(wolfSSL_set_max_early_data(serverTls12Ssl, 0), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_set_max_early_data(serverSsl, 0), 0);

    AssertIntEQ(wolfSSL_write_early_data(NULL, earlyData, sizeof(earlyData),
                                         &outSz), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_write_early_data(clientSsl, NULL, sizeof(earlyData),
                                         &outSz), BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_write_early_data(clientSsl, earlyData, -1, &outSz),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_write_early_data(clientSsl, earlyData,
                                         sizeof(earlyData), NULL),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_write_early_data(serverSsl, earlyData,
                                         sizeof(earlyData), &outSz),
                SIDE_ERROR);
    AssertIntEQ(wolfSSL_write_early_data(clientTls12Ssl, earlyData,
                                         sizeof(earlyData), &outSz),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_write_early_data(clientSsl, earlyData,
                                         sizeof(earlyData), &outSz),
                WOLFSSL_FATAL_ERROR);

    AssertIntEQ(wolfSSL_read_early_data(NULL, earlyDataBuffer,
                                        sizeof(earlyDataBuffer), &outSz),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_read_early_data(serverSsl, NULL,
                                        sizeof(earlyDataBuffer), &outSz),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_read_early_data(serverSsl, earlyDataBuffer, -1, &outSz),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_read_early_data(serverSsl, earlyDataBuffer,
                                        sizeof(earlyDataBuffer), NULL),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_read_early_data(clientSsl, earlyDataBuffer,
                                        sizeof(earlyDataBuffer), &outSz),
                SIDE_ERROR);
    AssertIntEQ(wolfSSL_read_early_data(serverTls12Ssl, earlyDataBuffer,
                                        sizeof(earlyDataBuffer), &outSz),
                BAD_FUNC_ARG);
    AssertIntEQ(wolfSSL_read_early_data(serverSsl, earlyDataBuffer,
                                        sizeof(earlyDataBuffer), &outSz),
                WOLFSSL_FATAL_ERROR);
#endif

    wolfSSL_free(serverSsl);
    wolfSSL_CTX_free(serverCtx);
    wolfSSL_free(clientSsl);
    wolfSSL_CTX_free(clientCtx);

    wolfSSL_free(serverTls12Ssl);
    wolfSSL_CTX_free(serverTls12Ctx);
    wolfSSL_free(clientTls12Ssl);
    wolfSSL_CTX_free(clientTls12Ctx);

    return ret;
}

#endif

#ifdef HAVE_HASHDRBG

static int test_wc_RNG_GenerateBlock()
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
#endif

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
    test_wolfSSL_read_write();
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
    test_wolfSSL_PKCS5();

    /*OCSP Stapling. */
    AssertIntEQ(test_wolfSSL_UseOCSPStapling(), WOLFSSL_SUCCESS);
    AssertIntEQ(test_wolfSSL_UseOCSPStaplingV2(), WOLFSSL_SUCCESS);

    /* Multicast */
    test_wolfSSL_mcast();

    /* compatibility tests */
    test_wolfSSL_DES();
    test_wolfSSL_certs();
    test_wolfSSL_ASN1_TIME_print();
    test_wolfSSL_private_keys();
    test_wolfSSL_PEM_PrivateKey();
    test_wolfSSL_tmp_dh();
    test_wolfSSL_ctrl();
    test_wolfSSL_EVP_PKEY_new_mac_key();
    test_wolfSSL_EVP_MD_hmac_signing();
    test_wolfSSL_CTX_add_extra_chain_cert();
    test_wolfSSL_ERR_peek_last_error_line();
    test_wolfSSL_X509_STORE_set_flags();
    test_wolfSSL_X509_LOOKUP_load_file();
    test_wolfSSL_X509_NID();
    test_wolfSSL_X509_STORE_CTX_set_time();
    test_wolfSSL_BN();
    test_wolfSSL_set_options();
    test_wolfSSL_PEM_read_bio();
    test_wolfSSL_BIO();
    test_wolfSSL_DES_ecb_encrypt();
    test_wolfSSL_set_tlsext_status_type();
    test_wolfSSL_ASN1_TIME_adj();
    test_wolfSSL_CTX_set_client_CA_list();
    test_wolfSSL_CTX_add_client_CA();
    test_wolfSSL_CTX_set_srp_username();
    test_wolfSSL_CTX_set_srp_password();
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
    AssertFalse(test_wc_InitRipeMd());
    AssertFalse(test_wc_RipeMdUpdate());
    AssertFalse(test_wc_RipeMdFinal());

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
    AssertIntEQ(test_wc_AesCcmSetKey(), 0);
    AssertIntEQ(test_wc_AesCcmEncryptDecrypt(), 0);
    AssertIntEQ(test_wc_Hc128_SetKey(), 0);
    AssertIntEQ(test_wc_Hc128_Process(), 0);
    AssertIntEQ(test_wc_InitDsaKey(), 0);
    AssertIntEQ(test_wc_DsaSignVerify(), 0);
    AssertIntEQ(test_wc_DsaPublicPrivateKeyDecode(), 0);
    AssertIntEQ(test_wc_MakeDsaKey(), 0);
    AssertIntEQ(test_wc_DsaKeyToDer(), 0);

#ifdef OPENSSL_EXTRA
    /*wolfSSS_EVP_get_cipherbynid test*/
    test_wolfSSL_EVP_get_cipherbynid();
    test_wolfSSL_EC();
#endif

#ifdef HAVE_HASHDRBG
    AssertIntEQ(test_wc_RNG_GenerateBlock(), 0);
#endif

    printf(" End API Tests\n");

}
