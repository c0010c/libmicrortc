include(FindPackageHandleStandardArgs)

if(MRTC_ENABLE_PHASE4_TRANSPORT)
    set(MRTC_CRYPTO_BACKEND "openssl" CACHE STRING "Crypto/DTLS backend: openssl or mbedtls")
    set_property(CACHE MRTC_CRYPTO_BACKEND PROPERTY STRINGS openssl mbedtls)
    option(MRTC_ENABLE_DATA_CHANNEL_TRANSPORT "Enable SCTP data channel transport dependencies" ON)

    if(MRTC_CRYPTO_BACKEND STREQUAL "mbedtls")
        find_path(MRTC_MBEDTLS_INCLUDE_DIR NAMES mbedtls/ssl.h)
        find_library(MRTC_MBEDTLS_LIBRARY NAMES mbedtls)
        find_library(MRTC_MBEDX509_LIBRARY NAMES mbedx509)
        find_library(MRTC_MBEDCRYPTO_LIBRARY NAMES mbedcrypto)
        find_package_handle_standard_args(MRTC_MBEDTLS
            DEFAULT_MSG
            MRTC_MBEDTLS_LIBRARY
            MRTC_MBEDX509_LIBRARY
            MRTC_MBEDCRYPTO_LIBRARY
            MRTC_MBEDTLS_INCLUDE_DIR)
    else()
        find_package(OpenSSL QUIET)
    endif()
    find_library(MRTC_SRTP_LIBRARY NAMES srtp2 srtp)
    find_path(MRTC_SRTP_INCLUDE_DIR NAMES srtp2/srtp.h srtp/srtp.h)
    if(MRTC_ENABLE_DATA_CHANNEL_TRANSPORT)
        find_library(MRTC_USRSCTP_LIBRARY NAMES usrsctp)
        find_path(MRTC_USRSCTP_INCLUDE_DIR NAMES usrsctp.h)
    endif()

    find_package_handle_standard_args(MRTC_SRTP DEFAULT_MSG MRTC_SRTP_LIBRARY MRTC_SRTP_INCLUDE_DIR)
    if(MRTC_ENABLE_DATA_CHANNEL_TRANSPORT)
        find_package_handle_standard_args(MRTC_USRSCTP DEFAULT_MSG MRTC_USRSCTP_LIBRARY MRTC_USRSCTP_INCLUDE_DIR)
    endif()

    if(MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS)
        set(MRTC_MISSING_TRANSPORT_DEPS "")
        if(MRTC_CRYPTO_BACKEND STREQUAL "mbedtls")
            if(NOT MRTC_MBEDTLS_FOUND)
                list(APPEND MRTC_MISSING_TRANSPORT_DEPS "mbedTLS")
            endif()
        else()
            if(NOT OpenSSL_FOUND)
                list(APPEND MRTC_MISSING_TRANSPORT_DEPS "OpenSSL")
            endif()
        endif()
        if(NOT MRTC_SRTP_FOUND)
            list(APPEND MRTC_MISSING_TRANSPORT_DEPS "libsrtp")
        endif()
        if(MRTC_ENABLE_DATA_CHANNEL_TRANSPORT AND NOT MRTC_USRSCTP_FOUND)
            list(APPEND MRTC_MISSING_TRANSPORT_DEPS "usrsctp")
        endif()
        if(MRTC_MISSING_TRANSPORT_DEPS)
            string(REPLACE ";" ", " MRTC_MISSING_TRANSPORT_DEPS_TEXT "${MRTC_MISSING_TRANSPORT_DEPS}")
            message(FATAL_ERROR
                "Missing required system transport dependencies: ${MRTC_MISSING_TRANSPORT_DEPS_TEXT}. "
                "A crypto backend, libsrtp and the enabled optional transports are required when MRTC_REQUIRE_SYSTEM_TRANSPORT_DEPS=ON. "
                "On Ubuntu/Debian install development packages for libssl or mbedtls, libsrtp2 and usrsctp when data channels are enabled before rerunning strict transport verification."
            )
        endif()
    endif()
endif()

function(mrtc_apply_transport_deps target)
    if(NOT MRTC_ENABLE_PHASE4_TRANSPORT)
        return()
    endif()

    target_compile_definitions(${target} PRIVATE MRTC_ENABLE_PHASE4_TRANSPORT=1)

    if(MRTC_CRYPTO_BACKEND STREQUAL "mbedtls")
        if(MRTC_MBEDTLS_FOUND)
            target_compile_definitions(${target} PRIVATE MRTC_HAVE_MBEDTLS=1)
            target_include_directories(${target} PRIVATE "${MRTC_MBEDTLS_INCLUDE_DIR}")
            target_link_libraries(${target} PRIVATE
                "${MRTC_MBEDTLS_LIBRARY}"
                "${MRTC_MBEDX509_LIBRARY}"
                "${MRTC_MBEDCRYPTO_LIBRARY}")
        endif()
    elseif(OpenSSL_FOUND)
        target_compile_definitions(${target} PRIVATE MRTC_HAVE_OPENSSL=1)
        target_link_libraries(${target} PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()

    if(MRTC_SRTP_FOUND)
        target_compile_definitions(${target} PRIVATE MRTC_HAVE_SRTP=1)
        target_include_directories(${target} PRIVATE "${MRTC_SRTP_INCLUDE_DIR}")
        target_link_libraries(${target} PRIVATE "${MRTC_SRTP_LIBRARY}")
    endif()

    if(MRTC_ENABLE_DATA_CHANNEL_TRANSPORT AND MRTC_USRSCTP_FOUND)
        target_compile_definitions(${target} PRIVATE MRTC_HAVE_USRSCTP=1)
        target_include_directories(${target} PRIVATE "${MRTC_USRSCTP_INCLUDE_DIR}")
        target_link_libraries(${target} PRIVATE "${MRTC_USRSCTP_LIBRARY}")
    endif()
endfunction()
