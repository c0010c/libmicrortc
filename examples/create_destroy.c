#include "rtc/rtc.h"

#include <stdint.h>

static rtc_status_t example_post(void *user_data, rtc_executor_task_fn task,
                                 void *task_user_data)
{
    (void)user_data;
    if (task != 0) {
        task(task_user_data);
    }
    return RTC_STATUS_OK;
}

static rtc_status_t example_schedule_timer(void *user_data, uint64_t delay_ms,
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

static rtc_status_t example_cancel_timer(void *user_data, uint64_t timer_id)
{
    (void)user_data;
    (void)timer_id;
    return RTC_STATUS_OK;
}

static rtc_executor_vtable_t example_executor(void)
{
    rtc_executor_vtable_t executor;
    executor.post = example_post;
    executor.schedule_timer = example_schedule_timer;
    executor.cancel_timer = example_cancel_timer;
    executor.user_data = 0;
    return executor;
}

int main(void)
{
    unsigned char arena[1024];
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    config.arena.data = arena;
    config.arena.size = sizeof(arena);
    config.limits.sdp.max_description_bytes = 1024;
    config.limits.ice.max_candidates = 8;
    config.limits.ice.max_timer_slots = 8;
    config.limits.dtls.max_sessions = 1;
    config.limits.rtp.max_packet_cache = 16;
    config.limits.rtcp.max_reports = 4;
    config.limits.trace.max_events = 16;
    config.platform = 0;
    config.executors.signaling = example_executor();
    config.executors.media = example_executor();
    config.executors.network = example_executor();
    config.observer.on_state = 0;
    config.observer.on_error = 0;
    config.observer.on_trace = 0;
    config.observer.on_local_candidate = 0;
    config.observer.on_media_frame = 0;
    config.observer.on_datagram = 0;
    config.observer.user_data = 0;
    config.security_backend = 0;

    if (rtc_peer_connection_create(&config, &diag, &pc) != RTC_STATUS_OK) {
        return 1;
    }

    return rtc_peer_connection_destroy(pc) == RTC_STATUS_OK ? 0 : 1;
}
