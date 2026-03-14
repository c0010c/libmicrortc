#include "test_common.h"

#include "datachannel.h"

int main(void) {
  rtc_dc_manager_t m;
  rtc_channel_t channels[2];
  rtc_channel_t *out = 0;

  ASSERT_EQ_INT(0, rtc_dc_init(&m, channels, 2));
  ASSERT_EQ_INT(0, rtc_dc_add(&m, 3, "chat", &out));
  ASSERT_TRUE(out != 0);
  ASSERT_EQ_INT(3, out->id);
  ASSERT_EQ_INT(RTC_CHANNEL_OPEN, out->state);

  ASSERT_TRUE(rtc_dc_find(&m, 3) != 0);
  ASSERT_EQ_INT(0, rtc_dc_close(&m, 3));
  ASSERT_EQ_INT(RTC_CHANNEL_CLOSED, rtc_dc_find(&m, 3)->state);
  return 0;
}
