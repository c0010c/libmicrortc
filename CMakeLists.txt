cmake_minimum_required(VERSION 3.16)

project(rtc LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(RTC_BUILD_TESTS "Build rtc tests" ON)
option(RTC_BUILD_EXAMPLES "Build rtc examples" ON)
option(RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY "Enable optional Chrome E2E DTLS/SRTP security dependencies" OFF)
option(RTC_BUILD_OPENSSL_STATIC_MEMORY_PROBE "Build a static-OpenSSL memory probe executable" OFF)

if(RTC_BUILD_EXAMPLES AND RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY)
    include(cmake/FindRtcOptionalSecurity.cmake)
endif()

add_library(rtc STATIC
    src/api/rtc.c
    src/api/peer_connection.c
    src/sdp/sdp_parser.c
    src/sdp/sdp_writer.c
    src/jsep/jsep.c
    src/ice/ice.c
    src/stun/stun.c
    src/net/demux.c
    src/security/security.c
    src/srtp/srtp.c
    src/rtp/rtp.c
    src/rtcp/rtcp.c
    src/media/media.c
    src/executor/executor.c
    src/memory/arena.c
    src/memory/allocator.c
    src/observability/observer.c
    src/observability/trace.c
    src/observability/counters.c
)

target_include_directories(rtc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(RTC_BUILD_TESTS)
    enable_testing()
    add_executable(rtc_tests
        tests/test_main.c
        tests/test_build.c
        tests/test_memory.c
        tests/test_executor_affinity.c
        tests/test_peer_connection.c
        tests/test_observability.c
        tests/test_sdp_writer.c
        tests/test_sdp_parser.c
        tests/test_jsep.c
        tests/test_stun.c
        tests/test_ice.c
        tests/test_datagram.c
        tests/test_security.c
        tests/test_media_api.c
        tests/test_rtp.c
        tests/test_rtcp.c
        tests/test_chrome_e2e_samples.c
        examples/chrome_e2e/media_samples.c
    )
    target_include_directories(rtc_tests PRIVATE src)
    target_link_libraries(rtc_tests PRIVATE rtc)
    add_test(NAME rtc_tests COMMAND rtc_tests)
    set_tests_properties(rtc_tests PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

if(RTC_BUILD_EXAMPLES)
    add_executable(rtc_create_destroy examples/create_destroy.c)
    target_link_libraries(rtc_create_destroy PRIVATE rtc)

    add_executable(rtc_chrome_e2e
        examples/chrome_e2e/rtc_chrome_e2e.c
        examples/chrome_e2e/jsonl.c
        examples/chrome_e2e/media_samples.c
        examples/chrome_e2e/security_backend_chrome.c
    )
    target_include_directories(rtc_chrome_e2e PRIVATE src)
    if(RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY)
        target_compile_definitions(rtc_chrome_e2e PRIVATE RTC_CHROME_E2E_WITH_OPTIONAL_SECURITY=1)
        target_include_directories(rtc_chrome_e2e PRIVATE ${RTC_OPTIONAL_SECURITY_INCLUDE_DIRS})
        target_link_libraries(rtc_chrome_e2e PRIVATE ${RTC_OPTIONAL_SECURITY_LIBRARIES})
    endif()
    target_link_libraries(rtc_chrome_e2e PRIVATE rtc)
endif()

if(RTC_BUILD_OPENSSL_STATIC_MEMORY_PROBE)
    if(NOT OPENSSL_ROOT_DIR)
        message(FATAL_ERROR "RTC_BUILD_OPENSSL_STATIC_MEMORY_PROBE requires OPENSSL_ROOT_DIR")
    endif()
    set(RTC_OPENSSL_STATIC_INCLUDE_DIR "${OPENSSL_ROOT_DIR}/include")
    set(RTC_OPENSSL_STATIC_SSL "${OPENSSL_ROOT_DIR}/lib/libssl.a")
    set(RTC_OPENSSL_STATIC_CRYPTO "${OPENSSL_ROOT_DIR}/lib/libcrypto.a")
    if(NOT EXISTS "${RTC_OPENSSL_STATIC_SSL}" OR NOT EXISTS "${RTC_OPENSSL_STATIC_CRYPTO}")
        message(FATAL_ERROR "OpenSSL static libraries not found under OPENSSL_ROOT_DIR=${OPENSSL_ROOT_DIR}")
    endif()
    find_package(Threads REQUIRED)
    add_executable(rtc_openssl_static_memory_probe
        examples/openssl_static_memory_probe.c
    )
    target_include_directories(rtc_openssl_static_memory_probe PRIVATE
        "${RTC_OPENSSL_STATIC_INCLUDE_DIR}"
    )
    target_link_libraries(rtc_openssl_static_memory_probe PRIVATE
        "${RTC_OPENSSL_STATIC_SSL}"
        "${RTC_OPENSSL_STATIC_CRYPTO}"
        Threads::Threads
        ${CMAKE_DL_LIBS}
    )
endif()

install(TARGETS rtc ARCHIVE DESTINATION lib)
install(DIRECTORY include/rtc DESTINATION include)
