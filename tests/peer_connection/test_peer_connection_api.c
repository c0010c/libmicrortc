#include <micrortc/micrortc.h>

#include <stdio.h>
#include <string.h>

static int read_fixture(char *buffer, size_t buffer_len)
{
    FILE *file = fopen("tests/fixtures/minimal_offer.sdp", "rb");
    size_t read_len;

    if (file == 0) {
        return 0;
    }

    read_len = fread(buffer, 1, buffer_len - 1, file);
    fclose(file);

    buffer[read_len] = '\0';
    return read_len > 0;
}

static int contains(const char *text, const char *needle)
{
    return strstr(text, needle) != 0;
}

typedef struct TestCallbacks {
    int candidate_count;
    int real_candidate_count;
    int saw_connecting;
    int saw_connected;
    int open_count;
    int text_pong_count;
    int text_nonce_pong_count;
    int binary_count;
    int close_count;
    int media_frame_count;
    int picture_loss_count;
} TestCallbacks;

static void on_ice_candidate(void *user_data, const char *candidate)
{
    TestCallbacks *callbacks = (TestCallbacks *) user_data;

    if (candidate != 0 && contains(candidate, "typ host")) {
        callbacks->candidate_count++;
        if (!contains(candidate, " 9 typ host")) {
            callbacks->real_candidate_count++;
        }
    }
}

static void on_connection_state_change(void *user_data, MRTC_PEER_CONNECTION_STATE state)
{
    TestCallbacks *callbacks = (TestCallbacks *) user_data;

    if (state == MRTC_PEER_CONNECTION_STATE_CONNECTING) {
        callbacks->saw_connecting = 1;
    } else if (state == MRTC_PEER_CONNECTION_STATE_CONNECTED) {
        callbacks->saw_connected = 1;
    }
}

static void on_data_channel_close(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel)
{
    TestCallbacks *callbacks = (TestCallbacks *) user_data;
    (void) channel;
    callbacks->close_count++;
}

static void on_data_channel_open(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel)
{
    TestCallbacks *callbacks = (TestCallbacks *) user_data;
    (void) channel;
    callbacks->open_count++;
}

static void on_data_channel_message(void *user_data,
                                    MRTC_DATA_CHANNEL_HANDLE channel,
                                    MRTC_DATA_CHANNEL_MESSAGE_TYPE message_type,
                                    const unsigned char *data,
                                    size_t data_len)
{
    TestCallbacks *callbacks = (TestCallbacks *) user_data;
    (void) channel;
    if (message_type == MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT && data_len == 4 && memcmp(data, "pong", 4) == 0) {
        callbacks->text_pong_count++;
    }
    if (message_type == MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT && data_len == 14 && memcmp(data, "pong:test-1234", 14) == 0) {
        callbacks->text_nonce_pong_count++;
    }
    if (message_type == MRTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY && data_len == 4 &&
        data[0] == 0x00 && data[1] == 0x01 && data[2] == 0xFE && data[3] == 0xFF) {
        callbacks->binary_count++;
    }
}

static void on_media_frame(void *user_data, MRTC_RTP_TRANSCEIVER_HANDLE transceiver, const MRTC_FRAME *frame)
{
    TestCallbacks *callbacks = (TestCallbacks *) user_data;
    (void) transceiver;
    if (frame != 0 && frame->data != 0 && frame->size > 0) {
        callbacks->media_frame_count++;
    }
}

static void on_picture_loss(void *user_data, MRTC_RTP_TRANSCEIVER_HANDLE transceiver)
{
    TestCallbacks *callbacks = (TestCallbacks *) user_data;
    (void) transceiver;
    callbacks->picture_loss_count++;
}

static int media_api_contract_is_visible(void)
{
    MRTC_TRANSCEIVER_INIT init = {0};
    MRTC_TRANSCEIVER_CALLBACKS media_callbacks = {0};
    MRTC_FRAME frame = {0};
    unsigned char payload[] = {0x65, 0x88};

    media_callbacks.on_frame = on_media_frame;
    media_callbacks.on_picture_loss = on_picture_loss;
    init.kind = MRTC_MEDIA_KIND_VIDEO;
    init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;
    init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    init.callbacks = media_callbacks;

    frame.data = payload;
    frame.size = sizeof(payload);
    frame.presentation_ts = 1000;
    frame.decoding_ts = 900;
    frame.duration = 33333;
    frame.index = 7;
    frame.flags = MRTC_FRAME_FLAG_KEY_FRAME;

    return init.kind == MRTC_MEDIA_KIND_VIDEO &&
           init.codec == MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1 &&
           init.direction == MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV &&
           init.callbacks.on_frame == on_media_frame &&
           frame.data == payload &&
           frame.size == sizeof(payload) &&
           frame.presentation_ts == 1000 &&
           frame.decoding_ts == 900 &&
           frame.duration == 33333 &&
           frame.index == 7 &&
           (frame.flags & MRTC_FRAME_FLAG_KEY_FRAME) != 0 &&
           MRTC_MEDIA_KIND_AUDIO == 0 &&
           MRTC_CODEC_OPUS == 1 &&
           MRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY == 1 &&
           MRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY == 2 &&
           MRTC_RTP_TRANSCEIVER_DIRECTION_INACTIVE == 3;
}

int main(void)
{
    MRTC_ICE_SERVER ice_server = {"turn:example.test:3478?transport=udp", "user", "secret"};
    MRTC_PEER_CONNECTION_CONFIG config = {0};
    MRTC_PEER_CONNECTION_CALLBACKS callbacks = {0};
    MRTC_DATA_CHANNEL_CALLBACKS channel_callbacks = {0};
    MRTC_TRANSCEIVER_CALLBACKS media_callbacks = {0};
    MRTC_DATA_CHANNEL_HANDLE channel = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE video_transceiver = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE audio_transceiver = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE recvonly_transceiver = 0;
    MRTC_PEER_CONNECTION_HANDLE handle = 0;
    TestCallbacks callback_state = {0};
    unsigned char encoded_payload[] = {0x65, 0x88, 0x84};
    MRTC_FRAME frame = {0};
    char offer[2048];
    char answer[4096];
    size_t required_len = 0;

    config.ice_servers = &ice_server;
    config.ice_server_count = 1;
    callbacks.on_ice_candidate = on_ice_candidate;
    callbacks.on_connection_state_change = on_connection_state_change;
    channel_callbacks.on_open = on_data_channel_open;
    channel_callbacks.on_message = on_data_channel_message;
    channel_callbacks.on_close = on_data_channel_close;
    media_callbacks.on_frame = on_media_frame;
    media_callbacks.on_picture_loss = on_picture_loss;
    frame.data = encoded_payload;
    frame.size = sizeof(encoded_payload);
    frame.presentation_ts = 3000;
    frame.decoding_ts = 3000;
    frame.duration = 33333;
    frame.index = 1;
    frame.flags = MRTC_FRAME_FLAG_KEY_FRAME;

    if (!read_fixture(offer, sizeof(offer))) {
        return 1;
    }

    if (!media_api_contract_is_visible()) {
        return 1;
    }

    mrtc_peer_connection_free(0);

    if (mrtc_peer_connection_create(0, &callbacks, 0, &handle) != MRTC_STATUS_INVALID_ARG) {
        return 1;
    }

    if (mrtc_peer_connection_create(&config, &callbacks, &callback_state, &handle) != MRTC_STATUS_OK || handle == 0) {
        return 1;
    }
    ice_server.urls = "turn:mutated.invalid:3478?transport=udp";

    {
        MRTC_TRANSCEIVER_INIT video_init = {0};
        MRTC_TRANSCEIVER_INIT audio_init = {0};
        MRTC_TRANSCEIVER_INIT recvonly_init = {0};
        MRTC_FRAME invalid_frame = frame;

        video_init.kind = MRTC_MEDIA_KIND_VIDEO;
        video_init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;
        video_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
        video_init.callbacks = media_callbacks;
        audio_init.kind = MRTC_MEDIA_KIND_AUDIO;
        audio_init.codec = MRTC_CODEC_OPUS;
        audio_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
        audio_init.callbacks = media_callbacks;
        recvonly_init.kind = MRTC_MEDIA_KIND_VIDEO;
        recvonly_init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;
        recvonly_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY;

        if (mrtc_peer_connection_add_transceiver(0, &video_init, &callback_state, &video_transceiver) != MRTC_STATUS_INVALID_ARG ||
            mrtc_peer_connection_add_transceiver(handle, 0, &callback_state, &video_transceiver) != MRTC_STATUS_INVALID_ARG ||
            mrtc_peer_connection_add_transceiver(handle, &video_init, &callback_state, 0) != MRTC_STATUS_INVALID_ARG) {
            mrtc_peer_connection_free(handle);
            return 1;
        }
        video_init.codec = MRTC_CODEC_OPUS;
        if (mrtc_peer_connection_add_transceiver(handle, &video_init, &callback_state, &video_transceiver) != MRTC_STATUS_INVALID_ARG) {
            mrtc_peer_connection_free(handle);
            return 1;
        }
        video_init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;

        if (mrtc_peer_connection_add_transceiver(handle, &video_init, &callback_state, &video_transceiver) != MRTC_STATUS_OK ||
            video_transceiver == 0 ||
            mrtc_peer_connection_add_transceiver(handle, &audio_init, &callback_state, &audio_transceiver) != MRTC_STATUS_OK ||
            audio_transceiver == 0 ||
            mrtc_peer_connection_add_transceiver(handle, &recvonly_init, &callback_state, &recvonly_transceiver) != MRTC_STATUS_OK ||
            recvonly_transceiver == 0) {
            mrtc_peer_connection_free(handle);
            return 1;
        }

        if (mrtc_transceiver_set_callbacks(0, &media_callbacks, &callback_state) != MRTC_STATUS_INVALID_ARG ||
            mrtc_transceiver_set_callbacks(video_transceiver, 0, &callback_state) != MRTC_STATUS_INVALID_ARG ||
            mrtc_transceiver_set_callbacks(video_transceiver, &media_callbacks, &callback_state) != MRTC_STATUS_OK ||
            mrtc_transceiver_on_frame(video_transceiver, on_media_frame, &callback_state) != MRTC_STATUS_OK ||
            mrtc_transceiver_on_picture_loss(video_transceiver, on_picture_loss, &callback_state) != MRTC_STATUS_OK) {
            mrtc_peer_connection_free(handle);
            return 1;
        }

        invalid_frame.data = 0;
        if (mrtc_transceiver_write_frame(0, &frame) != MRTC_STATUS_INVALID_ARG ||
            mrtc_transceiver_write_frame(video_transceiver, 0) != MRTC_STATUS_INVALID_ARG ||
            mrtc_transceiver_write_frame(video_transceiver, &invalid_frame) != MRTC_STATUS_INVALID_ARG) {
            mrtc_peer_connection_free(handle);
            return 1;
        }
        invalid_frame = frame;
        invalid_frame.size = 0;
        if (mrtc_transceiver_write_frame(video_transceiver, &invalid_frame) != MRTC_STATUS_INVALID_ARG ||
            mrtc_transceiver_write_frame(video_transceiver, &frame) != MRTC_STATUS_INVALID_STATE ||
            mrtc_transceiver_write_frame(audio_transceiver, &frame) != MRTC_STATUS_INVALID_STATE ||
            mrtc_transceiver_write_frame(recvonly_transceiver, &frame) != MRTC_STATUS_INVALID_STATE) {
            mrtc_peer_connection_free(handle);
            return 1;
        }
    }

    if (mrtc_peer_connection_create_answer(handle, answer, sizeof(answer), &required_len) != MRTC_STATUS_INVALID_STATE) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_set_remote_description(handle, "offer", "not-sdp") != MRTC_STATUS_PARSE_ERROR) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_set_remote_description(handle, "offer", offer) != MRTC_STATUS_OK) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    if (!callback_state.saw_connecting) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_create_data_channel(handle, "chat", 0, &channel_callbacks, &callback_state, &channel) != MRTC_STATUS_OK ||
        channel == 0) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    if (mrtc_data_channel_label(0) != 0 ||
        strcmp(mrtc_data_channel_label(channel), "chat") != 0 ||
        mrtc_data_channel_id(channel) != 0) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    if (mrtc_data_channel_send(channel, MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT, (const unsigned char *) "ping", 4) != MRTC_STATUS_INVALID_STATE) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_create_answer(handle, 0, 0, &required_len) != MRTC_STATUS_INVALID_ARG || required_len == 0) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_create_answer(handle, answer, sizeof(answer), &required_len) != MRTC_STATUS_OK) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (!contains(answer, "v=0") || !contains(answer, "m=") || !contains(answer, "a=fingerprint:sha-256") ||
        !contains(answer, "a=ice-ufrag:") || !contains(answer, "m=application") || !contains(answer, "a=sctp-port:5000") ||
        !contains(answer, "m=video 9 UDP/TLS/RTP/SAVPF 96") ||
        !contains(answer, "a=mid:0") ||
        !contains(answer, "a=rtpmap:96 H264/90000") ||
        !contains(answer, "a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f") ||
        !contains(answer, "a=rtcp-fb:96 nack") ||
        !contains(answer, "a=rtcp-fb:96 nack pli") ||
        !contains(answer, "m=audio 9 UDP/TLS/RTP/SAVPF 111") ||
        !contains(answer, "a=mid:1") ||
        !contains(answer, "a=rtpmap:111 opus/48000/2") ||
        !contains(answer, "a=fmtp:111 minptime=10;useinbandfec=1") ||
        !contains(answer, "a=mid:2") ||
        !contains(answer, "a=group:BUNDLE 0 1 2") ||
        strstr(answer, "m=video 9 UDP/TLS/RTP/SAVPF 96") > strstr(answer, "m=audio 9 UDP/TLS/RTP/SAVPF 111") ||
        strstr(answer, "m=audio 9 UDP/TLS/RTP/SAVPF 111") > strstr(answer, "m=application 9 UDP/DTLS/SCTP webrtc-datachannel") ||
        !contains(answer, "a=rtcp-mux")) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_set_local_description(handle, "answer", answer) != MRTC_STATUS_OK) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    if (callback_state.candidate_count == 0 || callback_state.real_candidate_count == 0) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_create_offer(handle, answer, sizeof(answer), &required_len) != MRTC_STATUS_OK ||
        !contains(answer, "m=video 9 UDP/TLS/RTP/SAVPF 96") ||
        !contains(answer, "m=audio 9 UDP/TLS/RTP/SAVPF 111") ||
        !contains(answer, "m=application 9 UDP/DTLS/SCTP webrtc-datachannel") ||
        !contains(answer, "a=setup:actpass")) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_add_ice_candidate(handle, "candidate:1 1 UDP 2122252543 192.0.2.1 54400 typ host") != MRTC_STATUS_OK) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    if (!callback_state.saw_connected || callback_state.open_count != 1) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    {
        const unsigned char binary[] = {0x00, 0x01, 0xFE, 0xFF};
        if (mrtc_data_channel_send(channel, MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT, (const unsigned char *) "ping", 4) != MRTC_STATUS_OK ||
            mrtc_data_channel_send(channel, MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT, (const unsigned char *) "ping:test-1234", 14) != MRTC_STATUS_OK ||
            mrtc_data_channel_send(channel, MRTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY, binary, sizeof(binary)) != MRTC_STATUS_OK ||
            callback_state.text_pong_count != 1 ||
            callback_state.text_nonce_pong_count != 1 ||
            callback_state.binary_count != 1) {
            mrtc_peer_connection_free(handle);
            return 1;
        }
    }
    if (mrtc_peer_connection_add_ice_candidate(handle, "not-a-candidate") != MRTC_STATUS_PARSE_ERROR) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    if (mrtc_data_channel_close(channel) != MRTC_STATUS_OK || callback_state.close_count != 1) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    if (mrtc_data_channel_close(channel) != MRTC_STATUS_INVALID_STATE) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    mrtc_peer_connection_free(handle);
    return 0;
}
