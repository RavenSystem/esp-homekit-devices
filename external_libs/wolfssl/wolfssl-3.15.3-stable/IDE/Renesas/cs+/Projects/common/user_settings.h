
#define NO_MAIN_DRIVER
#define BENCH_EMBEDDED
#define NO_WRITEV
#define WOLFSSL_USER_IO
#define NO_DEV_RANDOM
#define USE_CERT_BUFFERS_2048
#define WOLFSSL_USER_CURRTIME
#define SIZEOF_LONG_LONG 8
#define NO_WOLFSSL_DIR 
#define WOLFSSL_NO_CURRDIR
#define NO_FILESYSTEM
#define WOLFSSL_LOG_PRINTF

/* #define DEBUG_WOLFSSL */

#define OPENSSL_EXTRA

#define WOLFSSL_SMALL_STACK
#define WOLFSSL_DH_CONST
#define HAVE_TLS_EXTENSIONS
#define HAVE_SUPPORTED_CURVES

#define USER_TIME
#define XTIME time
#define USE_WOLF_SUSECONDS_T
#define USE_WOLF_TIMEVAL_T

#define WOLFSSL_USER_CURRTIME /* for benchmark */

#define WOLFSSL_GENSEED_FORTEST /* Wardning: define your own seed gen */

#define TFM_TIMING_RESISTANT
#define ECC_TIMING_RESISTANT
#define WC_RSA_BLINDING

#define SINGLE_THREADED  /* or define RTOS  option */
/* #define WOLFSSL_CMSIS_RTOS */

/* #define NO_DH */
#define HAVE_AESGCM
#define WOLFSSL_SHA512
#define HAVE_ECC
#define HAVE_CURVE25519
#define CURVE25519_SMALL
#define HAVE_ED25519

/* #define NO_WOLFSSL_STUB */
