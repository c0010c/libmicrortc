cmake_minimum_required(VERSION 3.16)

project(micrortc VERSION 0.1.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CheckIncludeFile)
include(FindPackageHandleStandardArgs)

option(MRTC_BUILD_TESTS "Build libmicrortc tests" ON)
option(MRTC_ENABLE_PHASE4_TRANSPORT "Build Phase 4 transport/security/DataChannel scaffolding" ON)
option(MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS "Fail configure when OpenSSL/libsrtp/usrsctp are unavailable" OFF)

find_package(Threads REQUIRED)

if(MRTC_ENABLE_PHASE4_TRANSPORT)
    find_package(OpenSSL QUIET)
    find_library(MRTC_SRTP_LIBRARY NAMES srtp2 srtp)
    find_path(MRTC_SRTP_INCLUDE_DIR NAMES srtp2/srtp.h srtp/srtp.h)
    find_library(MRTC_USRSCTP_LIBRARY NAMES usrsctp)
    find_path(MRTC_USRSCTP_INCLUDE_DIR NAMES usrsctp.h)

    find_package_handle_standard_args(MRTC_SRTP DEFAULT_MSG MRTC_SRTP_LIBRARY MRTC_SRTP_INCLUDE_DIR)
    find_package_handle_standard_args(MRTC_USRSCTP DEFAULT_MSG MRTC_USRSCTP_LIBRARY MRTC_USRSCTP_INCLUDE_DIR)

    if(MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS)
        if(NOT OpenSSL_FOUND)
            message(FATAL_ERROR "OpenSSL is required when MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON")
        endif()
        if(NOT MRTC_SRTP_FOUND)
            message(FATAL_ERROR "libsrtp is required when MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON")
        endif()
        if(NOT MRTC_USRSCTP_FOUND)
            message(FATAL_ERROR "usrsctp is required when MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON")
        endif()
    endif()
endif()

add_library(micrortc STATIC
    src/micrortc.c
    src/peer_connection.c
    src/sdp.c
    src/common/mrtc_common.c
    src/common/mrtc_mutex.c
    src/common/mrtc_socket.c
    src/ice/ice_agent.c
    src/ice/ice_config.c
    src/stun/stun_message.c
    src/turn/turn_client.c
    src/dtls/dtls_session.c
    src/srtp/srtp_session.c
    src/sctp/sctp_session.c
    src/data_channel/data_channel.c
)
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

if(MRTC_ENABLE_PHASE4_TRANSPORT)
    target_compile_definitions(micrortc PRIVATE MRTC_ENABLE_PHASE4_TRANSPORT=1)

    if(OpenSSL_FOUND)
        target_compile_definitions(micrortc PRIVATE MRTC_HAVE_OPENSSL=1)
        target_link_libraries(micrortc PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()

    if(MRTC_SRTP_FOUND)
        target_compile_definitions(micrortc PRIVATE MRTC_HAVE_SRTP=1)
        target_include_directories(micrortc PRIVATE "${MRTC_SRTP_INCLUDE_DIR}")
        target_link_libraries(micrortc PRIVATE "${MRTC_SRTP_LIBRARY}")
    endif()

    if(MRTC_USRSCTP_FOUND)
        target_compile_definitions(micrortc PRIVATE MRTC_HAVE_USRSCTP=1)
        target_include_directories(micrortc PRIVATE "${MRTC_USRSCTP_INCLUDE_DIR}")
        target_link_libraries(micrortc PRIVATE "${MRTC_USRSCTP_LIBRARY}")
    endif()
endif()

if(MRTC_BUILD_TESTS)
    enable_testing()

    add_executable(micrortc_smoke_test tests/smoke/test_link.c)
    target_link_libraries(micrortc_smoke_test PRIVATE micrortc::micrortc)
    add_test(NAME micrortc_smoke_test COMMAND micrortc_smoke_test)

    add_executable(mrtc_sdp_roundtrip_test tests/sdp/test_sdp_roundtrip.c)
    target_link_libraries(mrtc_sdp_roundtrip_test PRIVATE micrortc::micrortc)
    add_test(NAME mrtc_sdp_roundtrip_test COMMAND mrtc_sdp_roundtrip_test)
    set_tests_properties(mrtc_sdp_roundtrip_test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(mrtc_peer_connection_api_test tests/peer_connection/test_peer_connection_api.c)
    target_link_libraries(mrtc_peer_connection_api_test PRIVATE micrortc::micrortc)
    add_test(NAME mrtc_peer_connection_api_test COMMAND mrtc_peer_connection_api_test)
    set_tests_properties(mrtc_peer_connection_api_test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(mrtc_phase4_transport_smoke_test tests/transport/test_transport_smoke.c)
    target_link_libraries(mrtc_phase4_transport_smoke_test PRIVATE micrortc::micrortc)
    add_test(NAME mrtc_phase4_transport_smoke_test COMMAND mrtc_phase4_transport_smoke_test)
    set_tests_properties(mrtc_phase4_transport_smoke_test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(mrtc_ice_config_test tests/transport/test_ice_config.c)
    target_link_libraries(mrtc_ice_config_test PRIVATE micrortc::micrortc)
    add_test(NAME mrtc_ice_config_test COMMAND mrtc_ice_config_test)
    set_tests_properties(mrtc_ice_config_test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(mrtc_stun_message_test tests/transport/test_stun_message.c)
    target_link_libraries(mrtc_stun_message_test PRIVATE micrortc::micrortc)
    add_test(NAME mrtc_stun_message_test COMMAND mrtc_stun_message_test)
    set_tests_properties(mrtc_stun_message_test PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

    add_executable(mrtc_phase4_network_verify tests/integration/mrtc_phase4_network_verify.c)
    target_link_libraries(mrtc_phase4_network_verify PRIVATE micrortc::micrortc)
    set_target_properties(mrtc_phase4_network_verify
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/integration"
    )
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
