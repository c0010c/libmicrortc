#define _POSIX_C_SOURCE 200809L

#include <micrortc/micrortc.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FANOUT_MAX_SESSIONS 32u
#define FANOUT_LINE_MAX 262144u
#define FANOUT_SDP_MAX 131072u
#define FANOUT_PATH_MAX 4096u
#define FANOUT_VIDEO_DURATION_100NS 333333ull
#define FANOUT_AUDIO_DURATION_100NS 200000ull
#define FANOUT_SCAN_CHUNK 65536u

#ifndef FANOUT_ENABLE_DATA_CHANNEL
#define FANOUT_ENABLE_DATA_CHANNEL 0
#endif

typedef struct VideoFrame {
    off_t offset;
    size_t size;
    uint64_t pts_100ns;
    uint64_t duration_100ns;
    uint32_t flags;
} VideoFrame;

typedef struct AudioPacket {
    off_t offset;
    size_t size;
    uint64_t pts_100ns;
    uint64_t duration_100ns;
} AudioPacket;

typedef struct MediaSource {
    int video_fd;
    VideoFrame *video;
    size_t video_count;
    int audio_fd;
    AudioPacket *audio;
    size_t audio_count;
    unsigned char *scratch;
    size_t scratch_capacity;
    uint64_t duration_100ns;
} MediaSource;

struct FanoutApp;

typedef struct PeerSession {
    struct FanoutApp *app;
    char peer_id[64];
    MRTC_PEER_CONNECTION_HANDLE pc;
#if FANOUT_ENABLE_DATA_CHANNEL
    MRTC_DATA_CHANNEL_HANDLE data_channel;
#endif
    MRTC_RTP_TRANSCEIVER_HANDLE video;
    MRTC_RTP_TRANSCEIVER_HANDLE audio;
    MRTC_PEER_CONNECTION_STATE state;
    int connected;
    int sent_started;
    size_t next_video;
    size_t next_audio;
    uint64_t video_frames;
    uint64_t audio_frames;
    uint64_t video_bytes;
    uint64_t audio_bytes;
    uint64_t last_event_ns;
} PeerSession;

typedef struct FanoutApp {
    const char *media_dir;
    MediaSource media;
    PeerSession sessions[FANOUT_MAX_SESSIONS];
    size_t session_count;
    int media_started;
    uint64_t media_start_ns;
    int stopped;
    int failed;
} FanoutApp;

typedef struct ParsedMessage {
    char *type;
    char *peer_id;
    char *sdp;
    char *candidate;
} ParsedMessage;

static uint64_t now_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t) ts.tv_sec * 1000000000ull + (uint64_t) ts.tv_nsec;
}

static void json_print_escaped(FILE *out, const char *value)
{
    const unsigned char *cursor = (const unsigned char *) (value == 0 ? "" : value);
    fputc('"', out);
    while (*cursor != '\0') {
        unsigned char ch = *cursor++;
        if (ch == '"' || ch == '\\') {
            fputc('\\', out);
            fputc((int) ch, out);
        } else if (ch == '\n') {
            fputs("\\n", out);
        } else if (ch == '\r') {
            fputs("\\r", out);
        } else if (ch == '\t') {
            fputs("\\t", out);
        } else if (ch < 0x20u) {
            fprintf(out, "\\u%04x", ch);
        } else {
            fputc((int) ch, out);
        }
    }
    fputc('"', out);
}

static void emit_peer_event(PeerSession *session, const char *name, const char *fields_json)
{
    fputs("{\"type\":\"event\",\"peerId\":", stdout);
    json_print_escaped(stdout, session != 0 ? session->peer_id : "");
    fputs(",\"name\":", stdout);
    json_print_escaped(stdout, name);
    fputs(",\"fields\":", stdout);
    fputs(fields_json != 0 ? fields_json : "{}", stdout);
    fputs("}\n", stdout);
    fflush(stdout);
}

static void emit_error(FanoutApp *app, const char *stage, const char *message)
{
    (void) app;
    fputs("{\"type\":\"error\",\"stage\":", stdout);
    json_print_escaped(stdout, stage);
    fputs(",\"message\":", stdout);
    json_print_escaped(stdout, message);
    fputs("}\n", stdout);
    fflush(stdout);
}

static int path_join(char *out, size_t out_len, const char *dir, const char *leaf)
{
    int written;
    if (out == 0 || out_len == 0u || dir == 0 || leaf == 0) {
        return 0;
    }
    written = snprintf(out, out_len, "%s/%s", dir, leaf);
    return written > 0 && (size_t) written < out_len;
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

static size_t h264_next_start_code(const unsigned char *data, size_t size, size_t offset)
{
    size_t i;
    for (i = offset; i + 3u <= size; ++i) {
        if (h264_start_code_size_at(data, size, i) != 0) {
            return i;
        }
    }
    return size;
}

static int read_exact_at(int fd, off_t offset, unsigned char *data, size_t size)
{
    size_t total = 0;
    while (total < size) {
        ssize_t got = pread(fd, data + total, size - total, offset + (off_t) total);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (got == 0) {
            return 0;
        }
        total += (size_t) got;
    }
    return 1;
}

static int ensure_scratch(MediaSource *media, size_t size)
{
    unsigned char *grown;
    if (size <= media->scratch_capacity) {
        return 1;
    }
    grown = (unsigned char *) realloc(media->scratch, size);
    if (grown == 0) {
        return 0;
    }
    media->scratch = grown;
    media->scratch_capacity = size;
    return 1;
}

static unsigned char *read_media_span(MediaSource *media, int fd, off_t offset, size_t size)
{
    if (!ensure_scratch(media, size)) {
        return 0;
    }
    if (!read_exact_at(fd, offset, media->scratch, size)) {
        return 0;
    }
    return media->scratch;
}

static off_t file_size_for_fd(int fd)
{
    off_t current = lseek(fd, 0, SEEK_CUR);
    off_t end;
    if (current < 0) {
        return (off_t) -1;
    }
    end = lseek(fd, 0, SEEK_END);
    if (end < 0 || lseek(fd, current, SEEK_SET) < 0) {
        return (off_t) -1;
    }
    return end;
}

static int append_video_frame(MediaSource *media, off_t offset, size_t size)
{
    VideoFrame *grown;
    if (size == 0u) {
        return 1;
    }
    grown = (VideoFrame *) realloc(media->video, (media->video_count + 1u) * sizeof(*media->video));
    if (grown == 0) {
        return 0;
    }
    media->video = grown;
    media->video[media->video_count].offset = offset;
    media->video[media->video_count].size = size;
    media->video[media->video_count].pts_100ns = (uint64_t) media->video_count * FANOUT_VIDEO_DURATION_100NS;
    media->video[media->video_count].duration_100ns = FANOUT_VIDEO_DURATION_100NS;
    media->video[media->video_count].flags = MRTC_FRAME_FLAG_KEY_FRAME;
    media->video_count++;
    return 1;
}

static int load_video_frames(MediaSource *media, const char *path)
{
    unsigned char buffer[FANOUT_SCAN_CHUNK + 4u];
    off_t file_size;
    off_t scan_offset = 0;
    off_t frame_start = (off_t) -1;
    off_t last_start_code = (off_t) -1;

    media->video_fd = open(path, O_RDONLY);
    if (media->video_fd < 0) {
        return 0;
    }
    file_size = file_size_for_fd(media->video_fd);
    if (file_size <= 0) {
        return 0;
    }
    while (scan_offset < file_size) {
        size_t read_len = (size_t) ((file_size - scan_offset) > (off_t) FANOUT_SCAN_CHUNK ? FANOUT_SCAN_CHUNK : (file_size - scan_offset));
        size_t cursor;
        ssize_t got = pread(media->video_fd, buffer, read_len, scan_offset);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            return 0;
        }
        if (got == 0) {
            break;
        }
        read_len = (size_t) got;
        cursor = h264_next_start_code(buffer, read_len, 0u);
        while (cursor < read_len) {
            off_t global_offset = scan_offset + (off_t) cursor;
            int start_len = h264_start_code_size_at(buffer, read_len, cursor);
            unsigned char nalu_header = 0;
            int nalu_type;

            if (global_offset == last_start_code) {
                cursor = h264_next_start_code(buffer, read_len, cursor + 1u);
                continue;
            }
            last_start_code = global_offset;
            if (cursor + (size_t) start_len < read_len) {
                nalu_header = buffer[cursor + (size_t) start_len];
            } else if (!read_exact_at(media->video_fd, global_offset + (off_t) start_len, &nalu_header, 1u)) {
                return 0;
            }
            nalu_type = nalu_header & 0x1f;
            if (frame_start == (off_t) -1) {
                frame_start = global_offset;
            } else if (nalu_type == 9) {
                if (!append_video_frame(media, frame_start, (size_t) (global_offset - frame_start))) {
                    return 0;
                }
                frame_start = global_offset;
            }
            cursor = h264_next_start_code(buffer, read_len, cursor + (size_t) start_len);
        }
        if (scan_offset + (off_t) read_len >= file_size) {
            break;
        }
        scan_offset += read_len > 3u ? (off_t) (read_len - 3u) : (off_t) read_len;
    }
    if (frame_start != (off_t) -1 && frame_start < file_size) {
        if (!append_video_frame(media, frame_start, (size_t) (file_size - frame_start))) {
            return 0;
        }
    }
    return media->video_count > 0u;
}

static int load_audio_packets(MediaSource *media, const char *path)
{
    off_t file_size;
    off_t offset = 0;

    media->audio_fd = open(path, O_RDONLY);
    if (media->audio_fd < 0) {
        return 0;
    }
    file_size = file_size_for_fd(media->audio_fd);
    if (file_size <= 0) {
        return 0;
    }
    while (offset + 2 <= file_size) {
        unsigned char header[2];
        size_t packet_len;
        AudioPacket *grown;
        if (!read_exact_at(media->audio_fd, offset, header, sizeof(header))) {
            return 0;
        }
        packet_len = ((size_t) header[0] << 8u) | (size_t) header[1];
        offset += 2;
        if (packet_len == 0u || offset + (off_t) packet_len > file_size) {
            return 0;
        }
        grown = (AudioPacket *) realloc(media->audio, (media->audio_count + 1u) * sizeof(*media->audio));
        if (grown == 0) {
            return 0;
        }
        media->audio = grown;
        media->audio[media->audio_count].offset = offset;
        media->audio[media->audio_count].size = packet_len;
        media->audio[media->audio_count].pts_100ns = (uint64_t) media->audio_count * FANOUT_AUDIO_DURATION_100NS;
        media->audio[media->audio_count].duration_100ns = FANOUT_AUDIO_DURATION_100NS;
        media->audio_count++;
        offset += (off_t) packet_len;
    }
    return media->audio_count > 0u;
}

static int load_media(FanoutApp *app)
{
    char video_path[FANOUT_PATH_MAX];
    char audio_path[FANOUT_PATH_MAX];
    uint64_t video_duration;
    uint64_t audio_duration;

    if (!path_join(video_path, sizeof(video_path), app->media_dir, "video.h264") ||
        !path_join(audio_path, sizeof(audio_path), app->media_dir, "audio.opus")) {
        emit_error(app, "media", "invalid media directory path");
        return 0;
    }
    if (!load_video_frames(&app->media, video_path)) {
        emit_error(app, "media", "failed to load video.h264");
        return 0;
    }
    if (!load_audio_packets(&app->media, audio_path)) {
        emit_error(app, "media", "failed to load audio.opus");
        return 0;
    }
    video_duration = (uint64_t) app->media.video_count * FANOUT_VIDEO_DURATION_100NS;
    audio_duration = (uint64_t) app->media.audio_count * FANOUT_AUDIO_DURATION_100NS;
    app->media.duration_100ns = video_duration > audio_duration ? video_duration : audio_duration;
    return 1;
}

static void free_media(MediaSource *media)
{
    if (media->video_fd >= 0) {
        close(media->video_fd);
    }
    if (media->audio_fd >= 0) {
        close(media->audio_fd);
    }
    free(media->video);
    free(media->audio);
    free(media->scratch);
    memset(media, 0, sizeof(*media));
    media->video_fd = -1;
    media->audio_fd = -1;
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
    char pattern[80];
    const char *key_pos;
    const char *cursor;
    char *value;
    size_t capacity = 128u;
    size_t len = 0;

    if (json == 0 || key == 0 || strlen(key) + 4u >= sizeof(pattern)) {
        return 0;
    }
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
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

static int parse_message_line(const char *line, ParsedMessage *message)
{
    memset(message, 0, sizeof(*message));
    message->type = json_string_value(line, "type");
    message->peer_id = json_string_value(line, "peerId");
    message->sdp = json_string_value(line, "sdp");
    message->candidate = json_string_value(line, "candidate");
    return message->type != 0;
}

static void free_parsed_message(ParsedMessage *message)
{
    free(message->type);
    free(message->peer_id);
    free(message->sdp);
    free(message->candidate);
    memset(message, 0, sizeof(*message));
}

static PeerSession *find_session(FanoutApp *app, const char *peer_id)
{
    size_t i;
    if (peer_id == 0 || peer_id[0] == '\0') {
        return 0;
    }
    for (i = 0; i < app->session_count; ++i) {
        if (strcmp(app->sessions[i].peer_id, peer_id) == 0) {
            return &app->sessions[i];
        }
    }
    return 0;
}

static void on_ice_candidate(void *user_data, const char *candidate)
{
    PeerSession *session = (PeerSession *) user_data;
    fputs("{\"type\":\"candidate\",\"peerId\":", stdout);
    json_print_escaped(stdout, session->peer_id);
    fputs(",\"candidate\":", stdout);
    json_print_escaped(stdout, candidate);
    fputs(",\"sdpMid\":null,\"sdpMLineIndex\":0", stdout);
    fputs("}\n", stdout);
    fflush(stdout);
}

static void on_connection_state_change(void *user_data, MRTC_PEER_CONNECTION_STATE state)
{
    PeerSession *session = (PeerSession *) user_data;
    char fields[96];
    session->state = state;
    session->connected = state == MRTC_PEER_CONNECTION_STATE_CONNECTED;
    snprintf(fields, sizeof(fields), "{\"state\":%d}", (int) state);
    emit_peer_event(session, "pc.state", fields);
    if (session->connected) {
        emit_peer_event(session, "pc.connected", "{\"state\":\"connected\"}");
    }
}

#if FANOUT_ENABLE_DATA_CHANNEL
static void on_data_channel_open(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel)
{
    PeerSession *session = (PeerSession *) user_data;
    (void) channel;
    emit_peer_event(session, "datachannel.open", "{}");
}

static void on_data_channel_message(void *user_data,
                                    MRTC_DATA_CHANNEL_HANDLE channel,
                                    MRTC_DATA_CHANNEL_MESSAGE_TYPE type,
                                    const unsigned char *data,
                                    size_t data_len)
{
    PeerSession *session = (PeerSession *) user_data;
    const char *prefix = "ping:";
    (void) session;
    if (type == MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT && data != 0 && data_len >= strlen(prefix) &&
        memcmp(data, prefix, strlen(prefix)) == 0) {
        size_t reply_len = 5u + data_len - strlen(prefix);
        unsigned char *reply = (unsigned char *) malloc(reply_len + 1u);
        if (reply != 0) {
            memcpy(reply, "pong:", 5u);
            memcpy(reply + 5u, data + strlen(prefix), data_len - strlen(prefix));
            reply[reply_len] = '\0';
            (void) mrtc_data_channel_send(channel, MRTC_DATA_CHANNEL_MESSAGE_TYPE_TEXT, reply, reply_len);
            free(reply);
        }
    }
}

static void on_data_channel_close(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel)
{
    PeerSession *session = (PeerSession *) user_data;
    (void) channel;
    emit_peer_event(session, "datachannel.close", "{}");
}

static void on_remote_data_channel(void *user_data, MRTC_DATA_CHANNEL_HANDLE channel)
{
    PeerSession *session = (PeerSession *) user_data;
    MRTC_DATA_CHANNEL_CALLBACKS callbacks;
    memset(&callbacks, 0, sizeof(callbacks));
    callbacks.on_open = on_data_channel_open;
    callbacks.on_message = on_data_channel_message;
    callbacks.on_close = on_data_channel_close;
    (void) mrtc_data_channel_set_callbacks(channel, &callbacks, session);
    session->data_channel = channel;
}
#endif

static void on_media_frame(void *user_data, MRTC_RTP_TRANSCEIVER_HANDLE transceiver, const MRTC_FRAME *frame)
{
    (void) user_data;
    (void) transceiver;
    (void) frame;
}

static PeerSession *create_session(FanoutApp *app, const char *peer_id)
{
    PeerSession *session;
    MRTC_PEER_CONNECTION_CONFIG config;
    MRTC_PEER_CONNECTION_CALLBACKS pc_callbacks;
#if FANOUT_ENABLE_DATA_CHANNEL
    MRTC_DATA_CHANNEL_CALLBACKS dc_callbacks;
#endif
    MRTC_TRANSCEIVER_INIT video_init;
    MRTC_TRANSCEIVER_INIT audio_init;

    if (app->session_count >= FANOUT_MAX_SESSIONS || peer_id == 0 || peer_id[0] == '\0') {
        emit_error(app, "peer", "too many peers or missing peerId");
        return 0;
    }
    session = &app->sessions[app->session_count++];
    memset(session, 0, sizeof(*session));
    session->app = app;
    snprintf(session->peer_id, sizeof(session->peer_id), "%s", peer_id);

    memset(&config, 0, sizeof(config));
    memset(&pc_callbacks, 0, sizeof(pc_callbacks));
    pc_callbacks.on_ice_candidate = on_ice_candidate;
    pc_callbacks.on_connection_state_change = on_connection_state_change;
#if FANOUT_ENABLE_DATA_CHANNEL
    pc_callbacks.on_data_channel = on_remote_data_channel;
#endif
    if (mrtc_peer_connection_create(&config, &pc_callbacks, session, &session->pc) != MRTC_STATUS_OK) {
        emit_error(app, "peer", "failed to create PeerConnection");
        return 0;
    }

#if FANOUT_ENABLE_DATA_CHANNEL
    memset(&dc_callbacks, 0, sizeof(dc_callbacks));
    dc_callbacks.on_open = on_data_channel_open;
    dc_callbacks.on_message = on_data_channel_message;
    dc_callbacks.on_close = on_data_channel_close;
    (void) mrtc_peer_connection_create_data_channel(session->pc, "fanout-memory", 0, &dc_callbacks, session,
                                                    &session->data_channel);
#endif

    memset(&video_init, 0, sizeof(video_init));
    video_init.kind = MRTC_MEDIA_KIND_VIDEO;
    video_init.codec = MRTC_CODEC_H264_PROFILE_42E01F_PACKETIZATION_MODE_1;
    video_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    video_init.callbacks.on_frame = on_media_frame;
    if (mrtc_peer_connection_add_transceiver(session->pc, &video_init, session, &session->video) != MRTC_STATUS_OK) {
        emit_error(app, "peer", "failed to add video transceiver");
        return 0;
    }

    memset(&audio_init, 0, sizeof(audio_init));
    audio_init.kind = MRTC_MEDIA_KIND_AUDIO;
    audio_init.codec = MRTC_CODEC_OPUS;
    audio_init.direction = MRTC_RTP_TRANSCEIVER_DIRECTION_SENDRECV;
    audio_init.callbacks.on_frame = on_media_frame;
    if (mrtc_peer_connection_add_transceiver(session->pc, &audio_init, session, &session->audio) != MRTC_STATUS_OK) {
        emit_error(app, "peer", "failed to add audio transceiver");
        return 0;
    }

    emit_peer_event(session, "peer.created", "{}");
    return session;
}

static int handle_offer(FanoutApp *app, const ParsedMessage *message)
{
    PeerSession *session;
    char answer[FANOUT_SDP_MAX];
    size_t required_len = 0;

    session = find_session(app, message->peer_id);
    if (session == 0) {
        session = create_session(app, message->peer_id);
    }
    if (session == 0 || message->sdp == 0) {
        return 0;
    }
    if (mrtc_peer_connection_set_remote_description(session->pc, "offer", message->sdp) != MRTC_STATUS_OK ||
        mrtc_peer_connection_create_answer(session->pc, answer, sizeof(answer), &required_len) != MRTC_STATUS_OK ||
        required_len == 0u || required_len >= sizeof(answer) ||
        mrtc_peer_connection_set_local_description(session->pc, "answer", answer) != MRTC_STATUS_OK) {
        emit_error(app, "offer", "failed to accept offer");
        return 0;
    }

    fputs("{\"type\":\"answer\",\"peerId\":", stdout);
    json_print_escaped(stdout, session->peer_id);
    fputs(",\"sdp\":", stdout);
    json_print_escaped(stdout, answer);
    fputs("}\n", stdout);
    fflush(stdout);
    return 1;
}

static int handle_candidate(FanoutApp *app, const ParsedMessage *message)
{
    PeerSession *session = find_session(app, message->peer_id);
    if (session == 0 || message->candidate == 0) {
        return 0;
    }
    if (mrtc_peer_connection_add_ice_candidate(session->pc, message->candidate) != MRTC_STATUS_OK) {
        emit_error(app, "candidate", "failed to add candidate");
        return 0;
    }
    emit_peer_event(session, "candidate.added", "{}");
    return 1;
}

static void start_media(FanoutApp *app)
{
    if (!app->media_started) {
        app->media_start_ns = now_ns();
        app->media_started = 1;
        fputs("{\"type\":\"event\",\"name\":\"media.started\",\"fields\":{}}\n", stdout);
        fflush(stdout);
    }
}

static void emit_media_stats(PeerSession *session)
{
    char fields[256];
    snprintf(fields, sizeof(fields),
             "{\"videoFrames\":%" PRIu64 ",\"audioFrames\":%" PRIu64 ",\"videoBytes\":%" PRIu64 ",\"audioBytes\":%" PRIu64 "}",
             session->video_frames,
             session->audio_frames,
             session->video_bytes,
             session->audio_bytes);
    emit_peer_event(session, "media.stats", fields);
}

static void send_due_media(FanoutApp *app, PeerSession *session, uint64_t elapsed_100ns)
{
    uint64_t current_ns = now_ns();

    if (!session->connected || app->media.video_count == 0u || app->media.audio_count == 0u) {
        return;
    }
    if (!session->sent_started) {
        session->next_video = (size_t) (elapsed_100ns / FANOUT_VIDEO_DURATION_100NS);
        session->next_audio = (size_t) (elapsed_100ns / FANOUT_AUDIO_DURATION_100NS);
        session->sent_started = 1;
    }
    while (session->next_video < app->media.video_count &&
           app->media.video[session->next_video].pts_100ns <= elapsed_100ns) {
        MRTC_FRAME frame;
        VideoFrame *source = &app->media.video[session->next_video];
        unsigned char *data = read_media_span(&app->media, app->media.video_fd, source->offset, source->size);
        if (data == 0) {
            emit_error(app, "media", "failed to read video frame");
            app->failed = 1;
            app->stopped = 1;
            return;
        }
        memset(&frame, 0, sizeof(frame));
        frame.data = data;
        frame.size = source->size;
        frame.presentation_ts = source->pts_100ns;
        frame.decoding_ts = source->pts_100ns;
        frame.duration = source->duration_100ns;
        frame.index = session->video_frames;
        frame.flags = source->flags;
        if (mrtc_transceiver_write_frame(session->video, &frame) == MRTC_STATUS_OK) {
            session->video_frames++;
            session->video_bytes += source->size;
        }
        session->next_video++;
    }
    while (session->next_audio < app->media.audio_count &&
           app->media.audio[session->next_audio].pts_100ns <= elapsed_100ns) {
        MRTC_FRAME frame;
        AudioPacket *source = &app->media.audio[session->next_audio];
        unsigned char *data = read_media_span(&app->media, app->media.audio_fd, source->offset, source->size);
        if (data == 0) {
            emit_error(app, "media", "failed to read audio packet");
            app->failed = 1;
            app->stopped = 1;
            return;
        }
        memset(&frame, 0, sizeof(frame));
        frame.data = data;
        frame.size = source->size;
        frame.presentation_ts = source->pts_100ns;
        frame.decoding_ts = source->pts_100ns;
        frame.duration = source->duration_100ns;
        frame.index = session->audio_frames;
        frame.flags = MRTC_FRAME_FLAG_NONE;
        if (mrtc_transceiver_write_frame(session->audio, &frame) == MRTC_STATUS_OK) {
            session->audio_frames++;
            session->audio_bytes += source->size;
        }
        session->next_audio++;
    }
    if (current_ns - session->last_event_ns >= 1000000000ull) {
        session->last_event_ns = current_ns;
        emit_media_stats(session);
    }
}

static int dispatch_message(FanoutApp *app, const ParsedMessage *message)
{
    if (strcmp(message->type, "offer") == 0) {
        return handle_offer(app, message);
    }
    if (strcmp(message->type, "candidate") == 0) {
        return handle_candidate(app, message);
    }
    if (strcmp(message->type, "start-media") == 0) {
        start_media(app);
        return 1;
    }
    if (strcmp(message->type, "stop") == 0) {
        app->stopped = 1;
        return 1;
    }
    if (strcmp(message->type, "hello") == 0) {
        return 1;
    }
    return 0;
}

static int run_loop(FanoutApp *app)
{
    char *line = (char *) malloc(FANOUT_LINE_MAX);
    int stdin_fd = fileno(stdin);
    if (line == 0) {
        return 0;
    }
    while (!app->stopped) {
        fd_set read_fds;
        struct timeval timeout;
        int ready;
        size_t i;
        uint64_t elapsed_100ns = 0;

        for (i = 0; i < app->session_count; ++i) {
            if (app->sessions[i].pc != 0) {
                (void) mrtc_peer_connection_poll_transport(app->sessions[i].pc, 0);
            }
        }
        if (app->media_started) {
            uint64_t elapsed_ns = now_ns() - app->media_start_ns;
            elapsed_100ns = elapsed_ns / 100u;
            for (i = 0; i < app->session_count; ++i) {
                send_due_media(app, &app->sessions[i], elapsed_100ns);
            }
            if (elapsed_100ns >= app->media.duration_100ns) {
                fputs("{\"type\":\"event\",\"name\":\"media.complete\",\"fields\":{}}\n", stdout);
                fflush(stdout);
                app->media_started = 0;
            }
        }

        FD_ZERO(&read_fds);
        FD_SET(stdin_fd, &read_fds);
        timeout.tv_sec = 0;
        timeout.tv_usec = 20000;
        ready = select(stdin_fd + 1, &read_fds, 0, 0, &timeout);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (ready > 0 && FD_ISSET(stdin_fd, &read_fds)) {
            ParsedMessage message;
            if (fgets(line, (int) FANOUT_LINE_MAX, stdin) == 0) {
                break;
            }
            if (parse_message_line(line, &message)) {
                (void) dispatch_message(app, &message);
                free_parsed_message(&message);
            }
        }
    }
    free(line);
    return 1;
}

static void free_app(FanoutApp *app)
{
    size_t i;
    for (i = 0; i < app->session_count; ++i) {
        if (app->sessions[i].pc != 0) {
            mrtc_peer_connection_free(app->sessions[i].pc);
            app->sessions[i].pc = 0;
        }
    }
    free_media(&app->media);
}

static const char *arg_value(int argc, char **argv, const char *name)
{
    int i;
    for (i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    FanoutApp app;
    memset(&app, 0, sizeof(app));
    app.media.video_fd = -1;
    app.media.audio_fd = -1;
    app.media_dir = arg_value(argc, argv, "--media-dir");
    if (app.media_dir == 0) {
        fprintf(stderr, "Usage: fanout_answerer --media-dir <dir>\n");
        return 2;
    }
    if (mrtc_initialize() != MRTC_STATUS_OK) {
        fprintf(stderr, "fanout_answerer: mrtc_initialize failed\n");
        return 1;
    }
    if (!load_media(&app)) {
        free_app(&app);
        return 1;
    }
    printf("{\"type\":\"hello\",\"role\":\"fanout-answerer\",\"videoFrames\":%zu,\"audioPackets\":%zu,\"durationMs\":%" PRIu64
           ",\"dataChannel\":%s}\n",
           app.media.video_count,
           app.media.audio_count,
           (uint64_t) (app.media.duration_100ns / 10000ull),
           FANOUT_ENABLE_DATA_CHANNEL ? "true" : "false");
    fflush(stdout);
    (void) run_loop(&app);
    free_app(&app);
    mrtc_shutdown();
    return app.failed ? 1 : 0;
}
