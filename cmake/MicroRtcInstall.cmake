include(CMakePackageConfigHelpers)

function(mrtc_install_package target)
    install(
        TARGETS ${target}
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

    set(MRTC_CONFIG_FIND_OPENSSL "")
    if(MRTC_ENABLE_PHASE4_TRANSPORT AND OpenSSL_FOUND)
        set(MRTC_CONFIG_FIND_OPENSSL "find_dependency(OpenSSL)")
    endif()

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
endfunction()
