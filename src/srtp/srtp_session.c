#include "srtp_session.h"

#include <string.h>

#ifdef MRTC_HAVE_SRTP
#if defined(__has_include)
#if __has_include(<srtp2/srtp.h>)
#include <srtp2/srtp.h>
#else
#include <srtp/srtp.h>
#endif
#else
#include <srtp2/srtp.h>
#endif
#endif

static void mrtc_srtp_copy_key_material(uint8_t *combined, const uint8_t *key, const uint8_t *salt)
{
    memcpy(combined, key, 16u);
    memcpy(combined + 16u, salt, 14u);
}

#ifdef MRTC_HAVE_SRTP
static MRTC_STATUS mrtc_srtp_create_context(void **context, uint8_t *key, int outbound)
{
    srtp_policy_t policy;
    srtp_t created = 0;
    srtp_err_status_t err;

    if (context == 0 || key == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type = outbound ? ssrc_any_outbound : ssrc_any_inbound;
    policy.key = key;
    policy.next = 0;

    err = srtp_create(&created, &policy);
    if (err != srtp_err_status_ok) {
        return MRTC_STATUS_INVALID_STATE;
    }

    *context = created;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_srtp_init_libsrtp_contexts(MRTC_SRTP_SESSION *session)
{
    static int initialized = 0;
    MRTC_STATUS status;

    if (!initialized) {
        if (srtp_init() != srtp_err_status_ok) {
            return MRTC_STATUS_INVALID_STATE;
        }
        initialized = 1;
    }

    status = mrtc_srtp_create_context(&session->receive_session, session->receive_srtp_key, 0);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_srtp_create_context(&session->transmit_session, session->transmit_srtp_key, 1);
    if (status != MRTC_STATUS_OK) {
        if (session->receive_session != 0) {
            (void) srtp_dealloc((srtp_t) session->receive_session);
            session->receive_session = 0;
        }
        return status;
    }
    return MRTC_STATUS_OK;
}
#endif

MRTC_STATUS mrtc_srtp_session_init(MRTC_SRTP_SESSION *session)
{
    if (session == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(session, 0, sizeof(*session));
    session->ready = 1;
    session->passthrough = 1;
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
        memcpy(session->transmit_salt, keying_material->client_write_salt, sizeof(session->transmit_salt));
        memcpy(session->receive_salt, keying_material->server_write_salt, sizeof(session->receive_salt));
    } else {
        memcpy(session->transmit_key, keying_material->server_write_key, sizeof(session->transmit_key));
        memcpy(session->receive_key, keying_material->client_write_key, sizeof(session->receive_key));
        memcpy(session->transmit_salt, keying_material->server_write_salt, sizeof(session->transmit_salt));
        memcpy(session->receive_salt, keying_material->client_write_salt, sizeof(session->receive_salt));
    }
    mrtc_srtp_copy_key_material(session->transmit_srtp_key, session->transmit_key, session->transmit_salt);
    mrtc_srtp_copy_key_material(session->receive_srtp_key, session->receive_key, session->receive_salt);
    strncpy(session->profile, keying_material->profile, sizeof(session->profile) - 1);

#ifdef MRTC_HAVE_SRTP
    if (mrtc_srtp_init_libsrtp_contexts(session) != MRTC_STATUS_OK) {
        mrtc_srtp_session_deinit(session);
        return MRTC_STATUS_INVALID_STATE;
    }
    session->passthrough = 0;
#else
    session->passthrough = 1;
#endif

    return MRTC_STATUS_OK;
}

void mrtc_srtp_session_deinit(MRTC_SRTP_SESSION *session)
{
    if (session != 0) {
#ifdef MRTC_HAVE_SRTP
        if (session->transmit_session != 0) {
            (void) srtp_dealloc((srtp_t) session->transmit_session);
        }
        if (session->receive_session != 0) {
            (void) srtp_dealloc((srtp_t) session->receive_session);
        }
#endif
        memset(session, 0, sizeof(*session));
    }
}

int mrtc_srtp_session_is_passthrough(const MRTC_SRTP_SESSION *session)
{
    return session != 0 && session->ready && session->passthrough;
}

static MRTC_STATUS mrtc_srtp_passthrough_packet(MRTC_SRTP_SESSION *session, uint8_t *packet, size_t *packet_size)
{
    if (session == 0 || packet == 0 || packet_size == 0 || *packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!session->ready) {
        return MRTC_STATUS_INVALID_STATE;
    }
    return MRTC_STATUS_OK;
}

#ifdef MRTC_HAVE_SRTP
static MRTC_STATUS mrtc_srtp_call_protect(void *context, uint8_t *packet, size_t capacity, size_t *packet_size, int rtcp)
{
    int len;
    srtp_err_status_t err;

    if (context == 0 || packet == 0 || packet_size == 0 || *packet_size == 0u || *packet_size > (size_t) 0x7fffffff) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (capacity < *packet_size + 32u) {
        return MRTC_STATUS_INVALID_ARG;
    }

    len = (int) *packet_size;
    err = rtcp ? srtp_protect_rtcp((srtp_t) context, packet, &len) : srtp_protect((srtp_t) context, packet, &len);
    if (err != srtp_err_status_ok || len < 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    *packet_size = (size_t) len;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_srtp_call_unprotect(void *context, uint8_t *packet, size_t *packet_size, int rtcp)
{
    int len;
    srtp_err_status_t err;

    if (context == 0 || packet == 0 || packet_size == 0 || *packet_size == 0u || *packet_size > (size_t) 0x7fffffff) {
        return MRTC_STATUS_INVALID_ARG;
    }

    len = (int) *packet_size;
    err = rtcp ? srtp_unprotect_rtcp((srtp_t) context, packet, &len) : srtp_unprotect((srtp_t) context, packet, &len);
    if (err != srtp_err_status_ok || len < 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    *packet_size = (size_t) len;
    return MRTC_STATUS_OK;
}
#endif

MRTC_STATUS mrtc_srtp_protect_rtp(MRTC_SRTP_SESSION *session, uint8_t *packet, size_t capacity, size_t *packet_size)
{
    if (session == 0 || !session->ready) {
        return MRTC_STATUS_INVALID_STATE;
    }
#ifdef MRTC_HAVE_SRTP
    if (!session->passthrough) {
        return mrtc_srtp_call_protect(session->transmit_session, packet, capacity, packet_size, 0);
    }
#endif
    (void) capacity;
    return mrtc_srtp_passthrough_packet(session, packet, packet_size);
}

MRTC_STATUS mrtc_srtp_unprotect_rtp(MRTC_SRTP_SESSION *session, uint8_t *packet, size_t *packet_size)
{
    if (session == 0 || !session->ready) {
        return MRTC_STATUS_INVALID_STATE;
    }
#ifdef MRTC_HAVE_SRTP
    if (!session->passthrough) {
        return mrtc_srtp_call_unprotect(session->receive_session, packet, packet_size, 0);
    }
#endif
    return mrtc_srtp_passthrough_packet(session, packet, packet_size);
}

MRTC_STATUS mrtc_srtp_protect_rtcp(MRTC_SRTP_SESSION *session, uint8_t *packet, size_t capacity, size_t *packet_size)
{
    if (session == 0 || !session->ready) {
        return MRTC_STATUS_INVALID_STATE;
    }
#ifdef MRTC_HAVE_SRTP
    if (!session->passthrough) {
        return mrtc_srtp_call_protect(session->transmit_session, packet, capacity, packet_size, 1);
    }
#endif
    (void) capacity;
    return mrtc_srtp_passthrough_packet(session, packet, packet_size);
}

MRTC_STATUS mrtc_srtp_unprotect_rtcp(MRTC_SRTP_SESSION *session, uint8_t *packet, size_t *packet_size)
{
    if (session == 0 || !session->ready) {
        return MRTC_STATUS_INVALID_STATE;
    }
#ifdef MRTC_HAVE_SRTP
    if (!session->passthrough) {
        return mrtc_srtp_call_unprotect(session->receive_session, packet, packet_size, 1);
    }
#endif
    return mrtc_srtp_passthrough_packet(session, packet, packet_size);
}
