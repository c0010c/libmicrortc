#ifndef RTC_SCTP_ADAPTER_H
#define RTC_SCTP_ADAPTER_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/rtc_config.h"

typedef int (*rtc_sctp_send_packet_fn)(void *user, const uint8_t *data, size_t len);
typedef void (*rtc_sctp_on_open_fn)(void *user, uint16_t stream_id, const char *label);
typedef void (*rtc_sctp_on_message_fn)(void *user,
                                       uint16_t stream_id,
                                       const uint8_t *data,
                                       size_t len);
typedef void (*rtc_sctp_on_error_fn)(void *user, const char *msg);

typedef struct {
  int started;
  uint16_t next_id;
  int using_usrsctp;
  void *impl;

  rtc_sctp_send_packet_fn send_packet;
  void *io_user;

  rtc_sctp_on_open_fn on_open;
  rtc_sctp_on_message_fn on_message;
  rtc_sctp_on_error_fn on_error;
  void *cb_user;
} rtc_sctp_adapter_t;

void rtc_sctp_init(rtc_sctp_adapter_t *s);
void rtc_sctp_deinit(rtc_sctp_adapter_t *s);
void rtc_sctp_set_io(rtc_sctp_adapter_t *s, rtc_sctp_send_packet_fn fn, void *io_user);
void rtc_sctp_set_callbacks(rtc_sctp_adapter_t *s,
                            rtc_sctp_on_open_fn on_open,
                            rtc_sctp_on_message_fn on_message,
                            rtc_sctp_on_error_fn on_error,
                            void *cb_user);
void rtc_sctp_start(rtc_sctp_adapter_t *s);
void rtc_sctp_handle_incoming(rtc_sctp_adapter_t *s, const uint8_t *pkt, size_t len);
int rtc_sctp_open_channel(rtc_sctp_adapter_t *s, const char *label, uint16_t *id);
int rtc_sctp_send(rtc_sctp_adapter_t *s, uint16_t id, const uint8_t *data, size_t len);
int rtc_sctp_close_channel(rtc_sctp_adapter_t *s, uint16_t id);

#endif
