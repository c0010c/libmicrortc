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
    if (session == 0 || !session->initialized) {
        return MRTC_STATUS_INVALID_ARG;
    }
    session->connected = 0;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_sctp_session_receive_dcep_open(MRTC_SCTP_SESSION *session,
                                                uint16_t stream_id,
                                                const char *label)
{
    static const uint8_t dcep_ack[] = {0x02};
    uint8_t dcep_open[260];
    size_t label_len;

    if (session == 0 || !session->initialized || label == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    label_len = strlen(label);
    if (label_len > 255u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memset(dcep_open, 0, sizeof(dcep_open));
    dcep_open[0] = 0x03;
    dcep_open[1] = 0x00;
    dcep_open[8] = (uint8_t) ((label_len >> 8) & 0xffu);
    dcep_open[9] = (uint8_t) (label_len & 0xffu);
    memcpy(dcep_open + 12u, label, label_len);
    session->connected = 1;
    session->dcep_open_received = 1;
    if (session->on_message != 0) {
        session->on_message(session->user_data, stream_id, MRTC_SCTP_PPID_DCEP, dcep_open, 12u + label_len);
    }
    if (session->on_outbound != 0) {
        session->on_outbound(session->user_data, dcep_ack, sizeof(dcep_ack));
    }
    session->dcep_ack_sent = 1;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_sctp_session_receive_message(MRTC_SCTP_SESSION *session,
                                              uint16_t stream_id,
                                              uint32_t ppid,
                                              const uint8_t *message,
                                              size_t message_len)
{
    if (session == 0 || (message == 0 && message_len > 0u)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!session->initialized || !session->connected) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (session->on_message != 0) {
        session->on_message(session->user_data, stream_id, ppid, message, message_len);
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
    (void) stream_id;
    (void) ppid;
    return MRTC_STATUS_OK;
}

void mrtc_sctp_session_deinit(MRTC_SCTP_SESSION *session)
{
    if (session != 0 && session->initialized) {
        memset(session, 0, sizeof(*session));
        mrtc_sctp_global_deinit();
    }
}
