#ifndef RTC_SDP_SDP_H
#define RTC_SDP_SDP_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/config.h"
#include "rtc/status.h"

typedef enum rtc_sdp_type_t {
    RTC_SDP_TYPE_OFFER = 0,
    RTC_SDP_TYPE_ANSWER
} rtc_sdp_type_t;

typedef enum rtc_sdp_direction_t {
    RTC_SDP_DIRECTION_SENDRECV = 0,
    RTC_SDP_DIRECTION_RECVONLY
} rtc_sdp_direction_t;

typedef enum rtc_sdp_media_kind_t {
    RTC_SDP_MEDIA_AUDIO = 0,
    RTC_SDP_MEDIA_VIDEO
} rtc_sdp_media_kind_t;

typedef struct rtc_sdp_media_t {
    rtc_sdp_media_kind_t kind;
    char mid[8];
    rtc_sdp_direction_t direction;
    int has_rtcp_mux;
    uint8_t payload_type;
    char profile_level_id[16];
} rtc_sdp_media_t;

typedef struct rtc_sdp_description_t {
    rtc_sdp_type_t type;
    int has_bundle;
    char bundle_mids[16];
    char ice_ufrag[64];
    char ice_pwd[128];
    char dtls_fingerprint[128];
    char dtls_setup[16];
    rtc_sdp_media_t audio;
    rtc_sdp_media_t video;
} rtc_sdp_description_t;

rtc_status_t rtc_sdp_write_offer(const rtc_sdp_parameters_t *params,
                                 rtc_sdp_direction_t audio_direction,
                                 rtc_sdp_direction_t video_direction,
                                 char *out_sdp, size_t *inout_sdp_len);
rtc_status_t rtc_sdp_write_answer(const rtc_sdp_parameters_t *params,
                                  rtc_sdp_direction_t audio_direction,
                                  rtc_sdp_direction_t video_direction,
                                  char *out_sdp, size_t *inout_sdp_len);
rtc_status_t rtc_sdp_parse(const char *sdp, size_t sdp_len,
                           rtc_sdp_description_t *out_description);

#endif
