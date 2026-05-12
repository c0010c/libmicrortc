#ifndef MRTC_SRTP_SESSION_H
#define MRTC_SRTP_SESSION_H

#include <micrortc/micrortc.h>

#include "../dtls/dtls_session.h"

typedef struct MRTC_SRTP_SESSION {
    int ready;
    MRTC_DTLS_ROLE local_role;
    uint8_t transmit_key[16];
    uint8_t receive_key[16];
    char profile[32];
} MRTC_SRTP_SESSION;

MRTC_STATUS mrtc_srtp_session_init(MRTC_SRTP_SESSION *session);
MRTC_STATUS mrtc_srtp_session_init_from_dtls(MRTC_SRTP_SESSION *session,
                                             const MRTC_DTLS_KEYING_MATERIAL *keying_material,
                                             MRTC_DTLS_ROLE local_role);
void mrtc_srtp_session_deinit(MRTC_SRTP_SESSION *session);

#endif /* MRTC_SRTP_SESSION_H */
