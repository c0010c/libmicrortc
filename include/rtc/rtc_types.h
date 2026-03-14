#ifndef RTC_TYPES_H
#define RTC_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  RTC_OK = 0,
  RTC_ERR_INVALID_ARG = -1,
  RTC_ERR_NO_MEMORY = -2,
  RTC_ERR_NOT_FOUND = -3,
  RTC_ERR_STATE = -4,
  RTC_ERR_IO = -5,
  RTC_ERR_NOT_SUPPORTED = -6,
  RTC_ERR_OVERFLOW = -7
} rtc_result_t;

typedef enum {
  RTC_LOG_ERROR = 0,
  RTC_LOG_WARN = 1,
  RTC_LOG_INFO = 2,
  RTC_LOG_DEBUG = 3
} rtc_log_level_t;

typedef enum {
  RTC_ICE_NEW = 0,
  RTC_ICE_GATHERING,
  RTC_ICE_CHECKING,
  RTC_ICE_CONNECTED,
  RTC_ICE_FAILED,
  RTC_ICE_CLOSED
} rtc_ice_state_t;

typedef enum {
  RTC_DTLS_NEW = 0,
  RTC_DTLS_CONNECTING,
  RTC_DTLS_CONNECTED,
  RTC_DTLS_FAILED,
  RTC_DTLS_CLOSED
} rtc_dtls_state_t;

typedef enum {
  RTC_CHANNEL_CONNECTING = 0,
  RTC_CHANNEL_OPEN,
  RTC_CHANNEL_CLOSED
} rtc_channel_state_t;

typedef struct {
  const char *stun_server_ip;
  uint16_t stun_server_port;
  const char *bind_ip;
  uint16_t bind_port;
  size_t mempool_bytes;
  uint16_t max_channels;
  size_t max_message_size;
  rtc_log_level_t log_level;
  const char *dtls_cert_pem;
  const char *dtls_key_pem;
} rtc_config_t;

#ifdef __cplusplus
}
#endif

#endif
