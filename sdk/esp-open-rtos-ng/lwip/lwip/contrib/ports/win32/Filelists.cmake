# This file is indended to be included in end-user CMakeLists.txt
# include(/path/to/Filelists.cmake)
# It assumes the variable LWIP_CONTRIB_DIR is defined pointing to the
# root path of lwIP/contrib sources.
#
# This file is NOT designed (on purpose) to be used as cmake
# subdir via add_subdirectory()
# The intention is to provide greater flexibility to users to
# create their own targets using the *_SRCS variables.

if(NOT ${CMAKE_VERSION} VERSION_LESS "3.10.0")
    include_guard(GLOBAL)
endif()

set(lwipcontribportwindows_SRCS
    ${LWIP_CONTRIB_DIR}/ports/win32/sys_arch.c
    ${LWIP_CONTRIB_DIR}/ports/win32/sio.c
    ${LWIP_CONTRIB_DIR}/ports/win32/pcapif.c
    ${LWIP_CONTRIB_DIR}/ports/win32/pcapif_helper.c
)

# pcapif needs WinPcap developer package: https://www.winpcap.org/devel.htm
if(NOT DEFINED WPDPACK_DIR)
    set(WPDPACK_DIR ${LWIP_DIR}/../WpdPack)
    message(STATUS "WPDPACK_DIR not set - using default location ${WPDPACK_DIR}")
endif()
find_library(WPCAP  wpcap  HINTS ${WPDPACK_DIR}/lib)
find_library(PACKET packet HINTS ${WPDPACK_DIR}/lib)

add_library(lwipcontribportwindows EXCLUDE_FROM_ALL ${lwipcontribportwindows_SRCS})
target_include_directories(lwipcontribportwindows PRIVATE ${LWIP_INCLUDE_DIRS} "${WPDPACK_DIR}/include" ${LWIP_MBEDTLS_INCLUDE_DIRS})
target_compile_options(lwipcontribportwindows PRIVATE ${LWIP_COMPILER_FLAGS})
target_compile_definitions(lwipcontribaddons PRIVATE ${LWIP_DEFINITIONS} ${LWIP_MBEDTLS_DEFINITIONS})
target_link_libraries(lwipcontribportwindows PUBLIC ${WPCAP} ${PACKET} ${LWIP_MBEDTLS_LINK_LIBRARIES})
