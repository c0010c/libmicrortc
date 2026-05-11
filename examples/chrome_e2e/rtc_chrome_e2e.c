#include "jsonl.h"
#include "rtc/rtc.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#if defined(_WIN32)
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <unistd.h>
#endif

typedef struct e2e_options_t {
    const char *ws_url;
    const char *local_ip;
    uint16_t udp_port;
    const char *output_dir;
    uint32_t timeout_ms;
    const char *sample_opus;
    const char *sample_h264;
    int dry_run;
} e2e_options_t;

static rtc_status_t e2e_post(void *user_data, rtc_executor_task_fn task,
                             void *task_user_data)
{
    (void)user_data;
    if (task != 0) {
        task(task_user_data);
    }
    return RTC_STATUS_OK;
}

static rtc_status_t e2e_schedule_timer(void *user_data, uint64_t delay_ms,
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

static rtc_status_t e2e_cancel_timer(void *user_data, uint64_t timer_id)
{
    (void)user_data;
    (void)timer_id;
    return RTC_STATUS_OK;
}

static rtc_executor_vtable_t e2e_executor(void)
{
    rtc_executor_vtable_t executor;
    executor.post = e2e_post;
    executor.schedule_timer = e2e_schedule_timer;
    executor.cancel_timer = e2e_cancel_timer;
    executor.user_data = 0;
    return executor;
}

static void e2e_defaults(e2e_options_t *options)
{
    memset(options, 0, sizeof(*options));
    options->ws_url = "ws://127.0.0.1:8080/ws";
    options->local_ip = "127.0.0.1";
    options->udp_port = 50000;
    options->output_dir = "examples/chrome_e2e/out";
    options->timeout_ms = 30000;
    options->sample_opus = "sample1.opus";
    options->sample_h264 = "test-25fps.h264";
}

static int parse_u16(const char *text, uint16_t *out)
{
    char *end;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value == 0 ||
        value > 65535ul) {
        return -1;
    }
    *out = (uint16_t)value;
    return 0;
}

static int parse_u32(const char *text, uint32_t *out)
{
    char *end;
    unsigned long value;

    errno = 0;
    value = strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value > 0xfffffffful) {
        return -1;
    }
    *out = (uint32_t)value;
    return 0;
}

static int require_value(int argc, char **argv, int index)
{
    (void)argc;
    return argv[index + 1] != 0 && argv[index + 1][0] != '-';
}

static int parse_args(int argc, char **argv, e2e_options_t *options)
{
    int i;

    e2e_defaults(options);
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--dry-run") == 0) {
            options->dry_run = 1;
        } else if (strcmp(argv[i], "--ws-url") == 0 && i + 1 < argc &&
                   require_value(argc, argv, i)) {
            options->ws_url = argv[++i];
        } else if (strcmp(argv[i], "--local-ip") == 0 && i + 1 < argc &&
                   require_value(argc, argv, i)) {
            options->local_ip = argv[++i];
        } else if (strcmp(argv[i], "--udp-port") == 0 && i + 1 < argc &&
                   require_value(argc, argv, i)) {
            if (parse_u16(argv[++i], &options->udp_port) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc &&
                   require_value(argc, argv, i)) {
            options->output_dir = argv[++i];
        } else if (strcmp(argv[i], "--timeout-ms") == 0 && i + 1 < argc &&
                   require_value(argc, argv, i)) {
            if (parse_u32(argv[++i], &options->timeout_ms) != 0) {
                return -1;
            }
        } else if (strcmp(argv[i], "--sample-opus") == 0 && i + 1 < argc &&
                   require_value(argc, argv, i)) {
            options->sample_opus = argv[++i];
        } else if (strcmp(argv[i], "--sample-h264") == 0 && i + 1 < argc &&
                   require_value(argc, argv, i)) {
            options->sample_h264 = argv[++i];
        } else {
            return -1;
        }
    }

    return options->ws_url != 0 && options->local_ip != 0 &&
                   options->output_dir != 0 && options->sample_opus != 0 &&
                   options->sample_h264 != 0
               ? 0
               : -1;
}

static int file_exists(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (file == 0) {
        return 0;
    }
    fclose(file);
    return 1;
}

static int ensure_output_dir(const char *path)
{
    struct stat st;

    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    if (mkdir(path, 0775) == 0) {
        return 0;
    }
    return errno == EEXIST ? 0 : -1;
}

static int make_path(char *out, size_t out_len, const char *dir,
                     const char *name)
{
    int written = snprintf(out, out_len, "%s/%s", dir, name);
    return written > 0 && (size_t)written < out_len ? 0 : -1;
}

static void on_state(void *user_data, const char *state)
{
    rtc_e2e_jsonl_event((rtc_e2e_jsonl_t *)user_data, "state", "none", "ok",
                        state);
}

static void on_error(void *user_data, rtc_status_t status,
                     const char *subsystem, const char *operation,
                     int detail_code)
{
    char detail[160];
    snprintf(detail, sizeof(detail), "%s.%s status=%d detail=%d",
             subsystem == 0 ? "unknown" : subsystem,
             operation == 0 ? "unknown" : operation, (int)status,
             detail_code);
    rtc_e2e_jsonl_event((rtc_e2e_jsonl_t *)user_data, "error",
                        subsystem == 0 ? "none" : subsystem, "failed",
                        detail);
}

static void on_trace(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count)
{
    (void)fields;
    (void)field_count;
    rtc_e2e_jsonl_event((rtc_e2e_jsonl_t *)user_data, "trace", "none", "ok",
                        event);
}

static void on_local_candidate(void *user_data, const char *candidate,
                               size_t candidate_len)
{
    char detail[512];
    size_t copy_len = candidate_len;
    if (copy_len >= sizeof(detail)) {
        copy_len = sizeof(detail) - 1u;
    }
    memcpy(detail, candidate, copy_len);
    detail[copy_len] = '\0';
    rtc_e2e_jsonl_event((rtc_e2e_jsonl_t *)user_data, "candidate", "ice",
                        "ok", detail);
}

static void on_datagram(void *user_data, const uint8_t *data, size_t data_len)
{
    (void)data;
    (void)data_len;
    rtc_e2e_jsonl_event((rtc_e2e_jsonl_t *)user_data, "datagram", "network",
                        "queued", "on_datagram");
}

static void fill_pc_config(rtc_peer_connection_config_t *config,
                           unsigned char *arena, size_t arena_size,
                           const e2e_options_t *options,
                           rtc_e2e_jsonl_t *jsonl)
{
    memset(config, 0, sizeof(*config));
    config->arena.data = arena;
    config->arena.size = arena_size;
    config->limits.sdp.max_description_bytes = 8192;
    config->limits.ice.max_candidates = 8;
    config->limits.ice.max_candidate_pairs = 16;
    config->limits.ice.max_transactions = 4;
    config->limits.ice.max_timer_slots = 8;
    config->limits.dtls.max_sessions = 1;
    config->limits.rtp.max_packet_cache = 32;
    config->limits.rtp.max_payload_bytes = 1200;
    config->limits.rtp.max_packets_per_frame = 8;
    config->limits.rtp.max_reassembly_bytes = 65536;
    config->limits.rtp.max_media_queue_slots = 4;
    config->limits.rtcp.max_reports = 8;
    config->limits.rtcp.max_feedback_packets = 8;
    config->limits.rtcp.max_sdes_cname_bytes = 64;
    config->limits.trace.max_events = 64;
    config->sdp.ice_ufrag = "ce2eufrg";
    config->sdp.ice_ufrag_len = strlen(config->sdp.ice_ufrag);
    config->sdp.ice_pwd = "ce2eicepassword1234567890";
    config->sdp.ice_pwd_len = strlen(config->sdp.ice_pwd);
    config->sdp.dtls_fingerprint =
        "sha-256 00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
        "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF";
    config->sdp.dtls_fingerprint_len = strlen(config->sdp.dtls_fingerprint);
    config->sdp.dtls_setup = "active";
    config->sdp.dtls_setup_len = strlen(config->sdp.dtls_setup);
    config->sdp.session_id = 6002;
    config->sdp.session_version = 1;
    config->local_host_ip = options->local_ip;
    config->local_host_ip_len = strlen(options->local_ip);
    config->local_host_port = options->udp_port;
    config->executors.signaling = e2e_executor();
    config->executors.media = e2e_executor();
    config->executors.network = e2e_executor();
    config->observer.on_state = on_state;
    config->observer.on_error = on_error;
    config->observer.on_trace = on_trace;
    config->observer.on_local_candidate = on_local_candidate;
    config->observer.on_datagram = on_datagram;
    config->observer.user_data = jsonl;
}

static int run_dry_run(const e2e_options_t *options, rtc_e2e_jsonl_t *jsonl)
{
    if (!file_exists(options->sample_opus)) {
        rtc_e2e_jsonl_summary(jsonl, 0, "media_file", "sample opus missing");
        return 1;
    }
    if (!file_exists(options->sample_h264)) {
        rtc_e2e_jsonl_summary(jsonl, 0, "media_file", "sample h264 missing");
        return 1;
    }
    rtc_e2e_jsonl_event(jsonl, "status", "media_file", "ok",
                        "samples.available");
    rtc_e2e_jsonl_summary(jsonl, 1, "none", "dry-run passed");
    return 0;
}

static int run_runtime(const e2e_options_t *options, rtc_e2e_jsonl_t *jsonl)
{
    unsigned char arena[262144];
    rtc_peer_connection_config_t config;
    rtc_capacity_diagnostics_t diag;
    rtc_peer_connection_t *pc = 0;
    rtc_status_t status;

    fill_pc_config(&config, arena, sizeof(arena), options, jsonl);
    status = rtc_peer_connection_create(&config, &diag, &pc);
    if (status != RTC_STATUS_OK) {
        rtc_e2e_jsonl_summary(jsonl, 0, "signaling",
                              "PeerConnection create failed");
        return 1;
    }

    rtc_e2e_jsonl_event(jsonl, "status", "signaling", "pending",
                        "network runtime implemented in follow-up task");
    (void)rtc_peer_connection_destroy(pc);
    rtc_e2e_jsonl_summary(jsonl, 0, "signaling", "no offer received");
    return 1;
}

int main(int argc, char **argv)
{
    e2e_options_t options;
    rtc_e2e_jsonl_t jsonl;
    char jsonl_path[512];
    int rc;

    memset(&jsonl, 0, sizeof(jsonl));
    if (parse_args(argc, argv, &options) != 0 ||
        ensure_output_dir(options.output_dir) != 0 ||
        make_path(jsonl_path, sizeof(jsonl_path), options.output_dir,
                  "rtc_chrome_e2e.jsonl") != 0 ||
        rtc_e2e_jsonl_open(&jsonl, jsonl_path) != 0) {
        fprintf(stderr, "rtc_chrome_e2e: invalid arguments or output path\n");
        return 2;
    }

    rtc_e2e_jsonl_event(&jsonl, "status", "process", "ok",
                        "process.started");
    rc = options.dry_run ? run_dry_run(&options, &jsonl)
                         : run_runtime(&options, &jsonl);
    rtc_e2e_jsonl_close(&jsonl);
    return rc;
}
