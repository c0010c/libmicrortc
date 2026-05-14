cmake_minimum_required(VERSION 3.16)

project(micrortc VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)

option(MRTC_BUILD_TESTS "Build libmicrortc tests" ON)
option(MRTC_BUILD_EXAMPLES "Build libmicrortc examples" OFF)
option(MRTC_ENABLE_PHASE4_TRANSPORT "Build Phase 4 transport/security/DataChannel scaffolding" ON)
option(MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS "Fail configure when OpenSSL/libsrtp/usrsctp are unavailable" OFF)

find_package(Threads REQUIRED)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MicroRtcSources.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MicroRtcTransportDeps.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MicroRtcTests.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MicroRtcInstall.cmake")

add_library(micrortc STATIC ${MRTC_LIBRARY_SOURCES})
add_library(micrortc::micrortc ALIAS micrortc)

set_target_properties(micrortc
    PROPERTIES
        OUTPUT_NAME micrortc
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
)

target_compile_definitions(micrortc
    PRIVATE
        MRTC_VERSION_STRING="${PROJECT_VERSION}"
)

target_include_directories(micrortc
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(micrortc
    PRIVATE
        Threads::Threads
)

mrtc_apply_transport_deps(micrortc)

if(MRTC_BUILD_TESTS OR MRTC_BUILD_EXAMPLES)
    mrtc_add_chrome_e2e_targets()
endif()

if(MRTC_BUILD_TESTS)
    mrtc_add_project_tests()
endif()

mrtc_install_package(micrortc)
