#ifndef _SSL_CONNECTION_H_
#define _SSL_CONNECTION_H_

// this must be ahead of any mbedtls header files so the local mbedtls/config.h can be properly referenced
#include "mbedtls/config.h"

#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

typedef struct SSLConnection {
    mbedtls_net_context net_ctx;
    mbedtls_ssl_context ssl_ctx;
    mbedtls_ssl_config ssl_conf;

    mbedtls_ctr_drbg_context drbg_ctx;
    mbedtls_entropy_context entropy_ctx;

    mbedtls_x509_crt ca_cert;
    mbedtls_x509_crt client_cert;
    mbedtls_pk_context client_key;

    char *ca_cert_str;
    char *client_cert_str;
    char *client_key_str;
} SSLConnection;

extern void ssl_init(SSLConnection* n);
extern int ssl_connect(SSLConnection* n, const char* host, int port);
extern int ssl_destroy(SSLConnection* n);
extern int ssl_read(SSLConnection* n, unsigned char* buffer, int len,
        int timeout_ms);
extern int ssl_write(SSLConnection* n, unsigned char* buffer, int len,
        int timeout_ms);

#endif /* _SSL_CONNECTION_H_ */
