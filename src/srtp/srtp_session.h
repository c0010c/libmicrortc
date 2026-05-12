#ifndef MRTC_SRTP_SESSION_H
#define MRTC_SRTP_SESSION_H

#include <micrortc/micrortc.h>

typedef struct MRTC_SRTP_SESSION {
    int ready;
} MRTC_SRTP_SESSION;

MRTC_STATUS mrtc_srtp_session_init(MRTC_SRTP_SESSION *session);
void mrtc_srtp_session_deinit(MRTC_SRTP_SESSION *session);

#endif /* MRTC_SRTP_SESSION_H */
