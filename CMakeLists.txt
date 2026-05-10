cmake_minimum_required(VERSION 3.16)

project(rtc LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(RTC_BUILD_TESTS "Build rtc tests" ON)
option(RTC_BUILD_EXAMPLES "Build rtc examples" ON)

add_library(rtc STATIC
    src/api/rtc.c
    src/api/peer_connection.c
    src/sdp/sdp_parser.c
    src/sdp/sdp_writer.c
    src/jsep/jsep.c
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
endif()

install(TARGETS rtc ARCHIVE DESTINATION lib)
install(DIRECTORY include/rtc DESTINATION include)
