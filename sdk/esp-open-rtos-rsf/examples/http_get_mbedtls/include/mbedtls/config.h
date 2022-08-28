/* Special mbedTLS config file for http_get_mbedtls example,
   overrides supported cipher suite list.

   Overriding the set of cipher suites saves small amounts of ROM and
   RAM, and is a good practice in general if you know what server(s)
   you want to connect to.

  However it's extra important here because the howsmyssl API sends
  back the list of ciphers we send it as a JSON list in the, and we
  only have a 4096kB receive buffer. If the server supported maximum
  fragment length option then we wouldn't have this problem either,
  but we do so this is a good workaround.

  The ciphers chosen below are common ECDHE ciphers, the same ones
  Firefox uses when connecting to a TLSv1.2 server.
*/
#ifndef MBEDTLS_CONFIG_H

/* include_next picks up default config from extras/mbedtls/include/mbedtls/config.h */
#include_next<mbedtls/config.h>

#define MBEDTLS_SSL_CIPHERSUITES MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA

/* uncomment next line to include debug output from example */
//#define MBEDTLS_DEBUG_C

#endif
