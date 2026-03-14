#ifndef RTC_DEVICE_MANUAL_LOGIC_H
#define RTC_DEVICE_MANUAL_LOGIC_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
  RTC_DEVICE_INPUT_INVALID = 0,
  RTC_DEVICE_INPUT_QUIT = 1,
  RTC_DEVICE_INPUT_REMOTE_CANDIDATE = 2,
  RTC_DEVICE_INPUT_MESSAGE = 3,
} rtc_device_input_kind_t;

rtc_device_input_kind_t rtc_device_classify_input(const char *line);
int rtc_device_copy_message_text(const char *line, char *out, size_t out_len);
int rtc_device_build_open_greeting(const char *label, char *out, size_t out_len);
int rtc_device_build_echo_reply(const uint8_t *data, size_t len, char *out, size_t out_len);

#endif
