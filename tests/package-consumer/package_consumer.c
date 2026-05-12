#include <micrortc/micrortc.h>

int main(void)
{
    const char *version = mrtc_version_string();
    MRTC_PEER_CONNECTION_HANDLE handle = 0;
    MRTC_STATUS unavailable = MRTC_STATUS_NOT_IMPLEMENTED;
    MRTC_STATUS invalid_state = MRTC_STATUS_INVALID_STATE;
    MRTC_STATUS parse_error = MRTC_STATUS_PARSE_ERROR;

    if (version == 0 || version[0] == '\0') {
        return 1;
    }

    if (handle != 0 || unavailable == MRTC_STATUS_OK || invalid_state == MRTC_STATUS_OK || parse_error == MRTC_STATUS_OK) {
        return 1;
    }

    if (mrtc_initialize() != MRTC_STATUS_OK) {
        return 1;
    }

    mrtc_shutdown();
    return 0;
}
