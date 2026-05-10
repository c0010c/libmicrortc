#include "rtc/rtc.h"

#include <stdint.h>
#include <string.h>

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
    unsigned char arena[16384];
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc;

    config.arena.data = arena;
    config.arena.size = sizeof(arena);
    config.limits.sdp.max_description_bytes = 1024;
    config.limits.ice.max_candidates = 8;
    config.limits.ice.max_candidate_pairs = 16;
    config.limits.ice.max_transactions = 4;
    config.limits.ice.max_timer_slots = 8;
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
