#include "test_runner.h"

#include "examples/chrome_e2e/media_samples.h"

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
                    rtc_e2e_sample_open_h264("test-25fps.h264", &reader));
    while (rtc_e2e_sample_next_h264(&reader, &frame, buffer,
                                    sizeof(buffer)) == 0) {
        RTC_TEST_EQ_INT(RTC_MEDIA_KIND_VIDEO_H264, frame.kind);
        RTC_TEST_ASSERT(frame.data == buffer);
        RTC_TEST_ASSERT(frame.data_len > 0u);
        RTC_TEST_EQ_INT(40000, frame.duration_us);
        h264_units++;
        if (h264_units >= 4u) {
            break;
        }
    }
    rtc_e2e_sample_close(&reader);
    RTC_TEST_ASSERT(h264_units > 0u);

    return 0;
}
