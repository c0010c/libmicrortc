cmake_minimum_required(VERSION 3.16)

project(rtc_c_api C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

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

add_executable(rtc_api_test
    test/test_rtc_api.c
)

target_include_directories(rtc_api_test PRIVATE include)
target_link_libraries(rtc_api_test PRIVATE rtc_c)

add_test(NAME rtc_api_test COMMAND rtc_api_test)
