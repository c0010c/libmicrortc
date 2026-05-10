#include "api/peer_connection.h"
#include "executor/executor.h"
#include "ice/ice.h"
#include "rtc/trace.h"
#include "stun/stun.h"
#include "test_runner.h"

#include <string.h>

typedef struct ice_observer_state_t {
    int local_candidate_count;
    int datagram_count;
    int state_count;
    int error_count;
    char last_candidate[256];
    uint8_t last_datagram[64];
    size_t last_datagram_len;
    const char *last_state;
    const char *last_trace;
    const char *last_reason;
    rtc_executor_task_fn timer_task;
    void *timer_user_data;
} ice_observer_state_t;

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
    ice_observer_state_t *state = (ice_observer_state_t *)user_data;
    (void)delay_ms;
    state->timer_task = task;
    state->timer_user_data = task_user_data;
    if (out_timer_id != 0) {
        *out_timer_id = 7;
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
    ice_observer_state_t *state = (ice_observer_state_t *)user_data;
    state->state_count++;
    state->last_state = state_name;
}

static void on_local_candidate(void *user_data, const char *candidate,
                               size_t candidate_len)
{
    ice_observer_state_t *state = (ice_observer_state_t *)user_data;
    if (candidate_len >= sizeof(state->last_candidate)) {
        candidate_len = sizeof(state->last_candidate) - 1u;
    }
    memcpy(state->last_candidate, candidate, candidate_len);
    state->last_candidate[candidate_len] = '\0';
    state->local_candidate_count++;
}

static void on_datagram(void *user_data, const uint8_t *data, size_t data_len)
{
    ice_observer_state_t *state = (ice_observer_state_t *)user_data;
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
    ice_observer_state_t *state = (ice_observer_state_t *)user_data;
    (void)status;
    (void)subsystem;
    (void)operation;
    (void)detail_code;
    state->error_count++;
}

static void on_trace(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count)
{
    ice_observer_state_t *state = (ice_observer_state_t *)user_data;
    size_t i;

    state->last_trace = event;
    for (i = 0; i < field_count; ++i) {
        if (strcmp(fields[i].key, RTC_TRACE_FIELD_REASON) == 0) {
            state->last_reason = fields[i].value;
        }
    }
}

static rtc_peer_connection_config_t test_config(unsigned char *arena,
                                                size_t arena_size,
                                                ice_observer_state_t *state)
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

static void write_role_conflict_response(uint8_t *response,
                                         const uint8_t *request)
{
    memset(response, 0, 28);
    test_write_u16(response, RTC_STUN_BINDING_ERROR_RESPONSE);
    test_write_u16(response + 2, 8);
    test_write_u32(response + 4, RTC_STUN_MAGIC_COOKIE);
    memcpy(response + 8, request + 8, RTC_STUN_TRANSACTION_ID_BYTES);
    test_write_u16(response + 20, 0x0009);
    test_write_u16(response + 22, 4);
    response[24] = 0;
    response[25] = 0;
    response[26] = 4;
    response[27] = 87;
}

static int test_host_only_gathering(void)
{
    unsigned char arena[16384];
    ice_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(1, state.local_candidate_count);
    RTC_TEST_ASSERT(strstr(state.last_candidate, "typ host") != 0);
    RTC_TEST_EQ_INT(0, state.datagram_count);
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.gathering_complete") == 0);
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_peer_connection_gather_candidates(pc));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.ice.local_candidates);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_srflx_response_gathering(void)
{
    unsigned char arena[16384];
    uint8_t response[32];
    ice_observer_state_t state;
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
    RTC_TEST_EQ_INT(1, state.datagram_count);
    RTC_TEST_EQ_INT(20, state.last_datagram_len);
    RTC_TEST_ASSERT(rtc_stun_is_datagram(state.last_datagram,
                                         state.last_datagram_len));
    RTC_TEST_ASSERT(strstr(state.last_candidate, "typ host") != 0);

    write_success_response(response, state.last_datagram, 0xcb007107u, 54321);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_ice_handle_stun_response(pc, response,
                                                 sizeof(response)));
    RTC_TEST_EQ_INT(2, state.local_candidate_count);
    RTC_TEST_ASSERT(strstr(state.last_candidate, "typ srflx") != 0);
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.gathering_complete") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(2, (int)counters.ice.local_candidates);
    RTC_TEST_EQ_INT(1, (int)counters.stun.transactions_sent);
    RTC_TEST_EQ_INT(1, (int)counters.stun.transactions_received);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_stun_timeout(void)
{
    unsigned char arena[16384];
    ice_observer_state_t state;
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
    RTC_TEST_ASSERT(state.timer_task != 0);
    state.timer_task(state.timer_user_data);
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.gathering_complete") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.stun.transactions_timed_out);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_transaction_capacity(void)
{
    unsigned char arena[16384];
    ice_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);
    config.limits.ice.max_transactions = 1;
    config.stun_server_count = 1;
    config.stun_server.ip = "198.51.100.1";
    config.stun_server.ip_len = strlen(config.stun_server.ip);
    config.stun_server.port = 3478;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->stun_transactions[0].in_use = 1;
    pc->stun_transaction_count = 1;
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_STUN_TRANSACTIONS,
                    rtc_peer_connection_gather_candidates(pc));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static rtc_status_t add_remote_candidate(rtc_peer_connection_t *pc,
                                         const char *candidate)
{
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    return rtc_peer_connection_add_ice_candidate(pc, candidate,
                                                 strlen(candidate));
}

static int test_start_connectivity_checks_requires_local_candidate(void)
{
    unsigned char arena[16384];
    ice_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    add_remote_candidate(
                        pc, "candidate:1 1 udp 1 192.0.2.1 5000 typ host"));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_peer_connection_start_connectivity_checks(pc));
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.failed") == 0);
    RTC_TEST_EQ_INT(1, state.error_count);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_start_connectivity_checks_requires_remote_candidate(void)
{
    unsigned char arena[16384];
    ice_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_INVALID_STATE,
                    rtc_peer_connection_start_connectivity_checks(pc));
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.failed") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_controlling_regular_nomination_selects_pair(void)
{
    /* Covers regular nomination: first success triggers USE-CANDIDATE. */
    unsigned char arena[16384];
    uint8_t response[32];
    ice_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_role = RTC_ICE_ROLE_CONTROLLING;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    add_remote_candidate(
                        pc, "candidate:1 1 udp 1 192.0.2.1 5000 typ host"));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_start_connectivity_checks(pc));
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.checking") == 0);
    RTC_TEST_EQ_INT(1, state.datagram_count);

    write_success_response(response, state.last_datagram, 0xc0000201u, 5000);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_ice_handle_stun_response(pc, response,
                                                 sizeof(response)));
    RTC_TEST_EQ_INT(2, state.datagram_count);
    write_success_response(response, state.last_datagram, 0xc0000201u, 5000);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_ice_handle_stun_response(pc, response,
                                                 sizeof(response)));
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.connected") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.ice.selected_pairs);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_controlled_nominated_request_selects_pair(void)
{
    unsigned char arena[16384];
    uint8_t request[48];
    uint8_t txid[RTC_STUN_TRANSACTION_ID_BYTES];
    size_t request_len;
    ice_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    memset(txid, 0x44, sizeof(txid));
    config = test_config(arena, sizeof(arena), &state);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_role = RTC_ICE_ROLE_CONTROLLED;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    add_remote_candidate(
                        pc, "candidate:1 1 udp 1 192.0.2.1 5000 typ host"));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_start_connectivity_checks(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_stun_write_ice_binding_request(
                        request, sizeof(request), txid, 1, 1, 99,
                        &request_len));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_ice_handle_stun_response(pc, request, request_len));
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.connected") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_candidate_pair_capacity_and_trickle_pairs(void)
{
    unsigned char arena[16384];
    ice_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);
    config.limits.ice.max_candidate_pairs = 1;

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_role = RTC_ICE_ROLE_CONTROLLING;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    add_remote_candidate(
                        pc, "candidate:1 1 udp 1 192.0.2.1 5000 typ host"));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_start_connectivity_checks(pc));
    RTC_TEST_EQ_INT(1, (int)pc->candidate_pair_count);
    RTC_TEST_EQ_INT(RTC_STATUS_CAPACITY_ICE_PAIRS,
                    add_remote_candidate(
                        pc, "candidate:2 1 udp 1 192.0.2.2 5001 typ host"));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);
    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_role = RTC_ICE_ROLE_CONTROLLING;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    add_remote_candidate(
                        pc, "candidate:1 1 udp 1 192.0.2.1 5000 typ host"));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_start_connectivity_checks(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    add_remote_candidate(
                        pc, "candidate:2 1 udp 1 192.0.2.2 5001 typ host"));
    RTC_TEST_EQ_INT(2, (int)pc->candidate_pair_count);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_pair_check_timeout_exhausts_pairs(void)
{
    unsigned char arena[16384];
    ice_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_role = RTC_ICE_ROLE_CONTROLLING;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    add_remote_candidate(
                        pc, "candidate:1 1 udp 1 192.0.2.1 5000 typ host"));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_start_connectivity_checks(pc));
    RTC_TEST_ASSERT(state.timer_task != 0);
    state.timer_task(state.timer_user_data);
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.failed") == 0);
    RTC_TEST_ASSERT(strcmp(state.last_reason, "pair_check_exhausted") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.stun.transactions_timed_out);
    RTC_TEST_EQ_INT(1, (int)counters.ice.checks_failed);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_malformed_stun_fails_check(void)
{
    unsigned char arena[16384];
    uint8_t malformed[3] = {0xff, 0x00, 0x00};
    ice_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_role = RTC_ICE_ROLE_CONTROLLING;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    add_remote_candidate(
                        pc, "candidate:1 1 udp 1 192.0.2.1 5000 typ host"));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_start_connectivity_checks(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_ice_handle_stun_response(pc, malformed,
                                                 sizeof(malformed)));
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.failed") == 0);
    RTC_TEST_ASSERT(strcmp(state.last_reason, "malformed_stun") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

static int test_role_conflict_error_fails_with_trace(void)
{
    unsigned char arena[16384];
    uint8_t response[28];
    ice_observer_state_t state;
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    memset(&state, 0, sizeof(state));
    config = test_config(arena, sizeof(arena), &state);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    pc->ice_role = RTC_ICE_ROLE_CONTROLLING;
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    add_remote_candidate(
                        pc, "candidate:1 1 udp 1 192.0.2.1 5000 typ host"));
    rtc_executor_set_current_for_test(RTC_EXECUTOR_NETWORK);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_gather_candidates(pc));
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_start_connectivity_checks(pc));
    write_role_conflict_response(response, state.last_datagram);
    RTC_TEST_EQ_INT(RTC_STATUS_PROTOCOL_ERROR,
                    rtc_ice_handle_stun_response(pc, response,
                                                 sizeof(response)));
    RTC_TEST_ASSERT(strcmp(state.last_state, "ice.failed") == 0);
    RTC_TEST_ASSERT(strcmp(state.last_reason, "role_conflict") == 0);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    return 0;
}

int rtc_test_ice(void)
{
    RTC_TEST_EQ_INT(0, test_host_only_gathering());
    RTC_TEST_EQ_INT(0, test_srflx_response_gathering());
    RTC_TEST_EQ_INT(0, test_stun_timeout());
    RTC_TEST_EQ_INT(0, test_transaction_capacity());
    RTC_TEST_EQ_INT(0, test_start_connectivity_checks_requires_local_candidate());
    RTC_TEST_EQ_INT(0, test_start_connectivity_checks_requires_remote_candidate());
    RTC_TEST_EQ_INT(0, test_controlling_regular_nomination_selects_pair());
    RTC_TEST_EQ_INT(0, test_controlled_nominated_request_selects_pair());
    RTC_TEST_EQ_INT(0, test_candidate_pair_capacity_and_trickle_pairs());
    RTC_TEST_EQ_INT(0, test_pair_check_timeout_exhausts_pairs());
    RTC_TEST_EQ_INT(0, test_malformed_stun_fails_check());
    RTC_TEST_EQ_INT(0, test_role_conflict_error_fails_with_trace());
    return 0;
}
