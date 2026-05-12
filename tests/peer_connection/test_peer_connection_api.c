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
    int saw_connecting;
    int saw_connected;
    int open_count;
    int text_pong_count;
    int binary_count;
    int close_count;
} TestCallbacks;

static void on_ice_candidate(void *user_data, const char *candidate)
{
    TestCallbacks *callbacks = (TestCallbacks *) user_data;

    if (candidate != 0 && contains(candidate, "typ host")) {
        callbacks->candidate_count++;
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
    if (message_type == MRTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY && data_len == 4 &&
        data[0] == 0x00 && data[1] == 0x01 && data[2] == 0xFE && data[3] == 0xFF) {
        callbacks->binary_count++;
    }
}

int main(void)
{
    MRTC_ICE_SERVER ice_server = {"turn:example.test:3478?transport=udp", "user", "secret"};
    MRTC_PEER_CONNECTION_CONFIG config = {0};
    MRTC_PEER_CONNECTION_CALLBACKS callbacks = {0};
    MRTC_DATA_CHANNEL_CALLBACKS channel_callbacks = {0};
    MRTC_DATA_CHANNEL_HANDLE channel = 0;
    MRTC_PEER_CONNECTION_HANDLE handle = 0;
    TestCallbacks callback_state = {0};
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

    if (!read_fixture(offer, sizeof(offer))) {
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
        !contains(answer, "a=ice-ufrag:") || !contains(answer, "m=application") || !contains(answer, "a=sctp-port:5000")) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_set_local_description(handle, "answer", answer) != MRTC_STATUS_OK) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    if (callback_state.candidate_count == 0) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_create_offer(handle, answer, sizeof(answer), &required_len) != MRTC_STATUS_NOT_IMPLEMENTED) {
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
            mrtc_data_channel_send(channel, MRTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY, binary, sizeof(binary)) != MRTC_STATUS_OK ||
            callback_state.text_pong_count != 1 ||
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
