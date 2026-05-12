#include "dtls_session.h"

#include "common/mrtc_common.h"

#include <stdio.h>
#include <string.h>

MRTC_STATUS mrtc_dtls_generate_fingerprint(char *buffer, size_t buffer_len, size_t *required_len)
{
    uint8_t bytes[32];
    char text[96];
    size_t offset = 0;
    size_t i;

    if (required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (mrtc_random_bytes(bytes, sizeof(bytes)) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_STATE;
    }

    for (i = 0; i < sizeof(bytes); ++i) {
        int written = snprintf(text + offset, sizeof(text) - offset, "%s%02X", i == 0 ? "" : ":", bytes[i]);
        if (written < 0) {
            return MRTC_STATUS_INVALID_STATE;
        }
        offset += (size_t) written;
    }

    *required_len = strlen(text) + 1;
    if (buffer == 0 || buffer_len < *required_len) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memcpy(buffer, text, *required_len);
    return MRTC_STATUS_OK;
}

MRTC_DTLS_ROLE mrtc_dtls_role_from_remote_setup(const char *remote_setup)
{
    if (remote_setup != 0 && strcmp(remote_setup, "active") == 0) {
        return MRTC_DTLS_ROLE_SERVER;
    }
    return MRTC_DTLS_ROLE_CLIENT;
}
