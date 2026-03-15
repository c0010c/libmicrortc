cmake_minimum_required(VERSION 3.16)

project(rtc_c_api C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()
option(RTC_ENABLE_REAL_DTLS_SRTP "Enable mbedTLS+libSRTP integration" ON)

set(RTC_SUPPORTED_PLATFORMS linux)
set(RTC_TARGET_PLATFORM "" CACHE STRING "RTC platform implementation to compile")
set_property(CACHE RTC_TARGET_PLATFORM PROPERTY STRINGS ${RTC_SUPPORTED_PLATFORMS})

set(_rtc_selected_platform "")
set(_rtc_selected_platform_source "")

if(NOT RTC_TARGET_PLATFORM STREQUAL "")
  string(TOLOWER "${RTC_TARGET_PLATFORM}" _rtc_selected_platform)
  set(_rtc_selected_platform_source "RTC_TARGET_PLATFORM")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(_rtc_selected_platform "linux")
  set(_rtc_selected_platform_source "CMAKE_SYSTEM_NAME")
else()
  set(_rtc_selected_platform "linux")
  set(_rtc_selected_platform_source "default")
endif()

list(FIND RTC_SUPPORTED_PLATFORMS "${_rtc_selected_platform}" _rtc_platform_index)
if(_rtc_platform_index EQUAL -1)
  message(FATAL_ERROR
      "Unsupported RTC_TARGET_PLATFORM='${RTC_TARGET_PLATFORM}'. "
      "Supported platforms: ${RTC_SUPPORTED_PLATFORMS}")
endif()

if(_rtc_selected_platform STREQUAL "linux")
  set(RTC_PLATFORM_SOURCES
      src/platform/linux/rtc_platform.c)
else()
  message(FATAL_ERROR
      "No source mapping for selected platform '${_rtc_selected_platform}'")
endif()

message(STATUS
    "RTC platform selected: ${_rtc_selected_platform} "
    "(decision source: ${_rtc_selected_platform_source})")

if(RTC_ENABLE_REAL_DTLS_SRTP)
  set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON CACHE BOOL "" FORCE)
  set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
  set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
  set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
  set(MBEDTLS_USER_CONFIG_FILE
      "${CMAKE_CURRENT_SOURCE_DIR}/include/rtc/rtc_mbedtls_user_config.h"
      CACHE FILEPATH "" FORCE)
  add_subdirectory(third_party/mbedtls EXCLUDE_FROM_ALL)

  set(LIBSRTP_TEST_APPS OFF CACHE BOOL "" FORCE)
  set(ENABLE_WARNINGS OFF CACHE BOOL "" FORCE)
  set(ENABLE_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
  set(CRYPTO_LIBRARY internal CACHE STRING "" FORCE)
  add_subdirectory(third_party/libsrtp EXCLUDE_FROM_ALL)
endif()

add_library(rtc_c STATIC
    src/rtc_api.c
    ${RTC_PLATFORM_SOURCES}
    src/transport/rtc_transport.c
    src/ice/rtc_ice.c
    src/dtls/rtc_dtls.c
    src/srtp/rtc_srtp.c
    src/rtp/rtc_rtp.c
    src/session/rtc_session.c
)

target_include_directories(rtc_c PUBLIC include)
target_include_directories(rtc_c PRIVATE src)

if(RTC_ENABLE_REAL_DTLS_SRTP)
  target_include_directories(rtc_c PRIVATE third_party/libsrtp/include)
  target_link_libraries(rtc_c PRIVATE
      MbedTLS::mbedcrypto
      MbedTLS::mbedx509
      MbedTLS::mbedtls
      srtp3)
endif()

add_executable(rtc_api_test
    test/test_rtc_api.c
)

target_include_directories(rtc_api_test PRIVATE include)
target_include_directories(rtc_api_test PRIVATE src third_party/mbedtls/include)
target_link_libraries(rtc_api_test PRIVATE rtc_c)
if(RTC_ENABLE_REAL_DTLS_SRTP)
  target_compile_definitions(rtc_api_test PRIVATE
      MBEDTLS_USER_CONFIG_FILE=\"rtc/rtc_mbedtls_user_config.h\")
endif()

add_test(NAME rtc_api_test COMMAND rtc_api_test)

add_executable(rtc_transport_test
    test/test_rtc_transport.c
)

target_include_directories(rtc_transport_test PRIVATE include src)
target_link_libraries(rtc_transport_test PRIVATE rtc_c)

add_test(NAME rtc_transport_test COMMAND rtc_transport_test)

add_executable(rtc_rtp_test
    test/test_rtc_rtp.c
)

target_include_directories(rtc_rtp_test PRIVATE include src)
target_link_libraries(rtc_rtp_test PRIVATE rtc_c)

add_test(NAME rtc_rtp_test COMMAND rtc_rtp_test)

add_executable(rtc_interop_smoke_test
    test/test_rtc_interop_smoke.c
)

target_include_directories(rtc_interop_smoke_test PRIVATE include)
target_include_directories(rtc_interop_smoke_test PRIVATE src third_party/mbedtls/include)
target_link_libraries(rtc_interop_smoke_test PRIVATE rtc_c)
if(RTC_ENABLE_REAL_DTLS_SRTP)
  target_compile_definitions(rtc_interop_smoke_test PRIVATE
      MBEDTLS_USER_CONFIG_FILE=\"rtc/rtc_mbedtls_user_config.h\")
endif()

add_test(NAME rtc_interop_smoke_test COMMAND rtc_interop_smoke_test)

add_executable(rtc_srtp_test
    test/test_rtc_srtp.c
)

target_include_directories(rtc_srtp_test PRIVATE include src)
if(RTC_ENABLE_REAL_DTLS_SRTP)
  target_include_directories(rtc_srtp_test PRIVATE third_party/libsrtp/include)
endif()
target_link_libraries(rtc_srtp_test PRIVATE rtc_c)

add_test(NAME rtc_srtp_test COMMAND rtc_srtp_test)
