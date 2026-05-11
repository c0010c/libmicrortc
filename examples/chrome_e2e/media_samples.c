#include "media_samples.h"

#include <string.h>

#define RTC_E2E_SAMPLE_OK 0
#define RTC_E2E_SAMPLE_EOF 1
#define RTC_E2E_SAMPLE_ERROR -1
#define RTC_E2E_SAMPLE_CAPACITY -2

static int open_reader(const char *path, rtc_e2e_sample_reader_t *reader,
                       rtc_e2e_sample_reader_kind_t kind)
{
    if (path == 0 || reader == 0) {
        return RTC_E2E_SAMPLE_ERROR;
    }
    memset(reader, 0, sizeof(*reader));
    reader->file = fopen(path, "rb");
    if (reader->file == 0) {
        return RTC_E2E_SAMPLE_ERROR;
    }
    reader->kind = kind;
    return RTC_E2E_SAMPLE_OK;
}

int rtc_e2e_sample_open_h264(const char *path,
                             rtc_e2e_sample_reader_t *reader)
{
    return open_reader(path, reader, RTC_E2E_SAMPLE_READER_H264);
}

int rtc_e2e_sample_open_ogg_opus(const char *path,
                                 rtc_e2e_sample_reader_t *reader)
{
    return open_reader(path, reader, RTC_E2E_SAMPLE_READER_OGG_OPUS);
}

void rtc_e2e_sample_close(rtc_e2e_sample_reader_t *reader)
{
    if (reader != 0 && reader->file != 0) {
        fclose(reader->file);
        reader->file = 0;
    }
    if (reader != 0) {
        reader->kind = RTC_E2E_SAMPLE_READER_NONE;
    }
}

static int h264_start_code_len(const uint8_t *data, size_t len,
                               size_t *out_len)
{
    if (len >= 4u && data[len - 4u] == 0x00u &&
        data[len - 3u] == 0x00u && data[len - 2u] == 0x00u &&
        data[len - 1u] == 0x01u) {
        *out_len = 4u;
        return 1;
    }
    if (len >= 3u && data[len - 3u] == 0x00u &&
        data[len - 2u] == 0x00u && data[len - 1u] == 0x01u) {
        *out_len = 3u;
        return 1;
    }
    return 0;
}

static int h264_scan_to_start_code(FILE *file, uint8_t *out,
                                   size_t *out_len)
{
    int c;
    uint8_t window[4];
    size_t len = 0;
    size_t start_len = 0;

    while ((c = fgetc(file)) != EOF) {
        if (len < sizeof(window)) {
            window[len++] = (uint8_t)c;
        } else {
            memmove(window, window + 1u, sizeof(window) - 1u);
            window[sizeof(window) - 1u] = (uint8_t)c;
        }
        if (h264_start_code_len(window, len, &start_len)) {
            memcpy(out, window + len - start_len, start_len);
            *out_len = start_len;
            return RTC_E2E_SAMPLE_OK;
        }
    }
    return RTC_E2E_SAMPLE_EOF;
}

static int h264_nalu_type(const uint8_t *data, size_t len)
{
    if (len >= 5u && data[0] == 0x00u && data[1] == 0x00u &&
        data[2] == 0x00u && data[3] == 0x01u) {
        return data[4] & 0x1f;
    }
    if (len >= 4u && data[0] == 0x00u && data[1] == 0x00u &&
        data[2] == 0x01u) {
        return data[3] & 0x1f;
    }
    return 0;
}

static int h264_is_vcl(int nalu_type)
{
    return nalu_type >= 1 && nalu_type <= 5;
}

static int h264_read_next_nalu(rtc_e2e_sample_reader_t *reader,
                               uint8_t *buffer, size_t buffer_len,
                               size_t *out_len)
{
    size_t data_len = 0;
    size_t start_len = 0;
    int c;

    if (reader == 0 || buffer == 0 || out_len == 0 || buffer_len == 0u ||
        reader->file == 0 || reader->kind != RTC_E2E_SAMPLE_READER_H264) {
        return RTC_E2E_SAMPLE_ERROR;
    }
    *out_len = 0;
    if (reader->pending_start_code_len == 0u) {
        if (h264_scan_to_start_code(reader->file, reader->pending_start_code,
                                    &reader->pending_start_code_len) !=
            RTC_E2E_SAMPLE_OK) {
            return RTC_E2E_SAMPLE_EOF;
        }
    }
    if (reader->pending_start_code_len > buffer_len) {
        return RTC_E2E_SAMPLE_CAPACITY;
    }
    memcpy(buffer, reader->pending_start_code, reader->pending_start_code_len);
    data_len = reader->pending_start_code_len;
    reader->pending_start_code_len = 0;

    while ((c = fgetc(reader->file)) != EOF) {
        if (data_len >= buffer_len) {
            return RTC_E2E_SAMPLE_CAPACITY;
        }
        buffer[data_len++] = (uint8_t)c;
        if (h264_start_code_len(buffer, data_len, &start_len)) {
            data_len -= start_len;
            memcpy(reader->pending_start_code, buffer + data_len, start_len);
            reader->pending_start_code_len = start_len;
            break;
        }
    }
    if (data_len == 0u) {
        return RTC_E2E_SAMPLE_EOF;
    }
    *out_len = data_len;
    return RTC_E2E_SAMPLE_OK;
}

int rtc_e2e_sample_next_h264(rtc_e2e_sample_reader_t *reader,
                             rtc_e2e_sample_frame_t *frame,
                             uint8_t *buffer, size_t buffer_len)
{
    size_t data_len = 0;
    int saw_vcl = 0;

    if (reader == 0 || frame == 0 || buffer == 0 || buffer_len == 0u ||
        reader->file == 0 || reader->kind != RTC_E2E_SAMPLE_READER_H264) {
        return RTC_E2E_SAMPLE_ERROR;
    }

    while (!saw_vcl) {
        size_t nalu_len = 0;
        int nalu_type;
        int rc;

        rc = h264_read_next_nalu(reader, buffer + data_len,
                                 buffer_len - data_len, &nalu_len);
        if (rc != RTC_E2E_SAMPLE_OK) {
            return data_len > 0u ? RTC_E2E_SAMPLE_OK : rc;
        }
        nalu_type = h264_nalu_type(buffer + data_len, nalu_len);
        data_len += nalu_len;
        saw_vcl = h264_is_vcl(nalu_type);
    }

    memset(frame, 0, sizeof(*frame));
    frame->kind = RTC_MEDIA_KIND_VIDEO_H264;
    frame->data = buffer;
    frame->data_len = data_len;
    frame->duration_us = 40000u;
    return RTC_E2E_SAMPLE_OK;
}

static uint64_t read_le64(const uint8_t *p)
{
    uint64_t value = 0;
    int i;

    for (i = 7; i >= 0; --i) {
        value = (value << 8) | p[i];
    }
    return value;
}

static int ogg_read_page_header(rtc_e2e_sample_reader_t *reader,
                                uint64_t *out_granule)
{
    uint8_t header[27];

    if (fread(header, 1u, sizeof(header), reader->file) != sizeof(header)) {
        return RTC_E2E_SAMPLE_EOF;
    }
    if (memcmp(header, "OggS", 4u) != 0) {
        return RTC_E2E_SAMPLE_ERROR;
    }
    reader->ogg_segment_count = header[26];
    reader->ogg_segment_index = 0;
    if (reader->ogg_segment_count > sizeof(reader->ogg_lacing)) {
        return RTC_E2E_SAMPLE_ERROR;
    }
    if (fread(reader->ogg_lacing, 1u, reader->ogg_segment_count,
              reader->file) != reader->ogg_segment_count) {
        return RTC_E2E_SAMPLE_ERROR;
    }
    if (out_granule != 0) {
        *out_granule = read_le64(header + 6u);
    }
    return RTC_E2E_SAMPLE_OK;
}

static int ogg_read_packet(rtc_e2e_sample_reader_t *reader, uint8_t *buffer,
                           size_t buffer_len, size_t *out_len,
                           uint64_t *out_granule)
{
    size_t data_len = 0;

    while (1) {
        uint8_t segment_len;
        int rc;

        if (reader->ogg_segment_index >= reader->ogg_segment_count) {
            rc = ogg_read_page_header(reader, out_granule);
            if (rc != RTC_E2E_SAMPLE_OK) {
                return rc;
            }
        }

        segment_len = reader->ogg_lacing[reader->ogg_segment_index++];
        if (data_len + segment_len > buffer_len) {
            return RTC_E2E_SAMPLE_CAPACITY;
        }
        if (segment_len > 0u &&
            fread(buffer + data_len, 1u, segment_len, reader->file) !=
                segment_len) {
            return RTC_E2E_SAMPLE_ERROR;
        }
        data_len += segment_len;
        if (segment_len < 255u) {
            *out_len = data_len;
            return RTC_E2E_SAMPLE_OK;
        }
    }
}

int rtc_e2e_sample_next_opus(rtc_e2e_sample_reader_t *reader,
                             rtc_e2e_sample_frame_t *frame,
                             uint8_t *buffer, size_t buffer_len)
{
    size_t data_len = 0;
    uint64_t granule = 0;
    int rc;

    if (reader == 0 || frame == 0 || buffer == 0 || buffer_len == 0u ||
        reader->file == 0 ||
        reader->kind != RTC_E2E_SAMPLE_READER_OGG_OPUS) {
        return RTC_E2E_SAMPLE_ERROR;
    }

    do {
        rc = ogg_read_packet(reader, buffer, buffer_len, &data_len, &granule);
        if (rc != RTC_E2E_SAMPLE_OK) {
            return rc;
        }
    } while ((data_len >= 8u && memcmp(buffer, "OpusHead", 8u) == 0) ||
             (data_len >= 8u && memcmp(buffer, "OpusTags", 8u) == 0));

    (void)granule;
    memset(frame, 0, sizeof(*frame));
    frame->kind = RTC_MEDIA_KIND_AUDIO_OPUS;
    frame->data = buffer;
    frame->data_len = data_len;
    frame->duration_us = 20000u;
    return RTC_E2E_SAMPLE_OK;
}
