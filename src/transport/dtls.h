#ifndef RTC_DTLS_H
#define RTC_DTLS_H

#include <stddef.h>
#include <stdint.h>

#include "rtc/rtc_config.h"
#include "rtc/rtc_types.h"

typedef int (*rtc_dtls_send_packet_fn)(void *user, const uint8_t *data, size_t len);
typedef void (*rtc_dtls_on_appdata_fn)(void *user, const uint8_t *data, size_t len);
typedef void (*rtc_dtls_on_error_fn)(void *user, const char *msg);

typedef struct {
  rtc_dtls_state_t state;
  uint64_t handshake_started_ms;
  uint32_t simulated_handshake_ms;
  int using_mbedtls;
  void *impl;
  rtc_dtls_send_packet_fn send_packet;
  void *io_user;
  rtc_dtls_on_appdata_fn on_appdata;
  rtc_dtls_on_error_fn on_error;
  void *cb_user;
} rtc_dtls_t;

void rtc_dtls_init(rtc_dtls_t *d, const char *cert_pem, const char *key_pem);
void rtc_dtls_deinit(rtc_dtls_t *d);
void rtc_dtls_set_io(rtc_dtls_t *d, rtc_dtls_send_packet_fn send_packet, void *io_user);
void rtc_dtls_set_callbacks(rtc_dtls_t *d,
                            rtc_dtls_on_appdata_fn on_appdata,
                            rtc_dtls_on_error_fn on_error,
                            void *cb_user);
void rtc_dtls_start(rtc_dtls_t *d, uint64_t now_ms);
void rtc_dtls_poll(rtc_dtls_t *d, uint64_t now_ms);
void rtc_dtls_handle_incoming(rtc_dtls_t *d, const uint8_t *pkt, size_t len, uint64_t now_ms);
int rtc_dtls_send_application_data(rtc_dtls_t *d, const uint8_t *data, size_t len);
int rtc_dtls_get_local_fingerprint_sha256(rtc_dtls_t *d, char *out, size_t out_len);
int rtc_dtls_set_peer_transport_id(rtc_dtls_t *d, const uint8_t *id, size_t len);
const char *rtc_dtls_backend_name(const rtc_dtls_t *d);

#endif
