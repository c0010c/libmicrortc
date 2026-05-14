#include "../../src/sctp/sctp_session.h"

#include <stdio.h>
#include <string.h>

#define FAIL_AT(label) do { fprintf(stderr, "sctp test failed at %s\n", label); return 1; } while (0)

typedef struct PacketQueue {
    uint8_t packets[64][1600];
    size_t sizes[64];
    size_t head;
    size_t tail;
} PacketQueue;

typedef struct SctpEndpoint {
    MRTC_SCTP_SESSION session;
    PacketQueue outbound;
    int dcep_count;
    int dcep_ack_count;
    int string_count;
    int binary_count;
    int empty_string_count;
    int empty_binary_count;
} SctpEndpoint;

static int queue_push(PacketQueue *queue, const uint8_t *packet, size_t packet_len)
{
    if (packet_len > sizeof(queue->packets[0]) || queue->tail >= 64u) {
        return 0;
    }
    memcpy(queue->packets[queue->tail], packet, packet_len);
    queue->sizes[queue->tail] = packet_len;
    queue->tail++;
    return 1;
}

static int queue_pop(PacketQueue *queue, uint8_t *packet, size_t *packet_len)
{
    if (queue->head == queue->tail) {
        return 0;
    }
    memcpy(packet, queue->packets[queue->head], queue->sizes[queue->head]);
    *packet_len = queue->sizes[queue->head];
    queue->head++;
    if (queue->head == queue->tail) {
        queue->head = 0;
        queue->tail = 0;
    }
    return 1;
}

static void on_outbound(void *user_data, const uint8_t *packet, size_t packet_len)
{
    SctpEndpoint *endpoint = (SctpEndpoint *) user_data;

    (void) queue_push(&endpoint->outbound, packet, packet_len);
}

static void on_message(void *user_data, uint16_t stream_id, uint32_t ppid, const uint8_t *message, size_t message_len)
{
    SctpEndpoint *endpoint = (SctpEndpoint *) user_data;
    (void) stream_id;

    if (ppid == MRTC_SCTP_PPID_DCEP && message_len > 0 && message[0] == 0x03) {
        endpoint->dcep_count++;
    } else if (ppid == MRTC_SCTP_PPID_DCEP && message_len == 1 && message[0] == 0x02) {
        endpoint->dcep_ack_count++;
    } else if (ppid == MRTC_SCTP_PPID_STRING && message_len == 4 && memcmp(message, "ping", 4) == 0) {
        endpoint->string_count++;
    } else if (ppid == MRTC_SCTP_PPID_STRING_EMPTY && message_len == 0) {
        endpoint->empty_string_count++;
    } else if (ppid == MRTC_SCTP_PPID_BINARY && message_len == 4 &&
               message[0] == 0x00 && message[1] == 0x01 && message[2] == 0xFE && message[3] == 0xFF) {
        endpoint->binary_count++;
    } else if (ppid == MRTC_SCTP_PPID_BINARY_EMPTY && message_len == 0) {
        endpoint->empty_binary_count++;
    }
}

static int pump_one(SctpEndpoint *from, SctpEndpoint *to)
{
    uint8_t packet[1600];
    size_t packet_len = 0;
    int pumped = 0;

    while (queue_pop(&from->outbound, packet, &packet_len)) {
        if (mrtc_sctp_session_handle_inbound_packet(&to->session, packet, packet_len) != MRTC_STATUS_OK) {
            return -1;
        }
        pumped = 1;
    }
    return pumped;
}

static int pump_pair(SctpEndpoint *left, SctpEndpoint *right, int max_iterations)
{
    int i;

    for (i = 0; i < max_iterations; ++i) {
        int a = pump_one(left, left);
        int b = pump_one(right, right);
        if (a < 0 || b < 0) {
            return 0;
        }
        if (a == 0 && b == 0) {
            return 1;
        }
    }
    return 1;
}

int main(void)
{
    SctpEndpoint left;
    SctpEndpoint right;
    const uint8_t binary[] = {0x00, 0x01, 0xFE, 0xFF};

    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));

    if (mrtc_sctp_session_init(&left.session) != MRTC_STATUS_OK ||
        mrtc_sctp_session_init(&right.session) != MRTC_STATUS_OK) {
        FAIL_AT("init");
    }
    mrtc_sctp_session_set_callbacks(&left.session, on_outbound, on_message, &left);
    mrtc_sctp_session_set_callbacks(&right.session, on_outbound, on_message, &right);
    if (mrtc_sctp_session_set_remote_session(&left.session, &right.session) != MRTC_STATUS_OK ||
        mrtc_sctp_session_set_remote_session(&right.session, &left.session) != MRTC_STATUS_OK) {
        FAIL_AT("set remote");
    }

    if (mrtc_sctp_session_write_message(&left.session, 0, 0, (const uint8_t *) "ping", 4) != MRTC_STATUS_INVALID_STATE) {
        FAIL_AT("write before connect");
    }
    if (mrtc_sctp_session_connect(&left.session) != MRTC_STATUS_OK ||
        mrtc_sctp_session_connect(&right.session) != MRTC_STATUS_OK ||
        left.dcep_count != 0 ||
        right.dcep_count != 0) {
        FAIL_AT("connect");
    }
    if (!pump_pair(&left, &right, 128)) {
        fprintf(stderr, "connected=%d/%d queued=%lu/%lu %lu/%lu\n",
                left.session.connected,
                right.session.connected,
                (unsigned long) left.outbound.head,
                (unsigned long) left.outbound.tail,
                (unsigned long) right.outbound.head,
                (unsigned long) right.outbound.tail);
        FAIL_AT("association");
    }
    if (mrtc_sctp_session_write_message(&left.session, 0, 0, (const uint8_t *) "ping", 4) != MRTC_STATUS_OK ||
        mrtc_sctp_session_write_message(&left.session, 0, 1, binary, sizeof(binary)) != MRTC_STATUS_OK ||
        mrtc_sctp_session_write_message(&left.session, 0, 0, 0, 0) != MRTC_STATUS_OK ||
        mrtc_sctp_session_write_message(&left.session, 0, 1, 0, 0) != MRTC_STATUS_OK) {
        FAIL_AT("write messages");
    }
    if (left.session.last_outbound_ppid != MRTC_SCTP_PPID_BINARY_EMPTY) {
        FAIL_AT("last ppid");
    }
    if (!pump_pair(&left, &right, 128) ||
        right.string_count != 1 ||
        right.binary_count != 1 ||
        right.empty_string_count != 1 ||
        right.empty_binary_count != 1) {
        fprintf(stderr, "counts s=%d b=%d es=%d eb=%d queues %lu/%lu %lu/%lu\n",
                right.string_count,
                right.binary_count,
                right.empty_string_count,
                right.empty_binary_count,
                (unsigned long) left.outbound.head,
                (unsigned long) left.outbound.tail,
                (unsigned long) right.outbound.head,
                (unsigned long) right.outbound.tail);
        FAIL_AT("message delivery");
    }

    if (mrtc_sctp_session_write_dcep_open(&left.session, 0, "chat") != MRTC_STATUS_OK ||
        !pump_pair(&left, &right, 128) ||
        right.dcep_count != 1 ||
        !right.session.dcep_open_received ||
        !right.session.dcep_ack_sent ||
        !pump_pair(&left, &right, 128)) {
        FAIL_AT("dcep");
    }
    mrtc_sctp_session_deinit(&left.session);
    mrtc_sctp_session_deinit(&right.session);

    if (mrtc_sctp_session_init(&left.session) != MRTC_STATUS_OK ||
        mrtc_sctp_session_init(&right.session) != MRTC_STATUS_OK) {
        FAIL_AT("reinit");
    }
    mrtc_sctp_session_deinit(&left.session);
    mrtc_sctp_session_deinit(&right.session);

    return 0;
}
