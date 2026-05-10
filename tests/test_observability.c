#include "executor/executor.h"
#include "rtc/rtc.h"
#include "test_runner.h"

#include <string.h>

typedef struct observer_state_t {
    int error_count;
    int trace_count;
    rtc_status_t last_error;
    const char *last_trace;
} observer_state_t;

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
    (void)user_data;
    (void)delay_ms;
    (void)task;
    (void)task_user_data;
    if (out_timer_id != 0) {
        *out_timer_id = 1;
    }
    return RTC_STATUS_OK;
}

static rtc_status_t test_cancel_timer(void *user_data, uint64_t timer_id)
{
    (void)user_data;
    (void)timer_id;
    return RTC_STATUS_OK;
}

static rtc_executor_vtable_t test_executor(void)
{
    rtc_executor_vtable_t executor;
    executor.post = test_post;
    executor.schedule_timer = test_schedule_timer;
    executor.cancel_timer = test_cancel_timer;
    executor.user_data = 0;
    return executor;
}

static void on_error(void *user_data, rtc_status_t status,
                     const char *subsystem, const char *operation,
                     int detail_code)
{
    observer_state_t *state = (observer_state_t *)user_data;
    (void)subsystem;
    (void)operation;
    (void)detail_code;
    state->error_count++;
    state->last_error = status;
}

static void on_trace(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count)
{
    observer_state_t *state = (observer_state_t *)user_data;
    (void)fields;
    (void)field_count;
    state->trace_count++;
    state->last_trace = event;
}

static rtc_peer_connection_config_t test_config(unsigned char *arena,
                                                size_t arena_size,
                                                observer_state_t *state)
{
    rtc_peer_connection_config_t config;
    config.arena.data = arena;
    config.arena.size = arena_size;
    config.limits.sdp.max_description_bytes = 1024;
    config.limits.ice.max_candidates = 8;
    config.limits.ice.max_timer_slots = 8;
    config.limits.dtls.max_sessions = 1;
    config.limits.rtp.max_packet_cache = 16;
    config.limits.rtcp.max_reports = 4;
    config.limits.trace.max_events = 16;
    config.platform = 0;
    config.executors.signaling = test_executor();
    config.executors.media = test_executor();
    config.executors.network = test_executor();
    config.observer.on_state = 0;
    config.observer.on_error = on_error;
    config.observer.on_trace = on_trace;
    config.observer.on_local_candidate = 0;
    config.observer.on_media_frame = 0;
    config.observer.on_datagram = 0;
    config.observer.user_data = state;
    config.security_backend = 0;
    return config;
}

int rtc_test_observability(void)
{
    unsigned char arena[512];
    observer_state_t state = {0, 0, RTC_STATUS_OK, 0};
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;
    rtc_peer_connection_counters_t counters;

    config = test_config(arena, sizeof(arena), &state);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_create(&config, &diag, &pc));
    RTC_TEST_ASSERT(state.trace_count > 0);
    RTC_TEST_ASSERT(strcmp(state.last_trace, RTC_TRACE_PC_CREATE) == 0);

    RTC_TEST_EQ_INT(RTC_STATUS_UNSUPPORTED,
                    rtc_peer_connection_create_offer(pc, 0, 0));
    RTC_TEST_EQ_INT(RTC_STATUS_UNSUPPORTED, state.last_error);

    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.api.unsupported_api_calls);

    rtc_executor_set_current_for_test(RTC_EXECUTOR_MEDIA);
    RTC_TEST_EQ_INT(RTC_STATUS_AFFINITY_VIOLATION,
                    rtc_peer_connection_create_answer(pc, 0, 0));

    rtc_executor_set_current_for_test(RTC_EXECUTOR_SIGNALING);
    RTC_TEST_EQ_INT(RTC_STATUS_OK,
                    rtc_peer_connection_get_counters(pc, &counters));
    RTC_TEST_EQ_INT(1, (int)counters.executor.affinity_errors);
    RTC_TEST_EQ_INT(RTC_STATUS_OK, rtc_peer_connection_destroy(pc));
    RTC_TEST_ASSERT(strcmp(state.last_trace, RTC_TRACE_PC_DESTROY) == 0);

    return 0;
}
