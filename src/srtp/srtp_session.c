#include "srtp_session.h"

MRTC_STATUS mrtc_srtp_session_init(MRTC_SRTP_SESSION *session)
{
    if (session == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    session->ready = 1;
    return MRTC_STATUS_OK;
}

void mrtc_srtp_session_deinit(MRTC_SRTP_SESSION *session)
{
    if (session != 0) {
        session->ready = 0;
    }
}
