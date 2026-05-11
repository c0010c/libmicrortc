#ifndef RTC_CHROME_E2E_MEDIA_SAMPLES_H
#define RTC_CHROME_E2E_MEDIA_SAMPLES_H

#include "rtc/media.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum rtc_e2e_sample_reader_kind_t {
    RTC_E2E_SAMPLE_READER_NONE = 0,
    RTC_E2E_SAMPLE_READER_H264,
    RTC_E2E_SAMPLE_READER_OGG_OPUS
} rtc_e2e_sample_reader_kind_t;

typedef struct rtc_e2e_sample_reader_t {
    FILE *file;
    rtc_e2e_sample_reader_kind_t kind;
    uint8_t pending_start_code[4];
    size_t pending_start_code_len;
    uint8_t ogg_lacing[255];
    size_t ogg_segment_count;
    size_t ogg_segment_index;
} rtc_e2e_sample_reader_t;

typedef struct rtc_e2e_sample_frame_t {
    rtc_media_kind_t kind;
    const uint8_t *data;
    size_t data_len;
    uint32_t duration_us;
} rtc_e2e_sample_frame_t;

int rtc_e2e_sample_open_h264(const char *path,
                             rtc_e2e_sample_reader_t *reader);
int rtc_e2e_sample_next_h264(rtc_e2e_sample_reader_t *reader,
                             rtc_e2e_sample_frame_t *frame,
                             uint8_t *buffer, size_t buffer_len);
int rtc_e2e_sample_open_ogg_opus(const char *path,
                                 rtc_e2e_sample_reader_t *reader);
int rtc_e2e_sample_next_opus(rtc_e2e_sample_reader_t *reader,
                             rtc_e2e_sample_frame_t *frame,
                             uint8_t *buffer, size_t buffer_len);
void rtc_e2e_sample_close(rtc_e2e_sample_reader_t *reader);

#ifdef __cplusplus
}
#endif

#endif
