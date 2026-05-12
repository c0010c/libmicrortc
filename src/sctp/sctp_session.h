#ifndef MRTC_SCTP_SESSION_H
#define MRTC_SCTP_SESSION_H

#include <micrortc/micrortc.h>

typedef struct MRTC_SCTP_SESSION {
    int initialized;
    int connected;
} MRTC_SCTP_SESSION;

MRTC_STATUS mrtc_sctp_global_init(void);
void mrtc_sctp_global_deinit(void);
MRTC_STATUS mrtc_sctp_session_init(MRTC_SCTP_SESSION *session);
void mrtc_sctp_session_deinit(MRTC_SCTP_SESSION *session);

#endif /* MRTC_SCTP_SESSION_H */
