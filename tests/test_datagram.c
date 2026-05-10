#include "api/peer_connection.h"
#include "executor/executor.h"
#include "net/demux.h"
#include "rtc/trace.h"
#include "stun/stun.h"
#include "test_runner.h"

#include <string.h>

typedef struct datagram_observer_state_t {
    int datagram_count;
    int error_count;
    int local_candidate_count;
    int state_count;
    int trace_count;
    uint8_t last_datagram[64];
    size_t last_datagram_len;
    const char *last_state;
    const char *last_trace;
    const char *last_protocol;
    const char *last_reason;
    rtc_executor_task_fn timer_task;
    void *timer_user_data;
} datagram_observer_state_t;

static rtc_status_t test_post(void *user_data, rtc_executor_task_fn task,
                              void *task_user_data)
{
    (void)user_data;
    if (task != 0) {
        task(task_user_data);
    }
    return RTC_STATUS_OK;
}

static rtc_status_t test_schedule_timer(void *user_data, uint64_t delay_ms,
                                        rtc_executor_task_fn task,
                                        void *task_user_data,
                                        uint64_t *out_timer_id)
{
    datagram_observer_state_t *state =
        (datagram_observer_state_t *)user_data;
    (void)delay_ms;
    state->timer_task = task;
    state->timer_user_data = task_user_data;
    if (out_timer_id != 0) {
        *out_timer_id = 99;
    }
    return RTC_STATUS_OK;
}

static rtc_status_t test_cancel_timer(void *user_data, uint64_t timer_id)
{
    (void)user_data;
    (void)timer_id;
    return RTC_STATUS_OK;
}

static rtc_executor_vtable_t test_executor(void *user_data)
{
    rtc_executor_vtable_t executor;
    executor.post = test_post;
    executor.schedule_timer = test_schedule_timer;
    executor.cancel_timer = test_cancel_timer;
    executor.user_data = user_data;
    return executor;
}

static void on_state(void *user_data, const char *state_name)
{
    datagram_observer_state_t *state =
        (datagram_observer_state_t *)user_data;
    state->state_count++;
    state->last_state = state_name;
}

static void on_local_candidate(void *user_data, const char *candidate,
                               size_t candidate_len)
{
    datagram_observer_state_t *state =
        (datagram_observer_state_t *)user_data;
    (void)candidate;
    (void)candidate_len;
    state->local_candidate_count++;
}

static void on_datagram(void *user_data, const uint8_t *data, size_t data_len)
{
    datagram_observer_state_t *state =
        (datagram_observer_state_t *)user_data;
    if (data_len > sizeof(state->last_datagram)) {
        data_len = sizeof(state->last_datagram);
    }
    memcpy(state->last_datagram, data, data_len);
    state->last_datagram_len = data_len;
    state->datagram_count++;
}

static void on_error(void *user_data, rtc_status_t status,
                     const char *subsystem, const char *operation,
                     int detail_code)
{
    datagram_observer_state_t *state =
        (datagram_observer_state_t *)user_data;
    (void)status;
    (void)subsystem;
    (void)operation;
    (void)detail_code;
    state->error_count++;
}

static void on_trace(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count)
{
    datagram_observer_state_t *state =
        (datagram_observer_state_t *)user_data;
    size_t i;

    state->trace_count++;
    state->last_trace = event;
    for (i = 0; i < field_count; ++i) {
        if (strcmp(fields[i].key, RTC_TRACE_FIELD_PROTOCOL) == 0) {
            state->last_protocol = fields[i].value;
        } else if (strcmp(fields[i].key, RTC_TRACE_FIELD_REASON) == 0) {
            state->last_reason = fields[i].value;
        }
    }
}

static rtc_peer_connection_config_t test_config(
    unsigned char *arena, size_t arena_size, datagram_observer_state_t *state)
{
    rtc_peer_connection_config_t config;
    memset(&config, 0, sizeof(config));
    config.arena.data = arena;
    config.arena.size = arena_size;
    config.limits.sdp.max_description_bytes = 2048;
    config.limits.ice.max_candidates = 4;
    config.limits.ice.max_candidate_pairs = 4;
    config.limits.ice.max_transactions = 2;
    config.limits.ice.max_timer_slots = 4;
    config.limits.dtls.max_sessions = 1;
    config.limits.rtp.max_packet_cache = 16;
    config.limits.rtcp.max_reports = 4;
    config.limits.trace.max_events = 16;
    config.sdp.ice_ufrag = "testufrag";
    config.sdp.ice_ufrag_len = strlen(config.sdp.ice_ufrag);
    config.sdp.ice_pwd = "testpassword1234567890";
    config.sdp.ice_pwd_len = strlen(config.sdp.ice_pwd);
    config.sdp.dtls_fingerprint =
        "sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
        "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF";
    config.sdp.dtls_fingerprint_len = strlen(config.sdp.dtls_fingerprint);
    config.sdp.dtls_setup = "actpass";
    config.sdp.dtls_setup_len = strlen(config.sdp.dtls_setup);
    config.sdp.session_id = 1000;
    config.sdp.session_version = 2;
    config.local_host_ip = "192.0.2.10";
    config.local_host_ip_len = strlen(config.local_host_ip);
    config.local_host_port = 5000;
    config.executors.signaling = test_executor(state);
    config.executors.media = test_executor(state);
    config.executors.network = test_executor(state);
    config.observer.on_state = on_state;
    config.observer.on_error = on_error;
    config.observer.on_trace = on_trace;
    config.observer.on_local_candidate = on_local_candidate;
    config.observer.on_datagram = on_datagram;
    config.observer.user_data = state;
    return config;
}

static void test_write_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;
}

static void test_write_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void write_success_response(uint8_t *response, const uint8_t *request,
                                   uint32_t ip, uint16_t port)
{
    memset(response, 0, 32);
    test_write_u16(response, RTC_STUN_BINDING_SUCCESS_RESPONSE);
    test_write_u16(response + 2, 12);
    test_write_u32(response + 4, RTC_STUN_MAGIC_COOKIE);
    memcpy(response + 8, request + 8, RTC_STUN_TRANSACTION_ID_BYTES);
    test_write_u16(response + 20, 0x0020);
    test_write_u16(response + 22, 8);
    response[24] = 0;
    response[25] = 0x01;
    test_write_u16(response + 26,
                   (uint16_t)(port ^ (RTC_STUN_MAGIC_COOKIE >> 16)));
    test_write_u32(response + 28, ip ^ RTC_STUN_MAGIC_COOKIE);
}

static int test_demux_classifier(void)
{
    const uint8_t stun[20] = {0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xa4,
                              0x42, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
                              0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b};
    const uint8_t dtls[3] = {22, 0xfe, 0xfd};
    const uint8_t rtp[4] = {0x80, 96, 0, 1};
    const uint8_t rtcp[4] = {0x80, 200, 0, 0};
    const uint8_t unknown_datagram[3] = {0x70, 0, 0};
    rtc_net_protocol_t protocol = RTC_NET_PROTOCOL_UNKNOWN;

    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_ARGUMENT,
                    rtc_net_demux_datagram(0, 0, &protocol));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_net_demux_datagram(stun, sizeof(stun), &protocol));
    RTC_TEST_EQ_INT(RTC_NET_PROTOCOL_STUN, protocol);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_net_demux_datagram(dtls, sizeof(dtls), &protocol));
    RTC_TEST_EQ_INT(RTC_NET_PROTOCOL_DTLS, protocol);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_net_demux_datagram(rtp, sizeof(rtp), &protocol));
    RTC_TEST_EQ_INT(RTC_NET_PROTOCOL_RTP, protocol);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_net_demux_datagram(rtcp, sizeof(rtcp), &protocol));
    RTC_TEST_EQ_INT(RTC_NET_PROTOCOL_RTCP, protocol);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_net_demux_datagram(unknown_datagram,
                                           sizeof(unknown_datagram),
                                           &protocol));
    RTC_TEST_EQ_INT(RTC_NET_PROTOCOL_UNKNOWN, protocol);
    return 0;
}

static int test_receive_demux_and_errors(void)
{
    unsigned char arena[16384];
    datagram_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;
    const uint8_t dtls[3] = {22, 0xfe, 0xfd};
    const uint8_t rtp[4] = {0x80, 96, 0, 1};
    const uint8_t rtcp[4] = {0x80, 200, 0, 0};
    const uint8_t unknown_datagram[3] = {0x70, 0, 0};
    const uint8_t malformed_stun[20] = {0x00, 0x01, 0x00, 0x00, 0, 0, 0,
                                        0,    0,    0,    0,    0, 0, 0,
                                        0,    0,    0,    0,    0, 0};

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_AFFINITY_VIOLATION,
                    rtc_peer_connection_receive_datagram(pc, dtls,
                                                         sizeof(dtls)));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_peer_connection_receive_datagram(pc, dtls,
                                                         sizeof(dtls)));
    RTC_TEST_ASSERT(strcmp(state.last_trace, RTC_TRACE_DTLS_HANDSHAKE) == 0);
    RTC_TEST_ASSERT(strcmp(state.last_protocol, "dtls") == 0);
    RTC_TEST_ASSERT(strcmp(state.last_reason, "ice_not_connected") == 0);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_receive_datagram(pc, rtp, sizeof(rtp)));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_receive_datagram(pc, rtcp,
                                                         sizeof(rtcp)));
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_peer_connection_receive_datagram(pc, unknown_datagram,
                                                         sizeof(unknown_datagram)));
    RTC_TEST_ASSERT(strcmp(state.last_reason, "unknown_datagram") == 0);
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_peer_connection_receive_datagram(pc, malformed_stun,
                                                         sizeof(malformed_stun)));
    RTC_TEST_ASSERT(strcmp(state.last_reason, "malformed_stun") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.net.demux_stun);
    RTC_TEST_EQ_INT(1, (int)counters.net.demux_dtls);
    RTC_TEST_EQ_INT(1, (int)counters.net.demux_rtp);
    RTC_TEST_EQ_INT(1, (int)counters.net.demux_rtcp);
    RTC_TEST_EQ_INT(1, (int)counters.net.demux_unknown);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_receive_stun_drives_srflx(void)
{
    unsigned char arena[16384];
    uint8_t response[32];
    datagram_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);
    config.stun_server_count = 1;
    config.stun_server.ip = "198.51.100.1";
    config.stun_server.ip_len = strlen(config.stun_server.ip);
    config.stun_server.port = 3478;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    write_success_response(response, state.last_datagram, 0xcb007107u, 54321);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_receive_datagram(pc, response,
                                                         sizeof(response)));
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.gathering_complete") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.net.demux_stun);
    RTC_TEST_EQ_INT(2, (int)counters.ice.local_candidates);
    RTC_TEST_EQ_INT(1, (int)counters.stun.transactions_received);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_receive_stun_drives_pair_check(void)
{
    unsigned char arena[16384];
    uint8_t response[32];
    datagram_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;
    const char *remote =
        "candidate:2 1 udp 2130706430 192.0.2.20 6000 typ host";

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_add_ice_candidate(pc, remote,
                                                          strlen(remote)));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_start_connectivity_checks(pc));
    write_success_response(response, state.last_datagram, 0xc0000214u, 6000);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_receive_datagram(pc, response,
                                                         sizeof(response)));
    write_success_response(response, state.last_datagram, 0xc0000214u, 6000);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_receive_datagram(pc, response,
                                                         sizeof(response)));
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.connected") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(2, (int)counters.net.demux_stun);
    RTC_TEST_EQ_INT(1, (int)counters.ice.selected_pairs);
    RTC_TEST_EQ_INT(2, (int)counters.stun.transactions_received);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

int rtc_test_datagram(void)
{
    RTC_TEST_EQ_INT(0, test_demux_classifier());
    RTC_TEST_EQ_INT(0, test_receive_demux_and_errors());
    RTC_TEST_EQ_INT(0, test_receive_stun_drives_srflx());
    RTC_TEST_EQ_INT(0, test_receive_stun_drives_pair_check());
    return 0;
}
