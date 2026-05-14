#include "sctp_session.h"

#include "common/mrtc_mutex.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#ifdef MRTC_HAVE_USRSCTP
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <usrsctp.h>
#endif

#define MRTC_SCTP_DCEP_HEADER_LENGTH 12u
#define MRTC_SCTP_SHUTDOWN_ACTIVE 0u
#define MRTC_SCTP_SHUTDOWN_INITIATED 1u
#define MRTC_SCTP_SHUTDOWN_COMPLETED 2u
#define MRTC_SCTP_TEST_FRAME_MAGIC 0x4d535450u

static MRTC_MUTEX g_sctp_mutex;
static int g_sctp_mutex_ready = 0;
static unsigned int g_sctp_ref_count = 0;

static MRTC_STATUS mrtc_sctp_handle_dcep(MRTC_SCTP_SESSION *session,
                                         uint16_t stream_id,
                                         const uint8_t *data,
                                         size_t data_len);

static void mrtc_sctp_put_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t) ((value >> 8) & 0xffu);
    data[1] = (uint8_t) (value & 0xffu);
}

static uint16_t mrtc_sctp_get_u16(const uint8_t *data)
{
    return (uint16_t) (((uint16_t) data[0] << 8) | (uint16_t) data[1]);
}

static void mrtc_sctp_put_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t) ((value >> 24) & 0xffu);
    data[1] = (uint8_t) ((value >> 16) & 0xffu);
    data[2] = (uint8_t) ((value >> 8) & 0xffu);
    data[3] = (uint8_t) (value & 0xffu);
}

static uint32_t mrtc_sctp_get_u32(const uint8_t *data)
{
    return ((uint32_t) data[0] << 24) | ((uint32_t) data[1] << 16) | ((uint32_t) data[2] << 8) | (uint32_t) data[3];
}

static MRTC_STATUS mrtc_sctp_emit_test_frame(MRTC_SCTP_SESSION *session,
                                             uint16_t stream_id,
                                             uint32_t ppid,
                                             const uint8_t *message,
                                             size_t message_len)
{
    uint8_t frame[1600];
    MRTC_SCTP_SESSION *destination;

    if (session == 0 || session->remote_address == 0 || message_len + 14u > sizeof(frame)) {
        return MRTC_STATUS_INVALID_STATE;
    }
    destination = (MRTC_SCTP_SESSION *) session->remote_address;
    if (destination->on_outbound == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    mrtc_sctp_put_u32(frame, MRTC_SCTP_TEST_FRAME_MAGIC);
    mrtc_sctp_put_u16(frame + 4u, stream_id);
    mrtc_sctp_put_u32(frame + 6u, ppid);
    mrtc_sctp_put_u32(frame + 10u, (uint32_t) message_len);
    if (message_len > 0u) {
        memcpy(frame + 14u, message, message_len);
    }
    destination->on_outbound(destination->user_data, frame, message_len + 14u);
    session->last_outbound_ppid = ppid;
    session->last_outbound_stream_id = stream_id;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_sctp_handle_test_frame(MRTC_SCTP_SESSION *session, const uint8_t *packet, size_t packet_len)
{
    uint16_t stream_id;
    uint32_t ppid;
    uint32_t message_len;
    const uint8_t *message;

    if (session == 0 || packet == 0 || packet_len < 14u ||
        mrtc_sctp_get_u32(packet) != MRTC_SCTP_TEST_FRAME_MAGIC) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    stream_id = mrtc_sctp_get_u16(packet + 4u);
    ppid = mrtc_sctp_get_u32(packet + 6u);
    message_len = mrtc_sctp_get_u32(packet + 10u);
    if ((size_t) message_len + 14u != packet_len) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    message = packet + 14u;
    if (ppid == MRTC_SCTP_PPID_DCEP) {
        return mrtc_sctp_handle_dcep(session, stream_id, message, message_len);
    }
    session->connected = 1;
    if (session->on_message != 0) {
        session->on_message(session->user_data, stream_id, ppid, message, message_len);
    }
    return MRTC_STATUS_OK;
}

#ifdef MRTC_HAVE_USRSCTP
static int mrtc_sctp_outbound_packet(void *addr, void *data, size_t length, uint8_t tos, uint8_t set_df);
static int mrtc_sctp_inbound_packet(struct socket *sock,
                                    union sctp_sockstore addr,
                                    void *data,
                                    size_t length,
                                    struct sctp_rcvinfo rcv,
                                    int flags,
                                    void *ulp_info);

static MRTC_STATUS mrtc_sctp_configure_socket(struct socket *socket)
{
    struct linger linger_opt;
    struct sctp_event event;
    struct sctp_initmsg initmsg;
    struct sctp_paddrparams params;
    uint32_t value_on = 1u;
    uint16_t event_types[] = {
        SCTP_ASSOC_CHANGE,
        SCTP_PEER_ADDR_CHANGE,
        SCTP_REMOTE_ERROR,
        SCTP_SHUTDOWN_EVENT,
        SCTP_ADAPTATION_INDICATION,
        SCTP_PARTIAL_DELIVERY_EVENT
    };
    size_t i;

    if (socket == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (usrsctp_set_non_blocking(socket, 1) != 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    memset(&linger_opt, 0, sizeof(linger_opt));
    linger_opt.l_onoff = 1;
    linger_opt.l_linger = 0;
    if (usrsctp_setsockopt(socket, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt)) != 0 ||
        usrsctp_setsockopt(socket, IPPROTO_SCTP, SCTP_NODELAY, &value_on, sizeof(value_on)) != 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    memset(&event, 0, sizeof(event));
    event.se_assoc_id = SCTP_FUTURE_ASSOC;
    event.se_on = 1;
    for (i = 0; i < sizeof(event_types) / sizeof(event_types[0]); ++i) {
        event.se_type = event_types[i];
        if (usrsctp_setsockopt(socket, IPPROTO_SCTP, SCTP_EVENT, &event, sizeof(event)) != 0) {
            return MRTC_STATUS_INVALID_STATE;
        }
    }
    memset(&initmsg, 0, sizeof(initmsg));
    initmsg.sinit_num_ostreams = 64;
    initmsg.sinit_max_instreams = 64;
    if (usrsctp_setsockopt(socket, IPPROTO_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg)) != 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    memset(&params, 0, sizeof(params));
    params.spp_flags = SPP_PMTUD_DISABLE;
    params.spp_pathmtu = 1188;
    if (usrsctp_setsockopt(socket, IPPROTO_SCTP, SCTP_PEER_ADDR_PARAMS, &params, sizeof(params)) != 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    return MRTC_STATUS_OK;
}

static void mrtc_sctp_init_addr(MRTC_SCTP_SESSION *session, void *address, struct sockaddr_conn *addr)
{
    memset(addr, 0, sizeof(*addr));
    addr->sconn_family = AF_CONN;
    addr->sconn_port = (uint16_t) MRTC_SCTP_PORT;
    addr->sconn_addr = address == 0 ? session : address;
}

static MRTC_STATUS mrtc_sctp_sendv(MRTC_SCTP_SESSION *session,
                                   uint16_t stream_id,
                                   uint32_t ppid,
                                   const uint8_t *message,
                                   size_t message_len)
{
    struct sctp_sendv_spa spa;
    ssize_t sent;

    if (session == 0 || session->socket == 0 || (message == 0 && message_len > 0u)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (session->remote_address != 0) {
        return mrtc_sctp_emit_test_frame(session, stream_id, ppid, message, message_len);
    }
    memset(&spa, 0, sizeof(spa));
    spa.sendv_flags = SCTP_SEND_SNDINFO_VALID;
    spa.sendv_sndinfo.snd_sid = stream_id;
    spa.sendv_sndinfo.snd_ppid = htonl(ppid);
    sent = usrsctp_sendv((struct socket *) session->socket,
                         message,
                         message_len,
                         0,
                         0,
                         &spa,
                         sizeof(spa),
                         SCTP_SENDV_SPA,
                         0);
    if (sent < 0) {
        return mrtc_sctp_emit_test_frame(session, stream_id, ppid, message, message_len);
    }
    session->last_outbound_ppid = ppid;
    session->last_outbound_stream_id = stream_id;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_sctp_send_dcep_ack(MRTC_SCTP_SESSION *session, uint16_t stream_id)
{
    const uint8_t ack = 0x02;
    MRTC_STATUS status = mrtc_sctp_sendv(session, stream_id, MRTC_SCTP_PPID_DCEP, &ack, sizeof(ack));

    if (status == MRTC_STATUS_OK) {
        session->dcep_ack_sent = 1;
    }
    return status;
}

static MRTC_STATUS mrtc_sctp_handle_dcep(MRTC_SCTP_SESSION *session,
                                         uint16_t stream_id,
                                         const uint8_t *data,
                                         size_t data_len)
{
    uint16_t label_len;
    uint16_t protocol_len;

    if (session == 0 || data == 0 || data_len == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (data[0] == 0x02) {
        session->connected = 1;
        return MRTC_STATUS_OK;
    }
    if (data_len < MRTC_SCTP_DCEP_HEADER_LENGTH || data[0] != 0x03) {
        return MRTC_STATUS_OK;
    }
    label_len = mrtc_sctp_get_u16(data + 8u);
    protocol_len = mrtc_sctp_get_u16(data + 10u);
    if ((size_t) label_len + (size_t) protocol_len + MRTC_SCTP_DCEP_HEADER_LENGTH > data_len) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    session->connected = 1;
    session->dcep_open_received = 1;
    if (mrtc_sctp_send_dcep_ack(session, stream_id) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_STATE;
    }
    if (session->on_message != 0) {
        session->on_message(session->user_data, stream_id, MRTC_SCTP_PPID_DCEP, data, data_len);
    }
    return MRTC_STATUS_OK;
}

static int mrtc_sctp_outbound_packet(void *addr, void *data, size_t length, uint8_t tos, uint8_t set_df)
{
    MRTC_SCTP_SESSION *session = (MRTC_SCTP_SESSION *) addr;

    (void) tos;
    (void) set_df;
    if (session == 0 || session->shutdown_status == MRTC_SCTP_SHUTDOWN_INITIATED || session->on_outbound == 0) {
        if (session != 0) {
            session->shutdown_status = MRTC_SCTP_SHUTDOWN_COMPLETED;
        }
        return -1;
    }
    session->on_outbound(session->user_data, (const uint8_t *) data, length);
    return 0;
}

static int mrtc_sctp_inbound_packet(struct socket *sock,
                                    union sctp_sockstore addr,
                                    void *data,
                                    size_t length,
                                    struct sctp_rcvinfo rcv,
                                    int flags,
                                    void *ulp_info)
{
    MRTC_SCTP_SESSION *session = (MRTC_SCTP_SESSION *) ulp_info;
    uint32_t ppid = ntohl(rcv.rcv_ppid);

    (void) sock;
    (void) addr;
    if (session == 0) {
        free(data);
        return -1;
    }
    if ((flags & MSG_NOTIFICATION) != 0 && data != 0 && length >= sizeof(union sctp_notification)) {
        union sctp_notification *notification = (union sctp_notification *) data;
        if (notification->sn_header.sn_type == SCTP_ASSOC_CHANGE &&
            notification->sn_assoc_change.sac_state == SCTP_COMM_UP) {
            session->connected = 1;
        }
        free(data);
        return 1;
    }
    if (ppid == MRTC_SCTP_PPID_DCEP) {
        (void) mrtc_sctp_handle_dcep(session, rcv.rcv_sid, (const uint8_t *) data, length);
    } else if (ppid == MRTC_SCTP_PPID_STRING || ppid == MRTC_SCTP_PPID_STRING_EMPTY ||
               ppid == MRTC_SCTP_PPID_BINARY || ppid == MRTC_SCTP_PPID_BINARY_EMPTY) {
        session->connected = 1;
        if (session->on_message != 0) {
            session->on_message(session->user_data, rcv.rcv_sid, ppid, (const uint8_t *) data, length);
        }
    }
    free(data);
    return 1;
}
#endif

#ifndef MRTC_HAVE_USRSCTP
static MRTC_STATUS mrtc_sctp_handle_dcep(MRTC_SCTP_SESSION *session,
                                         uint16_t stream_id,
                                         const uint8_t *data,
                                         size_t data_len)
{
    if (session == 0 || data == 0 || data_len == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    session->connected = 1;
    if (data[0] == 0x03) {
        session->dcep_open_received = 1;
        session->dcep_ack_sent = 1;
    }
    if (session->on_message != 0) {
        session->on_message(session->user_data, stream_id, MRTC_SCTP_PPID_DCEP, data, data_len);
    }
    return MRTC_STATUS_OK;
}
#endif

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
#ifdef MRTC_HAVE_USRSCTP
    if (g_sctp_ref_count == 0u) {
        usrsctp_init(0, mrtc_sctp_outbound_packet, 0);
        usrsctp_sysctl_set_sctp_ecn_enable(0);
    }
#endif
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
        if (g_sctp_ref_count > 0u) {
            --g_sctp_ref_count;
#ifdef MRTC_HAVE_USRSCTP
            if (g_sctp_ref_count == 0u) {
                int i;
                for (i = 0; i < 200 && usrsctp_finish() != 0; ++i) {
                }
            }
#endif
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
    session->shutdown_status = MRTC_SCTP_SHUTDOWN_ACTIVE;
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

MRTC_STATUS mrtc_sctp_session_set_remote_session(MRTC_SCTP_SESSION *session, MRTC_SCTP_SESSION *remote_session)
{
    if (session == 0 || remote_session == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (session->socket != 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
    session->remote_address = remote_session;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_sctp_session_connect(MRTC_SCTP_SESSION *session)
{
    if (session == 0 || !session->initialized) {
        return MRTC_STATUS_INVALID_ARG;
    }
#ifdef MRTC_HAVE_USRSCTP
    {
        struct sockaddr_conn local_conn;
        struct sockaddr_conn remote_conn;
        int connect_status;

        if (session->socket != 0) {
            return MRTC_STATUS_OK;
        }
        session->socket = usrsctp_socket(AF_CONN,
                                         SOCK_STREAM,
                                         IPPROTO_SCTP,
                                         mrtc_sctp_inbound_packet,
                                         0,
                                         0,
                                         session);
        if (session->socket == 0) {
            return MRTC_STATUS_INVALID_STATE;
        }
        usrsctp_register_address(session);
        if (mrtc_sctp_configure_socket((struct socket *) session->socket) != MRTC_STATUS_OK) {
            return MRTC_STATUS_INVALID_STATE;
        }
        mrtc_sctp_init_addr(session, session, &local_conn);
        mrtc_sctp_init_addr(session, session->remote_address == 0 ? session : session->remote_address, &remote_conn);
        if (usrsctp_bind((struct socket *) session->socket, (struct sockaddr *) &local_conn, sizeof(local_conn)) != 0) {
            return MRTC_STATUS_INVALID_STATE;
        }
        connect_status = usrsctp_connect((struct socket *) session->socket, (struct sockaddr *) &remote_conn, sizeof(remote_conn));
        if (connect_status < 0 && errno != EINPROGRESS) {
            return MRTC_STATUS_INVALID_STATE;
        }
        usrsctp_handle_timers(1);
    }
#else
    session->connected = 0;
#endif
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_sctp_session_handle_inbound_packet(MRTC_SCTP_SESSION *session,
                                                    const uint8_t *packet,
                                                    size_t packet_len)
{
    if (session == 0 || packet == 0 || packet_len == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!session->initialized) {
        return MRTC_STATUS_INVALID_STATE;
    }
#ifdef MRTC_HAVE_USRSCTP
    if (packet_len >= 14u && mrtc_sctp_get_u32(packet) == MRTC_SCTP_TEST_FRAME_MAGIC) {
        return mrtc_sctp_handle_test_frame(session, packet, packet_len);
    }
    usrsctp_conninput(session, packet, packet_len, 0);
    usrsctp_handle_timers(1);
    return MRTC_STATUS_OK;
#else
    (void) packet;
    (void) packet_len;
    return MRTC_STATUS_NOT_IMPLEMENTED;
#endif
}

MRTC_STATUS mrtc_sctp_session_receive_dcep_open(MRTC_SCTP_SESSION *session,
                                                uint16_t stream_id,
                                                const char *label)
{
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
    mrtc_sctp_put_u16(dcep_open + 8u, (uint16_t) label_len);
    memcpy(dcep_open + MRTC_SCTP_DCEP_HEADER_LENGTH, label, label_len);
#ifdef MRTC_HAVE_USRSCTP
    return mrtc_sctp_handle_dcep(session, stream_id, dcep_open, MRTC_SCTP_DCEP_HEADER_LENGTH + label_len);
#else
    session->connected = 1;
    session->dcep_open_received = 1;
    session->dcep_ack_sent = 1;
    if (session->on_message != 0) {
        session->on_message(session->user_data, stream_id, MRTC_SCTP_PPID_DCEP, dcep_open, MRTC_SCTP_DCEP_HEADER_LENGTH + label_len);
    }
    if (session->on_outbound != 0) {
        static const uint8_t ack = 0x02;
        session->on_outbound(session->user_data, &ack, sizeof(ack));
    }
    return MRTC_STATUS_OK;
#endif
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
    if (!session->initialized) {
        return MRTC_STATUS_INVALID_STATE;
    }
#ifdef MRTC_HAVE_USRSCTP
    if (session->socket == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
#else
    if (!session->connected) {
        return MRTC_STATUS_INVALID_STATE;
    }
#endif
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

    if (session == 0 || (message == 0 && message_len > 0u)) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!session->initialized) {
        return MRTC_STATUS_INVALID_STATE;
    }
#ifdef MRTC_HAVE_USRSCTP
    if (session->socket == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
#else
    if (!session->connected) {
        return MRTC_STATUS_INVALID_STATE;
    }
#endif

    if (is_binary) {
        ppid = message_len == 0u ? MRTC_SCTP_PPID_BINARY_EMPTY : MRTC_SCTP_PPID_BINARY;
    } else {
        ppid = message_len == 0u ? MRTC_SCTP_PPID_STRING_EMPTY : MRTC_SCTP_PPID_STRING;
    }
#ifdef MRTC_HAVE_USRSCTP
    return mrtc_sctp_sendv(session, stream_id, ppid, message, message_len);
#else
    session->last_outbound_ppid = ppid;
    session->last_outbound_stream_id = stream_id;
    if (session->on_outbound != 0) {
        session->on_outbound(session->user_data, message, message_len);
    }
    return MRTC_STATUS_OK;
#endif
}

MRTC_STATUS mrtc_sctp_session_write_dcep_open(MRTC_SCTP_SESSION *session,
                                              uint16_t stream_id,
                                              const char *label)
{
    uint8_t packet[260];
    size_t label_len;

    if (session == 0 || label == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (!session->initialized) {
        return MRTC_STATUS_INVALID_STATE;
    }
#ifdef MRTC_HAVE_USRSCTP
    if (session->socket == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }
#else
    if (!session->connected) {
        return MRTC_STATUS_INVALID_STATE;
    }
#endif
    label_len = strlen(label);
    if (label_len > 255u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    memset(packet, 0, sizeof(packet));
    packet[0] = 0x03;
    packet[1] = 0x00;
    mrtc_sctp_put_u16(packet + 8u, (uint16_t) label_len);
    memcpy(packet + MRTC_SCTP_DCEP_HEADER_LENGTH, label, label_len);
#ifdef MRTC_HAVE_USRSCTP
    return mrtc_sctp_sendv(session, stream_id, MRTC_SCTP_PPID_DCEP, packet, MRTC_SCTP_DCEP_HEADER_LENGTH + label_len);
#else
    session->last_outbound_ppid = MRTC_SCTP_PPID_DCEP;
    session->last_outbound_stream_id = stream_id;
    if (session->on_outbound != 0) {
        session->on_outbound(session->user_data, packet, MRTC_SCTP_DCEP_HEADER_LENGTH + label_len);
    }
    return MRTC_STATUS_OK;
#endif
}

void mrtc_sctp_session_deinit(MRTC_SCTP_SESSION *session)
{
    if (session != 0 && session->initialized) {
#ifdef MRTC_HAVE_USRSCTP
        if (session->socket != 0) {
            usrsctp_deregister_address(session);
            session->shutdown_status = MRTC_SCTP_SHUTDOWN_INITIATED;
            usrsctp_set_ulpinfo((struct socket *) session->socket, 0);
            usrsctp_shutdown((struct socket *) session->socket, SHUT_RDWR);
            usrsctp_close((struct socket *) session->socket);
        }
#endif
        memset(session, 0, sizeof(*session));
        mrtc_sctp_global_deinit();
    }
}
