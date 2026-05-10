#ifndef RTC_MEDIA_H
#define RTC_MEDIA_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rtc_media_kind_t {
    RTC_MEDIA_KIND_AUDIO_OPUS = 0,
    RTC_MEDIA_KIND_VIDEO_H264
} rtc_media_kind_t;

typedef struct rtc_media_frame_t {
    rtc_media_kind_t kind;
    const uint8_t *data;
    size_t data_len;
    uint32_t timestamp;
    uint64_t capture_time_us;
    uint32_t flags;
} rtc_media_frame_t;

typedef enum rtc_media_feedback_type_t {
    RTC_MEDIA_FEEDBACK_PLI = 0,
    RTC_MEDIA_FEEDBACK_NACK
} rtc_media_feedback_type_t;

typedef struct rtc_media_feedback_t {
    rtc_media_feedback_type_t type;
    rtc_media_kind_t kind;
    uint32_t ssrc;
    uint16_t pid;
    uint16_t blp;
    uint32_t lost_sequence_numbers[17];
    size_t lost_sequence_number_count;
    int retransmit_performed;
} rtc_media_feedback_t;

#ifdef __cplusplus
}
#endif

#endif
