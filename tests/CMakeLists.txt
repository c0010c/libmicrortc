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
