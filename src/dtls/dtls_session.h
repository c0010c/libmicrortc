#ifndef MRTC_DTLS_SESSION_H
#define MRTC_DTLS_SESSION_H

#include <micrortc/micrortc.h>

#include <stddef.h>

typedef enum MRTC_DTLS_ROLE {
    MRTC_DTLS_ROLE_CLIENT = 1,
    MRTC_DTLS_ROLE_SERVER = 2
} MRTC_DTLS_ROLE;

MRTC_STATUS mrtc_dtls_generate_fingerprint(char *buffer, size_t buffer_len, size_t *required_len);
MRTC_DTLS_ROLE mrtc_dtls_role_from_remote_setup(const char *remote_setup);

#endif /* MRTC_DTLS_SESSION_H */
