cmake_minimum_required(VERSION 3.16)

project(rtc LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(RTC_BUILD_TESTS "Build rtc tests" ON)
option(RTC_BUILD_EXAMPLES "Build rtc examples" ON)

add_library(rtc STATIC
    src/api/rtc.c
)

target_include_directories(rtc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

if(RTC_BUILD_TESTS)
    enable_testing()
    add_executable(rtc_tests
        tests/test_main.c
        tests/test_build.c
    )
    target_link_libraries(rtc_tests PRIVATE rtc)
    add_test(NAME rtc_tests COMMAND rtc_tests)
endif()

install(TARGETS rtc ARCHIVE DESTINATION lib)
install(DIRECTORY include/rtc DESTINATION include)
