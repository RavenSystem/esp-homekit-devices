
/* #define SINGLE_THREADED      or define RTOS  option */
#define WOLFSSL_CMSIS_RTOS

/* #define NO_FILESYSTEM         or define Filesystem option */
#define WOLFSSL_KEIL_FS
#define NO_WOLFSSL_DIR 
#define WOLFSSL_NO_CURRDIR

/* #define WOLFSSL_USER_IO      or use BSD incompatible TCP stack */
#define WOLFSSL_KEIL_TCP_NET  /* KEIL_TCP + wolfssl_MDL_ARM.c for BSD compatibility */

#define NO_DEV_RANDOM
/* define your Rand gen for the operational use */
#define WOLFSSL_GENSEED_FORTEST

#define USE_WOLFSSL_MEMORY
#define WOLFSSL_MALLOC_CHECK

#define XVALIDATEDATE(d, f,t) (0)
#define WOLFSSL_USER_CURRTIME /* for benchmark */

#define USE_FAST_MATH
#define TFM_TIMING_RESISTANT

#define  BENCH_EMBEDDED

#define NO_WRITEV
#define NO_MAIN_DRIVER
