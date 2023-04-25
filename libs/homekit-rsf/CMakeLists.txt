idf_component_register(
    SRC_DIRS
        "src"
    INCLUDE_DIRS
        "include"
        "src"
    REQUIRES
        esp_wifi
        esp_netif
        nvs_flash
        app_update
        timers_helper
        wolfssl
        cJSON
        http_parser
        esp32_port
        adv_logger_ntp
)

list(APPEND WOLFSSL_COMPILE_OPTIONS
    -DWOLFSSL_USER_SETTINGS
    -DWOLFCRYPT_HAVE_SRP
    -DWOLFSSL_SHA512
    -DWOLFSSL_BASE64_ENCODE
    -DNO_MD5
    -DNO_SHA
    -DHAVE_HKDF
    -DHAVE_CHACHA
    -DHAVE_POLY1305
    -DHAVE_ED25519
    -DHAVE_CURVE25519
    -DNO_SESSION_CACHE
    -DRSA_LOW_MEM
    -DGCM_SMALL
    -DUSE_SLOW_SHA
    -DUSE_SLOW_SHA2
    -DUSE_SLOW_SHA256
    -DUSE_SLOW_SHA512
    -DWOLFCRYPT_ONLY
    -DTFM_TIMING_RESISTANT
)

if(HOMEKIT_SMALL)
    list(APPEND WOLFSSL_COMPILE_OPTIONS
        -DCURVE25519_SMALL
        -DED25519_SMALL
    )
endif()

idf_build_set_property(COMPILE_OPTIONS "${WOLFSSL_COMPILE_OPTIONS}" APPEND)
