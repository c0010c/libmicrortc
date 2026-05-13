#include <micrortc/micrortc.h>

#include "media/media_transceiver.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MRTC_ANSWERER_LINE_MAX 131072u
#define MRTC_ANSWERER_SDP_MAX 65536u
#define MRTC_ANSWERER_PATH_MAX 512u
#define MRTC_ANSWERER_ICE_MAX 8u

typedef struct CliOptions {
    const char *fixtures_dir;
    const char *ice_config_path;
    const char *session_id;
    int log_json;
    int self_test_media;
    int self_test_media_callbacks;
    int help;
} CliOptions;

typedef struct ParsedMessage {
    char *type;
    char *sdp;
    char *candidate;
} ParsedMessage;

typedef struct OwnedIceServer {
    char *urls;
    char *username;
    char *password;
} OwnedIceServer;

typedef struct FixtureBytes {
    unsigned char *data;
    size_t size;
} FixtureBytes;

typedef struct OpusFixture {
    FixtureBytes packets[8];
    size_t packet_count;
} OpusFixture;

typedef struct MediaSendStats {
    size_t video_frames;
    size_t video_packets;
    size_t video_bytes;
    size_t audio_frames;
    size_t audio_packets;
    size_t audio_bytes;
} MediaSendStats;

typedef struct MediaFrameStats {
    size_t frame_count;
    size_t byte_count;
    uint64_t last_timestamp;
    size_t monotonic_timestamp_failures;
    size_t keyframe_count;
} MediaFrameStats;

typedef struct ProtectedPacketCapture {
    uint8_t packets[32][1600];
    size_t sizes[32];
    size_t count;
} ProtectedPacketCapture;

typedef struct AnswererApp {
    CliOptions options;
    MRTC_PEER_CONNECTION_HANDLE pc;
    MRTC_DATA_CHANNEL_HANDLE data_channel;
    MRTC_RTP_TRANSCEIVER_HANDLE video_transceiver;
    MRTC_RTP_TRANSCEIVER_HANDLE audio_transceiver;
    OwnedIceServer owned_ice[MRTC_ANSWERER_ICE_MAX];
    MRTC_ICE_SERVER ice_servers[MRTC_ANSWERER_ICE_MAX];
    size_t ice_server_count;
    MediaSendStats media_send_stats;
    MediaFrameStats video_frame_stats;
    MediaFrameStats audio_frame_stats;
    ProtectedPacketCapture packet_capture;
    int failed;
    int stopped;
} AnswererApp;

static void emit_error(AnswererApp *app, const char *stage, const char *message);

static const char *usage(void)
{
    return "Usage: mrtc_chrome_answerer [options]\n"
           "\n"
           "Options:\n"
           "  --fixtures <dir>       Directory containing fixed H264/Opus fixtures.\n"
           "  --ice-config <path>    Optional local ICE config JSON file.\n"
           "  --session-id <id>      Optional session id included in emitted JSON.\n"
           "  --log-json             Keep diagnostics as JSON lines on stdout.\n"
           "  --self-test-media      Verify fixtures and send them through the media API.\n"
           "  --self-test-media-callbacks\n"
           "                         Verify fixture receive callbacks through protected RTP loopback.\n"
           "  --help                 Show this help.\n"
           "\n"
           "Input JSON message types: offer, candidate, start-media, stop.\n"
           "Output JSON message types: hello, answer, candidate, event, done, error.\n";
}

static int parse_args(int argc, char **argv, CliOptions *options)
{
    int i;

    memset(options, 0, sizeof(*options));
    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            options->help = 1;
        } else if (strcmp(arg, "--log-json") == 0) {
            options->log_json = 1;
        } else if (strcmp(arg, "--self-test-media") == 0) {
            options->self_test_media = 1;
        } else if (strcmp(arg, "--self-test-media-callbacks") == 0) {
            options->self_test_media = 1;
            options->self_test_media_callbacks = 1;
        } else if (strcmp(arg, "--fixtures") == 0 && i + 1 < argc) {
            options->fixtures_dir = argv[++i];
        } else if (strcmp(arg, "--ice-config") == 0 && i + 1 < argc) {
            options->ice_config_path = argv[++i];
        } else if (strcmp(arg, "--session-id") == 0 && i + 1 < argc) {
            options->session_id = argv[++i];
        } else {
            fprintf(stderr, "{\"type\":\"error\",\"stage\":\"args\",\"message\":\"unknown or incomplete argument\"}\n");
            return 0;
        }
    }
    return 1;
}

static char *duplicate_range(const char *start, size_t len)
{
    char *copy = (char *) malloc(len + 1u);
    if (copy == 0) {
        return 0;
    }
    memcpy(copy, start, len);
    copy[len] = '\0';
    return copy;
}

static char *read_file(const char *path, size_t *size)
{
    FILE *file;
    long file_size;
    char *buffer;
    size_t read_size;

    if (path == 0) {
        return 0;
    }
    file = fopen(path, "rb");
    if (file == 0) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    file_size = ftell(file);
    if (file_size < 0) {
        fclose(file);
        return 0;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    buffer = (char *) malloc((size_t) file_size + 1u);
    if (buffer == 0) {
        fclose(file);
        return 0;
    }
    read_size = fread(buffer, 1, (size_t) file_size, file);
    fclose(file);
    if (read_size != (size_t) file_size) {
        free(buffer);
        return 0;
    }
    buffer[read_size] = '\0';
    if (size != 0) {
        *size = read_size;
    }
    return buffer;
}

static int path_join(char *buffer, size_t buffer_len, const char *dir, const char *name)
{
    size_t dir_len;
    int written;

    if (buffer == 0 || buffer_len == 0u || dir == 0 || dir[0] == '\0' || name == 0) {
        return 0;
    }
    dir_len = strlen(dir);
    written = snprintf(buffer, buffer_len, "%s%s%s", dir, dir[dir_len - 1u] == '/' ? "" : "/", name);
    return written > 0 && (size_t) written < buffer_len;
}

static int read_binary_file(const char *path, FixtureBytes *bytes)
{
    size_t size = 0;
    char *data = read_file(path, &size);

    if (data == 0 || size == 0u || bytes == 0) {
        free(data);
        return 0;
    }
    bytes->data = (unsigned char *) data;
    bytes->size = size;
    return 1;
}

static int h264_start_code_size_at(const unsigned char *data, size_t size, size_t offset)
{
    if (offset + 3u <= size && data[offset] == 0u && data[offset + 1u] == 0u && data[offset + 2u] == 1u) {
        return 3;
    }
    if (offset + 4u <= size && data[offset] == 0u && data[offset + 1u] == 0u &&
        data[offset + 2u] == 0u && data[offset + 3u] == 1u) {
        return 4;
    }
    return 0;
}

static int h264_fixture_is_valid(const FixtureBytes *h264)
{
    size_t offset = 0;
    int saw_sps = 0;
    int saw_pps = 0;
    int saw_idr = 0;

    if (h264 == 0 || h264->data == 0 || h264->size < 5u) {
        return 0;
    }

    while (offset < h264->size) {
        int start_code_size = h264_start_code_size_at(h264->data, h264->size, offset);
        size_t nalu_start;
        size_t next_start;
        unsigned int nalu_type;

        if (start_code_size == 0) {
            return 0;
        }
        nalu_start = offset + (size_t) start_code_size;
        if (nalu_start >= h264->size) {
            return 0;
        }
        next_start = nalu_start + 1u;
        while (next_start < h264->size && h264_start_code_size_at(h264->data, h264->size, next_start) == 0) {
            ++next_start;
        }
        nalu_type = h264->data[nalu_start] & 0x1fu;
        if (nalu_type == 7u) {
            if (saw_idr) {
                return 0;
            }
            saw_sps = 1;
        } else if (nalu_type == 8u) {
            if (!saw_sps || saw_idr) {
                return 0;
            }
            saw_pps = 1;
        } else if (nalu_type == 5u) {
            if (!saw_sps || !saw_pps) {
                return 0;
            }
            saw_idr = 1;
        }
        offset = next_start;
    }
    return saw_sps && saw_pps && saw_idr;
}

static int read_opus_fixture(const char *path, OpusFixture *fixture)
{
    FixtureBytes raw;
    size_t offset = 0;

    memset(&raw, 0, sizeof(raw));
    memset(fixture, 0, sizeof(*fixture));
    if (!read_binary_file(path, &raw)) {
        return 0;
    }

    while (offset < raw.size) {
        size_t packet_len;

        if (offset + 2u > raw.size || fixture->packet_count >= 8u) {
            free(raw.data);
            return 0;
        }
        packet_len = ((size_t) raw.data[offset] << 8u) | (size_t) raw.data[offset + 1u];
        offset += 2u;
        if (packet_len == 0u || offset + packet_len > raw.size) {
            free(raw.data);
            return 0;
        }
        fixture->packets[fixture->packet_count].data = (unsigned char *) malloc(packet_len);
        if (fixture->packets[fixture->packet_count].data == 0) {
            free(raw.data);
            return 0;
        }
        memcpy(fixture->packets[fixture->packet_count].data, raw.data + offset, packet_len);
        fixture->packets[fixture->packet_count].size = packet_len;
        offset += packet_len;
        fixture->packet_count++;
    }
    free(raw.data);
    return fixture->packet_count >= 2u;
}

static void free_opus_fixture(OpusFixture *fixture)
{
    size_t i;

    if (fixture == 0) {
        return;
    }
    for (i = 0; i < fixture->packet_count; ++i) {
        free(fixture->packets[i].data);
        fixture->packets[i].data = 0;
        fixture->packets[i].size = 0;
    }
    fixture->packet_count = 0;
}

static int load_media_fixtures(AnswererApp *app, FixtureBytes *h264, OpusFixture *opus)
{
    char h264_path[MRTC_ANSWERER_PATH_MAX];
    char opus_path[MRTC_ANSWERER_PATH_MAX];

    memset(h264, 0, sizeof(*h264));
    memset(opus, 0, sizeof(*opus));
    if (app->options.fixtures_dir == 0 || app->options.fixtures_dir[0] == '\0') {
        emit_error(app, "fixtures", "missing --fixtures path");
        return 0;
    }
    if (!path_join(h264_path, sizeof(h264_path), app->options.fixtures_dir, "h264_annexb_sample.h264") ||
        !path_join(opus_path, sizeof(opus_path), app->options.fixtures_dir, "opus_packets.bin")) {
        emit_error(app, "fixtures", "invalid --fixtures path");
        return 0;
    }
    if (!read_binary_file(h264_path, h264) || !h264_fixture_is_valid(h264)) {
        emit_error(app, "fixtures", "h264_annexb_sample.h264 must contain SPS/PPS before IDR");
        free(h264->data);
        h264->data = 0;
        h264->size = 0;
        return 0;
    }
    if (!read_opus_fixture(opus_path, opus)) {
        emit_error(app, "fixtures", "opus_packets.bin must contain at least two length-prefixed packets");
        free(h264->data);
        h264->data = 0;
        h264->size = 0;
        return 0;
    }
    return 1;
}

static const char *skip_ws(const char *cursor)
{
    while (cursor != 0 && *cursor != '\0' && isspace((unsigned char) *cursor)) {
        ++cursor;
    }
    return cursor;
}

static char *json_string_value(const char *json, const char *key)
{
    char pattern[64];
    const char *key_pos;
    const char *cursor;
    char *value;
    size_t capacity = 64u;
    size_t len = 0;

    if (json == 0 || key == 0 || strlen(key) + 4u >= sizeof(pattern)) {
        return 0;
    }
    (void) snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    key_pos = strstr(json, pattern);
    if (key_pos == 0) {
        return 0;
    }
    cursor = strchr(key_pos + strlen(pattern), ':');
    if (cursor == 0) {
        return 0;
    }
    cursor = skip_ws(cursor + 1);
    if (cursor == 0 || *cursor != '"') {
        return 0;
    }
    ++cursor;

    value = (char *) malloc(capacity);
    if (value == 0) {
        return 0;
    }

    while (*cursor != '\0' && *cursor != '"') {
        char ch = *cursor++;
        if (ch == '\\') {
            ch = *cursor++;
            if (ch == '\0') {
                free(value);
                return 0;
            }
            if (ch == 'n') {
                ch = '\n';
            } else if (ch == 'r') {
                ch = '\r';
            } else if (ch == 't') {
                ch = '\t';
            }
        }
        if (len + 1u >= capacity) {
            char *grown;
            capacity *= 2u;
            grown = (char *) realloc(value, capacity);
            if (grown == 0) {
                free(value);
                return 0;
            }
            value = grown;
        }
        value[len++] = ch;
    }
    if (*cursor != '"') {
        free(value);
        return 0;
    }
    value[len] = '\0';
    return value;
}

static int json_looks_like_object(const char *line)
{
    const char *start = skip_ws(line);
    size_t len;

    if (start == 0 || *start != '{') {
        return 0;
    }
    len = strlen(start);
    while (len > 0u && isspace((unsigned char) start[len - 1u])) {
        --len;
    }
    return len > 0u && start[len - 1u] == '}';
}

static void json_print_escaped(FILE *stream, const char *value)
{
    const unsigned char *cursor = (const unsigned char *) (value == 0 ? "" : value);

    fputc('"', stream);
    while (*cursor != '\0') {
        unsigned char ch = *cursor++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', stream);
            fputc(ch, stream);
        } else if (ch == '\n') {
            fputs("\\n", stream);
        } else if (ch == '\r') {
            fputs("\\r", stream);
        } else if (ch == '\t') {
            fputs("\\t", stream);
        } else if (ch < 0x20u) {
            fprintf(stream, "\\u%04x", (unsigned int) ch);
        } else {
            fputc((int) ch, stream);
        }
    }
    fputc('"', stream);
}

static void json_begin(const AnswererApp *app, const char *type)
{
    fputs("{\"type\":", stdout);
    json_print_escaped(stdout, type);
    if (app != 0 && app->options.session_id != 0) {
        fputs(",\"session_id\":", stdout);
        json_print_escaped(stdout, app->options.session_id);
    }
}

static void json_end(void)
{
    fputs("}\n", stdout);
    fflush(stdout);
}

static void emit_error(AnswererApp *app, const char *stage, const char *message)
{
    if (app != 0) {
        app->failed = 1;
    }
    json_begin(app, "error");
    fputs(",\"stage\":", stdout);
    json_print_escaped(stdout, stage);
    fputs(",\"message\":", stdout);
    json_print_escaped(stdout, message);
    json_end();
}

static void emit_event(AnswererApp *app, const char *name, const char *key, const char *value)
{
    json_begin(app, "event");
    fputs(",\"name\":", stdout);
    json_print_escaped(stdout, name);
    if (key != 0 && value != 0) {
        fputs(",\"fields\":{", stdout);
        json_print_escaped(stdout, key);
        fputc(':', stdout);
        json_print_escaped(stdout, value);
        fputc('}', stdout);
    }
    json_end();
}

static void emit_datachannel_message(AnswererApp *app,
                                     const char *direction,
                                     MRTC_DATA_CHANNEL_MESSAGE_TYPE message_type,
                                     size_t bytes,
                                     const char *text)
{
    json_begin(app, "event");
    fputs(",\"name\":\"datachannel.message\",\"fields\":{\"direction\":", stdout);
    json_print_escaped(stdout, direction);
    fputs(",\"message_type\":", stdout);
    json_print_escaped(stdout, message_type == MRTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY ? "binary" : "text");
    fprintf(stdout, ",\"bytes\":%lu", (unsigned long) bytes);
    if (text != 0) {
        fputs(",\"text\":", stdout);
        json_print_escaped(stdout, text);
    }
    fputs("}", stdout);
    json_end();
}

static void emit_media_sent(AnswererApp *app,
                            const char *kind,
                            const char *codec,
                            size_t frames,
                            size_t packets,
                            size_t bytes)
{
    json_begin(app, "event");
    fputs(",\"name\":\"media.sent\",\"fields\":{\"kind\":", stdout);
    json_print_escaped(stdout, kind);
    fputs(",\"codec\":", stdout);
    json_print_escaped(stdout, codec);
    fprintf(stdout,
            ",\"frames\":%lu,\"packets\":%lu,\"bytes\":%lu}",
            (unsigned long) frames,
            (unsigned long) packets,
            (unsigned long) bytes);
    json_end();
}

static void emit_media_frame(AnswererApp *app,
                             const char *kind,
                             const char *codec,
                             const MediaFrameStats *stats,
                             int timestamp_monotonic)
{
    json_begin(app, "event");
    fputs(",\"name\":\"media.frame\",\"fields\":{\"kind\":", stdout);
    json_print_escaped(stdout, kind);
    fputs(",\"codec\":", stdout);
    json_print_escaped(stdout, codec);
    fprintf(stdout,
            ",\"count\":%lu,\"bytes\":%lu,\"timestamp_monotonic\":%s}",
            (unsigned long) stats->frame_count,
            (unsigned long) stats->byte_count,
            timestamp_monotonic ? "true" : "false");
    json_end();
}

static const char *pc_state_name(MRTC_PEER_CONNECTION_STATE state)
{
    switch (state) {
        case MRTC_PEER_CONNECTION_STATE_NEW:
            return "new";
        case MRTC_PEER_CONNECTION_STATE_CONNECTING:
            return "connecting";
        case MRTC_PEER_CONNECTION_STATE_CONNECTED:
            return "connected";
        case MRTC_PEER_CONNECTION_STATE_DISCONNECTED:
            return "disconnected";
        case MRTC_PEER_CONNECTION_STATE_FAILED:
            return "failed";
        case MRTC_PEER_CONNECTION_STATE_CLOSED:
            return "closed";
        default:
            return "unknown";
    }
}

static void on_ice_candidate(void *user_data, const char *candidate)
{
    AnswererApp *app = (AnswererApp *) user_data;

    json_begin(app, "candidate");
    fputs(",\"candidate\":", stdout);
    json_print_escaped(stdout, candidate);
    fputs(",\"sdpMid\":null,\"sdpMLineIndex\":0", stdout);
    json_end();
}

static void on_connection_state_change(void *user_data, MRTC_PEER_CONNECTION_STATE state)
{
    AnswererApp *app = (AnswererApp *) user_data;
    const char *name = pc_state_name(state);

    emit_event(app, "pc.state", "state", name);
    if (state == MRTC_PEER_CONNECTION_STATE_CONNECTED) {
        emit_event(app, "pc.connected", "state", name);
    }
}

static void on_data_channel_open(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel)
{
    AnswererApp *app = (AnswererApp *) user_data;

    json_begin(app, "event");
    fputs(",\"name\":\"datachannel.open\",\"fields\":{\"label\":", stdout);
    json_print_escaped(stdout, mrtc_data_channel_label(channel));
    fprintf(stdout, ",\"id\":%u}", (unsigned int) mrtc_data_channel_id(channel));
    json_end();
}

static void on_data_channel_message(void *user_data,
                                    MRTC_DATA_CHANNEL_HANDLE channel,
                                    MRTC_DATA_CHANNEL_MESSAGE_TYPE message_type,
                                    const unsigned char *data,
                                    size_t data_len)
{
    AnswererApp *app = (AnswererApp *) user_data;

    if (message_type == MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT) {
        char *text = duplicate_range((const char *) data, data_len);
        if (text == 0) {
            emit_error(app, "datachannel.message", "out of memory");
            return;
        }
        emit_datachannel_message(app, "inbound", message_type, data_len, text);
        if (data_len > 5u && memcmp(data, "ping:", 5u) == 0) {
            size_t reply_len = data_len;
            char *reply = (char *) malloc(reply_len + 1u);
            if (reply == 0) {
                free(text);
                emit_error(app, "datachannel.message", "out of memory");
                return;
            }
            memcpy(reply, "pong:", 5u);
            memcpy(reply + 5u, data + 5u, data_len - 5u);
            reply[reply_len] = '\0';
            if (mrtc_data_channel_send(channel,
                                       MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT,
                                       (const unsigned char *) reply,
                                       reply_len) == MRTC_STATUS_OK) {
                emit_datachannel_message(app, "outbound", message_type, reply_len, reply);
            } else {
                emit_error(app, "datachannel.send", "failed to send pong");
            }
            free(reply);
        }
        free(text);
    } else {
        emit_datachannel_message(app, "inbound", message_type, data_len, 0);
        if (mrtc_data_channel_send(channel, MRTC_DATA_CHANNEL_MESSAGE_TYPE_BINARY, data, data_len) == MRTC_STATUS_OK) {
            emit_datachannel_message(app, "outbound", message_type, data_len, 0);
        } else {
            emit_error(app, "datachannel.send", "failed to echo binary message");
        }
    }
}

static void on_data_channel_close(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel)
{
    AnswererApp *app = (AnswererApp *) user_data;

    emit_event(app, "datachannel.close", "label", mrtc_data_channel_label(channel));
}

static void on_remote_data_channel(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel)
{
    static const MRTC_DATA_CHANNEL_CALLBACKS callbacks = {
        on_data_channel_open,
        on_data_channel_message,
        on_data_channel_close
    };
    AnswererApp *app = (AnswererApp *) user_data;

    (void) mrtc_data_channel_set_callbacks(channel, &callbacks, app);
    app->data_channel = channel;
}

static MRTC_STATUS on_media_packet(void *user_data,
                                   MRTC_PEER_CONNECTION_HANDLE peer_connection,
                                   MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                   const uint8_t *packet,
                                   size_t packet_size)
{
    AnswererApp *app = (AnswererApp *) user_data;
    (void) peer_connection;

    if (app == 0 || transceiver == 0 || packet == 0 || packet_size == 0u) {
        return MRTC_STATUS_INVALID_ARG;
    }
    if (transceiver->kind == MRTC_MEDIA_KIND_VIDEO) {
        app->media_send_stats.video_packets++;
        app->media_send_stats.video_bytes += packet_size;
    } else if (transceiver->kind == MRTC_MEDIA_KIND_AUDIO) {
        app->media_send_stats.audio_packets++;
        app->media_send_stats.audio_bytes += packet_size;
    }
    if (app->packet_capture.count < 32u && packet_size <= sizeof(app->packet_capture.packets[0])) {
        size_t index = app->packet_capture.count++;
        memcpy(app->packet_capture.packets[index], packet, packet_size);
        app->packet_capture.sizes[index] = packet_size;
    }
    return MRTC_STATUS_OK;
}

static void on_media_frame(void *user_data, MRTC_RTP_TRANSCEIVER_HANDLE transceiver, const MRTC_FRAME *frame)
{
    AnswererApp *app = (AnswererApp *) user_data;
    MediaFrameStats *stats;
    const char *kind;
    const char *codec;
    int timestamp_monotonic = 1;

    if (app == 0 || transceiver == 0 || frame == 0 || frame->data == 0 || frame->size == 0u) {
        return;
    }
    if (transceiver->kind == MRTC_MEDIA_KIND_VIDEO) {
        stats = &app->video_frame_stats;
        kind = "video";
        codec = "h264";
    } else {
        stats = &app->audio_frame_stats;
        kind = "audio";
        codec = "opus";
    }
    if (stats->frame_count > 0u && frame->presentation_ts <= stats->last_timestamp) {
        stats->monotonic_timestamp_failures++;
        timestamp_monotonic = 0;
        app->failed = 1;
    }
    stats->frame_count++;
    stats->byte_count += frame->size;
    stats->last_timestamp = frame->presentation_ts;
    if ((frame->flags & MRTC_FRAME_FLAG_KEY_FRAME) != 0u) {
        stats->keyframe_count++;
    }
    emit_media_frame(app, kind, codec, stats, timestamp_monotonic);
}

static void free_parsed_message(ParsedMessage *message)
{
    if (message == 0) {
        return;
    }
    free(message->type);
    free(message->sdp);
    free(message->candidate);
    memset(message, 0, sizeof(*message));
}

static int parse_message_line(const char *line, ParsedMessage *message)
{
    memset(message, 0, sizeof(*message));
    if (!json_looks_like_object(line)) {
        return 0;
    }
    message->type = json_string_value(line, "type");
    if (message->type == 0 || message->type[0] == '\0') {
        free_parsed_message(message);
        return 0;
    }
    message->sdp = json_string_value(line, "sdp");
    message->candidate = json_string_value(line, "candidate");
    return 1;
}

static int parse_ice_config_object(const char *object, OwnedIceServer *owned)
{
    char *credential;

    owned->urls = json_string_value(object, "urls");
    owned->username = json_string_value(object, "username");
    credential = json_string_value(object, "credential");
    owned->password = credential != 0 ? credential : json_string_value(object, "password");
    return owned->urls != 0 && owned->urls[0] != '\0';
}

static int load_ice_config(AnswererApp *app)
{
    char *json;
    const char *cursor;
    size_t size = 0;

    if (app->options.ice_config_path == 0) {
        return 1;
    }

    json = read_file(app->options.ice_config_path, &size);
    if (json == 0 || size == 0u) {
        free(json);
        emit_error(app, "ice-config", "failed to read ICE config");
        return 0;
    }

    cursor = json;
    while ((cursor = strchr(cursor, '{')) != 0 && app->ice_server_count < MRTC_ANSWERER_ICE_MAX) {
        const char *end = strchr(cursor, '}');
        char *object;
        OwnedIceServer parsed;

        if (end == 0) {
            break;
        }
        object = duplicate_range(cursor, (size_t) (end - cursor + 1));
        if (object == 0) {
            free(json);
            emit_error(app, "ice-config", "out of memory parsing ICE config");
            return 0;
        }
        memset(&parsed, 0, sizeof(parsed));
        if (parse_ice_config_object(object, &parsed)) {
            size_t index = app->ice_server_count++;
            app->owned_ice[index] = parsed;
            app->ice_servers[index].urls = parsed.urls;
            app->ice_servers[index].username = parsed.username;
            app->ice_servers[index].password = parsed.password;
        } else {
            free(parsed.urls);
            free(parsed.username);
            free(parsed.password);
        }
        free(object);
        cursor = end + 1;
    }

    free(json);
    return 1;
}

static int create_peer_connection(AnswererApp *app)
{
    MRTC_PEER_CONNECTION_CONFIG config;
    MRTC_PEER_CONNECTION_CALLBACKS callbacks;
    MRTC_DATA_CHANNEL_CALLBACKS channel_callbacks;
    MRTC_TRANSCEIVER_INIT video_init;
    MRTC_TRANSCEIVER_INIT audio_init;
    MRTC_TRANSCEIVER_CALLBACKS video_callbacks;
    MRTC_TRANSCEIVER_CALLBACKS audio_callbacks;

    memset(&config, 0, sizeof(config));
    memset(&callbacks, 0, sizeof(callbacks));
    memset(&channel_callbacks, 0, sizeof(channel_callbacks));
    memset(&video_init, 0, sizeof(video_init));
    memset(&audio_init, 0, sizeof(audio_init));
    memset(&video_callbacks, 0, sizeof(video_callbacks));
    memset(&audio_callbacks, 0, sizeof(audio_callbacks));
    callbacks.on_ice_candidate = on_ice_candidate;
    callbacks.on_connection_state_change = on_connection_state_change;
    callbacks.on_data_channel = on_remote_data_channel;
    config.ice_servers = app->ice_server_count == 0u ? 0 : app->ice_servers;
    config.ice_server_count = app->ice_server_count;

    if (mrtc_peer_connection_create(&config, &callbacks, app, &app->pc) != MRTC_STATUS_OK) {
        emit_error(app, "peer_connection.create", "failed to create peer connection");
        return 0;
    }
    channel_callbacks.on_open = on_data_channel_open;
    channel_callbacks.on_message = on_data_channel_message;
    channel_callbacks.on_close = on_data_channel_close;
    if (mrtc_peer_connection_create_data_channel(app->pc,
                                                 "mrtc-e2e",
                                                 0,
                                                 &channel_callbacks,
                                                 app,
                                                 &app->data_channel) != MRTC_STATUS_OK) {
        emit_error(app, "datachannel.create", "failed to create data channel");
        return 0;
    }
    video_init.kind = MRTC_MEDIA_KIND_VIDEO;
    video_init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;
    video_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    video_callbacks.on_frame = on_media_frame;
    video_init.callbacks = video_callbacks;
    audio_init.kind = MRTC_MEDIA_KIND_AUDIO;
    audio_init.codec = MRTC_CODEC_OPUS;
    audio_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    audio_callbacks.on_frame = on_media_frame;
    audio_init.callbacks = audio_callbacks;
    if (mrtc_peer_connection_add_transceiver(app->pc, &video_init, app, &app->video_transceiver) != MRTC_STATUS_OK ||
        mrtc_peer_connection_add_transceiver(app->pc, &audio_init, app, &app->audio_transceiver) != MRTC_STATUS_OK) {
        emit_error(app, "transceiver.create", "failed to create media transceivers");
        return 0;
    }
    (void) mrtc_peer_connection_set_media_send_hook(app->pc, on_media_packet, app);
    return 1;
}

static int connect_with_minimal_offer(AnswererApp *app)
{
    char *offer;
    char answer[MRTC_ANSWERER_SDP_MAX];
    size_t required_len = 0;
    MRTC_STATUS status;

    offer = read_file("tests/fixtures/minimal_offer.sdp", 0);
    if (offer == 0) {
        emit_error(app, "self-test", "minimal offer fixture missing");
        return 0;
    }
    status = mrtc_peer_connection_set_remote_description(app->pc, "offer", offer);
    free(offer);
    if (status != MRTC_STATUS_OK ||
        mrtc_peer_connection_create_answer(app->pc, answer, sizeof(answer), &required_len) != MRTC_STATUS_OK ||
        mrtc_peer_connection_set_local_description(app->pc, "answer", answer) != MRTC_STATUS_OK ||
        mrtc_peer_connection_add_ice_candidate(app->pc,
                                               "candidate:1 1 UDP 2122252543 192.0.2.1 54400 typ host") != MRTC_STATUS_OK) {
        emit_error(app, "self-test", "failed to connect fixture peer connection");
        return 0;
    }
    return 1;
}

static int send_media_fixtures(AnswererApp *app)
{
    FixtureBytes h264;
    OpusFixture opus;
    MRTC_FRAME frame;
    size_t before_packets;
    size_t before_bytes;
    int ok = 1;

    if (!load_media_fixtures(app, &h264, &opus)) {
        return 0;
    }

    app->packet_capture.count = 0;
    memset(&frame, 0, sizeof(frame));
    before_packets = app->media_send_stats.video_packets;
    before_bytes = app->media_send_stats.video_bytes;
    frame.data = h264.data;
    frame.size = h264.size;
    frame.presentation_ts = 10000000ull;
    frame.decoding_ts = frame.presentation_ts;
    frame.duration = 333333ull;
    frame.index = app->media_send_stats.video_frames;
    frame.flags = MRTC_FRAME_FLAG_KEY_FRAME;
    if (mrtc_transceiver_write_frame(app->video_transceiver, &frame) != MRTC_STATUS_OK) {
        emit_error(app, "media.video", "failed to send H264 fixture");
        ok = 0;
    } else {
        app->media_send_stats.video_frames++;
        emit_media_sent(app,
                        "video",
                        "h264",
                        app->media_send_stats.video_frames,
                        app->media_send_stats.video_packets - before_packets,
                        app->media_send_stats.video_bytes - before_bytes);
    }

    if (ok) {
        size_t i;
        before_packets = app->media_send_stats.audio_packets;
        before_bytes = app->media_send_stats.audio_bytes;
        for (i = 0; i < opus.packet_count; ++i) {
            frame.data = opus.packets[i].data;
            frame.size = opus.packets[i].size;
            frame.presentation_ts = 200000ull + (uint64_t) i * 200000ull;
            frame.decoding_ts = frame.presentation_ts;
            frame.duration = 200000ull;
            frame.index = app->media_send_stats.audio_frames;
            frame.flags = MRTC_FRAME_FLAG_NONE;
            if (mrtc_transceiver_write_frame(app->audio_transceiver, &frame) != MRTC_STATUS_OK) {
                emit_error(app, "media.audio", "failed to send Opus fixture");
                ok = 0;
                break;
            }
            app->media_send_stats.audio_frames++;
        }
        if (ok) {
            emit_media_sent(app,
                            "audio",
                            "opus",
                            app->media_send_stats.audio_frames,
                            app->media_send_stats.audio_packets - before_packets,
                            app->media_send_stats.audio_bytes - before_bytes);
        }
    }

    free(h264.data);
    free_opus_fixture(&opus);
    return ok;
}

static int replay_captured_media_packets(AnswererApp *app)
{
    size_t i;

    for (i = 0; i < app->packet_capture.count; ++i) {
        if (mrtc_peer_connection_receive_protected_media_packet(app->pc,
                                                                app->packet_capture.packets[i],
                                                                app->packet_capture.sizes[i]) != MRTC_STATUS_OK) {
            emit_error(app, "media.receive", "failed to replay protected RTP packet");
            return 0;
        }
    }
    if (app->video_frame_stats.frame_count == 0u || app->audio_frame_stats.frame_count < 2u) {
        emit_error(app, "media.receive", "media callbacks did not observe expected H264/Opus frames");
        return 0;
    }
    if (app->video_frame_stats.monotonic_timestamp_failures > 0u ||
        app->audio_frame_stats.monotonic_timestamp_failures > 0u) {
        emit_error(app, "media.receive", "media callback timestamps were not monotonic");
        return 0;
    }
    return 1;
}

static int handle_offer(AnswererApp *app, const ParsedMessage *message)
{
    char answer[MRTC_ANSWERER_SDP_MAX];
    size_t required_len = 0;
    MRTC_STATUS status;

    if (message->sdp == 0 || message->sdp[0] == '\0') {
        emit_error(app, "offer", "missing sdp");
        return 0;
    }

    status = mrtc_peer_connection_set_remote_description(app->pc, "offer", message->sdp);
    if (status != MRTC_STATUS_OK) {
        emit_error(app, "offer", "remote offer rejected");
        return 0;
    }

    status = mrtc_peer_connection_create_answer(app->pc, answer, sizeof(answer), &required_len);
    if (status != MRTC_STATUS_OK || required_len == 0u || required_len >= sizeof(answer)) {
        emit_error(app, "answer", "answer creation failed");
        return 0;
    }
    status = mrtc_peer_connection_set_local_description(app->pc, "answer", answer);
    if (status != MRTC_STATUS_OK) {
        emit_error(app, "answer", "local answer rejected");
        return 0;
    }

    json_begin(app, "answer");
    fputs(",\"sdp\":", stdout);
    json_print_escaped(stdout, answer);
    json_end();
    return 1;
}

static int handle_candidate(AnswererApp *app, const ParsedMessage *message)
{
    MRTC_STATUS status;

    if (message->candidate == 0 || message->candidate[0] == '\0') {
        emit_error(app, "candidate", "missing candidate");
        return 0;
    }
    status = mrtc_peer_connection_add_ice_candidate(app->pc, message->candidate);
    if (status != MRTC_STATUS_OK) {
        emit_error(app, "candidate", "remote candidate rejected");
        return 0;
    }
    emit_event(app, "candidate.added", "candidate", "redacted");
    return 1;
}

static int dispatch_message(AnswererApp *app, const ParsedMessage *message)
{
    if (strcmp(message->type, "hello") == 0) {
        emit_event(app, "hello.peer", "role", "browser");
        return 1;
    }
    if (strcmp(message->type, "offer") == 0) {
        return handle_offer(app, message);
    }
    if (strcmp(message->type, "candidate") == 0) {
        return handle_candidate(app, message);
    }
    if (strcmp(message->type, "start-media") == 0) {
        return send_media_fixtures(app);
    }
    if (strcmp(message->type, "stop") == 0) {
        app->stopped = 1;
        return 1;
    }
    emit_error(app, "message", "unsupported message type");
    return 0;
}

static int run_loop(AnswererApp *app)
{
    char *line = (char *) malloc(MRTC_ANSWERER_LINE_MAX);

    if (line == 0) {
        emit_error(app, "runtime", "out of memory");
        return 0;
    }
    while (!app->stopped && fgets(line, (int) MRTC_ANSWERER_LINE_MAX, stdin) != 0) {
        ParsedMessage message;
        if (!parse_message_line(line, &message)) {
            emit_error(app, "json", "invalid JSON line");
            free(line);
            return 0;
        }
        (void) dispatch_message(app, &message);
        free_parsed_message(&message);
        if (app->failed) {
            free(line);
            return 0;
        }
    }
    free(line);
    return !app->failed;
}

static void emit_hello(AnswererApp *app)
{
    json_begin(app, "hello");
    fputs(",\"role\":\"answerer\",\"protocol\":\"mrtc-jsonl-v1\"", stdout);
    json_end();
}

static void emit_done(AnswererApp *app)
{
    json_begin(app, "done");
    fputs(",\"summary\":{\"failed\":", stdout);
    fputs(app->failed ? "true" : "false", stdout);
    fprintf(stdout,
            ",\"c_media\":{\"video\":{\"frames\":%lu,\"bytes\":%lu,\"monotonic_timestamp_failures\":%lu,\"keyframes\":%lu},"
            "\"audio\":{\"frames\":%lu,\"bytes\":%lu,\"monotonic_timestamp_failures\":%lu}}",
            (unsigned long) app->video_frame_stats.frame_count,
            (unsigned long) app->video_frame_stats.byte_count,
            (unsigned long) app->video_frame_stats.monotonic_timestamp_failures,
            (unsigned long) app->video_frame_stats.keyframe_count,
            (unsigned long) app->audio_frame_stats.frame_count,
            (unsigned long) app->audio_frame_stats.byte_count,
            (unsigned long) app->audio_frame_stats.monotonic_timestamp_failures);
    fputs("}", stdout);
    json_end();
}

static void free_app(AnswererApp *app)
{
    size_t i;

    if (app == 0) {
        return;
    }
    mrtc_peer_connection_free(app->pc);
    app->pc = 0;
    for (i = 0; i < app->ice_server_count; ++i) {
        free(app->owned_ice[i].urls);
        free(app->owned_ice[i].username);
        free(app->owned_ice[i].password);
    }
}

int main(int argc, char **argv)
{
    AnswererApp app;
    int ok;

    memset(&app, 0, sizeof(app));
    if (!parse_args(argc, argv, &app.options)) {
        return 2;
    }
    if (app.options.help) {
        fputs(usage(), stdout);
        return 0;
    }

    if (!load_ice_config(&app) || !create_peer_connection(&app)) {
        free_app(&app);
        return 2;
    }

    if (app.options.self_test_media) {
        emit_hello(&app);
        ok = connect_with_minimal_offer(&app) && send_media_fixtures(&app);
        if (ok && app.options.self_test_media_callbacks) {
            ok = replay_captured_media_packets(&app);
        }
        emit_done(&app);
        free_app(&app);
        return ok ? 0 : 1;
    }

    emit_hello(&app);
    ok = run_loop(&app);
    emit_done(&app);
    free_app(&app);
    return ok ? 0 : 1;
}
