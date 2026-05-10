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
    )
    target_include_directories(rtc_tests PRIVATE src)
    target_link_libraries(rtc_tests PRIVATE rtc)
    add_test(NAME rtc_tests COMMAND rtc_tests)
endif()

if(RTC_BUILD_EXAMPLES)
    add_executable(rtc_create_destroy examples/create_destroy.c)
    target_link_libraries(rtc_create_destroy PRIVATE rtc)
endif()

install(TARGETS rtc ARCHIVE DESTINATION lib)
install(DIRECTORY include/rtc DESTINATION include)
