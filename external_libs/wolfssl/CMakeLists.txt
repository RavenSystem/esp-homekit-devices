set(wolfssl_VERSION "3.13.0-stable")

file(REAL_PATH "wolfssl-${wolfssl_VERSION}" wolfssl_THIRDPARTY_ROOT)

idf_component_register(
    SRC_DIRS
        "${wolfssl_THIRDPARTY_ROOT}/src"
        "${wolfssl_THIRDPARTY_ROOT}/wolfcrypt/src"
    EXCLUDE_SRCS
        "${wolfssl_THIRDPARTY_ROOT}/wolfcrypt/src/aes_asm.s"
    INCLUDE_DIRS
        "."
        "include"
        "${wolfssl_THIRDPARTY_ROOT}"
)

list(APPEND WOLFSSL_COMPILE_OPTIONS
    -DWOLFSSL_USER_SETTINGS
)

idf_build_set_property(COMPILE_OPTIONS "${WOLFSSL_COMPILE_OPTIONS}" APPEND)
