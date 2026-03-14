#include "dtls/rtc_dtls.h"

#include <string.h>

#include "dtls/rtc_dtls_local_cert.h"

#if defined(MBEDTLS_DEBUG_C)
#include <mbedtls/debug.h>
#endif

#define RTC_DTLS_KEY_LABEL "EXTRACTOR-dtls_srtp"

static const char *g_rtc_dtls_local_fingerprint_sha256 =
    "B3:7D:98:A3:34:67:93:66:D0:17:9B:08:C5:0E:8B:94:57:B8:DA:E7:F2:A9:05:16:8D:A2:E6:F1:E0:49:A1:EB";

static const mbedtls_ssl_srtp_profile g_rtc_dtls_profiles[] = {
    MBEDTLS_TLS_SRTP_AES128_CM_HMAC_SHA1_80,
    MBEDTLS_TLS_SRTP_UNSET,
};

static void rtc_dtls_mailbox_reset(rtc_dtls_mailbox_t *box) {
  if (!box) {
    return;
  }
  memset(box, 0, sizeof(*box));
}

static int rtc_dtls_mailbox_push(rtc_dtls_mailbox_t *box,
                                 const uint8_t *data,
                                 uint16_t len) {
  rtc_dtls_datagram_t *slot;
  if (!box || !data || len == 0u || len > RTC_CFG_DTLS_MAX_DATAGRAM) {
    return 0;
  }
  if (box->count >= RTC_CFG_DTLS_MAILBOX_CAP) {
    return 0;
  }

  slot = &box->slots[box->tail];
  slot->len = len;
  memcpy(slot->data, data, len);

  box->tail = (uint16_t)((box->tail + 1u) % RTC_CFG_DTLS_MAILBOX_CAP);
  box->count++;
  return 1;
}

static int rtc_dtls_mailbox_pop(rtc_dtls_mailbox_t *box,
                                uint8_t *out,
                                uint16_t cap,
                                uint16_t *out_len) {
  rtc_dtls_datagram_t *slot;
  if (!box || !out || !out_len) {
    return 0;
  }
  if (box->count == 0u) {
    return 0;
  }

  slot = &box->slots[box->head];
  if (slot->len > cap) {
    return 0;
  }

  memcpy(out, slot->data, slot->len);
  *out_len = slot->len;

  box->head = (uint16_t)((box->head + 1u) % RTC_CFG_DTLS_MAILBOX_CAP);
  box->count--;
  return 1;
}

#if defined(MBEDTLS_DEBUG_C)
static void rtc_dtls_debug_log(void *ctx,
                               int level,
                               const char *file,
                               int line,
                               const char *str) {
  (void)ctx;
  (void)level;
  (void)file;
  (void)line;
  (void)str;
}
#endif

static int rtc_dtls_send_cb(void *ctx, const unsigned char *buf, size_t len) {
  rtc_dtls_endpoint_t *ep = (rtc_dtls_endpoint_t *)ctx;
  if (!ep || !buf) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  if (len == 0u || len > RTC_CFG_DTLS_MAX_DATAGRAM) {
    return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
  }
  if (!rtc_dtls_mailbox_push(ep->outbox, buf, (uint16_t)len)) {
    return MBEDTLS_ERR_SSL_WANT_WRITE;
  }
  return (int)len;
}

static int rtc_dtls_recv_cb(void *ctx, unsigned char *buf, size_t len) {
  rtc_dtls_endpoint_t *ep = (rtc_dtls_endpoint_t *)ctx;
  uint16_t packet_len = 0;

  if (!ep || !buf) {
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  if (len == 0u || len > UINT16_MAX) {
    return MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL;
  }

  if (!rtc_dtls_mailbox_pop(ep->inbox, buf, (uint16_t)len, &packet_len)) {
    return MBEDTLS_ERR_SSL_WANT_READ;
  }
  return (int)packet_len;
}

static void rtc_dtls_export_keys_cb(void *custom_data,
                                    mbedtls_ssl_key_export_type secret_type,
                                    const unsigned char *secret,
                                    size_t secret_len,
                                    const unsigned char client_random[32],
                                    const unsigned char server_random[32],
                                    mbedtls_tls_prf_types tls_prf_type) {
  rtc_dtls_endpoint_t *ep = (rtc_dtls_endpoint_t *)custom_data;

  if (!ep || !secret || !client_random || !server_random) {
    return;
  }
  if (secret_type != MBEDTLS_SSL_KEY_EXPORT_TLS12_MASTER_SECRET) {
    return;
  }
  if (secret_len != sizeof(ep->exported.master_secret)) {
    memset(&ep->exported, 0, sizeof(ep->exported));
    return;
  }

  memcpy(ep->exported.master_secret, secret, secret_len);
  memcpy(ep->exported.rand_bytes, client_random, 32u);
  memcpy(ep->exported.rand_bytes + 32u, server_random, 32u);
  ep->exported.prf = tls_prf_type;
  ep->exported.ready = 1u;
}

static void rtc_dtls_endpoint_init(rtc_dtls_endpoint_t *ep) {
  if (!ep) {
    return;
  }

  memset(ep, 0, sizeof(*ep));
  mbedtls_ssl_init(&ep->ssl);
  mbedtls_ssl_config_init(&ep->conf);
  mbedtls_x509_crt_init(&ep->cert);
  mbedtls_pk_init(&ep->key);
  mbedtls_entropy_init(&ep->entropy);
  mbedtls_ctr_drbg_init(&ep->drbg);
  memset(&ep->timer, 0, sizeof(ep->timer));
}

static void rtc_dtls_endpoint_deinit(rtc_dtls_endpoint_t *ep) {
  if (!ep) {
    return;
  }

  mbedtls_ssl_free(&ep->ssl);
  mbedtls_ssl_config_free(&ep->conf);
  mbedtls_x509_crt_free(&ep->cert);
  mbedtls_pk_free(&ep->key);
  mbedtls_ctr_drbg_free(&ep->drbg);
  mbedtls_entropy_free(&ep->entropy);

  memset(ep, 0, sizeof(*ep));
}

static rtc_result_t rtc_dtls_endpoint_setup(rtc_dtls_endpoint_t *ep,
                                            uint8_t is_server,
                                            uint8_t debug_enabled) {
  const char *pers = is_server ? "rtc-dtls-server" : "rtc-dtls-client";
  int role = is_server ? MBEDTLS_SSL_IS_SERVER : MBEDTLS_SSL_IS_CLIENT;

  if (!ep) {
    return RTC_ERR_INVALID_ARG;
  }

  if (mbedtls_ctr_drbg_seed(&ep->drbg, mbedtls_entropy_func, &ep->entropy,
                            (const unsigned char *)pers,
                            strlen(pers)) != 0) {
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  if (mbedtls_x509_crt_parse(
          &ep->cert,
          (const unsigned char *)g_rtc_dtls_local_cert_pem,
          sizeof(g_rtc_dtls_local_cert_pem)) != 0) {
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  if (mbedtls_pk_parse_key(
          &ep->key,
          (const unsigned char *)g_rtc_dtls_local_key_pem,
          sizeof(g_rtc_dtls_local_key_pem),
          NULL,
          0,
          mbedtls_ctr_drbg_random,
          &ep->drbg) != 0) {
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  if (mbedtls_ssl_config_defaults(&ep->conf,
                                  role,
                                  MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  mbedtls_ssl_conf_authmode(&ep->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
  mbedtls_ssl_conf_rng(&ep->conf, mbedtls_ctr_drbg_random, &ep->drbg);

#if defined(MBEDTLS_DEBUG_C)
  if (debug_enabled) {
    mbedtls_debug_set_threshold(2);
    mbedtls_ssl_conf_dbg(&ep->conf, rtc_dtls_debug_log, NULL);
  }
#endif

  if (mbedtls_ssl_conf_own_cert(&ep->conf, &ep->cert, &ep->key) != 0) {
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  if (mbedtls_ssl_conf_dtls_srtp_protection_profiles(&ep->conf,
                                                      g_rtc_dtls_profiles) != 0) {
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  mbedtls_ssl_conf_dtls_cookies(&ep->conf, NULL, NULL, NULL);

  if (mbedtls_ssl_setup(&ep->ssl, &ep->conf) != 0) {
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  mbedtls_ssl_set_export_keys_cb(&ep->ssl, rtc_dtls_export_keys_cb, ep);
  mbedtls_ssl_set_bio(&ep->ssl, ep, rtc_dtls_send_cb, rtc_dtls_recv_cb, NULL);
  mbedtls_ssl_set_timer_cb(&ep->ssl, &ep->timer, mbedtls_timing_set_delay,
                           mbedtls_timing_get_delay);
  mbedtls_ssl_set_mtu(&ep->ssl, RTC_CFG_MTU);

  ep->in_use = 1u;
  ep->is_server = is_server;
  return RTC_OK;
}

static rtc_result_t rtc_dtls_step_endpoint(rtc_dtls_endpoint_t *ep,
                                           uint8_t *out_progress) {
  int r;
  if (!ep || !out_progress) {
    return RTC_ERR_INVALID_ARG;
  }

  if (mbedtls_ssl_is_handshake_over(&ep->ssl)) {
    return RTC_OK;
  }

  r = mbedtls_ssl_handshake(&ep->ssl);
  if (r == 0) {
    *out_progress = 1u;
    return RTC_OK;
  }
  if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE ||
      r == MBEDTLS_ERR_SSL_TIMEOUT) {
    return RTC_OK;
  }
#ifdef MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS
  if (r == MBEDTLS_ERR_SSL_CRYPTO_IN_PROGRESS) {
    return RTC_OK;
  }
#endif
  return RTC_ERR_DTLS_HANDSHAKE_FAILED;
}

static uint8_t rtc_dtls_handshake_done(const rtc_dtls_ctx_t *ctx) {
  if (!ctx) {
    return 0u;
  }
  return (uint8_t)(mbedtls_ssl_is_handshake_over((mbedtls_ssl_context *)&ctx->client.ssl) &&
                   mbedtls_ssl_is_handshake_over((mbedtls_ssl_context *)&ctx->server.ssl));
}

static rtc_result_t rtc_dtls_derive_keying(rtc_dtls_ctx_t *ctx) {
  uint8_t keying_material[60];
  const rtc_dtls_exported_t *exp;

  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }

  exp = &ctx->client.exported;
  if (!exp->ready) {
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  if (mbedtls_ssl_tls_prf(exp->prf,
                          exp->master_secret,
                          sizeof(exp->master_secret),
                          RTC_DTLS_KEY_LABEL,
                          exp->rand_bytes,
                          sizeof(exp->rand_bytes),
                          keying_material,
                          sizeof(keying_material)) != 0) {
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  memset(&ctx->keying_material, 0, sizeof(ctx->keying_material));
  ctx->keying_material.key_len = 30u;
  ctx->keying_material.profile = RTC_SRTP_PROFILE_AES128_CM_HMAC_SHA1_80;

  memcpy(ctx->keying_material.client_write_key, keying_material, 16u);
  memcpy(ctx->keying_material.server_write_key, keying_material + 16u, 16u);
  memcpy(ctx->keying_material.client_write_key + 16u, keying_material + 32u, 14u);
  memcpy(ctx->keying_material.server_write_key + 16u, keying_material + 46u, 14u);

  return RTC_OK;
}

static void rtc_dtls_fail(rtc_dtls_ctx_t *ctx,
                          rtc_dtls_event_t *out_event,
                          rtc_result_t error,
                          uint32_t now_ms) {
  if (!ctx || !out_event) {
    return;
  }
  ctx->handshake_elapsed_ms = now_ms - ctx->handshake_start_ms;
  ctx->state = RTC_DTLS_STATE_FAILED;
  ctx->last_error = error;
  out_event->failed = 1u;
  out_event->error = error;
}

void rtc_dtls_init(rtc_dtls_ctx_t *ctx) {
  if (!ctx) {
    return;
  }

  memset(ctx, 0, sizeof(*ctx));
  ctx->state = RTC_DTLS_STATE_NEW;
  ctx->handshake_timeout_ms = RTC_CFG_DTLS_HANDSHAKE_TIMEOUT_MS;
  ctx->handshake_max_retries = 16u;
  ctx->last_error = RTC_OK;

  rtc_dtls_mailbox_reset(&ctx->client_inbox);
  rtc_dtls_mailbox_reset(&ctx->server_inbox);
  rtc_dtls_endpoint_init(&ctx->client);
  rtc_dtls_endpoint_init(&ctx->server);
}

void rtc_dtls_configure(rtc_dtls_ctx_t *ctx,
                        uint32_t peer_id,
                        uint16_t handshake_timeout_ms,
                        uint16_t handshake_max_retries,
                        uint8_t debug_enabled) {
  if (!ctx) {
    return;
  }

  ctx->peer_id = peer_id;
  if (handshake_timeout_ms == 0u) {
    ctx->handshake_timeout_ms = RTC_CFG_DTLS_HANDSHAKE_TIMEOUT_MS;
  } else {
    ctx->handshake_timeout_ms = handshake_timeout_ms;
  }
  if (handshake_max_retries == 0u) {
    ctx->handshake_max_retries = 16u;
  } else {
    ctx->handshake_max_retries = handshake_max_retries;
  }
  ctx->debug_enabled = (uint8_t)(debug_enabled ? 1u : 0u);
}

void rtc_dtls_deinit(rtc_dtls_ctx_t *ctx) {
  if (!ctx) {
    return;
  }

  rtc_dtls_endpoint_deinit(&ctx->client);
  rtc_dtls_endpoint_deinit(&ctx->server);
  rtc_dtls_mailbox_reset(&ctx->client_inbox);
  rtc_dtls_mailbox_reset(&ctx->server_inbox);

  ctx->state = RTC_DTLS_STATE_NEW;
  ctx->handshake_start_ms = 0u;
  ctx->handshake_elapsed_ms = 0u;
  ctx->handshake_attempts = 0u;
  ctx->last_error = RTC_OK;
  memset(&ctx->keying_material, 0, sizeof(ctx->keying_material));

  rtc_dtls_endpoint_init(&ctx->client);
  rtc_dtls_endpoint_init(&ctx->server);
}

rtc_result_t rtc_dtls_start(rtc_dtls_ctx_t *ctx, uint32_t now_ms) {
  rtc_result_t r;

  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }
  if (ctx->state == RTC_DTLS_STATE_CONNECTED ||
      ctx->state == RTC_DTLS_STATE_HANDSHAKE) {
    return RTC_OK;
  }
  if (ctx->state == RTC_DTLS_STATE_FAILED) {
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  rtc_dtls_endpoint_deinit(&ctx->client);
  rtc_dtls_endpoint_deinit(&ctx->server);
  rtc_dtls_mailbox_reset(&ctx->client_inbox);
  rtc_dtls_mailbox_reset(&ctx->server_inbox);
  rtc_dtls_endpoint_init(&ctx->client);
  rtc_dtls_endpoint_init(&ctx->server);

  ctx->client.inbox = &ctx->client_inbox;
  ctx->client.outbox = &ctx->server_inbox;
  ctx->server.inbox = &ctx->server_inbox;
  ctx->server.outbox = &ctx->client_inbox;

  r = rtc_dtls_endpoint_setup(&ctx->client, 0u, ctx->debug_enabled);
  if (r != RTC_OK) {
    rtc_dtls_endpoint_deinit(&ctx->client);
    rtc_dtls_endpoint_deinit(&ctx->server);
    ctx->state = RTC_DTLS_STATE_FAILED;
    ctx->last_error = RTC_ERR_DTLS_HANDSHAKE_FAILED;
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  r = rtc_dtls_endpoint_setup(&ctx->server, 1u, ctx->debug_enabled);
  if (r != RTC_OK) {
    rtc_dtls_endpoint_deinit(&ctx->client);
    rtc_dtls_endpoint_deinit(&ctx->server);
    ctx->state = RTC_DTLS_STATE_FAILED;
    ctx->last_error = RTC_ERR_DTLS_HANDSHAKE_FAILED;
    return RTC_ERR_DTLS_HANDSHAKE_FAILED;
  }

  ctx->state = RTC_DTLS_STATE_HANDSHAKE;
  ctx->handshake_start_ms = now_ms;
  ctx->handshake_elapsed_ms = 0u;
  ctx->handshake_attempts = 0u;
  ctx->last_error = RTC_OK;
  memset(&ctx->keying_material, 0, sizeof(ctx->keying_material));

  return RTC_OK;
}

void rtc_dtls_tick(rtc_dtls_ctx_t *ctx, uint32_t now_ms, rtc_dtls_event_t *out_event) {
  uint8_t round;
  uint8_t progress;
  rtc_result_t r;

  if (!ctx || !out_event) {
    return;
  }
  memset(out_event, 0, sizeof(*out_event));

  if (ctx->state != RTC_DTLS_STATE_HANDSHAKE) {
    return;
  }

  if ((now_ms - ctx->handshake_start_ms) > ctx->handshake_timeout_ms) {
    rtc_dtls_fail(ctx, out_event, RTC_ERR_DTLS_HANDSHAKE_FAILED, now_ms);
    return;
  }

  if (ctx->handshake_attempts >= ctx->handshake_max_retries) {
    rtc_dtls_fail(ctx, out_event, RTC_ERR_DTLS_HANDSHAKE_FAILED, now_ms);
    return;
  }

  ctx->handshake_attempts++;

  for (round = 0; round < 6u; ++round) {
    progress = 0u;

    r = rtc_dtls_step_endpoint(&ctx->client, &progress);
    if (r != RTC_OK) {
      rtc_dtls_fail(ctx, out_event, RTC_ERR_DTLS_HANDSHAKE_FAILED, now_ms);
      return;
    }

    r = rtc_dtls_step_endpoint(&ctx->server, &progress);
    if (r != RTC_OK) {
      rtc_dtls_fail(ctx, out_event, RTC_ERR_DTLS_HANDSHAKE_FAILED, now_ms);
      return;
    }

    if (rtc_dtls_handshake_done(ctx)) {
      r = rtc_dtls_derive_keying(ctx);
      if (r != RTC_OK) {
        rtc_dtls_fail(ctx, out_event, RTC_ERR_DTLS_HANDSHAKE_FAILED, now_ms);
        return;
      }
      ctx->state = RTC_DTLS_STATE_CONNECTED;
      ctx->handshake_elapsed_ms = now_ms - ctx->handshake_start_ms;
      out_event->connected = 1u;
      return;
    }

    if (!progress && ctx->client_inbox.count == 0u && ctx->server_inbox.count == 0u) {
      break;
    }
  }
}

const rtc_dtls_key_material_t *rtc_dtls_get_key_material(const rtc_dtls_ctx_t *ctx) {
  if (!ctx || ctx->state != RTC_DTLS_STATE_CONNECTED ||
      ctx->keying_material.key_len == 0u) {
    return NULL;
  }
  return &ctx->keying_material;
}

rtc_result_t rtc_dtls_get_last_error(const rtc_dtls_ctx_t *ctx) {
  if (!ctx) {
    return RTC_ERR_INVALID_ARG;
  }
  return ctx->last_error;
}

uint32_t rtc_dtls_get_handshake_elapsed_ms(const rtc_dtls_ctx_t *ctx, uint32_t now_ms) {
  if (!ctx) {
    return 0u;
  }
  if (ctx->state == RTC_DTLS_STATE_CONNECTED || ctx->state == RTC_DTLS_STATE_FAILED) {
    return ctx->handshake_elapsed_ms;
  }
  if (ctx->state == RTC_DTLS_STATE_HANDSHAKE) {
    return now_ms - ctx->handshake_start_ms;
  }
  return 0u;
}

const char *rtc_dtls_get_local_fingerprint_sha256(void) {
  return g_rtc_dtls_local_fingerprint_sha256;
}
