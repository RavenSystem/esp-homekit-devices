# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS
    ../../libs/form_urlencoded
    ../../libs/timers_helper
    ../../libs/esp32_port
    ../../libs/adv_logger
    ../../external_libs/wolfssl
)

list(APPEND WOLFSSL_COMPILE_OPTIONS
    -DWOLFSSL_STATIC_RSA
    -DWOLFSSL_SHA384
    -DWOLFSSL_SHA512
    -DUSE_SLOW_SHA
    -DUSE_SLOW_SHA2
    -DUSE_SLOW_SHA256
    -DUSE_SLOW_SHA512
    -DGCM_SMALL
    -DRSA_LOW_MEM
    -DHAVE_AESGCM
    -DHAVE_ECC
    -DHAVE_ECC_VERIFY
    -DHAVE_ECC_KEY_IMPORT
    -DHAVE_TLS_EXTENSIONS
    -DHAVE_ECC_DHE
    -DHAVE_SUPPORTED_CURVES
    -DHAVE_SNI
    -DNO_MD5
    -DNO_FILESYSTEM
    -DNO_WRITEV
    -DNO_WOLFSSL_SERVER
    -DNO_RABBIT
    -DNO_DH
    -DNO_PWDBASED
    -DNO_DES3
    -DNO_ERROR_STRINGS
    -DNO_OLD_TLS
    -DNO_RC4
    -DNO_PSK
    -DNO_MD4
    -DNO_HC128
    -DNO_DEV_RANDOM
    -DNO_SESSION_CACHE
    -DNO_DSA
)

list(APPEND EXTRA_COMPILE_OPTIONS
    -DHAA_CHIP_NAME="${IDF_TARGET}"
)

if(HAA_SINGLE_CORE)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SINGLE_CORE
    )
endif()

if(HAA_XTAL26)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_XTAL26
    )
endif()

if(HAA_SHELLY_Plus1)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Plus1
    )
endif()

if(HAA_SHELLY_Plus1PM)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Plus1PM
    )
endif()

if(HAA_SHELLY_Plus2PM)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Plus2PM
    )
endif()

if(HAA_SHELLY_PlusPlugS)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_PlusPlugS
    )
endif()

if(HAA_SHELLY_PlusPlugUK)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_PlusPlugUK
    )
endif()

if(HAA_SHELLY_PlusPlugIT)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_PlusPlugIT
    )
endif()

if(HAA_SHELLY_PlugUS)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_PlugUS
    )
endif()

if(HAA_SHELLY_PlusI4)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_PlusI4
    )
endif()

if(HAA_SHELLY_I4G3)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_I4G3
    )
endif()

if(HAA_SHELLY_PlusUni)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_PlusUni
    )
endif()

if(HAA_SHELLY_Plus10V)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Plus10V
    )
endif()

if(HAA_SHELLY_PlusRGBWPM)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_PlusRGBWPM
    )
endif()

if(HAA_SHELLY_Plus1Mini)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Plus1Mini
    )
endif()

if(HAA_SHELLY_Mini1G3)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Mini1G3
    )
endif()

if(HAA_SHELLY_Plus1PMMini)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Plus1PMMini
    )
endif()

if(HAA_SHELLY_Mini1PMG3)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Mini1PMG3
    )
endif()

if(HAA_SHELLY_PlusPMMini)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_PlusPMMini
    )
endif()

if(HAA_SHELLY_MiniPMG3)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_MiniPMG3
    )
endif()

if(HAA_SHELLY_Pro1)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Pro1
    )
endif()

if(HAA_SHELLY_Pro2)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Pro2
    )
endif()

if(HAA_SHELLY_Pro3)
    list(APPEND EXTRA_COMPILE_OPTIONS
        -DHAA_SHELLY_Pro3
    )
endif()


include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(HAA_mgos32_Installer)

idf_build_set_property(COMPILE_OPTIONS "${WOLFSSL_COMPILE_OPTIONS}" APPEND)
idf_build_set_property(COMPILE_OPTIONS "${EXTRA_COMPILE_OPTIONS}" APPEND)
