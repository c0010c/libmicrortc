#include "../../src/sctp/sctp_session.h"

#include <string.h>

typedef struct SctpTestState {
    int outbound_count;
    int dcep_count;
    int string_count;
    int binary_count;
} SctpTestState;

static void on_outbound(void *user_data, const uint8_t *packet, size_t packet_len)
{
    SctpTestState *state = (SctpTestState *) user_data;
    (void) packet;
    (void) packet_len;
    state->outbound_count++;
}

static void on_message(void *user_data, uint16_t stream_id, uint32_t ppid, const uint8_t *message, size_t message_len)
{
    SctpTestState *state = (SctpTestState *) user_data;
    (void) stream_id;

    if (ppid == MRTC_SCTP_PPID_DCEP && message_len > 0) {
        state->dcep_count++;
    } else if (ppid == MRTC_SCTP_PPID_STRING && message_len == 4 && memcmp(message, "ping", 4) == 0) {
        state->string_count++;
    } else if (ppid == MRTC_SCTP_PPID_BINARY && message_len == 4 &&
               message[0] == 0x00 && message[1] == 0x01 && message[2] == 0xFE && message[3] == 0xFF) {
        state->binary_count++;
    }
}

int main(void)
{
    MRTC_SCTP_SESSION session;
    SctpTestState state = {0};
    const uint8_t binary[] = {0x00, 0x01, 0xFE, 0xFF};

    if (mrtc_sctp_session_init(&session) != MRTC_STATUS_OK) {
        return 1;
    }
    mrtc_sctp_session_set_callbacks(&session, on_outbound, on_message, &state);
    if (mrtc_sctp_session_connect(&session) != MRTC_STATUS_OK || session.connected) {
        return 1;
    }
    if (mrtc_sctp_session_write_message(&session, 0, 0, (const uint8_t *) "ping", 4) != MRTC_STATUS_INVALID_STATE) {
        return 1;
    }
    if (state.outbound_count != 0 || state.dcep_count != 0) {
        return 1;
    }
    if (mrtc_sctp_session_receive_dcep_open(&session, 0, "chat") != MRTC_STATUS_OK ||
        !session.connected ||
        !session.dcep_open_received ||
        !session.dcep_ack_sent) {
        return 1;
    }
    if (state.outbound_count != 1 || state.dcep_count != 1) {
        return 1;
    }
    if (mrtc_sctp_session_write_message(&session, 0, 0, (const uint8_t *) "ping", 4) != MRTC_STATUS_OK ||
        mrtc_sctp_session_write_message(&session, 0, 1, binary, sizeof(binary)) != MRTC_STATUS_OK) {
        return 1;
    }
    if (state.outbound_count != 3 || state.string_count != 0 || state.binary_count != 0) {
        return 1;
    }
    if (mrtc_sctp_session_receive_message(&session, 0, MRTC_SCTP_PPID_STRING, (const uint8_t *) "ping", 4) != MRTC_STATUS_OK ||
        mrtc_sctp_session_receive_message(&session, 0, MRTC_SCTP_PPID_BINARY, binary, sizeof(binary)) != MRTC_STATUS_OK) {
        return 1;
    }
    if (state.string_count != 1 || state.binary_count != 1) {
        return 1;
    }
    mrtc_sctp_session_deinit(&session);
    if (mrtc_sctp_global_init() != MRTC_STATUS_OK) {
        return 1;
    }
    mrtc_sctp_global_deinit();
    return 0;
}
