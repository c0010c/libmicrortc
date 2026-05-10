#include "sdp/sdp.h"

#include <string.h>

typedef enum rtc_sdp_section_t {
    RTC_SDP_SECTION_SESSION = 0,
    RTC_SDP_SECTION_AUDIO,
    RTC_SDP_SECTION_VIDEO
} rtc_sdp_section_t;

static int rtc_sdp_line_eq(const char *line, size_t len, const char *text)
{
    size_t text_len = strlen(text);
    return len == text_len && memcmp(line, text, len) == 0;
}

static int rtc_sdp_line_starts(const char *line, size_t len, const char *prefix)
{
    size_t prefix_len = strlen(prefix);
    return len >= prefix_len && memcmp(line, prefix, prefix_len) == 0;
}

static int rtc_sdp_line_contains(const char *line, size_t len,
                                 const char *needle)
{
    size_t needle_len;
    size_t i;

    needle_len = strlen(needle);
    if (needle_len == 0 || len < needle_len) {
        return 0;
    }
    for (i = 0; i <= len - needle_len; ++i) {
        if (memcmp(line + i, needle, needle_len) == 0) {
            return 1;
        }
    }
    return 0;
}

static void rtc_sdp_copy(char *dst, size_t dst_size, const char *src,
                         size_t src_len)
{
    size_t copy_len;

    if (dst_size == 0) {
        return;
    }
    copy_len = src_len < dst_size - 1u ? src_len : dst_size - 1u;
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

static rtc_sdp_media_t *rtc_sdp_media_for(rtc_sdp_description_t *description,
                                          rtc_sdp_section_t section)
{
    if (section == RTC_SDP_SECTION_AUDIO) {
        return &description->audio;
    }
    if (section == RTC_SDP_SECTION_VIDEO) {
        return &description->video;
    }
    return 0;
}

static rtc_status_t rtc_sdp_note_direction(rtc_sdp_media_t *media,
                                           const char *line, size_t len)
{
    if (media == 0) {
        return RTC_STATUS_OK;
    }
    if (rtc_sdp_line_eq(line, len, "a=sendrecv")) {
        media->direction = RTC_SDP_DIRECTION_SENDRECV;
        return RTC_STATUS_OK;
    }
    if (rtc_sdp_line_eq(line, len, "a=recvonly")) {
        media->direction = RTC_SDP_DIRECTION_RECVONLY;
        return RTC_STATUS_OK;
    }
    if (rtc_sdp_line_eq(line, len, "a=sendonly") ||
        rtc_sdp_line_eq(line, len, "a=inactive")) {
        return RTC_STATUS_UNSUPPORTED;
    }
    return RTC_STATUS_OK;
}

static rtc_status_t rtc_sdp_validate(const rtc_sdp_description_t *description,
                                     int audio_codec, int video_codec,
                                     int video_fmtp)
{
    if (!description->has_bundle ||
        strcmp(description->bundle_mids, "0 1") != 0 ||
        strcmp(description->audio.mid, "0") != 0 ||
        strcmp(description->video.mid, "1") != 0 ||
        !description->audio.has_rtcp_mux || !description->video.has_rtcp_mux ||
        description->ice_ufrag[0] == '\0' || description->ice_pwd[0] == '\0' ||
        description->dtls_fingerprint[0] == '\0' ||
        description->dtls_setup[0] == '\0') {
        return RTC_STATUS_PROTOCOL_ERROR;
    }
    if (!audio_codec || !video_codec || !video_fmtp) {
        return RTC_STATUS_UNSUPPORTED;
    }
    return RTC_STATUS_OK;
}

rtc_status_t rtc_sdp_parse(const char *sdp, size_t sdp_len,
                           rtc_sdp_description_t *out_description)
{
    rtc_sdp_section_t section;
    size_t pos;
    int audio_codec;
    int video_codec;
    int video_fmtp;

    if (sdp == 0 || sdp_len == 0 || out_description == 0) {
        return RTC_STATUS_INVALID_ARGUMENT;
    }

    memset(out_description, 0, sizeof(*out_description));
    out_description->type = RTC_SDP_TYPE_ANSWER;
    out_description->audio.kind = RTC_SDP_MEDIA_AUDIO;
    out_description->video.kind = RTC_SDP_MEDIA_VIDEO;
    out_description->audio.direction = RTC_SDP_DIRECTION_SENDRECV;
    out_description->video.direction = RTC_SDP_DIRECTION_SENDRECV;
    section = RTC_SDP_SECTION_SESSION;
    pos = 0;
    audio_codec = 0;
    video_codec = 0;
    video_fmtp = 0;

    while (pos < sdp_len) {
        const char *line;
        size_t line_len;
        size_t line_start;
        rtc_sdp_media_t *media;
        rtc_status_t status;

        line_start = pos;
        while (pos < sdp_len && sdp[pos] != '\n') {
            pos++;
        }
        line = sdp + line_start;
        line_len = pos - line_start;
        if (line_len != 0 && line[line_len - 1u] == '\r') {
            line_len--;
        }
        if (pos < sdp_len && sdp[pos] == '\n') {
            pos++;
        }

        if (rtc_sdp_line_starts(line, line_len, "m=audio ")) {
            section = RTC_SDP_SECTION_AUDIO;
            continue;
        }
        if (rtc_sdp_line_starts(line, line_len, "m=video ")) {
            section = RTC_SDP_SECTION_VIDEO;
            continue;
        }
        if (rtc_sdp_line_starts(line, line_len, "a=group:BUNDLE ")) {
            out_description->has_bundle = 1;
            rtc_sdp_copy(out_description->bundle_mids,
                         sizeof(out_description->bundle_mids), line + 15,
                         line_len - 15u);
            continue;
        }

        media = rtc_sdp_media_for(out_description, section);
        status = rtc_sdp_note_direction(media, line, line_len);
        if (status != RTC_STATUS_OK) {
            return status;
        }
        if (media != 0 && rtc_sdp_line_eq(line, line_len, "a=rtcp-mux")) {
            media->has_rtcp_mux = 1;
        } else if (media != 0 &&
                   rtc_sdp_line_starts(line, line_len, "a=mid:")) {
            rtc_sdp_copy(media->mid, sizeof(media->mid), line + 6,
                         line_len - 6u);
        } else if (rtc_sdp_line_starts(line, line_len, "a=ice-ufrag:")) {
            rtc_sdp_copy(out_description->ice_ufrag,
                         sizeof(out_description->ice_ufrag), line + 12,
                         line_len - 12u);
        } else if (rtc_sdp_line_starts(line, line_len, "a=ice-pwd:")) {
            rtc_sdp_copy(out_description->ice_pwd,
                         sizeof(out_description->ice_pwd), line + 10,
                         line_len - 10u);
        } else if (rtc_sdp_line_starts(line, line_len, "a=fingerprint:")) {
            rtc_sdp_copy(out_description->dtls_fingerprint,
                         sizeof(out_description->dtls_fingerprint), line + 14,
                         line_len - 14u);
        } else if (rtc_sdp_line_starts(line, line_len, "a=setup:")) {
            rtc_sdp_copy(out_description->dtls_setup,
                         sizeof(out_description->dtls_setup), line + 8,
                         line_len - 8u);
            if (rtc_sdp_line_eq(line, line_len, "a=setup:actpass")) {
                out_description->type = RTC_SDP_TYPE_OFFER;
            }
        } else if (section == RTC_SDP_SECTION_AUDIO &&
                   rtc_sdp_line_eq(line, line_len,
                                   "a=rtpmap:111 opus/48000/2")) {
            out_description->audio.payload_type = 111u;
            audio_codec = 1;
        } else if (section == RTC_SDP_SECTION_VIDEO &&
                   rtc_sdp_line_eq(line, line_len,
                                   "a=rtpmap:103 H264/90000")) {
            out_description->video.payload_type = 103u;
            video_codec = 1;
        } else if (section == RTC_SDP_SECTION_VIDEO &&
                   rtc_sdp_line_starts(line, line_len, "a=fmtp:103 ") &&
                   rtc_sdp_line_contains(line, line_len,
                                         "packetization-mode=1") &&
                   rtc_sdp_line_contains(line, line_len,
                                         "profile-level-id=42001f")) {
            rtc_sdp_copy(out_description->video.profile_level_id,
                         sizeof(out_description->video.profile_level_id),
                         "42001f", 6);
            video_fmtp = 1;
        }
    }

    return rtc_sdp_validate(out_description, audio_codec, video_codec,
                            video_fmtp);
}
