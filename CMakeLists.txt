cmake_minimum_required(VERSION 3.16)
project(rtc_s1 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(RTC_BUILD_TESTS "Build tests" ON)
option(RTC_BUILD_EXAMPLES "Build examples" ON)
option(RTC_ENABLE_MBEDTLS "Enable mbedTLS DTLS adapter" ON)
option(RTC_ENABLE_USRSCTP "Enable usrsctp adapter" ON)
option(RTC_VENDOR_MBEDTLS "Use vendored mbedTLS under third_party/mbedtls" ON)
option(RTC_VENDOR_USRSCTP "Use vendored usrsctp under third_party/usrsctp" ON)

add_library(rtc STATIC
  src/core/mempool.c
  src/core/session.c
  src/transport/stun.c
  src/transport/ice.c
  src/transport/dtls.c
  src/datachannel/datachannel.c
  src/datachannel/sctp_adapter.c
  src/platform/linux/platform_linux.c
  src/platform/rtos/platform_rtos_stub.c
)

target_include_directories(rtc PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core
  ${CMAKE_CURRENT_SOURCE_DIR}/src/transport
  ${CMAKE_CURRENT_SOURCE_DIR}/src/datachannel
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/linux
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/rtos
)

target_compile_options(rtc PRIVATE -Wall -Wextra -Werror)

set(RTC_WITH_MBEDTLS_VALUE 0)
set(RTC_WITH_USRSCTP_VALUE 0)

if(RTC_ENABLE_MBEDTLS AND RTC_VENDOR_MBEDTLS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mbedtls/CMakeLists.txt)
  set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
  set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
  set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
  set(GEN_FILES ON CACHE BOOL "" FORCE)
  add_subdirectory(third_party/mbedtls)
  set(RTC_WITH_MBEDTLS_VALUE 1)
  message(STATUS "Using vendored mbedTLS from third_party/mbedtls")
endif()

if(RTC_ENABLE_USRSCTP AND RTC_VENDOR_USRSCTP AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/usrsctp/CMakeLists.txt)
  set(sctp_build_programs OFF CACHE BOOL "" FORCE)
  set(sctp_build_shared_lib OFF CACHE BOOL "" FORCE)
  set(sctp_werror OFF CACHE BOOL "" FORCE)
  set(sctp_debug OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/usrsctp)
  set(RTC_WITH_USRSCTP_VALUE 1)
  message(STATUS "Using vendored usrsctp from third_party/usrsctp")
endif()

if(RTC_ENABLE_MBEDTLS AND RTC_WITH_MBEDTLS_VALUE EQUAL 0)
  find_package(MbedTLS QUIET)
  if(MbedTLS_FOUND)
    set(RTC_WITH_MBEDTLS_VALUE 1)
    target_link_libraries(rtc PUBLIC MbedTLS::mbedtls MbedTLS::mbedcrypto MbedTLS::mbedx509)
    message(STATUS "mbedTLS found: DTLS adapter enabled")
  else()
    message(STATUS "mbedTLS not found: DTLS adapter will use fallback stub")
  endif()
endif()

if(RTC_ENABLE_USRSCTP AND RTC_WITH_USRSCTP_VALUE EQUAL 0)
  find_path(USRSCTP_INCLUDE_DIR usrsctp.h)
  find_library(USRSCTP_LIBRARY usrsctp)
  if(USRSCTP_INCLUDE_DIR AND USRSCTP_LIBRARY)
    set(RTC_WITH_USRSCTP_VALUE 1)
    target_include_directories(rtc PUBLIC ${USRSCTP_INCLUDE_DIR})
    target_link_libraries(rtc PUBLIC ${USRSCTP_LIBRARY})
    message(STATUS "usrsctp found: SCTP adapter enabled")
  else()
    message(STATUS "usrsctp not found: SCTP adapter will use fallback stub")
  endif()
endif()

target_compile_definitions(rtc PUBLIC
  RTC_WITH_MBEDTLS=${RTC_WITH_MBEDTLS_VALUE}
  RTC_WITH_USRSCTP=${RTC_WITH_USRSCTP_VALUE}
)

if(RTC_WITH_MBEDTLS_VALUE EQUAL 1)
  if(TARGET MbedTLS::mbedtls AND TARGET MbedTLS::mbedx509)
    target_link_libraries(rtc PUBLIC MbedTLS::mbedtls MbedTLS::mbedx509)
  elseif(TARGET mbedtls AND TARGET mbedx509)
    target_link_libraries(rtc PUBLIC mbedtls mbedx509)
  endif()
endif()

if(RTC_WITH_USRSCTP_VALUE EQUAL 1)
  if(TARGET usrsctp)
    target_link_libraries(rtc PUBLIC usrsctp)
    target_include_directories(rtc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/usrsctp/usrsctplib)
  endif()
endif()

if(RTC_BUILD_EXAMPLES)
  add_executable(example_device_manual
    examples/device/minimal_device_app.c
    examples/device/device_manual_logic.c)
  target_link_libraries(example_device_manual PRIVATE rtc)
  target_compile_options(example_device_manual PRIVATE -Wall -Wextra -Werror)
  target_include_directories(example_device_manual PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples/device)
endif()

if(RTC_BUILD_TESTS)
  enable_testing()

  add_executable(test_stun tests/test_stun.c src/transport/stun.c)
  target_compile_options(test_stun PRIVATE -Wall -Wextra -Werror)
  target_compile_definitions(test_stun PRIVATE RTC_WITH_MBEDTLS=${RTC_WITH_MBEDTLS_VALUE})
  target_include_directories(test_stun PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/transport)
  if(RTC_WITH_MBEDTLS_VALUE EQUAL 1)
    if(TARGET MbedTLS::mbedcrypto)
      target_link_libraries(test_stun PRIVATE MbedTLS::mbedcrypto)
    elseif(TARGET mbedcrypto)
      target_link_libraries(test_stun PRIVATE mbedcrypto)
    endif()
  endif()
  add_test(NAME test_stun COMMAND test_stun)

  add_executable(test_mempool tests/test_mempool.c src/core/mempool.c)
  target_compile_options(test_mempool PRIVATE -Wall -Wextra -Werror)
  target_include_directories(test_mempool PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/core)
  add_test(NAME test_mempool COMMAND test_mempool)

  add_executable(test_ice tests/test_ice.c src/transport/ice.c src/transport/stun.c)
  target_compile_options(test_ice PRIVATE -Wall -Wextra -Werror)
  target_include_directories(test_ice PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/transport ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_test(NAME test_ice COMMAND test_ice)

  add_executable(test_datachannel tests/test_datachannel.c src/datachannel/datachannel.c)
  target_compile_options(test_datachannel PRIVATE -Wall -Wextra -Werror)
  target_include_directories(test_datachannel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/datachannel ${CMAKE_CURRENT_SOURCE_DIR}/include)
  add_test(NAME test_datachannel COMMAND test_datachannel)

  add_executable(test_device_manual_logic
    tests/test_device_manual_logic.c
    examples/device/device_manual_logic.c)
  target_compile_options(test_device_manual_logic PRIVATE -Wall -Wextra -Werror)
  target_include_directories(test_device_manual_logic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/examples/device)
  add_test(NAME test_device_manual_logic COMMAND test_device_manual_logic)

  add_executable(test_session
    tests/test_session.c
    src/core/session.c
    src/core/mempool.c
    src/transport/stun.c
    src/transport/ice.c
    src/transport/dtls.c
    src/datachannel/datachannel.c
    src/datachannel/sctp_adapter.c)
  target_compile_options(test_session PRIVATE -Wall -Wextra -Werror)
  target_include_directories(test_session PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transport
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datachannel)
  add_test(NAME test_session COMMAND test_session)

  add_executable(test_dtls tests/test_dtls.c src/transport/dtls.c)
  target_compile_options(test_dtls PRIVATE -Wall -Wextra -Werror)
  target_compile_definitions(test_dtls PRIVATE RTC_WITH_MBEDTLS=${RTC_WITH_MBEDTLS_VALUE})
  target_include_directories(test_dtls PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/transport)
  if(RTC_WITH_MBEDTLS_VALUE EQUAL 1)
    if(TARGET MbedTLS::mbedtls AND TARGET MbedTLS::mbedcrypto AND TARGET MbedTLS::mbedx509)
      target_link_libraries(test_dtls PRIVATE MbedTLS::mbedtls MbedTLS::mbedcrypto MbedTLS::mbedx509)
    elseif(TARGET mbedtls AND TARGET mbedcrypto AND TARGET mbedx509)
      target_link_libraries(test_dtls PRIVATE mbedtls mbedcrypto mbedx509)
    endif()
  endif()
  add_test(NAME test_dtls COMMAND test_dtls)
endif()
