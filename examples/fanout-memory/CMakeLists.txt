cmake_minimum_required(VERSION 3.16)

project(micrortc_fanout_memory LANGUAGES C)

set(MICRORTC_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "Path to the libmicrortc repository root")
set(FANOUT_SRTP_ROOT "${CMAKE_CURRENT_BINARY_DIR}/deps/libsrtp-minimal/install"
    CACHE PATH "Optional fanout-local libsrtp install built with internal crypto")
set(FANOUT_MBEDTLS_ROOT "${CMAKE_CURRENT_BINARY_DIR}/deps/mbedtls/install"
    CACHE PATH "Optional fanout-local mbedTLS install")
option(FANOUT_ENABLE_DATA_CHANNEL "Enable browser DataChannel/SCTP for the fanout memory example" OFF)

set(MRTC_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MRTC_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MRTC_ENABLE_DATA_CHANNEL_TRANSPORT ${FANOUT_ENABLE_DATA_CHANNEL} CACHE BOOL "" FORCE)

if(EXISTS "${FANOUT_SRTP_ROOT}/include/srtp2/srtp.h" AND EXISTS "${FANOUT_SRTP_ROOT}/lib/libsrtp2.so")
    set(MRTC_SRTP_INCLUDE_DIR "${FANOUT_SRTP_ROOT}/include" CACHE PATH "fanout-local libsrtp include dir" FORCE)
    set(MRTC_SRTP_LIBRARY "${FANOUT_SRTP_ROOT}/lib/libsrtp2.so" CACHE FILEPATH "fanout-local minimal libsrtp" FORCE)
    list(APPEND CMAKE_BUILD_RPATH "${FANOUT_SRTP_ROOT}/lib")
    message(STATUS "fanout-memory: using minimal libsrtp at ${FANOUT_SRTP_ROOT}")
endif()

if(EXISTS "${FANOUT_MBEDTLS_ROOT}/include/mbedtls/ssl.h"
   AND EXISTS "${FANOUT_MBEDTLS_ROOT}/lib/libmbedtls.so"
   AND EXISTS "${FANOUT_MBEDTLS_ROOT}/lib/libmbedx509.so"
   AND EXISTS "${FANOUT_MBEDTLS_ROOT}/lib/libmbedcrypto.so")
    set(MRTC_CRYPTO_BACKEND "mbedtls" CACHE STRING "fanout-local crypto backend" FORCE)
    set(MRTC_MBEDTLS_INCLUDE_DIR "${FANOUT_MBEDTLS_ROOT}/include" CACHE PATH "fanout-local mbedTLS include dir" FORCE)
    set(MRTC_MBEDTLS_LIBRARY "${FANOUT_MBEDTLS_ROOT}/lib/libmbedtls.so" CACHE FILEPATH "fanout-local mbedTLS library" FORCE)
    set(MRTC_MBEDX509_LIBRARY "${FANOUT_MBEDTLS_ROOT}/lib/libmbedx509.so" CACHE FILEPATH "fanout-local mbedX509 library" FORCE)
    set(MRTC_MBEDCRYPTO_LIBRARY "${FANOUT_MBEDTLS_ROOT}/lib/libmbedcrypto.so" CACHE FILEPATH "fanout-local mbedCrypto library" FORCE)
    list(APPEND CMAKE_BUILD_RPATH "${FANOUT_MBEDTLS_ROOT}/lib")
    message(STATUS "fanout-memory: using mbedTLS at ${FANOUT_MBEDTLS_ROOT}")
endif()

add_subdirectory("${MICRORTC_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/micrortc-build" EXCLUDE_FROM_ALL)

add_executable(fanout_answerer fanout_answerer.c)
target_link_libraries(fanout_answerer PRIVATE micrortc::micrortc)
target_include_directories(fanout_answerer PRIVATE "${MICRORTC_ROOT}/src")
if(FANOUT_ENABLE_DATA_CHANNEL)
    target_compile_definitions(fanout_answerer PRIVATE FANOUT_ENABLE_DATA_CHANNEL=1)
endif()
set_target_properties(fanout_answerer
    PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)
