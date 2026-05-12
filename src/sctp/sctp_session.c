#include "sctp_session.h"

#include "common/mrtc_mutex.h"

#include <string.h>

static MRTC_MUTEX g_sctp_mutex;
static int g_sctp_mutex_ready = 0;
static unsigned int g_sctp_ref_count = 0;

MRTC_STATUS mrtc_sctp_global_init(void)
{
    if (!g_sctp_mutex_ready) {
        MRTC_STATUS status = mrtc_mutex_init(&g_sctp_mutex);
        if (status != MRTC_STATUS_OK) {
            return status;
        }
        g_sctp_mutex_ready = 1;
    }

    if (mrtc_mutex_lock(&g_sctp_mutex) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_STATE;
    }
    ++g_sctp_ref_count;
    mrtc_mutex_unlock(&g_sctp_mutex);
    return MRTC_STATUS_OK;
}

void mrtc_sctp_global_deinit(void)
{
    if (!g_sctp_mutex_ready) {
        return;
    }

    if (mrtc_mutex_lock(&g_sctp_mutex) == MRTC_STATUS_OK) {
        if (g_sctp_ref_count > 0) {
            --g_sctp_ref_count;
        }
        mrtc_mutex_unlock(&g_sctp_mutex);
    }
}

MRTC_STATUS mrtc_sctp_session_init(MRTC_SCTP_SESSION *session)
{
    MRTC_STATUS status;

    if (session == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(session, 0, sizeof(*session));
    status = mrtc_sctp_global_init();
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    session->initialized = 1;
    session->connected = 0;
    return MRTC_STATUS_OK;
}

void mrtc_sctp_session_set_callbacks(MRTC_SCTP_SESSION *session,
                                     MRTC_SCTP_OUTBOUND_CALLBACK on_outbound,
                                     MRTC_SCTP_MESSAGE_CALLBACK on_message,
                                     void *user_data)
{
    if (session == 0) {
        return;
    }
    session->on_outbound = on_outbound;
    session->on_message = on_message;
    session->user_data = user_data;
}

MRTC_STATUS mrtc_sctp_session_connect(MRTC_SCTP_SESSION *session)
{
    static const uint8_t dcep_open[] = {'D', 'C', 'E', 'P', '-', 'O', 'P', 'E', 'N'};

    if (session == 0 || !session->initialized) {
        return MRTC_STATUS_INVALID_ARG;
    }
    session->connected = 1;
    if (session->on_outbound != 0) {
        session->on_outbound(session->user_data, dcep_open, sizeof(dcep_open));
    }
    if (session->on_message != 0) {
        session->on_message(session->user_data, 0, MRTC_SCTP_PPID_DCEP, dcep_open, sizeof(dcep_open));
    }
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_sctp_session_write_message(MRTC_SCTP_SESSION *session,
                                            uint16_t stream_id,
                                            int is_binary,
                                            const uint8_t *message,
                                            size_t message_len)
{
    uint32_t ppid;

    if (session == 0 || (message == 0 && message_len > 0)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!session->initialized || !session->connected) {
        return MRTC_STATUS_INVALID_STATE;
    }

    if (is_binary) {
        ppid = message_len == 0 ? MRTC_SCTP_PPID_BINARY_EMPTY : MRTC_SCTP_PPID_BINARY;
    } else {
        ppid = message_len == 0 ? MRTC_SCTP_PPID_STRING_EMPTY : MRTC_SCTP_PPID_STRING;
    }

    if (session->on_outbound != 0) {
        session->on_outbound(session->user_data, message, message_len);
    }
    if (session->on_message != 0) {
        session->on_message(session->user_data, stream_id, ppid, message, message_len);
    }
    return MRTC_STATUS_OK;
}

void mrtc_sctp_session_deinit(MRTC_SCTP_SESSION *session)
{
    if (session != 0 && session->initialized) {
        memset(session, 0, sizeof(*session));
        mrtc_sctp_global_deinit();
    }
}
