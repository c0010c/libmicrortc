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

int main(void)
{
    MRTC_PEER_CONNECTION_CONFIG config = {0};
    MRTC_PEER_CONNECTION_CALLBACKS callbacks = {0};
    MRTC_PEER_CONNECTION_HANDLE handle = 0;
    char offer[2048];
    char answer[2048];
    size_t required_len = 0;

    if (!read_fixture(offer, sizeof(offer))) {
        return 1;
    }

    mrtc_peer_connection_free(0);

    if (mrtc_peer_connection_create(0, &callbacks, 0, &handle) != MRTC_STATUS_INVALID_ARG) {
        return 1;
    }

    if (mrtc_peer_connection_create(&config, &callbacks, &config, &handle) != MRTC_STATUS_OK || handle == 0) {
        return 1;
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

    if (mrtc_peer_connection_create_answer(handle, 0, 0, &required_len) != MRTC_STATUS_INVALID_ARG || required_len == 0) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_create_answer(handle, answer, sizeof(answer), &required_len) != MRTC_STATUS_OK) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (!contains(answer, "v=0") || !contains(answer, "m=")) {
        mrtc_peer_connection_free(handle);
        return 1;
    }

    if (mrtc_peer_connection_set_local_description(handle, "answer", answer) != MRTC_STATUS_OK) {
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

    mrtc_peer_connection_free(handle);
    return 0;
}
