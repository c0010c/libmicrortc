function(mrtc_add_test target source labels)
    set(options WITH_PRIVATE_INCLUDES)
    set(one_value_args WORKING_DIRECTORY)
    set(multi_value_args COMMAND_ARGS)
    cmake_parse_arguments(MRTC_TEST "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    add_executable(${target} "${source}")
    if(MRTC_TEST_WITH_PRIVATE_INCLUDES)
        target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    endif()
    target_link_libraries(${target} PRIVATE micrortc::micrortc)

    add_test(NAME ${target} COMMAND ${target} ${MRTC_TEST_COMMAND_ARGS})

    set_tests_properties(${target} PROPERTIES LABELS "${labels}")
    if(MRTC_TEST_WORKING_DIRECTORY)
        set_tests_properties(${target} PROPERTIES WORKING_DIRECTORY "${MRTC_TEST_WORKING_DIRECTORY}")
    endif()
endfunction()

function(mrtc_add_chrome_e2e_targets)
    set(MRTC_CHROME_E2E_DIR "${CMAKE_CURRENT_SOURCE_DIR}/examples/chrome-e2e")
    set(MRTC_CHROME_ANSWERER_SOURCE "${MRTC_CHROME_E2E_DIR}/mrtc_chrome_answerer.c")

    if(EXISTS "${MRTC_CHROME_ANSWERER_SOURCE}")
        add_executable(mrtc_chrome_answerer "${MRTC_CHROME_ANSWERER_SOURCE}")
        target_include_directories(mrtc_chrome_answerer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(mrtc_chrome_answerer PRIVATE micrortc::micrortc)
        set_target_properties(mrtc_chrome_answerer
            PROPERTIES
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/examples/chrome-e2e"
        )
    endif()

    add_custom_target(mrtc_e2e_help
        COMMAND "${CMAKE_COMMAND}" -E echo "Chrome E2E is explicit: npm --prefix tests/e2e run test:host"
        COMMAND "${CMAKE_COMMAND}" -E echo "Page: examples/chrome-e2e/index.html"
        VERBATIM
    )
endfunction()

function(mrtc_add_project_tests)
    enable_testing()

    mrtc_add_test(micrortc_smoke_test tests/smoke/test_link.c "protocol")
    mrtc_add_test(mrtc_sdp_roundtrip_test tests/sdp/test_sdp_roundtrip.c "protocol;sdp"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    mrtc_add_test(mrtc_peer_connection_api_test tests/peer_connection/test_peer_connection_api.c "protocol;peer_connection"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    mrtc_add_test(mrtc_rtp_packet_test tests/media/test_rtp_packet.c "protocol;media"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        WITH_PRIVATE_INCLUDES
    )
    mrtc_add_test(mrtc_h264_packetizer_test tests/media/test_h264_packetizer.c "protocol;media"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        WITH_PRIVATE_INCLUDES
    )
    mrtc_add_test(mrtc_opus_codec_test tests/media/test_opus_codec.c "protocol;media"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        WITH_PRIVATE_INCLUDES
    )
    mrtc_add_test(mrtc_media_send_test tests/media/test_media_send.c "protocol;media"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        WITH_PRIVATE_INCLUDES
    )
    mrtc_add_test(mrtc_media_fixtures_test tests/media/test_media_fixtures.c "protocol;media"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        WITH_PRIVATE_INCLUDES
    )
    mrtc_add_test(mrtc_rtcp_nack_pli_test tests/media/test_rtcp_packet.c "protocol;media"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        WITH_PRIVATE_INCLUDES
    )
    mrtc_add_test(mrtc_rtp_rolling_buffer_test tests/media/test_rtp_rolling_buffer.c "protocol;media"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        WITH_PRIVATE_INCLUDES
    )
    mrtc_add_test(mrtc_rtcp_retransmit_test tests/media/test_rtcp_retransmit.c "protocol;media"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        WITH_PRIVATE_INCLUDES
    )
    mrtc_add_test(mrtc_rtcp_pli_sr_rr_test tests/media/test_rtcp_pli_sr_rr.c "protocol;media"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        WITH_PRIVATE_INCLUDES
    )
    mrtc_add_test(mrtc_phase4_transport_smoke_test tests/transport/test_transport_smoke.c "protocol;transport"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    mrtc_add_test(mrtc_ice_config_test tests/transport/test_ice_config.c "protocol;transport"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    mrtc_add_test(mrtc_stun_message_test tests/transport/test_stun_message.c "protocol;transport"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    mrtc_add_test(mrtc_dtls_srtp_test tests/transport/test_dtls_srtp.c "protocol;transport"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    mrtc_add_test(mrtc_sctp_data_channel_test tests/transport/test_sctp_data_channel.c "protocol;transport;datachannel"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    add_executable(mrtc_phase4_network_verify tests/integration/mrtc_phase4_network_verify.c)
    target_link_libraries(mrtc_phase4_network_verify PRIVATE micrortc::micrortc)
    set_target_properties(mrtc_phase4_network_verify
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/integration"
    )

    mrtc_add_test(mrtc_phase5_media_verify tests/integration/mrtc_phase5_media_verify.c "integration;media"
        COMMAND_ARGS --fixtures "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        WITH_PRIVATE_INCLUDES
    )
    set_target_properties(mrtc_phase5_media_verify
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/integration"
    )
endfunction()
