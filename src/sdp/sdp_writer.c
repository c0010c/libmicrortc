#include "sdp/sdp.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct rtc_sdp_writer_t {
    char *data;
    size_t capacity;
    size_t required;
} rtc_sdp_writer_t;

static int rtc_sdp_param_valid(const char *value, size_t len)
{
    return value != 0 && len != 0;
}

static const char *rtc_sdp_direction_name(rtc_sdp_direction_t direction)
{
    return direction == RTC_SDP_DIRECTION_RECVONLY ? "recvonly" : "sendrecv";
}

static void rtc_sdp_append(rtc_sdp_writer_t *writer, const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int written;
    size_t remaining;

    va_start(args, fmt);
    va_copy(copy, args);
    written = vsnprintf(0, 0, fmt, args);
    va_end(args);
    if (written < 0) {
        va_end(copy);
        return;
    }

    remaining = writer->required < writer->capacity
                    ? writer->capacity - writer->required
                    : 0;
    if (writer->data != 0 && remaining != 0) {
        (void)vsnprintf(writer->data + writer->required, remaining, fmt, copy);
    }
    va_end(copy);
    writer->required += (size_t)written;
}

static rtc_status_t rtc_sdp_write_common(
    const rtc_sdp_parameters_t *params, const char *setup,
    rtc_sdp_direction_t audio_direction, rtc_sdp_direction_t video_direction,
    char *out_sdp, size_t *inout_sdp_len)
{
    rtc_sdp_writer_t writer;
    size_t capacity;

    if (params == 0 || inout_sdp_len == 0 ||
        !rtc_sdp_param_valid(params->ice_ufrag, params->ice_ufrag_len) ||
        !rtc_sdp_param_valid(params->ice_pwd, params->ice_pwd_len) ||
        !rtc_sdp_param_valid(params->dtls_fingerprint,
                             params->dtls_fingerprint_len) ||
        !rtc_sdp_param_valid(setup, strlen(setup))) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    capacity = out_sdp != 0 ? *inout_sdp_len : 0;
    writer.data = out_sdp;
    writer.capacity = capacity;
    writer.required = 0;

    rtc_sdp_append(&writer, "v=0\r\n");
    rtc_sdp_append(&writer, "o=- %llu %llu IN IP4 127.0.0.1\r\n",
                   (unsigned long long)params->session_id,
                   (unsigned long long)params->session_version);
    rtc_sdp_append(&writer, "s=-\r\n");
    rtc_sdp_append(&writer, "t=0 0\r\n");
    rtc_sdp_append(&writer, "a=group:BUNDLE 0 1\r\n");
    rtc_sdp_append(&writer, "a=msid-semantic: WMS *\r\n");
    rtc_sdp_append(&writer, "m=audio 9 UDP/TLS/RTP/SAVPF 111\r\n");
    rtc_sdp_append(&writer, "c=IN IP4 0.0.0.0\r\n");
    rtc_sdp_append(&writer, "a=rtcp:9 IN IP4 0.0.0.0\r\n");
    rtc_sdp_append(&writer, "a=ice-ufrag:%.*s\r\n",
                   (int)params->ice_ufrag_len, params->ice_ufrag);
    rtc_sdp_append(&writer, "a=ice-pwd:%.*s\r\n", (int)params->ice_pwd_len,
                   params->ice_pwd);
    rtc_sdp_append(&writer, "a=ice-options:trickle\r\n");
    rtc_sdp_append(&writer, "a=fingerprint:%.*s\r\n",
                   (int)params->dtls_fingerprint_len,
                   params->dtls_fingerprint);
    rtc_sdp_append(&writer, "a=setup:%s\r\n", setup);
    rtc_sdp_append(&writer, "a=mid:0\r\n");
    rtc_sdp_append(&writer, "a=%s\r\n",
                   rtc_sdp_direction_name(audio_direction));
    rtc_sdp_append(&writer, "a=rtcp-mux\r\n");
    rtc_sdp_append(&writer, "a=rtpmap:111 opus/48000/2\r\n");
    rtc_sdp_append(&writer, "a=fmtp:111 minptime=10;useinbandfec=1\r\n");
    rtc_sdp_append(&writer, "m=video 9 UDP/TLS/RTP/SAVPF 103\r\n");
    rtc_sdp_append(&writer, "c=IN IP4 0.0.0.0\r\n");
    rtc_sdp_append(&writer, "a=rtcp:9 IN IP4 0.0.0.0\r\n");
    rtc_sdp_append(&writer, "a=ice-ufrag:%.*s\r\n",
                   (int)params->ice_ufrag_len, params->ice_ufrag);
    rtc_sdp_append(&writer, "a=ice-pwd:%.*s\r\n", (int)params->ice_pwd_len,
                   params->ice_pwd);
    rtc_sdp_append(&writer, "a=ice-options:trickle\r\n");
    rtc_sdp_append(&writer, "a=fingerprint:%.*s\r\n",
                   (int)params->dtls_fingerprint_len,
                   params->dtls_fingerprint);
    rtc_sdp_append(&writer, "a=setup:%s\r\n", setup);
    rtc_sdp_append(&writer, "a=mid:1\r\n");
    rtc_sdp_append(&writer, "a=%s\r\n",
                   rtc_sdp_direction_name(video_direction));
    rtc_sdp_append(&writer, "a=rtcp-mux\r\n");
    rtc_sdp_append(&writer, "a=rtpmap:103 H264/90000\r\n");
    rtc_sdp_append(&writer, "a=rtcp-fb:103 nack pli\r\n");
    rtc_sdp_append(&writer,
                   "a=fmtp:103 level-asymmetry-allowed=1;"
                   "packetization-mode=1;profile-level-id=42001f\r\n");

    *inout_sdp_len = writer.required;
    if (out_sdp == 0 || capacity <= writer.required) {
        return RTC_STATUS_CAPACITY_SDP_BUFFER;
    }
    out_sdp[writer.required] = '\0';
    return RTC_STATUS_OK;
}

rtc_status_t rtc_sdp_write_offer(const rtc_sdp_parameters_t *params,
                                 rtc_sdp_direction_t audio_direction,
                                 rtc_sdp_direction_t video_direction,
                                 char *out_sdp, size_t *inout_sdp_len)
{
    const char *setup;

    setup = params != 0 && params->dtls_setup != 0 &&
                    params->dtls_setup_len != 0
                ? params->dtls_setup
                : "actpass";
    return rtc_sdp_write_common(params, setup, audio_direction, video_direction,
                                out_sdp, inout_sdp_len);
}

rtc_status_t rtc_sdp_write_answer(const rtc_sdp_parameters_t *params,
                                  rtc_sdp_direction_t audio_direction,
                                  rtc_sdp_direction_t video_direction,
                                  char *out_sdp, size_t *inout_sdp_len)
{
    (void)params;
    return rtc_sdp_write_common(params, "active", audio_direction,
                                video_direction, out_sdp, inout_sdp_len);
}
