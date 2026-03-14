#include "device_manual_logic.h"

#include <string.h>

static size_t trim_line_len(const char *line) {
  size_t len;
  if (!line) {
    return 0;
  }
  len = strlen(line);
  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
    len--;
  }
  return len;
}

rtc_device_input_kind_t rtc_device_classify_input(const char *line) {
  size_t len;
  if (!line) {
    return RTC_DEVICE_INPUT_INVALID;
  }
  len = trim_line_len(line);
  if (len == 0) {
    return RTC_DEVICE_INPUT_INVALID;
  }
  if (strncmp(line, "candidate:", 10) == 0 || strncmp(line, "a=candidate:", 12) == 0) {
    return RTC_DEVICE_INPUT_REMOTE_CANDIDATE;
  }
  if (len == 4 && strncmp(line, "quit", 4) == 0) {
    return RTC_DEVICE_INPUT_QUIT;
  }
  return RTC_DEVICE_INPUT_MESSAGE;
}

int rtc_device_copy_message_text(const char *line, char *out, size_t out_len) {
  size_t len;
  if (!line || !out || out_len == 0) {
    return -1;
  }
  len = trim_line_len(line);
  if (len == 0 || len >= out_len) {
    return -1;
  }
  memcpy(out, line, len);
  out[len] = '\0';
  return (int)len;
}

int rtc_device_build_open_greeting(const char *label, char *out, size_t out_len) {
  static const char kGreeting[] = "hello from device";
  (void)label;
  if (!out || out_len < sizeof(kGreeting)) {
    return -1;
  }
  memcpy(out, kGreeting, sizeof(kGreeting));
  return (int)(sizeof(kGreeting) - 1u);
}

int rtc_device_build_echo_reply(const uint8_t *data, size_t len, char *out, size_t out_len) {
  static const char kPrefix[] = "echo: ";
  size_t payload_len;
  if ((!data && len > 0) || !out || out_len <= sizeof(kPrefix)) {
    return -1;
  }
  memcpy(out, kPrefix, sizeof(kPrefix) - 1u);
  payload_len = len;
  if (payload_len > out_len - sizeof(kPrefix)) {
    payload_len = out_len - sizeof(kPrefix);
  }
  if (payload_len > 0) {
    memcpy(out + sizeof(kPrefix) - 1u, data, payload_len);
  }
  out[sizeof(kPrefix) - 1u + payload_len] = '\0';
  return (int)((sizeof(kPrefix) - 1u) + payload_len);
}
