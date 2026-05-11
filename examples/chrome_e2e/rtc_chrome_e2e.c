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
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#define E2E_RUN_ID "c-example"
#define E2E_WS_KEY "dGhlIHNhbXBsZSBub25jZQ=="
#define E2E_DATAGRAM_QUEUE_CAPACITY 8u
#define E2E_DATAGRAM_MAX_BYTES 1500u

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

typedef struct e2e_datagram_t {
    uint8_t bytes[E2E_DATAGRAM_MAX_BYTES];
    size_t len;
} e2e_datagram_t;

typedef struct e2e_context_t {
    rtc_e2e_jsonl_t *jsonl;
    int udp_fd;
    int ws_fd;
    int have_remote_addr;
    int have_local_candidate;
    int have_remote_candidate;
    struct sockaddr_storage remote_addr;
    socklen_t remote_addr_len;
    e2e_datagram_t queue[E2E_DATAGRAM_QUEUE_CAPACITY];
    size_t queue_head;
    size_t queue_tail;
    size_t queue_count;
} e2e_context_t;

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

static const char *json_value_string(const char *json, const char *key,
                                     char *out, size_t out_len)
{
    char pattern[64];
    const char *p;
    char *dst;
    size_t remaining;

    if (json == 0 || key == 0 || out == 0 || out_len == 0) {
        return 0;
    }
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (p == 0) {
        return 0;
    }
    p = strchr(p + strlen(pattern), ':');
    if (p == 0) {
        return 0;
    }
    ++p;
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    if (*p != '"') {
        return 0;
    }
    ++p;

    dst = out;
    remaining = out_len - 1u;
    while (*p != '\0' && *p != '"' && remaining > 0u) {
        if (*p == '\\' && p[1] != '\0') {
            ++p;
            if (*p == 'n') {
                *dst++ = '\n';
            } else if (*p == 'r') {
                *dst++ = '\r';
            } else if (*p == 't') {
                *dst++ = '\t';
            } else {
                *dst++ = *p;
            }
        } else {
            *dst++ = *p;
        }
        --remaining;
        ++p;
    }
    *dst = '\0';
    return out;
}

static int json_escape_string(const char *input, char *out, size_t out_len)
{
    size_t used = 0;

    if (input == 0 || out == 0 || out_len == 0) {
        return -1;
    }
    while (*input != '\0') {
        const char *replacement = 0;
        char escaped[7];

        switch (*input) {
        case '"':
            replacement = "\\\"";
            break;
        case '\\':
            replacement = "\\\\";
            break;
        case '\n':
            replacement = "\\n";
            break;
        case '\r':
            replacement = "\\r";
            break;
        case '\t':
            replacement = "\\t";
            break;
        default:
            if ((unsigned char)*input < 0x20u) {
                snprintf(escaped, sizeof(escaped), "\\u%04x",
                         (unsigned int)(unsigned char)*input);
                replacement = escaped;
            }
            break;
        }
        if (replacement != 0) {
            size_t len = strlen(replacement);
            if (used + len >= out_len) {
                return -1;
            }
            memcpy(out + used, replacement, len);
            used += len;
        } else {
            if (used + 1u >= out_len) {
                return -1;
            }
            out[used++] = *input;
        }
        ++input;
    }
    out[used] = '\0';
    return 0;
}

static int json_candidate_string(const char *json, char *out, size_t out_len)
{
    const char *outer = strstr(json, "\"candidate\"");
    const char *inner;

    if (outer == 0) {
        return -1;
    }
    inner = strstr(outer + strlen("\"candidate\""), "\"candidate\"");
    if (inner != 0) {
        return json_value_string(inner, "candidate", out, out_len) == 0 ? -1
                                                                        : 0;
    }
    return json_value_string(outer, "candidate", out, out_len) == 0 ? -1 : 0;
}

static int parse_candidate_addr(const char *candidate, e2e_context_t *ctx)
{
#if defined(_WIN32)
    (void)candidate;
    (void)ctx;
    return -1;
#else
    char scratch[768];
    char *parts[16];
    char *token;
    size_t count = 0;
    struct sockaddr_in *addr;
    uint16_t port;

    if (candidate == 0 || strlen(candidate) >= sizeof(scratch)) {
        return -1;
    }
    strcpy(scratch, candidate);
    token = strtok(scratch, " ");
    while (token != 0 && count < 16u) {
        parts[count++] = token;
        token = strtok(0, " ");
    }
    if (count < 6u || parse_u16(parts[5], &port) != 0) {
        return -1;
    }
    memset(&ctx->remote_addr, 0, sizeof(ctx->remote_addr));
    addr = (struct sockaddr_in *)&ctx->remote_addr;
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    if (inet_pton(AF_INET, parts[4], &addr->sin_addr) != 1) {
        return -1;
    }
    ctx->remote_addr_len = sizeof(*addr);
    ctx->have_remote_addr = 1;
    return 0;
#endif
}

static int parse_ws_url(const char *url, char *host, size_t host_len,
                        uint16_t *port, char *path, size_t path_len)
{
    const char *p;
    const char *slash;
    const char *colon;
    size_t len;

    if (strncmp(url, "ws://", 5) != 0) {
        return -1;
    }
    p = url + 5;
    slash = strchr(p, '/');
    if (slash == 0) {
        slash = p + strlen(p);
    }
    colon = memchr(p, ':', (size_t)(slash - p));
    if (colon == 0) {
        return -1;
    }
    len = (size_t)(colon - p);
    if (len == 0u || len >= host_len) {
        return -1;
    }
    memcpy(host, p, len);
    host[len] = '\0';
    {
        char port_text[16];
        len = (size_t)(slash - colon - 1);
        if (len == 0u || len >= sizeof(port_text)) {
            return -1;
        }
        memcpy(port_text, colon + 1, len);
        port_text[len] = '\0';
        if (parse_u16(port_text, port) != 0) {
            return -1;
        }
    }
    if (*slash == '\0') {
        return snprintf(path, path_len, "/") > 0 ? 0 : -1;
    }
    return snprintf(path, path_len, "%s", slash) > 0 ? 0 : -1;
}

static int set_nonblocking(int fd)
{
#if defined(_WIN32)
    (void)fd;
    return 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#endif
}

static int udp_open(const e2e_options_t *options, rtc_e2e_jsonl_t *jsonl)
{
#if defined(_WIN32)
    (void)options;
    (void)jsonl;
    return -1;
#else
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        rtc_e2e_jsonl_event(jsonl, "error", "ice", "failed",
                            "udp.socket_failed");
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(options->udp_port);
    if (inet_pton(AF_INET, options->local_ip, &addr.sin_addr) != 1) {
        close(fd);
        rtc_e2e_jsonl_event(jsonl, "error", "ice", "failed",
                            "udp.local_ip_invalid");
        return -1;
    }
    if (bind(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        rtc_e2e_jsonl_event(jsonl, "error", "ice", "failed",
                            "udp.bind_failed");
        return -1;
    }
    if (set_nonblocking(fd) != 0) {
        close(fd);
        return -1;
    }
    return fd;
#endif
}

static int ws_connect(const e2e_options_t *options, rtc_e2e_jsonl_t *jsonl)
{
#if defined(_WIN32)
    (void)options;
    (void)jsonl;
    return -1;
#else
    char host[128];
    char path[128];
    uint16_t port;
    int fd;
    struct sockaddr_in addr;
    char request[512];
    char response[1024];
    ssize_t n;

    if (parse_ws_url(options->ws_url, host, sizeof(host), &port, path,
                     sizeof(path)) != 0) {
        rtc_e2e_jsonl_event(jsonl, "error", "signaling", "failed",
                            "ws.url_invalid");
        return -1;
    }
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        rtc_e2e_jsonl_event(jsonl, "error", "signaling", "failed",
                            "ws.socket_failed");
        return -1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1 ||
        connect(fd, (const struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(fd);
        rtc_e2e_jsonl_event(jsonl, "error", "signaling", "failed",
                            "ws.connect_failed");
        return -1;
    }
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.1\r\n"
             "Host: %s:%u\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Key: %s\r\n"
             "Sec-WebSocket-Version: 13\r\n\r\n",
             path, host, (unsigned int)port, E2E_WS_KEY);
    if (send(fd, request, strlen(request), 0) < 0) {
        close(fd);
        return -1;
    }
    n = recv(fd, response, sizeof(response) - 1u, 0);
    if (n <= 0) {
        close(fd);
        return -1;
    }
    response[n] = '\0';
    if (strstr(response, " 101 ") == 0) {
        close(fd);
        rtc_e2e_jsonl_event(jsonl, "error", "signaling", "failed",
                            "ws.handshake_failed");
        return -1;
    }
    if (set_nonblocking(fd) != 0) {
        close(fd);
        return -1;
    }
    rtc_e2e_jsonl_event(jsonl, "status", "signaling", "connected",
                        "signaling.connected");
    return fd;
#endif
}

static int ws_send_text(int fd, const char *text)
{
#if defined(_WIN32)
    (void)fd;
    (void)text;
    return -1;
#else
    uint8_t frame[14 + 4096];
    size_t len = strlen(text);
    size_t header_len;
    size_t i;
    const uint8_t mask[4] = {0x12u, 0x34u, 0x56u, 0x78u};

    if (len > 4096u) {
        return -1;
    }
    frame[0] = 0x81u;
    if (len < 126u) {
        frame[1] = 0x80u | (uint8_t)len;
        memcpy(frame + 2, mask, 4);
        header_len = 6u;
    } else {
        frame[1] = 0x80u | 126u;
        frame[2] = (uint8_t)(len >> 8);
        frame[3] = (uint8_t)len;
        memcpy(frame + 4, mask, 4);
        header_len = 8u;
    }
    for (i = 0; i < len; ++i) {
        frame[header_len + i] = ((const uint8_t *)text)[i] ^ mask[i % 4u];
    }
    return send(fd, frame, header_len + len, 0) ==
                   (ssize_t)(header_len + len)
               ? 0
               : -1;
#endif
}

static int ws_recv_text(int fd, char *out, size_t out_len)
{
#if defined(_WIN32)
    (void)fd;
    (void)out;
    (void)out_len;
    return -1;
#else
    uint8_t header[4];
    uint8_t *payload;
    ssize_t n;
    size_t len;

    n = recv(fd, header, 2, 0);
    if (n <= 0) {
        return 0;
    }
    if (n != 2 || (header[0] & 0x0fu) != 0x01u) {
        return -1;
    }
    len = header[1] & 0x7fu;
    if (len == 126u) {
        if (recv(fd, header + 2, 2, 0) != 2) {
            return -1;
        }
        len = ((size_t)header[2] << 8) | header[3];
    } else if (len == 127u) {
        return -1;
    }
    if (len >= out_len) {
        return -1;
    }
    payload = (uint8_t *)out;
    n = recv(fd, payload, len, 0);
    if (n != (ssize_t)len) {
        return -1;
    }
    out[len] = '\0';
    return 1;
#endif
}

static void queue_datagram(e2e_context_t *ctx, const uint8_t *data,
                           size_t data_len)
{
    e2e_datagram_t *slot;

    if (ctx == 0 || data == 0 || data_len == 0u ||
        data_len > E2E_DATAGRAM_MAX_BYTES ||
        ctx->queue_count >= E2E_DATAGRAM_QUEUE_CAPACITY) {
        return;
    }
    slot = &ctx->queue[ctx->queue_tail];
    memcpy(slot->bytes, data, data_len);
    slot->len = data_len;
    ctx->queue_tail = (ctx->queue_tail + 1u) % E2E_DATAGRAM_QUEUE_CAPACITY;
    ctx->queue_count++;
}

static void flush_datagrams(e2e_context_t *ctx)
{
#if defined(_WIN32)
    (void)ctx;
#else
    while (ctx->queue_count > 0u && ctx->have_remote_addr) {
        e2e_datagram_t *slot = &ctx->queue[ctx->queue_head];
        (void)sendto(ctx->udp_fd, slot->bytes, slot->len, 0,
                     (const struct sockaddr *)&ctx->remote_addr,
                     ctx->remote_addr_len);
        ctx->queue_head = (ctx->queue_head + 1u) % E2E_DATAGRAM_QUEUE_CAPACITY;
        ctx->queue_count--;
    }
#endif
}

static void sleep_10ms(void)
{
#if defined(_WIN32)
#else
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000;
    (void)select(0, 0, 0, 0, &timeout);
#endif
}

static void send_status(e2e_context_t *ctx, const char *state)
{
    char message[256];
    if (ctx->ws_fd < 0) {
        return;
    }
    snprintf(message, sizeof(message),
             "{\"type\":\"status\",\"runId\":\"%s\",\"state\":\"%s\"}",
             E2E_RUN_ID, state);
    (void)ws_send_text(ctx->ws_fd, message);
}

static void on_state(void *user_data, const char *state)
{
    e2e_context_t *ctx = (e2e_context_t *)user_data;
    rtc_e2e_jsonl_event(ctx->jsonl, "state", "none", "ok", state);
    send_status(ctx, state);
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
    e2e_context_t *ctx = (e2e_context_t *)user_data;
    rtc_e2e_jsonl_event(ctx->jsonl, "error",
                        subsystem == 0 ? "none" : subsystem, "failed",
                        detail);
}

static void on_trace(void *user_data, const char *event,
                     const rtc_trace_field_t *fields, size_t field_count)
{
    (void)fields;
    (void)field_count;
    e2e_context_t *ctx = (e2e_context_t *)user_data;
    rtc_e2e_jsonl_event(ctx->jsonl, "trace", "none", "ok", event);
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
    e2e_context_t *ctx = (e2e_context_t *)user_data;
    char message[768];
    char escaped[512];
    rtc_e2e_jsonl_event(ctx->jsonl, "candidate", "ice", "ok", detail);
    ctx->have_local_candidate = 1;
    if (ctx->ws_fd >= 0 && json_escape_string(detail, escaped,
                                               sizeof(escaped)) == 0) {
        snprintf(message, sizeof(message),
                 "{\"type\":\"candidate\",\"runId\":\"%s\",\"candidate\":{\"candidate\":\"%s\",\"sdpMid\":\"0\",\"sdpMLineIndex\":0}}",
                 E2E_RUN_ID, escaped);
        (void)ws_send_text(ctx->ws_fd, message);
    }
}

static void on_datagram(void *user_data, const uint8_t *data, size_t data_len)
{
    e2e_context_t *ctx = (e2e_context_t *)user_data;
    uint8_t copy[E2E_DATAGRAM_MAX_BYTES];
    if (data_len <= sizeof(copy)) {
        memcpy(copy, data, data_len);
        queue_datagram(ctx, copy, data_len);
    }
    rtc_e2e_jsonl_event(ctx->jsonl, "datagram", "network",
                        "queued", "on_datagram");
}

static void fill_pc_config(rtc_peer_connection_config_t *config,
                           unsigned char *arena, size_t arena_size,
                           const e2e_options_t *options,
                           e2e_context_t *ctx)
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
    config->observer.user_data = ctx;
}

static int handle_offer(rtc_peer_connection_t *pc, e2e_context_t *ctx,
                        const char *offer_sdp)
{
    char answer[8192];
    char escaped_answer[8192];
    size_t answer_len = sizeof(answer);
    char message[9000];
    rtc_status_t status;

    status = rtc_peer_connection_set_remote_description(
        pc, offer_sdp, strlen(offer_sdp));
    if (status != RTC_STATUS_OK) {
        rtc_e2e_jsonl_event(ctx->jsonl, "error", "signaling", "failed",
                            "offer.set_remote_description_failed");
        return -1;
    }
    rtc_e2e_jsonl_event(ctx->jsonl, "status", "signaling", "ok",
                        "offer.received");

    status = rtc_peer_connection_create_answer(pc, answer, &answer_len);
    if (status != RTC_STATUS_OK) {
        rtc_e2e_jsonl_event(ctx->jsonl, "error", "signaling", "failed",
                            "answer.create_failed");
        return -1;
    }
    status = rtc_peer_connection_set_local_description(pc, answer, answer_len);
    if (status != RTC_STATUS_OK) {
        rtc_e2e_jsonl_event(ctx->jsonl, "error", "signaling", "failed",
                            "answer.set_local_description_failed");
        return -1;
    }
    answer[answer_len < sizeof(answer) ? answer_len : sizeof(answer) - 1u] =
        '\0';
    if (json_escape_string(answer, escaped_answer, sizeof(escaped_answer)) !=
        0) {
        rtc_e2e_jsonl_event(ctx->jsonl, "error", "signaling", "failed",
                            "answer.escape_failed");
        return -1;
    }
    snprintf(message, sizeof(message),
             "{\"type\":\"answer\",\"runId\":\"%s\",\"sdp\":\"%s\"}",
             E2E_RUN_ID, escaped_answer);
    (void)ws_send_text(ctx->ws_fd, message);
    rtc_e2e_jsonl_event(ctx->jsonl, "status", "signaling", "ok",
                        "answer.sent");
    (void)rtc_peer_connection_gather_candidates(pc);
    return 0;
}

static void handle_candidate(rtc_peer_connection_t *pc, e2e_context_t *ctx,
                             const char *candidate)
{
    rtc_status_t status;

    status = rtc_peer_connection_add_ice_candidate(pc, candidate,
                                                   strlen(candidate));
    if (status != RTC_STATUS_OK) {
        rtc_e2e_jsonl_event(ctx->jsonl, "error", "ice", "failed",
                            "candidate.add_failed");
        return;
    }
    ctx->have_remote_candidate = 1;
    (void)parse_candidate_addr(candidate, ctx);
    rtc_e2e_jsonl_event(ctx->jsonl, "status", "ice", "ok",
                        "remote_candidate.received");
    if (ctx->have_local_candidate && ctx->have_remote_candidate) {
        (void)rtc_peer_connection_start_connectivity_checks(pc);
    }
}

static void pump_udp(rtc_peer_connection_t *pc, e2e_context_t *ctx)
{
#if defined(_WIN32)
    (void)pc;
    (void)ctx;
#else
    uint8_t buffer[E2E_DATAGRAM_MAX_BYTES];
    struct sockaddr_storage from;
    socklen_t from_len = sizeof(from);
    ssize_t n;

    n = recvfrom(ctx->udp_fd, buffer, sizeof(buffer), 0,
                 (struct sockaddr *)&from, &from_len);
    if (n > 0) {
        memcpy(&ctx->remote_addr, &from, from_len);
        ctx->remote_addr_len = from_len;
        ctx->have_remote_addr = 1;
        (void)rtc_peer_connection_receive_datagram(pc, buffer, (size_t)n);
    }
#endif
}

static void pump_ws(rtc_peer_connection_t *pc, e2e_context_t *ctx)
{
    char message[8192];
    char type[32];
    char value[7000];
    int rc = ws_recv_text(ctx->ws_fd, message, sizeof(message));

    if (rc <= 0) {
        return;
    }
    if (json_value_string(message, "type", type, sizeof(type)) == 0) {
        rtc_e2e_jsonl_event(ctx->jsonl, "error", "signaling", "failed",
                            "invalid_json");
        return;
    }
    if (strcmp(type, "offer") == 0) {
        if (json_value_string(message, "sdp", value, sizeof(value)) != 0) {
            (void)handle_offer(pc, ctx, value);
        }
    } else if (strcmp(type, "candidate") == 0) {
        if (json_candidate_string(message, value, sizeof(value)) == 0) {
            handle_candidate(pc, ctx, value);
        }
    } else if (strcmp(type, "status") == 0 || strcmp(type, "summary") == 0) {
        rtc_e2e_jsonl_event(ctx->jsonl, "status", "signaling", "ok", type);
    } else {
        rtc_e2e_jsonl_event(ctx->jsonl, "error", "signaling", "failed",
                            "unknown_type");
    }
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
    e2e_context_t ctx;
    rtc_status_t status;
    uint32_t elapsed_ms = 0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.jsonl = jsonl;
    ctx.udp_fd = -1;
    ctx.ws_fd = -1;
    ctx.udp_fd = udp_open(options, jsonl);
    if (ctx.udp_fd < 0) {
        rtc_e2e_jsonl_summary(jsonl, 0, "ice", "UDP socket failed");
        return 1;
    }
    ctx.ws_fd = ws_connect(options, jsonl);
    if (ctx.ws_fd < 0) {
#if !defined(_WIN32)
        close(ctx.udp_fd);
#endif
        rtc_e2e_jsonl_summary(jsonl, 0, "signaling",
                              "WebSocket connection failed");
        return 1;
    }
    (void)ws_send_text(ctx.ws_fd,
                       "{\"type\":\"hello\",\"runId\":\"" E2E_RUN_ID
                       "\",\"role\":\"c-example\"}");

    fill_pc_config(&config, arena, sizeof(arena), options, &ctx);
    status = rtc_peer_connection_create(&config, &diag, &pc);
    if (status != RTC_STATUS_OK) {
        rtc_e2e_jsonl_summary(jsonl, 0, "signaling",
                              "PeerConnection create failed");
        return 1;
    }

    rtc_e2e_jsonl_event(jsonl, "status", "signaling", "pending",
                        "waiting.offer");
    while (elapsed_ms < options->timeout_ms) {
        pump_ws(pc, &ctx);
        pump_udp(pc, &ctx);
        flush_datagrams(&ctx);
        sleep_10ms();
        elapsed_ms += 10u;
    }
    (void)rtc_peer_connection_destroy(pc);
    rtc_e2e_jsonl_summary(jsonl, 0, "signaling", "no offer received");
#if !defined(_WIN32)
    close(ctx.ws_fd);
    close(ctx.udp_fd);
#endif
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
