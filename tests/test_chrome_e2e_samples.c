#include "test_runner.h"

#include "../examples/chrome_e2e/media_samples.h"

static int count_h264_type(const unsigned char *data, size_t data_len,
                           int target_type)
{
    size_t i;
    int count = 0;

    for (i = 0; i + 4u < data_len; ++i) {
        size_t start_len = 0;
        if (data[i] == 0x00u && data[i + 1u] == 0x00u &&
            data[i + 2u] == 0x01u) {
            start_len = 3u;
        } else if (data[i] == 0x00u && data[i + 1u] == 0x00u &&
                   data[i + 2u] == 0x00u && data[i + 3u] == 0x01u) {
            start_len = 4u;
        }
        if (start_len != 0u && i + start_len < data_len &&
            (int)(data[i + start_len] & 0x1fu) == target_type) {
            count++;
        }
    }
    return count;
}

int rtc_test_chrome_e2e_samples(void)
{
    rtc_e2e_sample_reader_t reader;
    rtc_e2e_sample_frame_t frame;
    unsigned char buffer[65536];
    size_t opus_packets = 0;
    size_t h264_units = 0;

    RTC_TEST_EQ_INT(0,
                    rtc_e2e_sample_open_ogg_opus("sample1.opus", &reader));
    while (rtc_e2e_sample_next_opus(&reader, &frame, buffer,
                                    sizeof(buffer)) == 0) {
        RTC_TEST_EQ_INT(RTC_MEDIA_KIND_AUDIO_OPUS, frame.kind);
        RTC_TEST_ASSERT(frame.data == buffer);
        RTC_TEST_ASSERT(frame.data_len > 0u);
        RTC_TEST_ASSERT(frame.duration_us > 0u);
        if (frame.duration_us != 20000u) {
            RTC_TEST_ASSERT(frame.duration_us > 0u);
        }
        opus_packets++;
        if (opus_packets >= 4u) {
            break;
        }
    }
    rtc_e2e_sample_close(&reader);
    RTC_TEST_ASSERT(opus_packets > 0u);

    RTC_TEST_EQ_INT(0,
                    rtc_e2e_sample_open_h264("chrome-25fps-42001f.h264",
                                             &reader));
    while (rtc_e2e_sample_next_h264(&reader, &frame, buffer,
                                    sizeof(buffer)) == 0) {
        RTC_TEST_EQ_INT(RTC_MEDIA_KIND_VIDEO_H264, frame.kind);
        RTC_TEST_ASSERT(frame.data == buffer);
        RTC_TEST_ASSERT(frame.data_len > 0u);
        RTC_TEST_EQ_INT(40000, frame.duration_us);
        if (h264_units == 0u) {
            RTC_TEST_ASSERT(count_h264_type(buffer, frame.data_len, 7) > 0);
            RTC_TEST_ASSERT(count_h264_type(buffer, frame.data_len, 8) > 0);
            RTC_TEST_ASSERT(count_h264_type(buffer, frame.data_len, 5) > 0);
        }
        h264_units++;
        if (h264_units >= 4u) {
            break;
        }
    }
    rtc_e2e_sample_close(&reader);
    RTC_TEST_ASSERT(h264_units > 0u);

    return 0;
}
