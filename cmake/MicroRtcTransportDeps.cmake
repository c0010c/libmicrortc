include(FindPackageHandleStandardArgs)

if(MRTC_ENABLE_PHASE4_TRANSPORT)
    find_package(OpenSSL QUIET)
    find_library(MRTC_SRTP_LIBRARY NAMES srtp2 srtp)
    find_path(MRTC_SRTP_INCLUDE_DIR NAMES srtp2/srtp.h srtp/srtp.h)
    find_library(MRTC_USRSCTP_LIBRARY NAMES usrsctp)
    find_path(MRTC_USRSCTP_INCLUDE_DIR NAMES usrsctp.h)

    find_package_handle_standard_args(MRTC_SRTP DEFAULT_MSG MRTC_SRTP_LIBRARY MRTC_SRTP_INCLUDE_DIR)
    find_package_handle_standard_args(MRTC_USRSCTP DEFAULT_MSG MRTC_USRSCTP_LIBRARY MRTC_USRSCTP_INCLUDE_DIR)

    if(MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS)
        set(MRTC_MISSING_TRANSPORT_DEPS "")
        if(NOT OpenSSL_FOUND)
            list(APPEND MRTC_MISSING_TRANSPORT_DEPS "OpenSSL")
        endif()
        if(NOT MRTC_SRTP_FOUND)
            list(APPEND MRTC_MISSING_TRANSPORT_DEPS "libsrtp")
        endif()
        if(NOT MRTC_USRSCTP_FOUND)
            list(APPEND MRTC_MISSING_TRANSPORT_DEPS "usrsctp")
        endif()
        if(MRTC_MISSING_TRANSPORT_DEPS)
            string(REPLACE ";" ", " MRTC_MISSING_TRANSPORT_DEPS_TEXT "${MRTC_MISSING_TRANSPORT_DEPS}")
            message(FATAL_ERROR
                "Missing required system transport dependencies: ${MRTC_MISSING_TRANSPORT_DEPS_TEXT}. "
                "OpenSSL, libsrtp and usrsctp are required when MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON. "
                "On Ubuntu/Debian install development packages for libssl, libsrtp2 and usrsctp before rerunning strict transport verification."
            )
        endif()
    endif()
endif()

function(mrtc_apply_transport_deps target)
    if(NOT MRTC_ENABLE_PHASE4_TRANSPORT)
        return()
    endif()

    target_compile_definitions(${target} PRIVATE MRTC_ENABLE_PHASE4_TRANSPORT=1)

    if(OpenSSL_FOUND)
        target_compile_definitions(${target} PRIVATE MRTC_HAVE_OPENSSL=1)
        target_link_libraries(${target} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()

    if(MRTC_SRTP_FOUND)
        target_compile_definitions(${target} PRIVATE MRTC_HAVE_SRTP=1)
        target_include_directories(${target} PRIVATE "${MRTC_SRTP_INCLUDE_DIR}")
        target_link_libraries(${target} PRIVATE "${MRTC_SRTP_LIBRARY}")
    endif()

    if(MRTC_USRSCTP_FOUND)
        target_compile_definitions(${target} PRIVATE MRTC_HAVE_USRSCTP=1)
        target_include_directories(${target} PRIVATE "${MRTC_USRSCTP_INCLUDE_DIR}")
        target_link_libraries(${target} PRIVATE "${MRTC_USRSCTP_LIBRARY}")
    endif()
endfunction()
