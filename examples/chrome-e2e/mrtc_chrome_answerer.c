#include <micrortc/micrortc.h>

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

typedef struct AnswererApp {
    CliOptions options;
    MRTC_PEER_CONNECTION_HANDLE pc;
    MRTC_DATA_CHANNEL_HANDLE data_channel;
    OwnedIceServer owned_ice[MRTC_ANSWERER_ICE_MAX];
    MRTC_ICE_SERVER ice_servers[MRTC_ANSWERER_ICE_MAX];
    size_t ice_server_count;
    int failed;
    int stopped;
} AnswererApp;

static const char *usage(void)
{
    return "Usage: mrtc_chrome_answerer [options]\n"
           "\n"
           "Options:\n"
           "  --fixtures <dir>       Directory containing fixed H264/Opus fixtures.\n"
           "  --ice-config <path>    Optional local ICE config JSON file.\n"
           "  --session-id <id>      Optional session id included in emitted JSON.\n"
           "  --log-json             Keep diagnostics as JSON lines on stdout.\n"
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

    memset(&config, 0, sizeof(config));
    memset(&callbacks, 0, sizeof(callbacks));
    memset(&channel_callbacks, 0, sizeof(channel_callbacks));
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
        emit_event(app, "media.start.pending", "reason", "media task not installed");
        return 1;
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

    emit_hello(&app);
    ok = run_loop(&app);
    emit_done(&app);
    free_app(&app);
    return ok ? 0 : 1;
}
