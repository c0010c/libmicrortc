#include "srtp_session.h"

#include <string.h>

MRTC_STATUS mrtc_srtp_session_init(MRTC_SRTP_SESSION *session)
{
    if (session == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    session->ready = 1;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_srtp_session_init_from_dtls(MRTC_SRTP_SESSION *session,
                                             const MRTC_DTLS_KEYING_MATERIAL *keying_material,
                                             MRTC_DTLS_ROLE local_role)
{
    if (session == 0 || keying_material == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(session, 0, sizeof(*session));
    session->ready = 1;
    session->local_role = local_role;
    if (local_role == MRTC_DTLS_ROLE_CLIENT) {
        memcpy(session->transmit_key, keying_material->client_write_key, sizeof(session->transmit_key));
        memcpy(session->receive_key, keying_material->server_write_key, sizeof(session->receive_key));
    } else {
        memcpy(session->transmit_key, keying_material->server_write_key, sizeof(session->transmit_key));
        memcpy(session->receive_key, keying_material->client_write_key, sizeof(session->receive_key));
    }
    strncpy(session->profile, keying_material->profile, sizeof(session->profile) - 1);
    return MRTC_STATUS_OK;
}

void mrtc_srtp_session_deinit(MRTC_SRTP_SESSION *session)
{
    if (session != 0) {
        memset(session, 0, sizeof(*session));
    }
}
