#ifndef wolfcrypt_user_settings_h
#define wolfcrypt_user_settings_h

#include <esp/hwrand.h>

static inline int hwrand_generate_block(uint8_t *buf, size_t len) {
    hwrand_fill(buf, len);
    return 0;
}

#define FREERTOS
#define WC_NO_HARDEN
#define NO_WOLFSSL_DIR
#define SINGLE_THREADED
#define WOLFSSL_LWIP
#define NO_INLINE

#define NO_WOLFSSL_MEMORY
#define NO_WOLFSSL_SMALL_STACK
#define MP_LOW_MEM

#define CUSTOM_RAND_GENERATE_BLOCK hwrand_generate_block

#endif
