#include "sdp.h"

#include "media/media_transceiver.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MRTC_SDP {
    char *normalized;
    char *first_media_line;
    char *ice_ufrag;
    char *ice_pwd;
    char *fingerprint;
    char *setup;
    size_t media_count;
    size_t attribute_count;
    int has_application;
    int has_sctp_port;
    int has_candidate;
};

static int mrtc_has_prefix(const char *line, size_t line_len, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    return line_len >= prefix_len && memcmp(line, prefix, prefix_len) == 0;
}

static MRTC_STATUS mrtc_copy_string(const char *source, size_t len, char **target)
{
    char *copy = (char *) malloc(len + 1);

    if (copy == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    if (len > 0) {
        memcpy(copy, source, len);
    }
    copy[len] = '\0';
    *target = copy;
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_replace_sdp_string(char **target, const char *source, size_t len)
{
    char *copy;

    if (target == 0 || source == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    copy = 0;
    if (mrtc_copy_string(source, len, &copy) != MRTC_STATUS_OK) {
        return MRTC_STATUS_INVALID_ARG;
    }

    free(*target);
    *target = copy;
    return MRTC_STATUS_OK;
}

static int mrtc_line_equals(const char *line, size_t line_len, const char *value)
{
    size_t value_len = strlen(value);
    return line_len == value_len && memcmp(line, value, value_len) == 0;
}

static MRTC_STATUS mrtc_parse_attribute_value(char **target,
                                             const char *line_start,
                                             size_t line_len,
                                             const char *prefix)
{
    size_t prefix_len = strlen(prefix);

    if (!mrtc_has_prefix(line_start, line_len, prefix)) {
        return MRTC_STATUS_OK;
    }

    return mrtc_replace_sdp_string(target, line_start + prefix_len, line_len - prefix_len);
}

static int mrtc_fingerprint_value_is_valid(const char *value)
{
    size_t len;
    size_t i;

    if (value == 0) {
        return 0;
    }

    len = strlen(value);
    if (len < 8) {
        return 0;
    }

    for (i = 0; i < len; ++i) {
        char ch = value[i];
        if (!((ch >= '0' && ch <= '9') ||
              (ch >= 'A' && ch <= 'F') ||
              (ch >= 'a' && ch <= 'f') ||
              ch == ':')) {
            return 0;
        }
    }

    return strchr(value, ':') != 0;
}

static const char *mrtc_local_setup_from_remote(const char *remote_setup)
{
    if (remote_setup != 0 && strcmp(remote_setup, "active") == 0) {
        return "passive";
    }
    return "active";
}

static MRTC_STATUS mrtc_append_line(char **buffer, size_t *len, size_t *capacity, const char *line, size_t line_len)
{
    size_t needed = *len + line_len + 2 + 1;
    char *grown;
    size_t new_capacity = *capacity;

    if (needed > new_capacity) {
        while (needed > new_capacity) {
            new_capacity = new_capacity == 0 ? 256 : new_capacity * 2;
        }

        grown = (char *) realloc(*buffer, new_capacity);
        if (grown == 0) {
            return MRTC_STATUS_INVALID_ARG;
        }

        *buffer = grown;
        *capacity = new_capacity;
    }

    memcpy(*buffer + *len, line, line_len);
    *len += line_len;
    (*buffer)[(*len)++] = '\r';
    (*buffer)[(*len)++] = '\n';
    (*buffer)[*len] = '\0';
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_append_literal_line(char **buffer, size_t *len, size_t *capacity, const char *line)
{
    return mrtc_append_line(buffer, len, capacity, line, strlen(line));
}

static MRTC_STATUS mrtc_append_formatted_line(char **buffer, size_t *len, size_t *capacity, const char *format, ...)
{
    char line[256];
    va_list args;
    int written;

    va_start(args, format);
    written = vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if (written < 0 || (size_t) written >= sizeof(line)) {
        return MRTC_STATUS_INVALID_ARG;
    }

    return mrtc_append_line(buffer, len, capacity, line, (size_t) written);
}

static MRTC_STATUS mrtc_write_buffer(const char *value, char *buffer, size_t buffer_len, size_t *required_len)
{
    size_t needed;

    if (value == 0 || required_len == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    needed = strlen(value) + 1;
    *required_len = needed;

    if (buffer == 0 || buffer_len < needed) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memcpy(buffer, value, needed);
    return MRTC_STATUS_OK;
}

static MRTC_STATUS mrtc_append_bundle_line(char **buffer,
                                           size_t *len,
                                           size_t *capacity,
                                           MRTC_RTP_TRANSCEIVER_HANDLE transceivers,
                                           int include_data_channel)
{
    char line[256];
    size_t used;
    MRTC_RTP_TRANSCEIVER_HANDLE current;

    (void) snprintf(line, sizeof(line), "a=group:BUNDLE");
    used = strlen(line);
    current = transceivers;
    while (current != 0) {
        int written = snprintf(line + used, sizeof(line) - used, " %s", current->mid);
        if (written < 0 || (size_t) written >= sizeof(line) - used) {
            return MRTC_STATUS_INVALID_ARG;
        }
        used += (size_t) written;
        current = current->next;
    }
    if (include_data_channel) {
        int written = snprintf(line + used, sizeof(line) - used, " data");
        if (written < 0 || (size_t) written >= sizeof(line) - used) {
            return MRTC_STATUS_INVALID_ARG;
        }
    }

    return mrtc_append_line(buffer, len, capacity, line, strlen(line));
}

static MRTC_STATUS mrtc_append_common_media_attrs(char **buffer,
                                                  size_t *len,
                                                  size_t *capacity,
                                                  MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                                  const char *local_ice_ufrag,
                                                  const char *local_ice_pwd,
                                                  const char *local_fingerprint,
                                                  const char *local_setup)
{
    MRTC_STATUS status;

    status = mrtc_append_literal_line(buffer, len, capacity, "c=IN IP4 0.0.0.0");
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_formatted_line(buffer, len, capacity, "a=mid:%s", transceiver->mid);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_formatted_line(buffer,
                                        len,
                                        capacity,
                                        "a=%s",
                                        mrtc_media_transceiver_direction_name(transceiver->direction));
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_literal_line(buffer, len, capacity, "a=rtcp-mux");
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_formatted_line(buffer, len, capacity, "a=ice-ufrag:%s", local_ice_ufrag);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_formatted_line(buffer, len, capacity, "a=ice-pwd:%s", local_ice_pwd);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_formatted_line(buffer,
                                        len,
                                        capacity,
                                        "a=fingerprint:sha-256 %s",
                                        local_fingerprint);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    return mrtc_append_formatted_line(buffer, len, capacity, "a=setup:%s", local_setup);
}

static MRTC_STATUS mrtc_append_transceiver_media(char **buffer,
                                                 size_t *len,
                                                 size_t *capacity,
                                                 MRTC_RTP_TRANSCEIVER_HANDLE transceiver,
                                                 const char *local_ice_ufrag,
                                                 const char *local_ice_pwd,
                                                 const char *local_fingerprint,
                                                 const char *local_setup)
{
    MRTC_STATUS status;

    if (transceiver->kind == MRTC_MEDIA_KIND_AUDIO) {
        status = mrtc_append_literal_line(buffer, len, capacity, "m=audio 9 UDP/TLS/RTP/SAVPF 111");
    } else {
        status = mrtc_append_literal_line(buffer, len, capacity, "m=video 9 UDP/TLS/RTP/SAVPF 96");
    }
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    status = mrtc_append_common_media_attrs(buffer,
                                            len,
                                            capacity,
                                            transceiver,
                                            local_ice_ufrag,
                                            local_ice_pwd,
                                            local_fingerprint,
                                            local_setup);
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    if (transceiver->kind == MRTC_MEDIA_KIND_AUDIO) {
        status = mrtc_append_literal_line(buffer, len, capacity, "a=rtpmap:111 opus/48000/2");
        if (status != MRTC_STATUS_OK) {
            return status;
        }
        status = mrtc_append_literal_line(buffer, len, capacity, "a=fmtp:111 minptime=10;useinbandfec=1");
    } else {
        status = mrtc_append_literal_line(buffer, len, capacity, "a=rtpmap:96 H264/90000");
        if (status != MRTC_STATUS_OK) {
            return status;
        }
        status = mrtc_append_literal_line(buffer,
                                          len,
                                          capacity,
                                          "a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f");
        if (status != MRTC_STATUS_OK) {
            return status;
        }
        status = mrtc_append_literal_line(buffer, len, capacity, "a=rtcp-fb:96 nack");
        if (status != MRTC_STATUS_OK) {
            return status;
        }
        status = mrtc_append_literal_line(buffer, len, capacity, "a=rtcp-fb:96 nack pli");
    }
    if (status != MRTC_STATUS_OK) {
        return status;
    }

    status = mrtc_append_formatted_line(buffer,
                                        len,
                                        capacity,
                                        "a=ssrc:%u cname:libmicrortc",
                                        transceiver->local_ssrc);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    return mrtc_append_formatted_line(buffer,
                                      len,
                                      capacity,
                                      "a=ssrc:%u msid:libmicrortc %s",
                                      transceiver->local_ssrc,
                                      transceiver->mid);
}

static MRTC_STATUS mrtc_append_data_channel_media(char **buffer,
                                                  size_t *len,
                                                  size_t *capacity,
                                                  const char *local_ice_ufrag,
                                                  const char *local_ice_pwd,
                                                  const char *local_fingerprint,
                                                  const char *local_setup)
{
    MRTC_STATUS status;

    status = mrtc_append_literal_line(buffer, len, capacity, "m=application 9 UDP/DTLS/SCTP webrtc-datachannel");
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_literal_line(buffer, len, capacity, "c=IN IP4 0.0.0.0");
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_literal_line(buffer, len, capacity, "a=mid:data");
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_formatted_line(buffer, len, capacity, "a=ice-ufrag:%s", local_ice_ufrag);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_formatted_line(buffer, len, capacity, "a=ice-pwd:%s", local_ice_pwd);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_formatted_line(buffer, len, capacity, "a=fingerprint:sha-256 %s", local_fingerprint);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    status = mrtc_append_formatted_line(buffer, len, capacity, "a=setup:%s", local_setup);
    if (status != MRTC_STATUS_OK) {
        return status;
    }
    return mrtc_append_literal_line(buffer, len, capacity, "a=sctp-port:5000");
}

static MRTC_STATUS mrtc_sdp_create_media_description(MRTC_RTP_TRANSCEIVER_HANDLE transceivers,
                                                     int include_data_channel,
                                                     const char *local_ice_ufrag,
                                                     const char *local_ice_pwd,
                                                     const char *local_fingerprint,
                                                     const char *local_setup,
                                                     char *buffer,
                                                     size_t buffer_len,
                                                     size_t *required_len)
{
    MRTC_STATUS status;
    char *sdp = 0;
    size_t len = 0;
    size_t capacity = 0;
    MRTC_RTP_TRANSCEIVER_HANDLE current;

    status = mrtc_append_literal_line(&sdp, &len, &capacity, "v=0");
    if (status != MRTC_STATUS_OK) {
        free(sdp);
        return status;
    }
    status = mrtc_append_literal_line(&sdp, &len, &capacity, "o=- 0 0 IN IP4 127.0.0.1");
    if (status != MRTC_STATUS_OK) {
        free(sdp);
        return status;
    }
    status = mrtc_append_literal_line(&sdp, &len, &capacity, "s=libmicrortc");
    if (status != MRTC_STATUS_OK) {
        free(sdp);
        return status;
    }
    status = mrtc_append_literal_line(&sdp, &len, &capacity, "t=0 0");
    if (status != MRTC_STATUS_OK) {
        free(sdp);
        return status;
    }
    status = mrtc_append_bundle_line(&sdp, &len, &capacity, transceivers, include_data_channel);
    if (status != MRTC_STATUS_OK) {
        free(sdp);
        return status;
    }

    current = transceivers;
    while (current != 0) {
        status = mrtc_append_transceiver_media(&sdp,
                                               &len,
                                               &capacity,
                                               current,
                                               local_ice_ufrag,
                                               local_ice_pwd,
                                               local_fingerprint,
                                               local_setup);
        if (status != MRTC_STATUS_OK) {
            free(sdp);
            return status;
        }
        current = current->next;
    }

    if (include_data_channel) {
        status = mrtc_append_data_channel_media(&sdp,
                                                &len,
                                                &capacity,
                                                local_ice_ufrag,
                                                local_ice_pwd,
                                                local_fingerprint,
                                                local_setup);
        if (status != MRTC_STATUS_OK) {
            free(sdp);
            return status;
        }
    }

    status = mrtc_write_buffer(sdp, buffer, buffer_len, required_len);
    free(sdp);
    return status;
}

MRTC_STATUS mrtc_sdp_parse(const char *sdp, MRTC_SDP **parsed_sdp)
{
    const char *cursor;
    int saw_version = 0;
    MRTC_STATUS status;
    MRTC_SDP *result;
    char *normalized = 0;
    size_t normalized_len = 0;
    size_t normalized_capacity = 0;

    if (sdp == 0 || sdp[0] == '\0' || parsed_sdp == 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    result = (MRTC_SDP *) calloc(1, sizeof(*result));
    if (result == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    cursor = sdp;
    while (*cursor != '\0') {
        const char *line_start = cursor;
        size_t line_len;

        while (*cursor != '\0' && *cursor != '\n') {
            cursor++;
        }

        line_len = (size_t) (cursor - line_start);
        if (line_len > 0 && line_start[line_len - 1] == '\r') {
            line_len--;
        }

        if (line_len < 2 || line_start[1] != '=') {
            mrtc_sdp_free(result);
            free(normalized);
            return MRTC_STATUS_PARSE_ERROR;
        }

        if (mrtc_has_prefix(line_start, line_len, "v=0")) {
            saw_version = 1;
        } else if (mrtc_has_prefix(line_start, line_len, "m=")) {
            result->media_count++;
            if (mrtc_has_prefix(line_start, line_len, "m=application")) {
                result->has_application = 1;
            }
            if (result->first_media_line == 0) {
                status = mrtc_copy_string(line_start, line_len, &result->first_media_line);
                if (status != MRTC_STATUS_OK) {
                    mrtc_sdp_free(result);
                    free(normalized);
                    return status;
                }
            }
        } else if (mrtc_has_prefix(line_start, line_len, "a=")) {
            result->attribute_count++;
            if (mrtc_has_prefix(line_start, line_len, "a=ice-ufrag:")) {
                status = mrtc_parse_attribute_value(&result->ice_ufrag, line_start, line_len, "a=ice-ufrag:");
            } else if (mrtc_has_prefix(line_start, line_len, "a=ice-pwd:")) {
                status = mrtc_parse_attribute_value(&result->ice_pwd, line_start, line_len, "a=ice-pwd:");
            } else if (mrtc_has_prefix(line_start, line_len, "a=setup:")) {
                status = mrtc_parse_attribute_value(&result->setup, line_start, line_len, "a=setup:");
                if (status == MRTC_STATUS_OK &&
                    !mrtc_line_equals(result->setup, strlen(result->setup), "actpass") &&
                    !mrtc_line_equals(result->setup, strlen(result->setup), "active") &&
                    !mrtc_line_equals(result->setup, strlen(result->setup), "passive")) {
                    status = MRTC_STATUS_PARSE_ERROR;
                }
            } else if (mrtc_has_prefix(line_start, line_len, "a=fingerprint:")) {
                const char *prefix = "a=fingerprint:sha-256 ";
                if (!mrtc_has_prefix(line_start, line_len, prefix)) {
                    status = MRTC_STATUS_PARSE_ERROR;
                } else {
                    status = mrtc_parse_attribute_value(&result->fingerprint, line_start, line_len, prefix);
                    if (status == MRTC_STATUS_OK && !mrtc_fingerprint_value_is_valid(result->fingerprint)) {
                        status = MRTC_STATUS_PARSE_ERROR;
                    }
                }
            } else if (mrtc_has_prefix(line_start, line_len, "a=candidate:")) {
                result->has_candidate = 1;
                status = MRTC_STATUS_OK;
            } else if (mrtc_has_prefix(line_start, line_len, "a=sctp-port:")) {
                result->has_sctp_port = 1;
                status = MRTC_STATUS_OK;
            } else {
                status = MRTC_STATUS_OK;
            }
            if (status != MRTC_STATUS_OK) {
                mrtc_sdp_free(result);
                free(normalized);
                return MRTC_STATUS_PARSE_ERROR;
            }
        }

        status = mrtc_append_line(&normalized, &normalized_len, &normalized_capacity, line_start, line_len);
        if (status != MRTC_STATUS_OK) {
            mrtc_sdp_free(result);
            free(normalized);
            return status;
        }

        if (*cursor == '\n') {
            cursor++;
        }
    }

    if (!saw_version || result->media_count == 0) {
        mrtc_sdp_free(result);
        free(normalized);
        return MRTC_STATUS_PARSE_ERROR;
    }

    result->normalized = normalized;
    *parsed_sdp = result;
    return MRTC_STATUS_OK;
}

MRTC_STATUS mrtc_sdp_serialize(const MRTC_SDP *parsed_sdp, char *buffer, size_t buffer_len, size_t *required_len)
{
    if (parsed_sdp == 0 || parsed_sdp->normalized == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    return mrtc_write_buffer(parsed_sdp->normalized, buffer, buffer_len, required_len);
}

MRTC_STATUS mrtc_sdp_create_answer(const char *remote_offer, char *buffer, size_t buffer_len, size_t *required_len)
{
    return mrtc_sdp_create_answer_ex(remote_offer,
                                     0,
                                     "mrtcufrag",
                                     "mrtcpassword000000000000",
                                     "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF",
                                     buffer,
                                     buffer_len,
                                     required_len);
}

MRTC_STATUS mrtc_sdp_create_answer_ex(const char *remote_offer,
                                      int include_data_channel,
                                      const char *local_ice_ufrag,
                                      const char *local_ice_pwd,
                                      const char *local_fingerprint,
                                      char *buffer,
                                      size_t buffer_len,
                                      size_t *required_len)
{
    return mrtc_sdp_create_answer_with_media(remote_offer,
                                             include_data_channel,
                                             0,
                                             local_ice_ufrag,
                                             local_ice_pwd,
                                             local_fingerprint,
                                             buffer,
                                             buffer_len,
                                             required_len);
}

MRTC_STATUS mrtc_sdp_create_answer_with_media(const char *remote_offer,
                                              int include_data_channel,
                                              MRTC_RTP_TRANSCEIVER_HANDLE transceivers,
                                              const char *local_ice_ufrag,
                                              const char *local_ice_pwd,
                                              const char *local_fingerprint,
                                              char *buffer,
                                              size_t buffer_len,
                                              size_t *required_len)
{
    MRTC_STATUS status;
    MRTC_SDP *offer = 0;
    char answer[2048];
    const char *local_setup;

    if (local_ice_ufrag == 0 || local_ice_pwd == 0 || local_fingerprint == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    status = mrtc_sdp_parse(remote_offer, &offer);
    if (status != MRTC_STATUS_OK) {
        return MRTC_STATUS_PARSE_ERROR;
    }

    local_setup = mrtc_local_setup_from_remote(offer->setup);
    if (transceivers != 0) {
        mrtc_sdp_free(offer);
        return mrtc_sdp_create_media_description(transceivers,
                                                 include_data_channel,
                                                 local_ice_ufrag,
                                                 local_ice_pwd,
                                                 local_fingerprint,
                                                 local_setup,
                                                 buffer,
                                                 buffer_len,
                                                 required_len);
    }

    (void) snprintf(answer,
                   sizeof(answer),
                   "v=0\r\n"
                   "o=- 0 0 IN IP4 127.0.0.1\r\n"
                   "s=libmicrortc\r\n"
                   "t=0 0\r\n"
                   "a=group:BUNDLE 0%s\r\n"
                   "%s\r\n"
                   "c=IN IP4 0.0.0.0\r\n"
                   "a=mid:0\r\n"
                   "a=recvonly\r\n",
                   include_data_channel ? " data" : "",
                   offer->first_media_line);

    if (strlen(answer) + 512 < sizeof(answer)) {
        size_t answer_len = strlen(answer);
        (void) snprintf(answer + answer_len,
                        sizeof(answer) - answer_len,
                        "a=ice-ufrag:%s\r\n"
                        "a=ice-pwd:%s\r\n"
                        "a=fingerprint:sha-256 %s\r\n"
                        "a=setup:%s\r\n",
                        local_ice_ufrag,
                        local_ice_pwd,
                        local_fingerprint,
                        local_setup);
    }

    if (include_data_channel && strlen(answer) + 700 < sizeof(answer)) {
        size_t answer_len = strlen(answer);
        (void) snprintf(answer + answer_len,
                        sizeof(answer) - answer_len,
                        "m=application 9 UDP/DTLS/SCTP webrtc-datachannel\r\n"
                        "c=IN IP4 0.0.0.0\r\n"
                        "a=mid:data\r\n"
                        "a=ice-ufrag:%s\r\n"
                        "a=ice-pwd:%s\r\n"
                        "a=fingerprint:sha-256 %s\r\n"
                        "a=setup:%s\r\n"
                        "a=sctp-port:5000\r\n",
                        local_ice_ufrag,
                        local_ice_pwd,
                        local_fingerprint,
                        local_setup);
    }

    mrtc_sdp_free(offer);
    return mrtc_write_buffer(answer, buffer, buffer_len, required_len);
}

MRTC_STATUS mrtc_sdp_create_offer_with_media(int include_data_channel,
                                             MRTC_RTP_TRANSCEIVER_HANDLE transceivers,
                                             const char *local_ice_ufrag,
                                             const char *local_ice_pwd,
                                             const char *local_fingerprint,
                                             char *buffer,
                                             size_t buffer_len,
                                             size_t *required_len)
{
    if (transceivers == 0 || local_ice_ufrag == 0 || local_ice_pwd == 0 || local_fingerprint == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    return mrtc_sdp_create_media_description(transceivers,
                                             include_data_channel,
                                             local_ice_ufrag,
                                             local_ice_pwd,
                                             local_fingerprint,
                                             "actpass",
                                             buffer,
                                             buffer_len,
                                             required_len);
}

const char *mrtc_sdp_get_setup(const MRTC_SDP *parsed_sdp)
{
    return parsed_sdp == 0 ? 0 : parsed_sdp->setup;
}

const char *mrtc_sdp_get_fingerprint(const MRTC_SDP *parsed_sdp)
{
    return parsed_sdp == 0 ? 0 : parsed_sdp->fingerprint;
}

int mrtc_sdp_has_application(const MRTC_SDP *parsed_sdp)
{
    return parsed_sdp != 0 && parsed_sdp->has_application;
}

void mrtc_sdp_free(MRTC_SDP *parsed_sdp)
{
    if (parsed_sdp == 0) {
        return;
    }

    free(parsed_sdp->normalized);
    free(parsed_sdp->first_media_line);
    free(parsed_sdp->ice_ufrag);
    free(parsed_sdp->ice_pwd);
    free(parsed_sdp->fingerprint);
    free(parsed_sdp->setup);
    free(parsed_sdp);
}
