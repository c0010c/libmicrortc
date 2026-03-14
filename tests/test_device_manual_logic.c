#include "test_common.h"

#include <stdint.h>
#include <string.h>

#include "device_manual_logic.h"

int main(void) {
  char out[128];

  ASSERT_EQ_INT(RTC_DEVICE_INPUT_REMOTE_CANDIDATE, (int)rtc_device_classify_input("candidate:1 1 UDP 1 1.1.1.1 100 typ host\n"));
  ASSERT_EQ_INT(RTC_DEVICE_INPUT_REMOTE_CANDIDATE, (int)rtc_device_classify_input("a=candidate:1 1 UDP 1 1.1.1.1 100 typ host\r\n"));
  ASSERT_EQ_INT(RTC_DEVICE_INPUT_QUIT, (int)rtc_device_classify_input("quit\n"));
  ASSERT_EQ_INT(RTC_DEVICE_INPUT_MESSAGE, (int)rtc_device_classify_input("hello browser\n"));

  ASSERT_EQ_INT(13, rtc_device_copy_message_text("hello browser\r\n", out, sizeof(out)));
  ASSERT_TRUE(strcmp(out, "hello browser") == 0);

  ASSERT_EQ_INT(17, rtc_device_build_open_greeting("chat", out, sizeof(out)));
  ASSERT_TRUE(strcmp(out, "hello from device") == 0);

  ASSERT_EQ_INT(8,
                rtc_device_build_echo_reply((const uint8_t *)"hi", 2, out, sizeof(out)));
  ASSERT_TRUE(strcmp(out, "echo: hi") == 0);

  ASSERT_EQ_INT(15,
                rtc_device_build_echo_reply((const uint8_t *)"12345678901234567890",
                                            20,
                                            out,
                                            16));
  ASSERT_TRUE(strcmp(out, "echo: 123456789") == 0);

  return 0;
}
