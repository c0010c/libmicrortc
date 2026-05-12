cmake_minimum_required(VERSION 3.16)

project(micrortc VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(MRTC_BUILD_TESTS "Build libmicrortc tests" ON)

add_library(micrortc STATIC src/micrortc.c)
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
)

if(MRTC_BUILD_TESTS)
    enable_testing()

    add_executable(micrortc_smoke_test tests/smoke/test_link.c)
    target_link_libraries(micrortc_smoke_test PRIVATE micrortc::micrortc)
    add_test(NAME micrortc_smoke_test COMMAND micrortc_smoke_test)
endif()

install(
    TARGETS micrortc
    EXPORT micrortcTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(EXPORT micrortcTargets
    NAMESPACE micrortc::
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/micrortc"
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/micrortcConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/micrortcConfig.cmake"
    INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/micrortc"
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/micrortcConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/micrortcConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/micrortcConfigVersion.cmake"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/micrortc"
)
