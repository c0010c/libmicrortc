cmake_minimum_required(VERSION 3.6.3)

project(s2_webrtc LANGUAGES C)

include(cmake/options.cmake)

if(WEBRTC_PORT_POSIX AND WEBRTC_PORT_RTOS)
    message(FATAL_ERROR "WEBRTC_PORT_POSIX and WEBRTC_PORT_RTOS cannot both be ON")
endif()

if((NOT WEBRTC_PORT_POSIX) AND (NOT WEBRTC_PORT_RTOS))
    message(FATAL_ERROR "At least one port must be enabled: WEBRTC_PORT_POSIX or WEBRTC_PORT_RTOS")
endif()

add_library(webrtc_core STATIC
    src/api/webrtc_api.c
)

target_include_directories(webrtc_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(webrtc_core
    PUBLIC
        WEBRTC_ENABLE_AUDIO=$<BOOL:${WEBRTC_ENABLE_AUDIO}>
        WEBRTC_ENABLE_VIDEO=$<BOOL:${WEBRTC_ENABLE_VIDEO}>
        WEBRTC_ENABLE_DATA_CHANNEL=$<BOOL:${WEBRTC_ENABLE_DATA_CHANNEL}>
        WEBRTC_ENABLE_TURN=$<BOOL:${WEBRTC_ENABLE_TURN}>
        WEBRTC_USE_MBEDTLS=$<BOOL:${WEBRTC_USE_MBEDTLS}>
        WEBRTC_PORT_POSIX=$<BOOL:${WEBRTC_PORT_POSIX}>
        WEBRTC_PORT_RTOS=$<BOOL:${WEBRTC_PORT_RTOS}>
)

enable_testing()
add_subdirectory(tests)
