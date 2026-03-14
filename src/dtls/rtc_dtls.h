#ifndef RTC_DTLS_H_
#define RTC_DTLS_H_

#include <stdint.h>

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ssl.h>
#include <mbedtls/timing.h>
#include <mbedtls/x509_crt.h>

#include "rtc/rtc.h"

typedef enum rtc_srtp_profile {
  RTC_SRTP_PROFILE_AES128_CM_HMAC_SHA1_80 = 1
} rtc_srtp_profile_t;

typedef struct rtc_dtls_key_material {
  uint8_t client_write_key[30];
  uint8_t server_write_key[30];
  uint8_t key_len;
  rtc_srtp_profile_t profile;
} rtc_dtls_key_material_t;

typedef struct rtc_dtls_event {
  uint8_t connected;
  uint8_t failed;
  rtc_result_t error;
} rtc_dtls_event_t;

typedef struct rtc_dtls_datagram {
  uint16_t len;
  uint8_t data[RTC_CFG_DTLS_MAX_DATAGRAM];
} rtc_dtls_datagram_t;

typedef struct rtc_dtls_mailbox {
  rtc_dtls_datagram_t slots[RTC_CFG_DTLS_MAILBOX_CAP];
  uint16_t head;
  uint16_t tail;
  uint16_t count;
} rtc_dtls_mailbox_t;

typedef struct rtc_dtls_exported {
  uint8_t ready;
  uint8_t master_secret[48];
  uint8_t rand_bytes[64];
  mbedtls_tls_prf_types prf;
} rtc_dtls_exported_t;

typedef struct rtc_dtls_endpoint {
  uint8_t in_use;
  uint8_t is_server;
  mbedtls_ssl_context ssl;
  mbedtls_ssl_config conf;
  mbedtls_x509_crt cert;
  mbedtls_pk_context key;
  mbedtls_entropy_context entropy;
  mbedtls_ctr_drbg_context drbg;
  mbedtls_timing_delay_context timer;
  rtc_dtls_mailbox_t *inbox;
  rtc_dtls_mailbox_t *outbox;
  rtc_dtls_exported_t exported;
} rtc_dtls_endpoint_t;

typedef struct rtc_dtls_ctx {
  rtc_dtls_state_t state;
  uint32_t peer_id;
  uint32_t handshake_start_ms;
  uint32_t handshake_elapsed_ms;
  uint16_t handshake_timeout_ms;
  uint16_t handshake_max_retries;
  uint16_t handshake_attempts;
  uint8_t debug_enabled;
  rtc_result_t last_error;
  rtc_dtls_key_material_t keying_material;
  rtc_dtls_endpoint_t client;
  rtc_dtls_endpoint_t server;
  rtc_dtls_mailbox_t client_inbox;
  rtc_dtls_mailbox_t server_inbox;
} rtc_dtls_ctx_t;

void rtc_dtls_init(rtc_dtls_ctx_t *ctx);
void rtc_dtls_configure(rtc_dtls_ctx_t *ctx, uint32_t peer_id,
                        uint16_t handshake_timeout_ms,
                        uint16_t handshake_max_retries,
                        uint8_t debug_enabled);
void rtc_dtls_deinit(rtc_dtls_ctx_t *ctx);
rtc_result_t rtc_dtls_start(rtc_dtls_ctx_t *ctx, uint32_t now_ms);
void rtc_dtls_tick(rtc_dtls_ctx_t *ctx, uint32_t now_ms, rtc_dtls_event_t *out_event);
const rtc_dtls_key_material_t *rtc_dtls_get_key_material(const rtc_dtls_ctx_t *ctx);
rtc_result_t rtc_dtls_get_last_error(const rtc_dtls_ctx_t *ctx);
uint32_t rtc_dtls_get_handshake_elapsed_ms(const rtc_dtls_ctx_t *ctx, uint32_t now_ms);

#endif  // RTC_DTLS_H_
