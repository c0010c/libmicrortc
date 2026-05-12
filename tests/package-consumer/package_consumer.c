#include <micrortc/micrortc.h>

int main(void)
{
    const char *version = mrtc_version_string();
    MRTC_ICE_SERVER ice_server = {"turn:example.test:3478?transport=udp", "user", "secret"};
    MRTC_PEER_CONNECTION_CONFIG config = {0};
    MRTC_DATA_CHANNEL_INIT channel_init = {0};
    MRTC_DATA_CHANNEL_CALLBACKS channel_callbacks = {0};
    MRTC_PEER_CONNECTION_HANDLE handle = 0;
    MRTC_DATA_CHANNEL_HANDLE channel = 0;
    MRTC_STATUS unavailable = MRTC_STATUS_NOT_IMPLEMENTED;
    MRTC_STATUS invalid_state = MRTC_STATUS_INVALID_STATE;
    MRTC_STATUS parse_error = MRTC_STATUS_PARSE_ERROR;

    config.ice_servers = &ice_server;
    config.ice_server_count = 1;
    channel_init.ordered = 1;

    if (version == 0 || version[0] == '\0') {
        return 1;
    }

    if (handle != 0 || unavailable == MRTC_STATUS_OK || invalid_state == MRTC_STATUS_OK || parse_error == MRTC_STATUS_OK) {
        return 1;
    }

    if (mrtc_initialize() != MRTC_STATUS_OK) {
        return 1;
    }
    if (mrtc_peer_connection_create(&config, 0, 0, &handle) != MRTC_STATUS_OK) {
        return 1;
    }
    if (mrtc_peer_connection_create_data_channel(handle, "package", &channel_init, &channel_callbacks, 0, &channel) != MRTC_STATUS_OK) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    if (mrtc_data_channel_send(channel, MRTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY, (const unsigned char *) "x", 1) != MRTC_STATUS_INVALID_STATE) {
        mrtc_peer_connection_free(handle);
        return 1;
    }
    mrtc_peer_connection_free(handle);

    mrtc_shutdown();
    return 0;
}
