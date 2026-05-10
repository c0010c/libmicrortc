#include "api/peer_connection.h"
#include "executor/executor.h"
#include "ice/ice.h"
#include "stun/stun.h"
#include "test_runner.h"

#include <string.h>

typedef struct ice_observer_state_t {
    int local_candidate_count;
    int datagram_count;
    int state_count;
    char last_candidate[256];
    uint8_t last_datagram[64];
    size_t last_datagram_len;
    const char *last_state;
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

int rtc_test_ice(void)
{
    RTC_TEST_EQ_INT(0, test_host_only_gathering());
    RTC_TEST_EQ_INT(0, test_srflx_response_gathering());
    RTC_TEST_EQ_INT(0, test_stun_timeout());
    RTC_TEST_EQ_INT(0, test_transaction_capacity());
    return 0;
}
