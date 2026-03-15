add_executable(webrtc_smoke_test
    test_smoke.c
)

target_link_libraries(webrtc_smoke_test
    PRIVATE
        webrtc_core
)

add_test(
    NAME test_smoke
    COMMAND webrtc_smoke_test
)

add_executable(test_config_defaults
    test_config_defaults.c
)

target_link_libraries(test_config_defaults
    PRIVATE
        webrtc_core
)

add_test(
    NAME test_config_defaults
    COMMAND test_config_defaults
)

add_executable(test_config_validate_bounds
    test_config_validate_bounds.c
)

target_link_libraries(test_config_validate_bounds
    PRIVATE
        webrtc_core
)

add_test(
    NAME test_config_validate_bounds
    COMMAND test_config_validate_bounds
)

add_executable(test_event_queue_bounded
    test_event_queue_bounded.c
)

target_link_libraries(test_event_queue_bounded
    PRIVATE
        webrtc_core
)

add_test(
    NAME test_event_queue_bounded
    COMMAND test_event_queue_bounded
)

add_executable(test_event_queue_backpressure
    test_event_queue_backpressure.c
)

target_link_libraries(test_event_queue_backpressure
    PRIVATE
        webrtc_core
)

add_test(
    NAME test_event_queue_backpressure
    COMMAND test_event_queue_backpressure
)

if(WEBRTC_PORT_POSIX)
    add_executable(test_pal_posix_smoke
        test_pal_posix_smoke.c
    )

    target_include_directories(test_pal_posix_smoke
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )

    target_link_libraries(test_pal_posix_smoke
        PRIVATE
            webrtc_core
    )

    add_test(
        NAME test_pal_posix_smoke
        COMMAND test_pal_posix_smoke
    )

    add_executable(test_pal_contract
        test_pal_contract.c
    )

    target_include_directories(test_pal_contract
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/.."
    )

    target_link_libraries(test_pal_contract
        PRIVATE
            webrtc_core
    )

    add_test(
        NAME test_pal_contract
        COMMAND test_pal_contract
    )
endif()
