cmake_minimum_required(VERSION 3.16)

project(rtc_c_api C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

enable_testing()

add_library(rtc_c STATIC
    src/rtc.c
)

target_include_directories(rtc_c PUBLIC include)

add_executable(rtc_api_test
    test/test_rtc_api.c
)

target_include_directories(rtc_api_test PRIVATE include)
target_link_libraries(rtc_api_test PRIVATE rtc_c)

add_test(NAME rtc_api_test COMMAND rtc_api_test)
