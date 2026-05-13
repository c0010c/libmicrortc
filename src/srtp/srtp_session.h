#ifndef MRTC_SRTP_SESSION_H
#define MRTC_SRTP_SESSION_H

#include <micrortc/micrortc.h>

#include "../dtls/dtls_session.h"

typedef struct MRTC_SRTP_SESSION {
    int ready;
    int passthrough;
    MRTC_DTLS_ROLE local_role;
    uint8_t transmit_key[16];
    uint8_t receive_key[16];
    uint8_t transmit_salt[14];
    uint8_t receive_salt[14];
    uint8_t transmit_srtp_key[30];
    uint8_t receive_srtp_key[30];
    char profile[32];
    void *transmit_session;
    void *receive_session;
} MRTC_SRTP_SESSION;

MRTC_STATUS mrtc_srtp_session_init(MRTC_SRTP_SESSION *session);
MRTC_STATUS mrtc_srtp_session_init_from_dtls(MRTC_SRTP_SESSION *session,
                                             const MRTC_DTLS_KEYING_MATERIAL *keying_material,
                                             MRTC_DTLS_ROLE local_role);
void mrtc_srtp_session_deinit(MRTC_SRTP_SESSION *session);
int mrtc_srtp_session_is_passthrough(const MRTC_SRTP_SESSION *session);
MRTC_STATUS mrtc_srtp_protect_rtp(MRTC_SRTP_SESSION *session, uint8_t *packet, size_t capacity, size_t *packet_size);
MRTC_STATUS mrtc_srtp_unprotect_rtp(MRTC_SRTP_SESSION *session, uint8_t *packet, size_t *packet_size);
MRTC_STATUS mrtc_srtp_protect_rtcp(MRTC_SRTP_SESSION *session, uint8_t *packet, size_t capacity, size_t *packet_size);
MRTC_STATUS mrtc_srtp_unprotect_rtcp(MRTC_SRTP_SESSION *session, uint8_t *packet, size_t *packet_size);

#endif /* MRTC_SRTP_SESSION_H */
